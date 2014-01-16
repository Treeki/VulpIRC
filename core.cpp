#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <gnutls/gnutls.h>

#include "buffer.h"
#include "dns.h"

#define CLIENT_LIMIT 100
#define SERVER_LIMIT 20

#define SESSION_KEEPALIVE 5

struct SocketRWCommon {
	Buffer inputBuf, outputBuf;

	enum ConnState {
		CS_DISCONNECTED = 0,
		CS_WAITING_DNS = 1, // server only
		CS_WAITING_CONNECT = 2, // server only
		CS_TLS_HANDSHAKE = 3,
		CS_CONNECTED = 4
	};
	ConnState state;

	int sock;
	gnutls_session_t tls;
	bool isTls, gnutlsSessionInited;

	SocketRWCommon() {
		sock = -1;
		state = CS_DISCONNECTED;
		isTls = false;
		gnutlsSessionInited = false;
	}

	~SocketRWCommon() {
		close();
	}

	void tryTLSHandshake();
	virtual void close();
};

struct Client : SocketRWCommon {
	time_t deadTime;

	void startService(int _sock, bool withTls);
	void processInput();
	void close();

private:
	void handleLine(char *line, int size);
};

struct Server : SocketRWCommon {
	char hostname[256];
	int port;
	int dnsQueryId;

	Server() {
		dnsQueryId = -1;
	}

	~Server() {
		if (dnsQueryId != -1)
			DNS::closeQuery(dnsQueryId);
	}

	void beginConnect();
	void tryConnectPhase();
	void connectionSuccessful();

	void close();

	void processInput();
private:
	void handleLine(char *line, int size);
};


Client *clients[CLIENT_LIMIT];
Server *servers[SERVER_LIMIT];
int clientCount, serverCount;
bool quitFlag = false;

static gnutls_dh_params_t dh_params;
static gnutls_certificate_credentials_t serverCreds, clientCreds;


void SocketRWCommon::tryTLSHandshake() {
	int hsRet = gnutls_handshake(tls);
	if (gnutls_error_is_fatal(hsRet)) {
		printf("[SocketRWCommon::tryTLSHandshake] gnutls_handshake borked\n");
		gnutls_perror(hsRet);
		close();
		return;
	}

	if (hsRet == GNUTLS_E_SUCCESS) {
		// We're in !!
		state = CS_CONNECTED;

		inputBuf.clear();
		outputBuf.clear();

		printf("[SocketRWCommon connected via SSL!]\n");
	}
}

void SocketRWCommon::close() {
	if (sock != -1) {
		if (gnutlsSessionInited)
			gnutls_bye(tls, GNUTLS_SHUT_RDWR);
		shutdown(sock, SHUT_RDWR);
		::close(sock);
	}

	sock = -1;
	inputBuf.clear();
	outputBuf.clear();
	state = CS_DISCONNECTED;

	if (gnutlsSessionInited) {
		gnutls_deinit(tls);
		gnutlsSessionInited = false;
	}
}


void Client::startService(int _sock, bool withTls) {
	close();

	sock = _sock;

	if (withTls) {
		int initRet = gnutls_init(&tls, GNUTLS_SERVER);
		if (initRet != GNUTLS_E_SUCCESS) {
			printf("[Client::startService] gnutls_init borked\n");
			gnutls_perror(initRet);
			close();
			return;
		}

		// TODO: error check this
		int ret;
		const char *errPos;

		ret = gnutls_priority_set_direct(tls, "PERFORMANCE:%SERVER_PRECEDENCE", &errPos);
		if (ret != GNUTLS_E_SUCCESS) {
			printf("gnutls_priority_set_direct failure: %s\n", gnutls_strerror(ret));
			close();
			return;
		}

		ret = gnutls_credentials_set(tls, GNUTLS_CRD_CERTIFICATE, clientCreds);
		if (ret != GNUTLS_E_SUCCESS) {
			printf("gnutls_credentials_set failure: %s\n", gnutls_strerror(ret));
			close();
			return;
		}

		gnutls_certificate_server_set_request(tls, GNUTLS_CERT_IGNORE);

		gnutls_transport_set_int(tls, sock);

		gnutlsSessionInited = true;

		state = CS_TLS_HANDSHAKE;
	} else {
		state = CS_CONNECTED;
	}
}

void Client::close() {
	SocketRWCommon::close();
	// TODO: add canSafelyKeepAlive var, check it here, to kill
	// never-authed conns instantly
	deadTime = time(NULL) + SESSION_KEEPALIVE;
}

void Client::handleLine(char *line, int size) {
	if (strncmp(line, "all ", 4) == 0) {
		for (int i = 0; i < clientCount; i++) {
			clients[i]->outputBuf.append(&line[4], size - 4);
			clients[i]->outputBuf.append("\n", 1);
		}
	} else if (strcmp(line, "quit") == 0) {
		quitFlag = true;
	} else if (strncmp(line, "resolve ", 8) == 0) {
		DNS::makeQuery(&line[8]);
	} else if (strncmp(&line[1], "ddsrv ", 6) == 0) {
		servers[serverCount] = new Server;
		strcpy(servers[serverCount]->hostname, &line[7]);
		servers[serverCount]->port = 1191;
		servers[serverCount]->isTls = (line[0] == 's');
		serverCount++;
		outputBuf.append("Your wish is my command!\n", 25);
	} else if (strncmp(line, "connsrv", 7) == 0) {
		int sid = line[7] - '0';
		servers[sid]->beginConnect();
	} else if (line[0] >= '0' && line[0] <= '9') {
		int sid = line[0] - '0';
		servers[sid]->outputBuf.append(&line[1], size - 1);
		servers[sid]->outputBuf.append("\r\n", 2);
	}
}

void Client::processInput() {
	// Try to process as many lines as we can
	// This function will be changed to custom protocol eventually
	char *buf = inputBuf.data();
	int bufSize = inputBuf.size();
	int lineBegin = 0, pos = 0;

	while (pos < bufSize) {
		if (buf[pos] == '\r' || buf[pos] == '\n') {
			if (pos > lineBegin) {
				buf[pos] = 0;
				handleLine(&buf[lineBegin], pos - lineBegin);
			}

			lineBegin = pos + 1;
		}

		pos++;
	}

	// If we managed to handle anything, lop it off the buffer
	inputBuf.trimFromStart(pos);
}


void Server::handleLine(char *line, int size) {
	for (int i = 0; i < clientCount; i++) {
		clients[i]->outputBuf.append(line, size);
		clients[i]->outputBuf.append("\n", 1);
	}
}
void Server::processInput() {
	// Try to process as many lines as we can
	char *buf = inputBuf.data();
	int bufSize = inputBuf.size();
	int lineBegin = 0, pos = 0;

	while (pos < bufSize) {
		if (buf[pos] == '\r' || buf[pos] == '\n') {
			if (pos > lineBegin) {
				buf[pos] = 0;
				handleLine(&buf[lineBegin], pos - lineBegin);
			}

			lineBegin = pos + 1;
		}

		pos++;
	}

	// If we managed to handle anything, lop it off the buffer
	inputBuf.trimFromStart(pos);
}



void Server::beginConnect() {
	if (state == CS_DISCONNECTED) {
		DNS::closeQuery(dnsQueryId); // just in case
		dnsQueryId = DNS::makeQuery(hostname);

		if (dnsQueryId == -1) {
			// TODO: better error reporting
			printf("DNS query failed!\n");
		} else {
			state = CS_WAITING_DNS;
		}
	}
}

void Server::tryConnectPhase() {
	if (state == CS_WAITING_DNS) {
		in_addr result;
		bool isError;

		if (DNS::checkQuery(dnsQueryId, &result, &isError)) {
			DNS::closeQuery(dnsQueryId);
			dnsQueryId = -1;

			if (isError) {
				printf("DNS query failed at phase 2!\n");
				state = CS_DISCONNECTED;
			} else {
				// OK, if there was no error, we can go ahead and do this...

				sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (sock == -1) {
					perror("[Server] Failed to socket()");
					close();
					return;
				}

				int opts = fcntl(sock, F_GETFL);
				if (opts < 0) {
					perror("[Server] Could not get fcntl options");
					close();
					return;
				}
				opts |= O_NONBLOCK;
				if (fcntl(sock, F_SETFL, opts) == -1) {
					perror("[Server] Could not set fcntl options");
					close();
					return;
				}

				// We have our non-blocking socket, let's try connecting!
				sockaddr_in outAddr;
				outAddr.sin_family = AF_INET;
				outAddr.sin_port = htons(port);
				outAddr.sin_addr.s_addr = result.s_addr;

				if (connect(sock, (sockaddr *)&outAddr, sizeof(outAddr)) == -1) {
					if (errno == EINPROGRESS) {
						state = CS_WAITING_CONNECT;
					} else {
						perror("[Server] Could not connect");
					}
				} else {
					// Whoa, we're connected? Neat.
					connectionSuccessful();
				}
			}
		}
	}
}

void Server::connectionSuccessful() {
	state = CS_CONNECTED;

	inputBuf.clear();
	outputBuf.clear();

	// Do we need to do any TLS junk?
	if (isTls) {
		int initRet = gnutls_init(&tls, GNUTLS_CLIENT);
		if (initRet != GNUTLS_E_SUCCESS) {
			printf("[Server::connectionSuccessful] gnutls_init borked\n");
			gnutls_perror(initRet);
			close();
			return;
		}

		// TODO: error check this
		const char *errPos;
		gnutls_priority_set_direct(tls, "NORMAL", &errPos);

		gnutls_credentials_set(tls, GNUTLS_CRD_CERTIFICATE, serverCreds);

		gnutls_transport_set_int(tls, sock);

		gnutlsSessionInited = true;
		state = CS_TLS_HANDSHAKE;
	}
}

void Server::close() {
	SocketRWCommon::close();

	if (dnsQueryId != -1) {
		DNS::closeQuery(dnsQueryId);
		dnsQueryId = -1;
	}
}


int main(int argc, char **argv) {
	clientCount = 0;
	for (int i = 0; i < CLIENT_LIMIT; i++)
		clients[i] = NULL;

	int ret;
	ret = gnutls_global_init();
	if (ret != GNUTLS_E_SUCCESS) {
		printf("gnutls_global_init failure: %s\n", gnutls_strerror(ret));
		return 1;
	}

	unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_LEGACY);

	ret = gnutls_dh_params_init(&dh_params);
	if (ret != GNUTLS_E_SUCCESS) {
		printf("dh_params_init failure: %s\n", gnutls_strerror(ret));
		return 1;
	}

	ret = gnutls_dh_params_generate2(dh_params, bits);
	if (ret != GNUTLS_E_SUCCESS) {
		printf("dh_params_generate2 failure: %s\n", gnutls_strerror(ret));
		return 1;
	}

	gnutls_certificate_allocate_credentials(&clientCreds);
	ret = gnutls_certificate_set_x509_key_file(clientCreds, "ssl_test.crt", "ssl_test.key", GNUTLS_X509_FMT_PEM);
	if (ret != GNUTLS_E_SUCCESS) {
		printf("set_x509_key_file failure: %s\n", gnutls_strerror(ret));
		return 1;
	}
	gnutls_certificate_set_dh_params(clientCreds, dh_params);

	gnutls_certificate_allocate_credentials(&serverCreds);

	DNS::start();


	// prepare the listen socket
	int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == -1) {
		perror("Could not create the listener socket");
		return 1;
	}

	int v = 1;
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)) == -1) {
		perror("Could not set SO_REUSEADDR");
		return 1;
	}

	sockaddr_in listenAddr;
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_port = htons(5454);
	listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listener, (sockaddr *)&listenAddr, sizeof(listenAddr)) == -1) {
		perror("Could not bind to the listener socket");
		return 1;
	}

	int opts = fcntl(listener, F_GETFL);
	if (opts < 0) {
		perror("Could not get fcntl options\n");
		return 1;
	}
	opts |= O_NONBLOCK;
	if (fcntl(listener, F_SETFL, opts) == -1) {
		perror("Could not set fcntl options\n");
		return 1;
	}

	if (listen(listener, 10) == -1) {
		perror("Could not listen()");
		return 1;
	}

	printf("Listening!\n");


	// do stuff!
	while (!quitFlag) {
		fd_set readSet, writeSet;
		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);

		int maxFD = listener;
		FD_SET(listener, &readSet);

		time_t now = time(NULL);

		for (int i = 0; i < clientCount; i++) {
			if (clients[i]->state == Client::CS_TLS_HANDSHAKE)
				clients[i]->tryTLSHandshake();

			if (clients[i]->sock != -1) {
				if (clients[i]->sock > maxFD)
					maxFD = clients[i]->sock;

				if (clients[i]->state == Client::CS_CONNECTED)
					FD_SET(clients[i]->sock, &readSet);
				if (clients[i]->outputBuf.size() > 0)
					FD_SET(clients[i]->sock, &writeSet);

			} else {
				// Outdated session, can we kill it?
				if (now >= clients[i]->deadTime) {
					printf("[%d] Session expired, deleting\n", now);

					// Yep.
					Client *client = clients[i];

					// If this is the last socket in the list, we can just
					// decrement clientCount and all will be fine.
					clientCount--;

					// Otherwise, we move that pointer into this slot, and
					// we subtract one from i so that we'll process that slot
					// on the next loop iteration.
					if (i != clientCount) {
						clients[i] = clients[clientCount];
						i--;
					}
				}
			}
		}

		for (int i = 0; i < serverCount; i++) {
			if (servers[i]->state == Server::CS_WAITING_DNS)
				servers[i]->tryConnectPhase();
			else if (servers[i]->state == Server::CS_TLS_HANDSHAKE)
				servers[i]->tryTLSHandshake();

			if (servers[i]->sock != -1) {
				if (servers[i]->sock > maxFD)
					maxFD = servers[i]->sock;

				if (servers[i]->state == Server::CS_CONNECTED)
					FD_SET(servers[i]->sock, &readSet);
				if (servers[i]->outputBuf.size() > 0 || servers[i]->state == Server::CS_WAITING_CONNECT)
					FD_SET(servers[i]->sock, &writeSet);
			}
		}

		timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		int numFDs = select(maxFD+1, &readSet, &writeSet, NULL, &timeout);

		now = time(NULL);
		printf("[%lu select:%d]\n", now, numFDs);


		// This is really not very DRY.
		// Once I implement SSL properly, I'll look at the common bits between these
		// two blocks and make it cleaner...


		for (int i = 0; i < clientCount; i++) {
			if (clients[i]->sock != -1) {
				if (FD_ISSET(clients[i]->sock, &writeSet)) {
					// What can we get rid of...?
					Client *client = clients[i];
					ssize_t amount;
					if (client->gnutlsSessionInited) {
						amount = gnutls_record_send(client->tls,
								client->outputBuf.data(),
								client->outputBuf.size());
					} else {
						amount = send(client->sock,
								client->outputBuf.data(),
								client->outputBuf.size(),
								0);
					}

					if (amount > 0) {
						printf("[%d] Wrote %d bytes\n", i, amount);
						client->outputBuf.trimFromStart(amount);
					} else if (amount == 0)
						printf("Sent 0!\n");
					else if (amount < 0)
						perror("Error while sending!");
					// Close connection in that case, if a fatal error occurs?
				}


				if (FD_ISSET(clients[i]->sock, &readSet) || (clients[i]->gnutlsSessionInited && gnutls_record_check_pending(clients[i]->tls) > 0)) {
					Client *client = clients[i];

					// Ensure we have at least 0x200 bytes space free
					// (Up this, maybe?)
					int bufSize = client->inputBuf.size();
					int requiredSize = bufSize + 0x200;
					if (requiredSize < client->inputBuf.capacity())
						client->inputBuf.setCapacity(requiredSize);

					ssize_t amount;
					if (client->gnutlsSessionInited) {
						amount = gnutls_record_recv(client->tls,
								&client->inputBuf.data()[bufSize],
								0x200);
					} else {
						amount = recv(client->sock,
								&client->inputBuf.data()[bufSize],
								0x200,
								0);
					}


					if (amount > 0) {
						// Yep, we have data
						printf("[%d] Read %d bytes\n", i, amount);
						client->inputBuf.resize(bufSize + amount);

						client->processInput();

					} else if (amount == 0) {
						printf("[%d] Read 0! Client closing.\n", i);
						client->close();

					} else if (amount < 0)
						perror("Error while reading!");
					// Close connection in that case, if a fatal error occurs?
				}
			}
		}



		for (int i = 0; i < serverCount; i++) {
			if (servers[i]->sock != -1) {
				if (FD_ISSET(servers[i]->sock, &writeSet)) {
					Server *server = servers[i];

					if (server->state == Server::CS_WAITING_CONNECT) {
						// Welp, this means we're connected!
						// Maybe.
						// We might have an error condition, in which case,
						// we're screwed.
						bool didSucceed = false;
						int sockErr;
						socklen_t sockErrSize = sizeof(sockErr);

						if (getsockopt(server->sock, SOL_SOCKET, SO_ERROR, &sockErr, &sockErrSize) == 0) {
							if (sockErr == 0)
								didSucceed = true;
						}

						if (didSucceed) {
							// WE'RE IN fuck yeah
							printf("[%d] Connection succeeded!\n", i);
							server->connectionSuccessful();
						} else {
							// Nope. Nuke it.
							printf("[%d] Connection failed: %d\n", i, sockErr);
							server->close();
						}

					} else {
						// What can we get rid of...?

						ssize_t amount;
						if (server->gnutlsSessionInited) {
							amount = gnutls_record_send(server->tls,
									server->outputBuf.data(),
									server->outputBuf.size());
						} else {
							amount = send(server->sock,
									server->outputBuf.data(),
									server->outputBuf.size(),
									0);
						}

						if (amount > 0) {
							printf("[%d] Wrote %d bytes\n", i, amount);
							server->outputBuf.trimFromStart(amount);
						} else if (amount == 0)
							printf("Sent 0!\n");
						else if (amount < 0)
							perror("Error while sending!");
						// Close connection in that case, if a fatal error occurs?
					}
				}


				if (FD_ISSET(servers[i]->sock, &readSet) || (servers[i]->gnutlsSessionInited && gnutls_record_check_pending(servers[i]->tls) > 0)) {
					Server *server = servers[i];

					// Ensure we have at least 0x200 bytes space free
					// (Up this, maybe?)
					int bufSize = server->inputBuf.size();
					int requiredSize = bufSize + 0x200;
					if (requiredSize < server->inputBuf.capacity())
						server->inputBuf.setCapacity(requiredSize);

					ssize_t amount;
					if (server->gnutlsSessionInited) {
						amount = gnutls_record_recv(server->tls,
								&server->inputBuf.data()[bufSize],
								0x200);
					} else {
						amount = recv(server->sock,
								&server->inputBuf.data()[bufSize],
								0x200,
								0);
					}


					if (amount > 0) {
						// Yep, we have data
						printf("[%d] Read %d bytes\n", i, amount);
						server->inputBuf.resize(bufSize + amount);

						server->processInput();

					} else if (amount == 0) {
						printf("[%d] Read 0! Server closing.\n", i);
						server->close();

					} else if (amount < 0)
						perror("Error while reading!");
					// Close connection in that case, if a fatal error occurs?
				}
			}
		}



		if (FD_ISSET(listener, &readSet)) {
			// Yay, we have a new connection
			int sock = accept(listener, NULL, NULL);

			if (clientCount >= CLIENT_LIMIT) {
				// We can't accept it.
				printf("Too many connections, we can't accept this one. THIS SHOULD NEVER HAPPEN.\n");
				shutdown(sock, SHUT_RDWR);
				close(sock);
			} else {
				// Create a new connection
				printf("[%d] New connection, fd=%d\n", clientCount, sock);

				Client *client = new Client;

				clients[clientCount] = client;
				++clientCount;

				client->startService(sock, true);
			}
		}
	}

	// Need to shut down all sockets here
	for (int i = 0; i < serverCount; i++)
		servers[i]->close();

	for (int i = 0; i < clientCount; i++)
		clients[i]->close();

	shutdown(listener, SHUT_RDWR);
	close(listener);
}


