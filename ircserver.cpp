#include "core.h"

IRCServer::IRCServer(Bouncer *_bouncer) : Server(_bouncer) {
	bouncer = _bouncer;
}

void IRCServer::connect() {
	Server::connect(config.hostname, config.port, config.useTls);
}
