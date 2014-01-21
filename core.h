#ifndef CORE_H
#define CORE_H 

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

#include "buffer.h"

#define CLIENT_LIMIT 100
#define SERVER_LIMIT 20

#define SESSION_KEEPALIVE 30

#define SESSION_KEY_SIZE 16

#define PROTOCOL_VERSION 1

#define SERVE_VIA_TLS false

struct NetCore;
struct Bouncer;

struct SocketRWCommon {
	static bool setSocketNonBlocking(int fd); // Move me!

	NetCore *netCore;

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

	SocketRWCommon(NetCore *_netCore);
	virtual ~SocketRWCommon();

	bool tryTLSHandshake();
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

	Client(NetCore *_netCore);
	~Client();

	void startService(int _sock, bool withTls);
	void close();

	void sendPacket(Packet::Type type, const Buffer &data, bool allowUnauthed = false);

private:
	int readBufPosition;
	void processReadBuffer();

	void generateSessionKey();
	void resumeSession(Client *other, int lastReceivedByClient);

	void handlePacket(Packet::Type type, char *data, int size);
	void sendPacketOverWire(const Packet *packet);
	void clearCachedPackets(int maxID);

	// Events!
public:
	virtual void sessionEndEvent() = 0;
private:
	virtual void sessionStartEvent() = 0;
	virtual void packetReceivedEvent(Packet::Type type, Buffer &pkt) = 0;
};

struct MobileClient : Client {
	Bouncer *bouncer;

	MobileClient(Bouncer *_bouncer);

private:
	virtual void sessionStartEvent();
	virtual void sessionEndEvent();
	virtual void packetReceivedEvent(Packet::Type type, Buffer &pkt);

	void handleDebugCommand(char *line, int size);
};

struct Server : SocketRWCommon {
	int port;
	bool useTls;

	int dnsQueryId;

	Server(NetCore *_netCore);
	~Server();

protected:
	void connect(const char *hostname, int _port, bool _useTls);

public:
	void tryConnectPhase();
	void connectionSuccessful();

	void sendLine(const char *line); // protect me!
	void close();

private:
	void processReadBuffer();

public:
	virtual void connectedEvent() = 0; // PRIVATE ME
private:
	virtual void disconnectedEvent() = 0;
	virtual void lineReceivedEvent(char *line, int size) = 0;
};

struct IRCNetworkConfig {
	char hostname[512];
	char nickname[128];
	char username[128];
	char realname[128];
	char password[128];
	int port;
	bool useTls;
};

struct IRCServer : Server {
	Bouncer *bouncer;
	IRCNetworkConfig config;

	IRCServer(Bouncer *_bouncer);

	void connect();

	// Events!
private:
	virtual void connectedEvent();
	virtual void disconnectedEvent();
	virtual void lineReceivedEvent(char *line, int size);
};


struct NetCore {
	NetCore();

	Client *clients[CLIENT_LIMIT];
	Server *servers[SERVER_LIMIT];
	int clientCount;
	int serverCount;

	bool quitFlag;

	int execute();

	Client *findClientWithSessionKey(uint8_t *key) const;
private:
	virtual Client *constructClient() = 0;

public:
	int registerServer(Server *server); // THIS FUNCTION WILL BE PROTECTED LATER
protected:
	void deregisterServer(int id);
	int findServerID(Server *server) const;
};

struct Bouncer : NetCore {
private:
	virtual Client *constructClient();
};




// This is ugly as crap, TODO FIXME etc etc
extern gnutls_certificate_credentials_t g_serverCreds, g_clientCreds;

#endif /* CORE_H */
