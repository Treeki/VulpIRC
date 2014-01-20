#include "core.h"
#include "dns.h"

Server::Server(NetCore *_netCore) : SocketRWCommon(_netCore) {
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
	for (int i = 0; i < netCore->clientCount; i++)
		if (netCore->clients[i]->authState == Client::AS_AUTHED)
			netCore->clients[i]->sendPacket(Packet::B2C_STATUS, pkt);
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

		gnutls_credentials_set(tls, GNUTLS_CRD_CERTIFICATE, g_serverCreds);

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
