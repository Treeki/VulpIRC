#include "core.h"

IRCServer::IRCServer(Bouncer *_bouncer) :
	Server(_bouncer),
	bouncer(_bouncer),
	status(this)
{
}

IRCServer::~IRCServer() {
	bouncer->deregisterWindow(&status);

	for (auto &i : channels) {
		bouncer->deregisterWindow(i.second);
		delete i.second;
	}
}

void IRCServer::attachedToCore() {
	bouncer->registerWindow(&status);
}

void IRCServer::connect() {
	status.pushMessage("Connecting...");
	Server::connect(config.hostname, config.port, config.useTls);
}


void IRCServer::resetIRCState() {
	strcpy(currentNick, "");

	strcpy(serverPrefix, "@+");
	strcpy(serverPrefixMode, "ov");
}


Channel *IRCServer::findChannel(const char *name, bool createIfNeeded) {
	std::map<std::string, Channel *>::iterator
		check = channels.find(name);

	if (check == channels.end()) {
		if (createIfNeeded) {
			Channel *c = new Channel(this, name);
			channels[name] = c;
			return c;
		} else {
			return 0;
		}
	} else {
		return check->second;
	}
}



void IRCServer::connectedEvent() {
	resetIRCState();

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


	// Process this line...!
	UserRef user;

	// Is there a prefix?
	if (line[0] == ':') {
		char nickBuf[512], identBuf[512], hostBuf[512];
		int nickPos = 0, identPos = 0, hostPos = 0;
		int phase = 0;

		++line; // skip colon

		while ((*line != ' ') && (*line != 0)) {
			if (phase == 0) {
				// Nick
				if (*line == '!')
					phase = 1;
				else if (*line == '@')
					phase = 2;
				else {
					if (nickPos < 511)
						nickBuf[nickPos++] = *line;
				}
			} else if (phase == 1) {
				// Ident
				if (*line == '@')
					phase = 2;
				else {
					if (identPos < 511)
						identBuf[identPos++] = *line;
				}
			} else if (phase == 2) {
				if (hostPos < 511)
					hostBuf[hostPos++] = *line;
			}

			++line;
		}

		if (*line == 0) {
			// Invalid line. Can't parse this.
			return;
		}

		++line; // skip the space

		nickBuf[nickPos] = 0;
		identBuf[identPos] = 0;
		hostBuf[hostPos] = 0;

		user.nick = nickBuf;
		user.ident = identBuf;
		user.hostmask = hostBuf;

		user.isValid = true;
		user.isSelf = (strcmp(nickBuf, currentNick) == 0);
	}

	// Get the command
	char cmdBuf[512];
	int cmdPos = 0;

	while ((*line != ' ') && (*line != 0)) {
		if (cmdPos < 511)
			cmdBuf[cmdPos++] = *line;
		++line;
	}
	cmdBuf[cmdPos] = 0;

	if (*line == 0) {
		// Invalid line.
		return;
	}

	++line; // skip the space

	// Skip the : if there is one
	if (*line == ':')
		++line;

	// Get the first param, or "target" in many cases
	char *allParams = line;
	char *paramsAfterFirst = line;

	char targetBuf[512];
	int targetPos = 0;

	while ((*paramsAfterFirst != ' ') && (*paramsAfterFirst != 0)) {
		if (targetPos < 511)
			targetBuf[targetPos++] = *paramsAfterFirst;
		++paramsAfterFirst;
	}

	targetBuf[targetPos] = 0;

	// If we didn't reach the end of the line, skip the space
	if (*paramsAfterFirst == ' ')
		++paramsAfterFirst;

	// And if the params begin with :, skip it
	if (*paramsAfterFirst == ':')
		++paramsAfterFirst;

	// Now figure out what to do with this...!

	if (strcmp(cmdBuf, "PING") == 0) {
		char out[512];
		snprintf(out, 512, "PONG %s", allParams);
		sendLine(out);

	} else if (strcmp(cmdBuf, "JOIN") == 0) {
		Channel *c = findChannel(targetBuf, true);
		if (c)
			c->handleJoin(user);

	} else if (strcmp(cmdBuf, "PRIVMSG") == 0) {
		Channel *c = findChannel(targetBuf, true);
		if (c)
			c->handlePrivmsg(user, paramsAfterFirst);

	} else if (strcmp(cmdBuf, "001") == 0) {
		status.pushMessage("[debug: currentNick change detected]");

		strncpy(currentNick, targetBuf, sizeof(currentNick));
		currentNick[sizeof(currentNick) - 1] = 0;

	} else if (strcmp(cmdBuf, "005") == 0) {
		processISupport(paramsAfterFirst);

	} else {
		status.pushMessage("!! Unhandled !!");
	}
}


void IRCServer::processISupport(const char *line) {
	while (*line != 0) {
		char keyBuf[512], valueBuf[512];
		int keyPos = 0, valuePos = 0;
		int phase = 0;

		// This means we've reached the end
		if (*line == ':')
			return;

		while ((*line != 0) && (*line != ' ')) {
			if (phase == 0) {
				if (*line == '=')
					phase = 1;
				else if (keyPos < 511)
					keyBuf[keyPos++] = *line;
			} else {
				if (valuePos < 511)
					valueBuf[valuePos++] = *line;
			}

			++line;
		}

		if (*line == ' ')
			++line;

		keyBuf[keyPos] = 0;
		valueBuf[valuePos] = 0;


		// Now process the thing

		if (strcmp(keyBuf, "PREFIX") == 0) {
			int prefixCount = (valuePos - 2) / 2;

			if (valueBuf[0] == '(' && valueBuf[1+prefixCount] == ')') {
				if (prefixCount < 32) {
					strncpy(serverPrefixMode, &valueBuf[1], prefixCount);
					strncpy(serverPrefix, &valueBuf[2+prefixCount], prefixCount);

					serverPrefixMode[prefixCount] = 0;
					serverPrefix[prefixCount] = 0;
				}
			}
		} else if (strcmp(keyBuf, "CHANMODES") == 0) {
			char *proc = &valueBuf[0];

			for (int index = 0; index < 4; index++) {
				if (*proc == 0)
					break;

				char *start = proc;
				char *end = proc;

				while ((*end != ',') && (*end != 0))
					++end;

				// If this is a zero, we can't read any more
				bool endsHere = (*end == 0);
				*end = 0;

				serverChannelModes[index] = start;
				char moof[1000];
				sprintf(moof, "set chanmodes %d to [%s]", index, serverChannelModes[index].c_str());
				status.pushMessage(moof);

				if (endsHere)
					break;
				else
					proc = end + 1;
			}
		}
	}
}
