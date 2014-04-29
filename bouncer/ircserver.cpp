#include "core.h"

// This really could stand to go in a better place...
void IRCNetworkConfig::writeToBuffer(Buffer &out) const {
	out.writeStr(title.c_str());
	out.writeStr(hostname.c_str());
	out.writeStr(username.c_str());
	out.writeStr(realname.c_str());
	out.writeStr(nickname.c_str());
	out.writeStr(altNick.c_str());
	out.writeStr(password.c_str());
	out.writeU32(port);
	out.writeU8(useTls ? 1 : 0);
}
void IRCNetworkConfig::readFromBuffer(Buffer &in) {
	char bits[1024];

	in.readStr(bits, sizeof(bits)); title = bits;
	in.readStr(bits, sizeof(bits)); hostname = bits;
	in.readStr(bits, sizeof(bits)); username = bits;
	in.readStr(bits, sizeof(bits)); realname = bits;
	in.readStr(bits, sizeof(bits)); nickname = bits;
	in.readStr(bits, sizeof(bits)); altNick = bits;
	in.readStr(bits, sizeof(bits)); password = bits;
	port = in.readU32();
	useTls = (in.readU8() != 0);
}




/*static*/ void IRCServer::ircStringToLowercase(const char *in, char *out, int outSize) {
	int i = 0;

	while ((in[i] != 0) && (i < (outSize - 1))) {
		char c = in[i];

		if ((c >= 'A') && (c <= 'Z'))
			c += ('a' - 'A');
		else if (c == '[')
			c = '{';
		else if (c == ']')
			c = '}';
		else if (c == '\\')
			c = '|';
		else if (c == '~')
			c = '^';

		out[i] = c;
		++i;
	}

	out[i] = 0;
}



IRCServer::IRCServer(Bouncer *_bouncer) :
	Server(_bouncer),
	bouncer(_bouncer),
	status(this)
{
	isActive = false;

	reconnectTime = 0;
	connectionAttempt = 0;
}

IRCServer::~IRCServer() {
	bouncer->deregisterWindow(&status);

	for (auto &i : channels) {
		bouncer->deregisterWindow(i.second);
		delete i.second;
	}

	for (auto &i : queries) {
		bouncer->deregisterWindow(i.second);
		delete i.second;
	}
}

void IRCServer::attachedToCore() {
	bouncer->registerWindow(&status);
}

void IRCServer::requestConnect() {
	status.pushMessage("Connecting...");

	isActive = true;
	connectionAttempt = 1;
	reconnectTime = 0;

	Server::connect(
		config.hostname.c_str(), config.port,
		config.useTls);
}

void IRCServer::requestDisconnect() {
	if (isActive && (reconnectTime != 0)) {
		status.pushMessage("Reconnect cancelled.");
	}

	isActive = false;
	reconnectTime = 0;

	close();
}

void IRCServer::doReconnect() {
	status.pushMessage("Reconnecting...");
	reconnectTime = 0;

	Server::connect(
		config.hostname.c_str(), config.port,
		config.useTls);
}

void IRCServer::scheduleReconnect() {
	int delay;

	if (connectionAttempt < 6)
		delay = connectionAttempt * 15;
	else
		delay = (connectionAttempt - 3) * 30;

	if (delay > 300)
		delay = 300;
	reconnectTime = time(NULL) + delay;

	char buf[256];
	snprintf(buf, sizeof(buf),
		"Reconnection attempt #%d in %d seconds...",
		connectionAttempt, delay);
	status.pushMessage(buf);

	connectionAttempt++;
}


void IRCServer::connectionStateChangedEvent() {
	Buffer pkt;
	pkt.writeU32(id);
	pkt.writeU32((int)state());

	bouncer->sendToClients(
		Packet::B2C_SERVER_CONNSTATE, pkt);
}


void IRCServer::resetIRCState() {
	strcpy(currentNick, "");
	strcpy(currentNickLower, "");

	strcpy(serverPrefix, "@+");
	strcpy(serverPrefixMode, "ov");
}


Channel *IRCServer::findChannel(const char *name, bool createIfNeeded) {
	char lowerName[512];
	ircStringToLowercase(name, lowerName, sizeof(lowerName));

	std::map<std::string, Channel *>::iterator
		check = channels.find(lowerName);

	if (check == channels.end()) {
		if (createIfNeeded) {
			Channel *c = new Channel(this, name);
			channels[lowerName] = c;
			return c;
		} else {
			return 0;
		}
	} else {
		return check->second;
	}
}
void IRCServer::deleteChannel(Channel *channel) {
	// I sense some ugly code duplication going on here ._.;;
	// TODO: Clean this up eventually...
	char lowerName[512];
	ircStringToLowercase(channel->name.c_str(), lowerName, sizeof(lowerName));

	auto i = channels.find(lowerName);
	if (i != channels.end()) {
		bouncer->deregisterWindow(channel);

		channels.erase(i);
		delete channel;
	}
}

Query *IRCServer::findQuery(const char *name, bool createIfNeeded) {
	char lowerName[512];
	ircStringToLowercase(name, lowerName, sizeof(lowerName));

	std::map<std::string, Query *>::iterator
		check = queries.find(lowerName);

	if (check == queries.end()) {
		if (createIfNeeded) {
			Query *q = new Query(this, name);
			queries[lowerName] = q;
			return q;
		} else {
			return 0;
		}
	} else {
		return check->second;
	}
}

Query *IRCServer::createQuery(const char *name) {
	return findQuery(name, true);
}

void IRCServer::deleteQuery(Query *query) {
	char lowerName[512];
	ircStringToLowercase(query->partner.c_str(), lowerName, sizeof(lowerName));

	auto i = queries.find(lowerName);
	if (i != queries.end()) {
		bouncer->deregisterWindow(query);

		queries.erase(i);
		delete query;
	}
}



void IRCServer::connectedEvent() {
	resetIRCState();

	// reset reconnection attempts
	connectionAttempt = 1;

	printf("[IRCServer:%p] connectedEvent\n", this);
	status.pushMessage("Connected, identifying to IRC...");

	char buf[2048];

	if (config.password.size() > 0) {
		sprintf(buf, "PASS %s", config.password.c_str());
		sendLine(buf);
	}

	sprintf(buf, "USER %s 0 * :%s\r\nNICK %s",
		config.username.c_str(),
		config.realname.c_str(),
		config.nickname.c_str());

	sendLine(buf);


	// Initialise currentNick with the nick we *think* we have,
	// because FurNet sends us a VERSION request as the very first
	// thing-- before any other commands, including RPL_WELCOME
	strncpy(currentNick, config.nickname.c_str(), sizeof(currentNick));
	currentNick[sizeof(currentNick) - 1] = 0;

	ircStringToLowercase(currentNick, currentNickLower, sizeof(currentNickLower));
}
void IRCServer::disconnectedEvent() {
	printf("[IRCServer:%p] disconnectedEvent\n", this);
	status.pushMessage("Disconnected.");

	for (auto &i : channels)
		i.second->disconnected();

	if (isActive)
		scheduleReconnect();
}

void IRCServer::connectionErrorEvent() {
	status.pushMessage("Connection failed.");

	if (isActive)
		scheduleReconnect();
}


void IRCServer::lineReceivedEvent(char *line, int size) {
	printf("[%d] { %s }\n", size, line);

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
		snprintf(out, 512, "PONG :%s", allParams);
		sendLine(out);
		return;

	} else if (strcmp(cmdBuf, "JOIN") == 0) {
		Channel *c = findChannel(targetBuf, true);
		if (c) {
			c->handleJoin(user);
			return;
		}

	} else if (strcmp(cmdBuf, "PART") == 0) {
		Channel *c = findChannel(targetBuf, false);
		if (c) {
			c->handlePart(user, paramsAfterFirst);
			return;
		}

	} else if (strcmp(cmdBuf, "QUIT") == 0) {
		for (auto &i : channels)
			i.second->handleQuit(user, allParams);

		if (Query *q = findQuery(user.nick.c_str(), false))
			q->handleQuit(allParams);
		return;

	} else if (strcmp(cmdBuf, "KICK") == 0) {
		char *space = strchr(paramsAfterFirst, ' ');
		const char *kickMsg = "";

		if (space) {
			*space = 0;
			kickMsg = space + 1;

			if (*kickMsg == ':')
				++kickMsg;
		}

		Channel *c = findChannel(targetBuf, false);
		if (c) {
			c->handleKick(user, paramsAfterFirst, kickMsg);
			return;
		}

	} else if (strcmp(cmdBuf, "NICK") == 0) {
		if (user.isSelf) {
			strncpy(currentNick, allParams, sizeof(currentNick));
			currentNick[sizeof(currentNick) - 1] = 0;

			ircStringToLowercase(currentNick, currentNickLower, sizeof(currentNickLower));

			char buf[1024];
			snprintf(buf, 1024, "You are now known as %s", currentNick);
			status.pushMessage(buf);

			for (auto &it : queries)
				it.second->showNickChange(user, allParams);
		}

		if (Query *q = findQuery(user.nick.c_str(), false)) {
			if (!user.isSelf)
				q->showNickChange(user, allParams);

			// Should we *rename* the query window, or not?
			Query *check = findQuery(allParams, false);
			if (check) {
				// If we already have one with the destination
				// nick, we shouldn't replace it..
				// ...but we should still show a notification there.
				// Unless check and q are the same, which can happen
				// if a user changes their nick's case. 
				if ((!user.isSelf) && (check != q))
					check->showNickChange(user, allParams);

				// And if the name's case changed, we need to update
				// the title to match!
				if (check->partner != allParams)
					check->renamePartner(allParams);

			} else {
				// We didn't have one, so it's safe to move!
				char lowerName[512];

				// First, remove the old entry..
				ircStringToLowercase(user.nick.c_str(), lowerName, sizeof(lowerName));

				auto iter = queries.find(lowerName);
				queries.erase(iter);

				// ...and then add a new one
				ircStringToLowercase(allParams, lowerName, sizeof(lowerName));
				queries[lowerName] = q;

				q->renamePartner(allParams);
			}
		}

		for (auto &i : channels)
			i.second->handleNick(user, allParams);

		return;

	} else if (strcmp(cmdBuf, "MODE") == 0) {
		Channel *c = findChannel(targetBuf, false);
		if (c) {
			c->handleMode(user, paramsAfterFirst);
			return;
		}

	} else if (strcmp(cmdBuf, "TOPIC") == 0) {
		Channel *c = findChannel(targetBuf, false);
		if (c) {
			c->handleTopic(user, paramsAfterFirst);
			return;
		}

	} else if (strcmp(cmdBuf, "PRIVMSG") == 0) {

		int endPos = strlen(paramsAfterFirst) - 1;
		bool requireQueryWindow = true;
		const char *ctcpType = NULL, *ctcpParams = NULL;

		if ((endPos > 0) &&
			(paramsAfterFirst[0] == 1) &&
			(paramsAfterFirst[endPos] == 1))
		{
			// Try to parse a CTCP
			// Cut off the 01 char at the end
			paramsAfterFirst[endPos] = 0;

			// Split the string into type + params
			char *space = strchr(paramsAfterFirst, ' ');

			ctcpType = &paramsAfterFirst[1];

			if (space) {
				*space = 0;
				ctcpParams = space + 1;
			} else {
				ctcpParams = "";
			}

			if (strcmp(ctcpType, "ACTION") != 0)
				requireQueryWindow = false;

			// This needs to be extracted into a separate
			// method at some point
			if (strcmp(ctcpType, "VERSION") == 0) {
				char reply[1000];
				snprintf(reply, sizeof(reply),
					"NOTICE %s :\x01VERSION " VULPIRC_VERSION_STRING "\x01",
					user.nick.c_str());
				sendLine(reply);

			} else if (strcmp(ctcpType, "PING") == 0) {
				char reply[1000];
				snprintf(reply, sizeof(reply),
					"NOTICE %s :\x01PING %s\x01",
					user.nick.c_str(),
					ctcpParams);
				sendLine(reply);

			} else if (strcmp(ctcpType, "TIME") == 0) {
				char reply[1000], formatTime[200];
				time_t now = time(NULL);
				tm *nowtm = localtime(&now);

				strftime(formatTime, sizeof(formatTime),
					"%c", nowtm);

				snprintf(reply, sizeof(reply),
					"NOTICE %s :\x01TIME :%s\x01",
					user.nick.c_str(),
					formatTime);
				sendLine(reply);
			}
		}



		char targetBufLower[512];
		ircStringToLowercase(targetBuf, targetBufLower, 512);

		if (strcmp(targetBufLower, currentNickLower) == 0) {
			Query *q = findQuery(user.nick.c_str(), requireQueryWindow);
			if (q) {
				if (ctcpType)
					q->handleCtcp(ctcpType, ctcpParams);
				else
					q->handlePrivmsg(paramsAfterFirst);
				return;
			} else if (ctcpType) {
				// This CTCP didn't require a query window to be
				// open, and we don't already have one, so
				// dump a notification into the status window.
				char buf[1000];
				snprintf(buf, sizeof(buf),
					"CTCP from %s : %s %s",
					user.nick.c_str(),
					ctcpType,
					ctcpParams);
				status.pushMessage(buf);
				return;
			}
		} else {
			Channel *c = findChannel(targetBuf, true);
			if (c) {
				if (ctcpType)
					c->handleCtcp(user, ctcpType, ctcpParams);
				else
					c->handlePrivmsg(user, paramsAfterFirst);
				return;
			}
		}

	} else if (strcmp(cmdBuf, "001") == 0) {
		status.pushMessage("[debug: currentNick change detected]");

		strncpy(currentNick, targetBuf, sizeof(currentNick));
		currentNick[sizeof(currentNick) - 1] = 0;

		ircStringToLowercase(currentNick, currentNickLower, sizeof(currentNickLower));
	}

	int n = atoi(cmdBuf);
	if ((n > 0) && (n <= 999) && (strlen(cmdBuf) == 3)) {
		if (dispatchNumeric(n, paramsAfterFirst))
			return;
	}

	char tmpstr[2048];
	if (user.isValid)
		snprintf(tmpstr, sizeof(tmpstr),
			"[[Unhandled \"%s\" from %s!%s@%s]]",
			cmdBuf, user.nick.c_str(), user.ident.c_str(), user.hostmask.c_str());
	else
		snprintf(tmpstr, sizeof(tmpstr),
			"[[Unhandled \"%s\"]]",
			cmdBuf);

	status.pushMessage(tmpstr);
	status.pushMessage(line);
}





uint32_t IRCServer::getUserFlag(char search, const char *array) const {
	uint32_t flag = 0x80000000;

	// Is this character a valid prefix?
	while (*array != 0) {
		if (*array == search)
			return flag;

		flag >>= 1;
		++array;
	}

	return 0;
}

uint32_t IRCServer::getUserFlagByPrefix(char prefix) const {
	return getUserFlag(prefix, serverPrefix);
} 
uint32_t IRCServer::getUserFlagByMode(char mode) const {
	return getUserFlag(mode, serverPrefixMode);
}

int IRCServer::getChannelModeType(char mode) const {
	for (int i = 0; i < 4; i++) {
		const char *modes = serverChannelModes[i].c_str();

		while (*modes != 0) {
			if (*modes == mode)
				return i + 1;
			++modes;
		}
	}

	return 0;
}

char IRCServer::getEffectivePrefixChar(uint32_t modes) const {
	uint32_t flag = 0x80000000;
	const char *prefixes = serverPrefix;

	while (*prefixes != 0) {
		if (modes & flag)
			return *prefixes;

		++prefixes;
		flag >>= 1;
	}

	return 0;
}



void IRCServer::loadFromConfig(std::map<std::string, std::string> &data) {
	config.title = data["title"];
	config.hostname = data["hostname"];
	config.username = data["username"];
	config.realname = data["realname"];
	config.nickname = data["nickname"];
	config.altNick = data["altnick"];
	config.password = data["password"];
	config.useTls = (data["tls"] == "y");
	config.port = atoi(data["port"].c_str());
}

void IRCServer::saveToConfig(std::map<std::string, std::string> &data) {
	data["type"] = "IRCServer";
	data["title"] = config.title;
	data["hostname"] = config.hostname;
	data["username"] = config.username;
	data["realname"] = config.realname;
	data["nickname"] = config.nickname;
	data["altnick"] = config.altNick;
	data["password"] = config.password;
	data["tls"] = config.useTls ? "y" : "n";

	char portstr[50];
	snprintf(portstr, sizeof(portstr), "%d", config.port);
	data["port"] = portstr;
}

void IRCServer::notifyConfigChanged() {
	Buffer pkt;
	pkt.writeU32(id);
	config.writeToBuffer(pkt);

	bouncer->sendToClients(
		Packet::B2C_SERVER_CONFIG, pkt);
}

void IRCServer::_syncStateForClientInternal(Buffer &output) {
	output.writeU32(state());
	output.writeU32(status.id);
	config.writeToBuffer(output);
}

void IRCServer::handleServerConnStateChange(int action) {
	// TODO: present these errors in a prettier way

	if (action == 1) {
		if (config.nickname.size() == 0) {
			status.pushMessage("Use /defaultnick <name> to set a nickname");
		} else if (config.altNick.size() == 0) {
			status.pushMessage("Use /altnick <name> to set an alternate nickname");
		} else if (config.hostname.size() == 0) {
			status.pushMessage("Use /server <name> to set an IRC server to connect to");
		} else {
			requestConnect();
		}
	} else if (action == 2) {
		requestDisconnect();
	}
}

void IRCServer::handleServerConfigUpdate(Buffer &buf) {
	config.readFromBuffer(buf);
	notifyConfigChanged();
}
