#include <string.h>
#include <stdint.h>
#include <stdlib.h>
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
#include <list>

#include "dns.h"
#include "core.h"


Client *clients[CLIENT_LIMIT];
Server *servers[SERVER_LIMIT];
int clientCount, serverCount;
bool quitFlag = false;

static gnutls_dh_params_t dh_params;
static gnutls_certificate_credentials_t serverCreds, clientCreds;



static bool setSocketNonBlocking(int sock) {
	int opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		perror("Could not get fcntl options\n");
		return false;
	}
	opts |= O_NONBLOCK;
	if (fcntl(sock, F_SETFL, opts) == -1) {
		perror("Could not set fcntl options\n");
		return false;
	}
	return true;
}


static bool isNullSessionKey(uint8_t *key) {
	for (int i = 0; i < SESSION_KEY_SIZE; i++)
		if (key[i] != 0)
			return false;

	return true;
}

static Client *findClientWithKey(uint8_t *key) {
	for (int i = 0; i < clientCount; i++)
		if (!memcmp(clients[i]->sessionKey, key, SESSION_KEY_SIZE))
			return clients[i];

	return 0;
}



SocketRWCommon::SocketRWCommon() {
	sock = -1;
	state = CS_DISCONNECTED;
	tlsActive = false;
}
SocketRWCommon::~SocketRWCommon() {
	close();
}

bool SocketRWCommon::hasTlsPendingData() const {
	if (tlsActive)
		return (gnutls_record_check_pending(tls) > 0);
	else
		return false;
}

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
		if (tlsActive)
			gnutls_bye(tls, GNUTLS_SHUT_RDWR);
		shutdown(sock, SHUT_RDWR);
		::close(sock);
	}

	sock = -1;
	inputBuf.clear();
	outputBuf.clear();
	state = CS_DISCONNECTED;

	if (tlsActive) {
		gnutls_deinit(tls);
		tlsActive = false;
	}
}

void SocketRWCommon::readAction() {
	// Ensure we have at least 0x200 bytes space free
	// (Up this, maybe?)
	int bufSize = inputBuf.size();
	int requiredSize = bufSize + 0x200;
	if (requiredSize > inputBuf.capacity())
		inputBuf.setCapacity(requiredSize);

	ssize_t amount;
	if (tlsActive) {
		amount = gnutls_record_recv(tls,
				&inputBuf.data()[bufSize],
				0x200);
	} else {

		amount = recv(sock,
				&inputBuf.data()[bufSize],
				0x200,
				0);
	}


	if (amount > 0) {
		// Yep, we have data
		printf("[fd=%d] Read %d bytes\n", sock, amount);
		inputBuf.resize(bufSize + amount);

		processReadBuffer();

	} else if (amount == 0) {
		printf("[fd=%d] Read 0! Socket closing.\n", sock);
		close();

	} else if (amount < 0) {
		if (tlsActive) {
			if (gnutls_error_is_fatal(amount)) {
				printf("Error while reading [gnutls %d]!\n", amount);
				close();
			}
		} else {
			perror("Error while reading!");
			close();
		}
	}
}

void SocketRWCommon::writeAction() {
	// What can we get rid of...?
	ssize_t amount;
	if (tlsActive) {
		amount = gnutls_record_send(tls,
				outputBuf.data(),
				outputBuf.size());
	} else {
		amount = send(sock,
				outputBuf.data(),
				outputBuf.size(),
				0);
	}

	if (amount > 0) {
		printf("[fd=%d] Wrote %d bytes\n", sock, amount);
		outputBuf.trimFromStart(amount);
	} else if (amount == 0)
		printf("Sent 0!\n");
	else if (amount < 0) {
		if (tlsActive) {
			if (gnutls_error_is_fatal(amount)) {
				printf("Error while sending [gnutls %d]!\n", amount);
				close();
			}
		} else {
			perror("Error while sending!");
			close();
		}
	}
}




Client::Client() {
	authState = AS_LOGIN_WAIT;
	memset(sessionKey, 0, sizeof(sessionKey));
	readBufPosition = 0;

	nextPacketID = 1;
	lastReceivedPacketID = 0;
}
Client::~Client() {
	std::list<Packet *>::iterator
		i = packetCache.begin(),
		  e = packetCache.end();

	for (; i != e; ++i)
		delete *i;
}


void Client::startService(int _sock, bool withTls) {
	close();

	sock = _sock;

	if (!setSocketNonBlocking(sock)) {
		perror("[Client::startService] Could not set non-blocking");
		close();
		return;
	}

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

		tlsActive = true;

		state = CS_TLS_HANDSHAKE;

		printf("[fd=%d] preparing for TLS handshake\n", sock);
	} else {
		state = CS_CONNECTED;
	}
}

void Client::close() {
	SocketRWCommon::close();

	if (authState == AS_AUTHED)
		deadTime = time(NULL) + SESSION_KEEPALIVE;
	else
		deadTime = time(NULL) - 1; // kill instantly
}


void Client::generateSessionKey() {
	time_t now = time(NULL);

	while (true) {
		for (int i = 0; i < SESSION_KEY_SIZE; i++) {
			if (i < sizeof(time_t))
				sessionKey[i] = ((uint8_t*)&now)[i];
			else
				sessionKey[i] = rand() & 255;
		}

		// Is any other client already using this key?
		// It's ridiculously unlikely, but... probably best
		// to check just in case!
		bool foundMatch = false;

		for (int i = 0; i < clientCount; i++) {
			if (clients[i] != this) {
				if (!memcmp(clients[i]->sessionKey, sessionKey, SESSION_KEY_SIZE))
					foundMatch = true;
			}
		}

		// If there's none, we can safely leave!
		if (!foundMatch)
			break;
	}
}

void Client::clearCachedPackets(int maxID) {
	packetCache.remove_if([maxID](Packet *&pkt) {
			return (pkt->id <= maxID);
			});
}


void Client::handleCommand(char *line, int size) {
	// This is a terrible mess that will be replaced shortly
	if (authState == AS_AUTHED) {
		if (strncmp(line, "all ", 4) == 0) {
			Buffer pkt;
			pkt.writeStr(&line[4]);
			for (int i = 0; i < clientCount; i++)
				clients[i]->sendPacket(Packet::B2C_STATUS, pkt);

		} else if (strcmp(line, "quit") == 0) {
			quitFlag = true;
		} else if (strncmp(line, "resolve ", 8) == 0) {
			DNS::makeQuery(&line[8]);
		} else if (strncmp(&line[1], "ddsrv ", 6) == 0) {
			servers[serverCount] = new Server;
			strcpy(servers[serverCount]->ircHostname, &line[7]);
			servers[serverCount]->ircPort = 1191;
			servers[serverCount]->ircUseTls = (line[0] == 's');
			serverCount++;

			Buffer pkt;
			pkt.writeStr("Your wish is my command!");
			for (int i = 0; i < clientCount; i++)
				clients[i]->sendPacket(Packet::B2C_STATUS, pkt);

		} else if (strncmp(line, "connsrv", 7) == 0) {
			int sid = line[7] - '0';
			servers[sid]->beginConnect();
		} else if (line[0] >= '0' && line[0] <= '9') {
			int sid = line[0] - '0';
			servers[sid]->outputBuf.append(&line[1], size - 1);
			servers[sid]->outputBuf.append("\r\n", 2);
		}
	} else {
	}
}

void Client::handlePacket(Packet::Type type, char *data, int size) {
	Buffer pkt;
	pkt.useExistingBuffer(data, size);

	printf("[fd=%d] Packet : type %d, size %d\n", sock, type, size);

	if (authState == AS_LOGIN_WAIT) {
		if (type == Packet::C2B_OOB_LOGIN) {
			int error = 0;

			uint32_t protocolVersion = pkt.readU32();
			if (protocolVersion != PROTOCOL_VERSION)
				error = 1;

			uint32_t lastReceivedByClient = pkt.readU32();

			if (!pkt.readRemains(SESSION_KEY_SIZE))
				error = 2;

			// Authentication goes here at some point, too


			if (error != 0) {
				// Send an error...
				Buffer pkt;
				pkt.writeU32(error);
				sendPacket(Packet::B2C_OOB_LOGIN_FAILED, pkt, /*allowUnauthed=*/true);

				// Would close() now but this means the login failed packet never gets sent
				// need to figure out a fix for this. TODO FIXME etc etc.

			} else {
				// or log us in!
				uint8_t reqKey[SESSION_KEY_SIZE];
				pkt.read((char *)reqKey, SESSION_KEY_SIZE);

				printf("[fd=%d] Client authenticating\n", sock);

				if (!isNullSessionKey(reqKey)) {
					printf("[fd=%d] Trying to resume session...", sock);
					printf("(last they received = %d, last we sent = %d)\n", lastReceivedByClient, nextPacketID - 1);

					Client *other = findClientWithKey(reqKey);
					printf("[fd=%d] Got client %p\n", sock, other);

					if (other && other->authState == AS_AUTHED) {
						// Yep, we can go!
						other->resumeSession(this, lastReceivedByClient);
						return;
					}
				}

				// If we got here, it means we couldn't resume the session.
				// Start over.
				printf("[fd=%d] Creating new session\n", sock);

				generateSessionKey();
				authState = AS_AUTHED;

				Buffer pkt;
				pkt.append((char *)sessionKey, SESSION_KEY_SIZE);
				sendPacket(Packet::B2C_OOB_LOGIN_SUCCESS, pkt);
			}

		} else {
			printf("[fd=%d] Unrecognised packet in AS_LOGIN_WAIT authstate: type %d, size %d\n",
					sock, type, size);
		}
	} else if (authState == AS_AUTHED) {
		if (type == Packet::C2B_COMMAND) {
			char cmd[2048];
			pkt.readStr(cmd, sizeof(cmd));
			handleCommand(cmd, strlen(cmd));

		} else {
			printf("[fd=%d] Unrecognised packet in AS_AUTHED authstate: type %d, size %d\n",
					sock, type, size);
		}
	}
}

void Client::processReadBuffer() {
	// Try to process as many packets as we have in inputBuf

	// Basic header is 8 bytes
	// Extended (non-OOB) header is 16 bytes
	inputBuf.readSeek(0);
	readBufPosition = 0;

	while (inputBuf.readRemains(8)) {
		// We have 8 bytes, so we can try to read a basic header
		Packet::Type type = (Packet::Type)inputBuf.readU16();
		int reserved = inputBuf.readU16();
		uint32_t packetSize = inputBuf.readU32();

		// Do we now have the whole packet in memory...?
		int extHeaderSize = (type & Packet::T_OUT_OF_BAND_FLAG) ? 0 : 8;

		if (!inputBuf.readRemains(packetSize + extHeaderSize))
			break;


		if (!(type & Packet::T_OUT_OF_BAND_FLAG)) {
			// Handle packet system things for non-OOB packets
			uint32_t packetID = inputBuf.readU32();
			uint32_t lastReceivedByClient = inputBuf.readU32();

			lastReceivedPacketID = packetID;
			clearCachedPackets(lastReceivedByClient);
		}

		// Yep, we can process it!

		// Save the position of the next packet
		readBufPosition = inputBuf.readTell() + packetSize;
		handlePacket(type, &inputBuf.data()[inputBuf.readTell()], packetSize);

		inputBuf.readSeek(readBufPosition);
	}

	// If we managed to handle anything, lop it off the buffer
	inputBuf.trimFromStart(readBufPosition);
	readBufPosition = 0;
}


void Client::resumeSession(Client *other, int lastReceivedByClient) {
	close();

	inputBuf.clear();
	inputBuf.append(
			&other->inputBuf.data()[other->readBufPosition],
			other->inputBuf.size() - other->readBufPosition);

	// Not sure if we need to copy the outputbuf but it can't hurt
	outputBuf.clear();
	outputBuf.append(other->outputBuf.data(), other->outputBuf.size());

	sock = other->sock;
	tls = other->tls;
	tlsActive = other->tlsActive;
	state = other->state;

	other->sock = -1;
	other->tls = 0;
	other->tlsActive = false;
	other->state = CS_DISCONNECTED;
	other->close();

	// Now send them everything we've got!
	Buffer pkt;
	pkt.writeU32(lastReceivedPacketID);
	sendPacket(Packet::B2C_OOB_SESSION_RESUMED, pkt);

	clearCachedPackets(lastReceivedByClient);

	std::list<Packet*>::iterator
		i = packetCache.begin(),
		  e = packetCache.end();

	for (; i != e; ++i)
		sendPacketOverWire(*i);
}

void Client::sendPacket(Packet::Type type, const Buffer &data, bool allowUnauthed) {
	Packet *packet = new Packet;
	packet->type = type;
	packet->data.append(data);

	if (type & Packet::T_OUT_OF_BAND_FLAG) {
		packet->id = 0;
	} else {
		packet->id = nextPacketID;
		nextPacketID++;
	}

	if (state == CS_CONNECTED)
		if (authState == AS_AUTHED || allowUnauthed)
			sendPacketOverWire(packet);

	if (type & Packet::T_OUT_OF_BAND_FLAG)
		delete packet;
	else
		packetCache.push_back(packet);
}

void Client::sendPacketOverWire(const Packet *packet) {
	Buffer header;
	header.writeU16(packet->type);
	header.writeU16(0);
	header.writeU32(packet->data.size());

	if (!(packet->type & Packet::T_OUT_OF_BAND_FLAG)) {
		header.writeU32(packet->id);
		header.writeU32(lastReceivedPacketID);
	}

	outputBuf.append(header);
	outputBuf.append(packet->data);
}



Server::Server() {
	dnsQueryId = -1;
	ircUseTls = false;
}
Server::~Server() {
	if (dnsQueryId != -1)
		DNS::closeQuery(dnsQueryId);
}



void Server::handleLine(char *line, int size) {
	printf("[%d] { %s }\n", size, line);

	Buffer pkt;
	pkt.writeStr(line, size);
	for (int i = 0; i < clientCount; i++)
		if (clients[i]->authState == Client::AS_AUTHED)
			clients[i]->sendPacket(Packet::B2C_STATUS, pkt);
}
void Server::processReadBuffer() {
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
	inputBuf.trimFromStart(lineBegin);
}



void Server::beginConnect() {
	if (state == CS_DISCONNECTED) {
		DNS::closeQuery(dnsQueryId); // just in case
		dnsQueryId = DNS::makeQuery(ircHostname);

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

				if (!setSocketNonBlocking(sock)) {
					perror("[Server] Could not set non-blocking");
					close();
					return;
				}

				// We have our non-blocking socket, let's try connecting!
				sockaddr_in outAddr;
				outAddr.sin_family = AF_INET;
				outAddr.sin_port = htons(ircPort);
				outAddr.sin_addr.s_addr = result.s_addr;

				if (connect(sock, (sockaddr *)&outAddr, sizeof(outAddr)) == -1) {
					if (errno == EINPROGRESS) {
						state = CS_WAITING_CONNECT;
					} else {
						perror("[Server] Could not connect");
						close();
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
	if (ircUseTls) {
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

		tlsActive = true;
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
	serverCount = 0;
	for (int i = 0; i < SERVER_LIMIT; i++)
		servers[i] = NULL;


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

	if (!setSocketNonBlocking(listener)) {
		perror("[Listener] Could not set non-blocking");
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
		//printf("[%lu select:%d]\n", now, numFDs);


		for (int i = 0; i < clientCount; i++) {
			if (clients[i]->sock != -1) {
				if (FD_ISSET(clients[i]->sock, &writeSet))
					clients[i]->writeAction();
				if (FD_ISSET(clients[i]->sock, &readSet) || clients[i]->hasTlsPendingData())
					clients[i]->readAction();
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
						server->writeAction();
					}
				}


				if (FD_ISSET(servers[i]->sock, &readSet) || servers[i]->hasTlsPendingData())
					servers[i]->readAction();
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

				client->startService(sock, SERVE_VIA_TLS);
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


