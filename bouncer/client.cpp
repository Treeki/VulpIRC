#include "core.h"

static bool isNullSessionKey(uint8_t *key) {
	for (int i = 0; i < SESSION_KEY_SIZE; i++)
		if (key[i] != 0)
			return false;

	return true;
}



Client::Client(NetCore *_netCore) : SocketRWCommon(_netCore) {
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

#ifdef USE_GNUTLS
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

		ret = gnutls_credentials_set(tls, GNUTLS_CRD_CERTIFICATE, g_clientCreds);
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
	} else
#endif
	{
		state = CS_CONNECTED;
	}
}

void Client::close() {
	SocketRWCommon::close();

	if (authState == AS_AUTHED)
		deadTime = time(NULL) + netCore->sessionKeepalive;
	else
		deadTime = time(NULL) - 1; // kill instantly
}


void Client::generateSessionKey() {
	time_t now = time(NULL);

	while (true) {
		for (int i = 0; i < SESSION_KEY_SIZE; i++) {
			if ((unsigned)i < sizeof(time_t))
				sessionKey[i] = ((uint8_t*)&now)[i];
			else
				sessionKey[i] = rand() & 255;
		}

		// Is any other client already using this key?
		// It's ridiculously unlikely, but... probably best
		// to check just in case!
		bool foundMatch = false;

		for (int i = 0; i < netCore->clientCount; i++) {
			if (netCore->clients[i] != this) {
				if (!memcmp(netCore->clients[i]->sessionKey, sessionKey, SESSION_KEY_SIZE))
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
		if (pkt->id <= maxID) {
			delete pkt;
			return true;
		} else {
			return false;
		}
	});
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

			char pwBuf[512] = "";
			pkt.readStr(pwBuf, sizeof(pwBuf));
			if (strcmp(pwBuf, netCore->bouncerPassword.c_str()) != 0)
				error = 3;

			if (!pkt.readRemains(SESSION_KEY_SIZE))
				error = 2;



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
					printf("(last they received = %d)\n", lastReceivedByClient);

					Client *other = netCore->findClientWithSessionKey(reqKey);
					printf("[fd=%d] Got client %p\n", sock, other);

					if (other && other->authState == AS_AUTHED) {
						printf("Valid: last packet we sent = %d\n", other->nextPacketID - 1);
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

				sessionStartEvent();
			}

		} else {
			printf("[fd=%d] Unrecognised packet in AS_LOGIN_WAIT authstate: type %d, size %d\n",
					sock, type, size);
		}
	} else if (authState == AS_AUTHED) {
		packetReceivedEvent(type, pkt);
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
		inputBuf.readU16(); // reserved value
		uint32_t packetSize = inputBuf.readU32();
		bool silentlyIgnore = false;

		// Do we now have the whole packet in memory...?
		int extHeaderSize = (type & Packet::T_OUT_OF_BAND_FLAG) ? 0 : 8;

		if (!inputBuf.readRemains(packetSize + extHeaderSize))
			break;


		if (!(type & Packet::T_OUT_OF_BAND_FLAG)) {
			// Handle packet system things for non-OOB packets
			uint32_t packetID = inputBuf.readU32();
			uint32_t lastReceivedByClient = inputBuf.readU32();

			if (packetID > lastReceivedPacketID) {
				// This is a new packet
				lastReceivedPacketID = packetID;
			} else {
				// We've already seen this packet, silently ignore it!
				silentlyIgnore = true;
			}

			clearCachedPackets(lastReceivedByClient);
		}

		// Yep, we can process it!

		// Save the position of the next packet
		readBufPosition = inputBuf.readTell() + packetSize;

		if (!silentlyIgnore)
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
	state = other->state;
#ifdef USE_GNUTLS
	tls = other->tls;
	tlsActive = other->tlsActive;
#endif

	other->sock = -1;
	other->state = CS_DISCONNECTED;
#ifdef USE_GNUTLS
	other->tls = 0;
	other->tlsActive = false;
#endif

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
