#include "core.h"

MobileClient::MobileClient(Bouncer *_bouncer) : Client(_bouncer) {
	bouncer = _bouncer;
}

void MobileClient::sessionStartEvent() {
	printf("{Session started}\n");
}
void MobileClient::sessionEndEvent() {
	printf("{Session ended}\n");
}
void MobileClient::packetReceivedEvent(Packet::Type type, Buffer &pkt) {
	if (type == Packet::C2B_COMMAND) {
		char cmd[2048];
		pkt.readStr(cmd, sizeof(cmd));
		handleDebugCommand(cmd, strlen(cmd));

	} else {
		printf("[MobileClient:%p] Unrecognised packet for MobileClient: type %d, size %d\n",
			this, type, pkt.size());
	}
}

void MobileClient::handleDebugCommand(char *line, int size) {
	// This is a terrible mess that will be replaced shortly
	if (strncmp(line, "all ", 4) == 0) {
		Buffer pkt;
		pkt.writeStr(&line[4]);
		for (int i = 0; i < bouncer->clientCount; i++)
			bouncer->clients[i]->sendPacket(Packet::B2C_STATUS, pkt);

	} else if (strcmp(line, "quit") == 0) {
		bouncer->quitFlag = true;
	} else if (strncmp(&line[1], "ddsrv ", 6) == 0) {
		IRCServer *srv = new IRCServer(bouncer);
		strcpy(srv->config.hostname, &line[7]);
		srv->config.useTls = (line[0] == 's');
		srv->config.port = (line[0] == 's') ? 1191 : 6667;
		strcpy(srv->config.nickname, "Ninjifox");
		strcpy(srv->config.username, "boop");
		strcpy(srv->config.realname, "boop");
		strcpy(srv->config.password, "");
		bouncer->registerServer(srv);

		Buffer pkt;
		pkt.writeStr("Your wish is my command!");
		for (int i = 0; i < bouncer->clientCount; i++)
			bouncer->clients[i]->sendPacket(Packet::B2C_STATUS, pkt);

	} else if (strncmp(line, "srvpw", 5) == 0) {
		int sid = line[5] - '0';
			// ugly hack, fuck casting, will fix later
		strcpy(((IRCServer*)bouncer->servers[sid])->config.password, &line[7]);

	} else if (strncmp(line, "connsrv", 7) == 0) {
		int sid = line[7] - '0';
			// ugly hack, fuck casting, will fix later
		((IRCServer*)bouncer->servers[sid])->connect();
	} else if (line[0] >= '0' && line[0] <= '9') {
		int sid = line[0] - '0';
		bouncer->servers[sid]->sendLine(&line[1]);
	}
}

