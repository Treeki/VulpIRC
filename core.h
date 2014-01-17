#ifndef CORE_H
#define CORE_H 

#include "buffer.h"

#define CLIENT_LIMIT 100
#define SERVER_LIMIT 20

#define SESSION_KEEPALIVE 30

#define SESSION_KEY_SIZE 16

#define PROTOCOL_VERSION 1

#define SERVE_VIA_TLS false

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
	bool tlsActive;

	SocketRWCommon();
	virtual ~SocketRWCommon();

	void tryTLSHandshake();
	virtual void close();

	void readAction();
	void writeAction();
	bool hasTlsPendingData() const;
private:
	virtual void processReadBuffer() = 0;
};


struct Packet {
	enum Type {
		T_OUT_OF_BAND_FLAG = 0x8000,

		C2B_COMMAND = 1,
		B2C_STATUS = 1,

		C2B_OOB_LOGIN = 0x8001,

		B2C_OOB_LOGIN_SUCCESS = 0x8001,
		B2C_OOB_LOGIN_FAILED = 0x8002,
		B2C_OOB_SESSION_RESUMED = 0x8003,
	};

	Type type;
	int id;
	Buffer data;
};

struct Client : SocketRWCommon {
	enum AuthState {
		AS_LOGIN_WAIT = 0,
		AS_AUTHED = 1
	};

	AuthState authState;
	uint8_t sessionKey[SESSION_KEY_SIZE];
	time_t deadTime;

	std::list<Packet *> packetCache;
	int nextPacketID, lastReceivedPacketID;

	Client();
	~Client();

	void startService(int _sock, bool withTls);
	void close();

	void sendPacket(Packet::Type type, const Buffer &data, bool allowUnauthed = false);

private:
	int readBufPosition;
	void processReadBuffer();
	void handlePacket(Packet::Type type, char *data, int size);
	void handleCommand(char *line, int size);

	void generateSessionKey();
	void resumeSession(Client *other, int lastReceivedByClient);
	void sendPacketOverWire(const Packet *packet);
	void clearCachedPackets(int maxID);
};

struct Server : SocketRWCommon {
	char ircHostname[256];
	int ircPort;
	int dnsQueryId;
	bool ircUseTls;

	Server();
	~Server();

	void beginConnect();
	void tryConnectPhase();
	void connectionSuccessful();

	void close();

private:
	void processReadBuffer();
	void handleLine(char *line, int size);
};


#endif /* CORE_H */
