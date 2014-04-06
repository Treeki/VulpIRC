#include "core.h"
#include "richtext.h"

Channel::Channel(IRCServer *_server, const char *_name) :
	IRCWindow(_server),
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

void Channel::handleUserClosed() {
	if (inChannel) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "PART %s :Leaving", name.c_str());
		server->sendLine(buf);
	} else
		server->deleteChannel(this);
}

void Channel::handleCommand(const char *cmd, const char *args) {
	char msgBuf[16384];

	if (strcmp(cmd, "me") == 0) {
		if (args[0] != 0) {
			outputUserMessage(server->currentNick, args, /*isAction=*/true);

			snprintf(msgBuf, sizeof(msgBuf),
				"PRIVMSG %s :\x01" "ACTION %s\x01",
				name.c_str(),
				args);
			server->sendLine(msgBuf);
		}
	} else {
		IRCWindow::handleCommand(cmd, args);
	}
}

void Channel::handleUserInput(const char *str) {
	char msgBuf[16384];

	outputUserMessage(server->currentNick, str, /*isAction=*/false);

	snprintf(msgBuf, sizeof(msgBuf),
		"PRIVMSG %s :%s",
		name.c_str(),
		str);
	server->sendLine(msgBuf);
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
		rt.appendNick(user.nick.c_str());
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
		rt.appendNick(user.nick.c_str());

	} else {
		if (user.isSelf) {
			rt.append("You have kicked ");
		} else {
			rt.bold();
			rt.appendNick(user.nick.c_str());
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
		rt.appendNick(user.nick.c_str());
		rt.endBold();
		rt.append(" is now known as ");
	}

	rt.bold();
	rt.appendNick(newNick);
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
			rt.appendNick(user.nick.c_str());
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
			rt.appendNick(user.nick.c_str());
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
	rt.appendNick(nick);
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
		rt.appendNick(user.nick.c_str());
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
		rt.appendNick(user.nick.c_str());
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

