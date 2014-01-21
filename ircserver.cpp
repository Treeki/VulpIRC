#include "core.h"

IRCServer::IRCServer(Bouncer *_bouncer) : Server(_bouncer) {
	bouncer = _bouncer;
}

void IRCServer::connect() {
	Server::connect(config.hostname, config.port, config.useTls);
}


void IRCServer::connectedEvent() {
	printf("[IRCServer:%p] connectedEvent\n", this);

	char buf[2048];

	if (strlen(config.password) > 0) {
		sprintf(buf, "PASS %s", config.password);
		sendLine(buf);
	}

	sprintf(buf, "USER %s 0 * :%s\r\nNICK %s",
		config.username, config.realname, config.nickname);
	sendLine(buf);
}
void IRCServer::disconnectedEvent() {
	printf("[IRCServer:%p] disconnectedEvent\n", this);
}
void IRCServer::lineReceivedEvent(char *line, int size) {
	printf("[%d] { %s }\n", size, line);

	Buffer pkt;
	pkt.writeStr(line, size);
	for (int i = 0; i < bouncer->clientCount; i++)
		if (bouncer->clients[i]->isAuthed())
			bouncer->clients[i]->sendPacket(Packet::B2C_STATUS, pkt);
}
