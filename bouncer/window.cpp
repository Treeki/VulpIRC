#include "core.h"
#include "richtext.h"

Window::Window(NetCore *_core) {
	core = _core;
}

void Window::syncStateForClient(Buffer &output) {
	output.writeU32(getType());
	output.writeU32(id);
	output.writeStr(getTitle());

	output.writeU32(messages.size());

	std::list<std::string>::iterator
		i = messages.begin(),
		e = messages.end();

	for (; i != e; ++i) {
		output.writeStr(i->c_str());
	}
}

void Window::notifyWindowRename() {
	Buffer packet;
	packet.writeU32(id);
	packet.writeStr(getTitle());

	core->sendToClients(
		Packet::B2C_WINDOW_RENAME, packet);
}

void Window::pushMessage(const char *str, int priority) {
	if (messages.size() >= core->maxWindowMessageCount)
		messages.pop_front();
	messages.push_back(str);

	bool createdPacket = false;
	Buffer packet;

	for (int i = 0; i < core->clientCount; i++) {
		if (core->clients[i]->isAuthed()) {
			if (!createdPacket) {
				packet.writeU32(id);
				packet.writeU8(priority);
				packet.writeStr(str);
				createdPacket = true;
			}

			core->clients[i]->sendPacket(Packet::B2C_WINDOW_MESSAGE, packet);
		}
	}
}

void Window::handleUserClosed() {
	// Do nothing. (For now?)
}




StatusWindow::StatusWindow(IRCServer *_server) :
	Window(_server->bouncer),
	server(_server)
{
}

const char *StatusWindow::getTitle() const {
	if (server->config.hostname.size() == 0)
		return "<New Server>";
	else
		return server->config.hostname.c_str();
}

int StatusWindow::getType() const {
	return 1;
}

void StatusWindow::handleUserInput(const char *str) {
	char buf[1024];

	if (str[0] == '/') {
		// moof
		if (strcmp(str, "/connect") == 0) {

			// Check if we have everything needed...
			if (server->config.nickname.size() == 0) {
				pushMessage("Use /defaultnick <name> to set a nickname");
			} else if (server->config.altNick.size() == 0) {
				pushMessage("Use /altnick <name> to set an alternate nickname");
			} else if (server->config.hostname.size() == 0) {
				pushMessage("Use /server <name> to set an IRC server to connect to");
			} else {
				server->connect();
			}

		} else if (strcmp(str, "/disconnect") == 0) {
			server->close();

		} else if (strncmp(str, "/defaultnick ", 13) == 0) {
			server->config.nickname = &str[13];

			// generate a default altnick if we don't have one already
			if (server->config.altNick.size() == 0) {
				server->config.altNick = server->config.nickname + "_";
			}

			snprintf(buf, sizeof(buf),
				"Default nickname changed to: %s",
				server->config.nickname.c_str());
			pushMessage(buf);

		} else if (strncmp(str, "/altnick ", 9) == 0) {
			server->config.altNick = &str[9];

			snprintf(buf, sizeof(buf),
				"Alternate nickname changed to: %s",
				server->config.altNick.c_str());
			pushMessage(buf);

		} else if (strncmp(str, "/server ", 8) == 0) {
			server->config.hostname = &str[8];

			snprintf(buf, sizeof(buf),
				"Server address changed to: %s",
				server->config.hostname.c_str());
			pushMessage(buf);

			notifyWindowRename();

		} else if (strncmp(str, "/port ", 6) == 0) {
			const char *p = &str[6];
			if (*p == '+') {
				server->config.useTls = true;
				++p;
			} else {
				server->config.useTls = false;
			}
			server->config.port = atoi(p);

			snprintf(buf, sizeof(buf),
				"Server port changed to %d, TLS %s",
				server->config.port,
				server->config.useTls ? "on" : "off");
			pushMessage(buf);

		} else if (strncmp(str, "/username ", 10) == 0) {
			server->config.username = &str[10];

			snprintf(buf, sizeof(buf),
				"Username changed to: %s",
				server->config.username.c_str());
			pushMessage(buf);

		} else if (strncmp(str, "/realname ", 10) == 0) {
			server->config.realname = &str[10];

			snprintf(buf, sizeof(buf),
				"Real name changed to: %s",
				server->config.username.c_str());
			pushMessage(buf);

		} else if (strncmp(str, "/password", 9) == 0) {

			if (strlen(str) == 9)
				server->config.password = "";
			else
				server->config.password = &str[10];

			if (server->config.password.size() > 0)
				pushMessage("Server password changed.");
			else
				pushMessage("Server password cleared.");
		}
	} else {
		server->sendLine(str);
	}
}




Channel::Channel(IRCServer *_server, const char *_name) :
	Window(_server->bouncer),
	server(_server),
	inChannel(false),
	name(_name)
{
	server->bouncer->registerWindow(this);
}

const char *Channel::getTitle() const {
	return name.c_str();
}

int Channel::getType() const {
	return 2;
}

void Channel::handleUserInput(const char *str) {
	char msgBuf[16384];

	if (str[0] == '/') {
		if (strncmp(str, "/me ", 4) == 0) {
			outputUserMessage(server->currentNick, &str[4], /*isAction=*/true);

			snprintf(msgBuf, sizeof(msgBuf),
				"PRIVMSG %s :\x01" "ACTION %s\x01",
				name.c_str(),
				&str[4]);
			server->sendLine(msgBuf);
		}
	} else {
		outputUserMessage(server->currentNick, str, /*isAction=*/false);

		snprintf(msgBuf, sizeof(msgBuf),
			"PRIVMSG %s :%s",
			name.c_str(),
			str);
		server->sendLine(msgBuf);
	}
}

void Channel::syncStateForClient(Buffer &output) {
	Window::syncStateForClient(output);

	output.writeU32(users.size());

	for (auto &i : users) {
		output.writeStr(i.first.c_str());
		output.writeU32(i.second);
		output.writeU8(server->getEffectivePrefixChar(i.second));
	}

	RichTextBuilder topicConverter;
	topicConverter.appendIRC(topic.c_str());
	output.writeStr(topicConverter.c_str());
}


void Channel::handleNameReply(const char *str) {
	char copy[4096];
	strncpy(copy, str, 4096);
	copy[4095] = 0;

	char *strtok_var;
	char *name = strtok_r(copy, " ", &strtok_var);

	int nameCount = 0;

	Buffer packet;
	packet.writeU32(id);
	packet.writeU32(0); // Dummy value..!

	while (name) {
		uint32_t modes = 0;

		// Check the beginning of the name for as many valid
		// mode prefixes as possible
		// Servers may only send one, but I want to take into
		// account the possibility that they might send multiple
		// ones. Just in case.

		while (*name != 0) {
			uint32_t flag = server->getUserFlagByPrefix(*name);

			if (flag == 0)
				break;
			else
				modes |= flag;

			++name;
		}

		// Got it!
		users[name] = modes;

		nameCount++;
		packet.writeStr(name);
		packet.writeU32(modes);
		packet.writeU8(server->getEffectivePrefixChar(modes));

		// Get the next name
		name = strtok_r(NULL, " ", &strtok_var);
	}

	if (nameCount > 0) {
		uint32_t nameCountU32 = nameCount;
		memcpy(&packet.data()[4], &nameCountU32, sizeof(uint32_t));

		server->bouncer->sendToClients(
			Packet::B2C_CHANNEL_USER_ADD, packet);
	}
}

void Channel::outputUserAction(int colour, const UserRef &user, const char *prefix, const char *verb, const char *message, bool showChannel) {
	RichTextBuilder rt;
	rt.foreground(COL_LEVEL_BASE, colour);
	rt.append(prefix);
	rt.writeU8(' ');

	if (user.isSelf) {
		rt.append("You have ");
	} else {
		rt.bold();
		rt.append(user.nick.c_str());
		rt.endBold();

		rt.append(" (");

		rt.foreground(COL_LEVEL_BASE + 1, COL_DEFAULT_FG);
		rt.append(user.ident.c_str());
		rt.writeU8('@');
		rt.append(user.hostmask.c_str());
		rt.endForeground(COL_LEVEL_BASE + 1);

		rt.append(") has ");
	}

	rt.append(verb);

	if (showChannel) {
		rt.writeU8(' ');
		rt.append(name.c_str());
	}

	if (message) {
		rt.foreground(COL_LEVEL_BASE + 1, COL_DEFAULT_FG);
		rt.append(" (");
		rt.appendIRC(message);
		rt.writeU8(')');
	}

	pushMessage(rt.c_str());
}

void Channel::handleJoin(const UserRef &user) {
	if (user.isSelf) {
		Buffer packet;
		packet.writeU32(id);
		packet.writeU32(0);

		server->bouncer->sendToClients(
			Packet::B2C_CHANNEL_USER_REMOVE, packet);

		users.clear();

		inChannel = true;

	} else {
		Buffer packet;
		packet.writeU32(id);
		packet.writeU32(1);
		packet.writeStr(user.nick.c_str());
		packet.writeU32(0);
		packet.writeU8(0);

		server->bouncer->sendToClients(
			Packet::B2C_CHANNEL_USER_ADD, packet);

		users[user.nick] = 0;
	}

	outputUserAction(COL_JOIN, user, "->", "joined", 0);
}

void Channel::handlePart(const UserRef &user, const char *message) {
	auto i = users.find(user.nick);
	if (i != users.end()) {
		users.erase(i);

		Buffer packet;
		packet.writeU32(id);
		packet.writeU32(1);
		packet.writeStr(user.nick.c_str());

		server->bouncer->sendToClients(
			Packet::B2C_CHANNEL_USER_REMOVE, packet);
	}

	if (user.isSelf) {
		inChannel = false;
	}

	outputUserAction(COL_PART, user, "<-", "parted", message);
}

void Channel::handleQuit(const UserRef &user, const char *message) {
	if (user.isSelf)
		inChannel = false;

	auto i = users.find(user.nick);
	if (i == users.end())
		return;

	users.erase(i);

	Buffer packet;
	packet.writeU32(id);
	packet.writeU32(1);
	packet.writeStr(user.nick.c_str());

	server->bouncer->sendToClients(
		Packet::B2C_CHANNEL_USER_REMOVE, packet);

	outputUserAction(COL_QUIT, user, "<-", "quit", message, false);
}

void Channel::handleKick(const UserRef &user, const char *target, const char *message) {
	auto i = users.find(target);
	if (i != users.end()) {
		users.erase(i);

		Buffer packet;
		packet.writeU32(id);
		packet.writeU32(1);
		packet.writeStr(target);

		server->bouncer->sendToClients(
			Packet::B2C_CHANNEL_USER_REMOVE, packet);
	}

	RichTextBuilder rt;
	rt.foreground(COL_LEVEL_BASE, COL_KICK);
	rt.append("*** ");

	if (strcmp(target, server->currentNick) == 0) {
		inChannel = false;

		rt.append("You have been kicked by ");
		rt.append(user.nick.c_str());

	} else {
		if (user.isSelf) {
			rt.append("You have kicked ");
		} else {
			rt.bold();
			rt.append(user.nick.c_str());
			rt.endBold();
			rt.append(" has kicked ");
		}
		rt.bold();
		rt.append(target);
		rt.endBold();
	}

	rt.foreground(COL_LEVEL_BASE + 1, COL_DEFAULT_FG);
	rt.append(" (");
	rt.appendIRC(message);
	rt.writeU8(')');

	pushMessage(rt.c_str());
}

void Channel::handleNick(const UserRef &user, const char *newNick) {
	auto i = users.find(user.nick);
	if (i == users.end())
		return;

	users[newNick] = i->second;
	users.erase(i);

	Buffer packet;
	packet.writeU32(id);
	packet.writeStr(user.nick.c_str());
	packet.writeStr(newNick);

	server->bouncer->sendToClients(
		Packet::B2C_CHANNEL_USER_RENAME, packet);

	RichTextBuilder rt;
	rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);
	rt.append("*** ");

	if (user.isSelf) {
		rt.append("You are now known as ");
	} else {
		rt.bold();
		rt.append(user.nick.c_str());
		rt.endBold();
		rt.append(" is now known as ");
	}

	rt.bold();
	rt.append(newNick);
	rt.endBold();

	pushMessage(rt.c_str());
}

void Channel::handleMode(const UserRef &user, const char *str) {
	char copy[4096];
	strncpy(copy, str, 4096);
	copy[4095] = 0;

	char *strtok_var;
	char *modes = strtok_r(copy, " ", &strtok_var);

	if (!modes)
		return;

	bool addFlag = true;

	RichTextBuilder rt;

	while (*modes != 0) {
		char mode = *(modes++);

		uint32_t flag;

		if (mode == '+') {
			addFlag = true;
		} else if (mode == '-') {
			addFlag = false;

		} else if ((flag = server->getUserFlagByMode(mode)) != 0) {
			bool oops = false;
			char *target = strtok_r(NULL, " ", &strtok_var);

			auto i = users.find(target);
			if (i == users.end()) {
				// Oops? Spit out an error...
				oops = true;
			} else {
				uint32_t flags = i->second;
				if (addFlag)
					flags |= flag;
				else
					flags &= ~flag;
				users[target] = flags;

				Buffer packet;
				packet.writeU32(id);
				packet.writeStr(target);
				packet.writeU32(flags);
				packet.writeU8(server->getEffectivePrefixChar(flags));

				server->bouncer->sendToClients(
					Packet::B2C_CHANNEL_USER_MODES, packet);
			}

			rt.clear();
			rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);
			rt.append("-- ");

			rt.bold();
			rt.append(user.nick.c_str());
			rt.endBold();

			rt.append(addFlag ? " set mode " : " cleared mode ");
			rt.writeS8(mode);
			rt.append(" on ");

			rt.bold();
			rt.append(target);
			rt.endBold();

			if (oops)
				rt.append(", but something went wrong!");

			pushMessage(rt.c_str());

		} else {
			int type = server->getChannelModeType(mode);
			char *param = 0;

			switch (type) {
				case 1:
				case 2:
					// Always get a parameter
					param = strtok_r(NULL, " ", &strtok_var);
					break;
				case 3:
					// Only get a parameter if adding
					if (addFlag)
						param = strtok_r(NULL, " ", &strtok_var);
					break;
			}

			rt.clear();
			rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);
			rt.append("-- ");

			rt.bold();
			rt.append(user.nick.c_str());
			rt.endBold();

			rt.append(addFlag ? " set channel mode " : " cleared channel mode ");
			rt.writeS8(mode);
			if (param) {
				rt.writeS8(' ');
				rt.append(param);
			}

			pushMessage(rt.c_str());
		}
	}
}

void Channel::handlePrivmsg(const UserRef &user, const char *str) {
	outputUserMessage(user.nick.c_str(), str, /*isAction=*/false);
}

void Channel::outputUserMessage(const char *nick, const char *str, bool isAction) {
	RichTextBuilder rt;

	if (isAction) {
		rt.foreground(COL_LEVEL_BASE, COL_ACTION);
		rt.append("* ");
	} else {
		rt.writeS8('<');
	}

	char prefix = getEffectivePrefixChar(nick);
	if (prefix != 0)
		rt.writeS8(prefix);

	rt.bold();
	rt.append(nick);
	rt.endBold();
	rt.append(isAction ? " " : "> ");

	rt.appendIRC(str);

	pushMessage(rt.c_str(), 2);
}

void Channel::handleCtcp(const UserRef &user, const char *type, const char *params) {
	if (strcmp(type, "ACTION") == 0) {
		outputUserMessage(user.nick.c_str(), params, /*isAction=*/true);
	} else {
		RichTextBuilder rt;

		rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);

		rt.append("*** CTCP from ");
		rt.bold();
		rt.append(user.nick.c_str());
		rt.endBold();
		rt.append(": ");

		rt.foreground(COL_LEVEL_BASE, COL_DEFAULT_FG);
		rt.appendIRC(params);

		pushMessage(rt.c_str(), 2);
	}
}



void Channel::handleTopic(const UserRef &user, const char *message) {
	RichTextBuilder rt;
	rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);

	if (user.isValid) {
		rt.append("*** ");
		rt.bold();
		rt.append(user.nick.c_str());
		rt.endBold();
		rt.append(" changed the topic to: ");
	} else {
		rt.append("*** Topic: ");
	}
	rt.foreground(COL_LEVEL_BASE, COL_DEFAULT_FG);
	rt.appendIRC(message);

	pushMessage(rt.c_str());

	topic = message;

	// Send a parsed topic
	rt.clear();
	rt.appendIRC(message);

	Buffer packet;
	packet.writeU32(id);
	packet.writeStr(rt.c_str());
	server->bouncer->sendToClients(
		Packet::B2C_CHANNEL_TOPIC, packet);
}

void Channel::handleTopicInfo(const char *user, int timestamp) {
	char intConv[50];
	snprintf(intConv, sizeof(intConv), "%d", timestamp);

	RichTextBuilder rt;
	rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);

	rt.append("*** Topic set by ");
	rt.bold();
	rt.append(user);
	rt.endBold();
	rt.append(" at ");
	rt.append(intConv);

	pushMessage(rt.c_str());
}



char Channel::getEffectivePrefixChar(const char *nick) const {
	auto i = users.find(nick);
	if (i == users.end())
		return 0;

	return server->getEffectivePrefixChar(i->second);
}


void Channel::disconnected() {
	if (inChannel) {
		inChannel = false;

		RichTextBuilder rt;
		rt.foreground(COL_LEVEL_BASE, COL_QUIT);
		rt.append("*** You have been disconnected.");
		pushMessage(rt.c_str());
	}
}






Query::Query(IRCServer *_server, const char *_partner) :
	Window(_server->bouncer),
	server(_server),
	partner(_partner)
{
	server->bouncer->registerWindow(this);
}

const char *Query::getTitle() const {
	return partner.c_str();
}

int Query::getType() const {
	return 3;
}


void Query::handleUserClosed() {
	server->deleteQuery(this);
}

void Query::handleUserInput(const char *str) {
	char msgBuf[16384];

	if (str[0] == '/') {
		if (strncmp(str, "/me ", 4) == 0) {
			// The duplication of code between here and
			// handlePrivmsg is ugly. TODO: fixme.

			snprintf(msgBuf, sizeof(msgBuf),
				"* %s %s",
				server->currentNick,
				&str[4]);
			pushMessage(msgBuf);

			snprintf(msgBuf, sizeof(msgBuf),
				"PRIVMSG %s :\x01" "ACTION %s\x01",
				partner.c_str(),
				&str[4]);
			server->sendLine(msgBuf);
		}
	} else {
		// Aaaand this is also pretty ugly ><;;
		// TODO: fixme.

		snprintf(msgBuf, sizeof(msgBuf),
			"<%s> %s",
			server->currentNick,
			str);
		pushMessage(msgBuf);

		snprintf(msgBuf, sizeof(msgBuf),
			"PRIVMSG %s :%s",
			partner.c_str(),
			str);
		server->sendLine(msgBuf);
	}
}

void Query::handleQuit(const char *message) {
	char buf[1024];

	snprintf(buf, 1024,
		"%s has quit (%s)",
		partner.c_str(),
		message);

	pushMessage(buf);
}

void Query::showNickChange(const UserRef &user, const char *newNick) {
	char buf[1024];

	if (user.isSelf) {
		snprintf(buf, 1024,
			"You are now known as %s",
			newNick);
	} else {
		snprintf(buf, 1024,
			"%s is now known as %s",
			user.nick.c_str(),
			newNick);
	}

	pushMessage(buf);
}

void Query::handlePrivmsg(const char *str) {
	char buf[15000];
	snprintf(buf, 15000,
		"<%s> %s",
		partner.c_str(),
		str);

	pushMessage(buf, 2);
}

void Query::handleCtcp(const char *type, const char *params) {
	char buf[15000];

	if (strcmp(type, "ACTION") == 0) {
		snprintf(buf, sizeof(buf),
			"* %s %s",
			partner.c_str(),
			params);

	} else {
		snprintf(buf, sizeof(buf),
			"CTCP from %s : %s %s",
			partner.c_str(),
			type,
			params);
	}

	pushMessage(buf, 2);
}

void Query::renamePartner(const char *_partner) {
	partner = _partner;

	notifyWindowRename();
}
