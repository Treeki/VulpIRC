#include "core.h"

IRCServer::IRCServer(Bouncer *_bouncer) :
	Server(_bouncer),
	bouncer(_bouncer),
	status(this)
{
}

IRCServer::~IRCServer() {
	bouncer->deregisterWindow(&status);
}

void IRCServer::attachedToCore() {
	bouncer->registerWindow(&status);
}

void IRCServer::connect() {
	status.pushMessage("Connecting...");
	Server::connect(config.hostname, config.port, config.useTls);
}


void IRCServer::connectedEvent() {
	printf("[IRCServer:%p] connectedEvent\n", this);
	status.pushMessage("Connected, identifying to IRC...");

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
	status.pushMessage("Disconnected.");
}
void IRCServer::lineReceivedEvent(char *line, int size) {
	printf("[%d] { %s }\n", size, line);

	status.pushMessage(line);
}
