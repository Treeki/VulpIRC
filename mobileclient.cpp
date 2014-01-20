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
		printf("[fd=%d] Unrecognised packet for MobileClient: type %d, size %d\n",
			sock, type, pkt.size());
	}
}

void MobileClient::handleDebugCommand(char *line, int size) {
	// This is a terrible mess that will be replaced shortly
	if (authState == AS_AUTHED) {
		if (strncmp(line, "all ", 4) == 0) {
			Buffer pkt;
			pkt.writeStr(&line[4]);
			for (int i = 0; i < netCore->clientCount; i++)
				netCore->clients[i]->sendPacket(Packet::B2C_STATUS, pkt);

		} else if (strcmp(line, "quit") == 0) {
			netCore->quitFlag = true;
		} else if (strncmp(&line[1], "ddsrv ", 6) == 0) {
			IRCServer *srv = new IRCServer(bouncer);
			strcpy(srv->config.hostname, &line[7]);
			srv->config.port = 1191;
			srv->config.useTls = (line[0] == 's');
			bouncer->registerServer(srv);

			Buffer pkt;
			pkt.writeStr("Your wish is my command!");
			for (int i = 0; i < netCore->clientCount; i++)
				netCore->clients[i]->sendPacket(Packet::B2C_STATUS, pkt);

		} else if (strncmp(line, "connsrv", 7) == 0) {
			int sid = line[7] - '0';
			// ugly hack, fuck casting, will fix later
			((IRCServer*)netCore->servers[sid])->connect();
		} else if (line[0] >= '0' && line[0] <= '9') {
			int sid = line[0] - '0';
			netCore->servers[sid]->outputBuf.append(&line[1], size - 1);
			netCore->servers[sid]->outputBuf.append("\r\n", 2);
		}
	} else {
	}
}

