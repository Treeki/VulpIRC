#ifndef CORE_H
#define CORE_H 

// Set in build.sh
//#define USE_GNUTLS

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
#include <list>
#include <map>
#include <string>

#include "buffer.h"

#ifdef USE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#define CLIENT_LIMIT 100
#define SERVER_LIMIT 20

#define SESSION_KEEPALIVE 30

#define SESSION_KEY_SIZE 16

#define PROTOCOL_VERSION 1

#define SERVE_VIA_TLS false

class NetCore;
class Bouncer;
class IRCServer;

// Need to move this somewhere more appropriate
struct UserRef {
	std::string nick, ident, hostmask;
	bool isSelf, isValid;

	UserRef() { isSelf = false; isValid = false; }
};


class Window {
public:
	NetCore *core;

	Window(NetCore *_core);
	virtual ~Window() { }

	int id;
	std::list<std::string> messages;

	virtual const char *getTitle() const = 0;
	virtual int getType() const = 0;
	virtual void syncStateForClient(Buffer &output);
	virtual void handleUserInput(const char *str) { }

	void pushMessage(const char *str);
};

class StatusWindow : public Window {
public:
	StatusWindow(IRCServer *_server);

	IRCServer *server;

	virtual const char *getTitle() const;
	virtual int getType() const;
	virtual void handleUserInput(const char *str);
};

class Channel : public Window {
public:
	Channel(IRCServer *_server, const char *_name);

	IRCServer *server;

	bool inChannel;

	std::string name, topic;
	std::map<std::string, uint32_t> users;

	virtual const char *getTitle() const;
	virtual int getType() const;
	virtual void handleUserInput(const char *str);
	virtual void syncStateForClient(Buffer &output);

	void handleNameReply(const char *str);
	void handleJoin(const UserRef &user);
	void handlePart(const UserRef &user, const char *message);
	void handleQuit(const UserRef &user, const char *message);
	void handleNick(const UserRef &user, const char *newNick);
	void handleMode(const UserRef &user, const char *str);
	void handlePrivmsg(const UserRef &user, const char *str);

	char getEffectivePrefixChar(const char *nick) const;

	void disconnected();
};



class SocketRWCommon {
public:
	static bool setSocketNonBlocking(int fd); // Move me!

	friend class NetCore;

protected:
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
#ifdef USE_GNUTLS
	gnutls_session_t tls;
	bool tlsActive;
#endif

public:
	SocketRWCommon(NetCore *_netCore);
	virtual ~SocketRWCommon();

	virtual void close();

private:
#ifdef USE_GNUTLS
	bool tryTLSHandshake();
	bool hasTlsPendingData() const;
#endif

	void readAction();
	void writeAction();

	virtual void processReadBuffer() = 0;
};


struct Packet {
	enum Type {
		T_OUT_OF_BAND_FLAG = 0x8000,

		C2B_COMMAND = 1,
		B2C_STATUS = 1,

		B2C_WINDOW_ADD = 0x100,
		B2C_WINDOW_REMOVE = 0x101,
		B2C_WINDOW_MESSAGE = 0x102,
		B2C_WINDOW_RENAME = 0x103,

		C2B_WINDOW_INPUT = 0x102,

		B2C_CHANNEL_USER_ADD = 0x120,
		B2C_CHANNEL_USER_REMOVE = 0x121,
		B2C_CHANNEL_USER_RENAME = 0x122,
		B2C_CHANNEL_USER_MODES = 0x123,

		C2B_OOB_LOGIN = 0x8001,

		B2C_OOB_LOGIN_SUCCESS = 0x8001,
		B2C_OOB_LOGIN_FAILED = 0x8002,
		B2C_OOB_SESSION_RESUMED = 0x8003,
	};

	Type type;
	int id;
	Buffer data;
};

class Client : private SocketRWCommon {
	friend class NetCore;
private:
	enum AuthState {
		AS_LOGIN_WAIT = 0,
		AS_AUTHED = 1
	};

	AuthState authState;
	uint8_t sessionKey[SESSION_KEY_SIZE];
	time_t deadTime;

	std::list<Packet *> packetCache;
	int nextPacketID, lastReceivedPacketID;

public:
	Client(NetCore *_netCore);
	~Client();

	bool isAuthed() const { return (authState == AS_AUTHED); }

	void close();

	void sendPacket(Packet::Type type, const Buffer &data, bool allowUnauthed = false);

private:
	void startService(int _sock, bool withTls);

	int readBufPosition;
	void processReadBuffer();

	void generateSessionKey();
	void resumeSession(Client *other, int lastReceivedByClient);

	void handlePacket(Packet::Type type, char *data, int size);
	void sendPacketOverWire(const Packet *packet);
	void clearCachedPackets(int maxID);

	// Events!
	virtual void sessionStartEvent() = 0;
	virtual void sessionEndEvent() = 0;
	virtual void packetReceivedEvent(Packet::Type type, Buffer &pkt) = 0;
};

class MobileClient : public Client {
public:
	Bouncer *bouncer;

	MobileClient(Bouncer *_bouncer);

private:
	virtual void sessionStartEvent();
	virtual void sessionEndEvent();
	virtual void packetReceivedEvent(Packet::Type type, Buffer &pkt);

	void handleDebugCommand(char *line, int size);
};

class Server : private SocketRWCommon {
	friend class NetCore;

	int port;
	bool useTls;

	int dnsQueryId;

public:
	Server(NetCore *_netCore);
	virtual ~Server();

protected:
	void connect(const char *hostname, int _port, bool _useTls);

public:
	void sendLine(const char *line); // protect me!
	void close();

private:
	void tryConnectPhase();
	void connectionSuccessful();
	void processReadBuffer();

	virtual void connectedEvent() = 0;
	virtual void disconnectedEvent() = 0;
	virtual void lineReceivedEvent(char *line, int size) = 0;

	virtual void attachedToCore() { }
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

class IRCServer : public Server {
public:
	Bouncer *bouncer;

	StatusWindow status;
	std::map<std::string, Channel *> channels;

	IRCNetworkConfig config;

	char currentNick[128];
	char serverPrefix[32], serverPrefixMode[32];
	std::string serverChannelModes[4];

	uint32_t getUserFlag(char search, const char *array) const;
	uint32_t getUserFlagByPrefix(char prefix) const;
	uint32_t getUserFlagByMode(char mode) const;
	int getChannelModeType(char mode) const;

	IRCServer(Bouncer *_bouncer);
	~IRCServer();

	void connect();

	// Events!
private:
	virtual void connectedEvent();
	virtual void disconnectedEvent();
	virtual void lineReceivedEvent(char *line, int size);

	virtual void attachedToCore();


	void resetIRCState();
	void processISupport(const char *str);

	Channel *findChannel(const char *name, bool createIfNeeded);
};


class NetCore {
public:
	NetCore();

	Client *clients[CLIENT_LIMIT];
	Server *servers[SERVER_LIMIT];
	int clientCount;
	int serverCount;

	void sendToClients(Packet::Type type, const Buffer &data);

	std::list<Window *> windows;
	int nextWindowID;

	int registerWindow(Window *window);
	void deregisterWindow(Window *window);
	Window *findWindow(int id) const;

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

class Bouncer : public NetCore {
private:
	virtual Client *constructClient();
};




// This is ugly as crap, TODO FIXME etc etc
#ifdef USE_GNUTLS
extern gnutls_certificate_credentials_t g_serverCreds, g_clientCreds;
#endif

#endif /* CORE_H */
