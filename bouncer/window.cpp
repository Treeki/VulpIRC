#include "core.h"

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

void Window::pushMessage(const char *str) {
	messages.push_back(str);

	bool createdPacket = false;
	Buffer packet;

	for (int i = 0; i < core->clientCount; i++) {
		if (core->clients[i]->isAuthed()) {
			if (!createdPacket) {
				packet.writeU32(id);
				packet.writeStr(str);
				createdPacket = true;
			}

			core->clients[i]->sendPacket(Packet::B2C_WINDOW_MESSAGE, packet);
		}
	}
}




StatusWindow::StatusWindow(IRCServer *_server) :
	Window(_server->bouncer),
	server(_server)
{
}

const char *StatusWindow::getTitle() const {
	return server->config.hostname;
}

int StatusWindow::getType() const {
	return 1;
}

void StatusWindow::handleUserInput(const char *str) {
	if (str[0] == '/') {
		// moof
		if (strcmp(str, "/connect") == 0) {
			server->connect();
		} else if (strcmp(str, "/disconnect") == 0) {
			server->close();
		} else if (strncmp(str, "/password ", 10) == 0) {
			pushMessage("Password set.");

			// This is ugly, ugh
			strncpy(
				server->config.password,
				&str[10],
				sizeof(server->config.password));
			server->config.password[sizeof(server->config.password) - 1] = 0;
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
			// The duplication of code between here and
			// handlePrivmsg is ugly. TODO: fixme.
			char prefix[2];
			prefix[0] = getEffectivePrefixChar(server->currentNick);
			prefix[1] = 0;

			snprintf(msgBuf, sizeof(msgBuf),
				"* %s%s %s",
				prefix,
				server->currentNick,
				&str[4]);
			pushMessage(msgBuf);

			snprintf(msgBuf, sizeof(msgBuf),
				"PRIVMSG %s :\x01" "ACTION %s\x01",
				name.c_str(),
				&str[4]);
			server->sendLine(msgBuf);
		}
	} else {
		// Aaaand this is also pretty ugly ><;;
		// TODO: fixme.
		char prefix[2];
		prefix[0] = getEffectivePrefixChar(server->currentNick);
		prefix[1] = 0;

		snprintf(msgBuf, sizeof(msgBuf),
			"<%s%s> %s",
			prefix,
			server->currentNick,
			str);
		pushMessage(msgBuf);

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

	output.writeStr(topic.c_str());
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

void Channel::handleJoin(const UserRef &user) {
	if (user.isSelf) {
		Buffer packet;
		packet.writeU32(id);
		packet.writeU32(0);

		server->bouncer->sendToClients(
			Packet::B2C_CHANNEL_USER_REMOVE, packet);


		users.clear();

		inChannel = true;
		pushMessage("You have joined the channel!");
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

		char buf[1024];
		snprintf(buf, 1024,
			"%s (%s@%s) has joined",
			user.nick.c_str(),
			user.ident.c_str(),
			user.hostmask.c_str());

		pushMessage(buf);
	}
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

	char buf[1024];

	if (user.isSelf) {
		inChannel = false;

		snprintf(buf, 1024,
			"You have left the channel (%s)",
			message);
		pushMessage(buf);
	} else {
		snprintf(buf, 1024,
			"%s (%s@%s) has parted (%s)",
			user.nick.c_str(),
			user.ident.c_str(),
			user.hostmask.c_str(),
			message);

		pushMessage(buf);
	}
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

	char buf[1024];

	snprintf(buf, 1024,
		"%s (%s@%s) has quit (%s)",
		user.nick.c_str(),
		user.ident.c_str(),
		user.hostmask.c_str(),
		message);

	pushMessage(buf);
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

	char buf[1024];

	if (strcmp(target, server->currentNick) == 0) {
		inChannel = false;

		snprintf(buf, sizeof(buf),
			"You have been kicked by %s (%s)",
			user.nick.c_str(),
			message);

	} else if (user.isSelf) {
		snprintf(buf, sizeof(buf),
			"You have kicked %s (%s)",
			target,
			message);
	} else {
		snprintf(buf, sizeof(buf),
			"%s has kicked %s (%s)",
			user.nick.c_str(),
			target,
			message);
	}

	pushMessage(buf);
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

void Channel::handleMode(const UserRef &user, const char *str) {
	char copy[4096];
	strncpy(copy, str, 4096);
	copy[4095] = 0;

	char *strtok_var;
	char *modes = strtok_r(copy, " ", &strtok_var);

	if (!modes)
		return;

	bool addFlag = true;

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

			char buf[1024];
			snprintf(buf, 1024,
				"%s %s mode %c on %s%s",
				user.nick.c_str(),
				addFlag ? "set" : "cleared",
				mode,
				target,
				oops ? ", but something went wrong!" : "");
			pushMessage(buf);

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

			char buf[1024];
			snprintf(buf, 1024,
				"%s %s channel mode %c%s%s",
				user.nick.c_str(),
				addFlag ? "set" : "cleared",
				mode,
				param ? " " : "",
				param ? param : "");
			pushMessage(buf);
		}
	}
}

void Channel::handlePrivmsg(const UserRef &user, const char *str) {
	char prefix[2];
	prefix[0] = getEffectivePrefixChar(user.nick.c_str());
	prefix[1] = 0;

	char buf[15000];
	snprintf(buf, 15000,
		"<%s%s> %s",
		prefix,
		user.nick.c_str(),
		str);

	pushMessage(buf);
}




void Channel::handleTopic(const UserRef &user, const char *message) {
	char buf[1024];

	if (user.isValid) {
		snprintf(buf, sizeof(buf),
			"%s changed the topic to: %s",
			user.nick.c_str(),
			message);
	} else {
		snprintf(buf, sizeof(buf),
			"Topic: %s",
			message);
	}
	pushMessage(buf);

	topic = message;

	Buffer packet;
	packet.writeU32(id);
	packet.writeStr(message);
	server->bouncer->sendToClients(
		Packet::B2C_CHANNEL_TOPIC, packet);
}

void Channel::handleTopicInfo(const char *user, int timestamp) {
	char buf[1024];
	snprintf(buf, sizeof(buf),
		"Topic set by %s at %d",
		user,
		timestamp);
	pushMessage(buf);
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
		pushMessage("You have been disconnected.");
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

	pushMessage(buf);
}

void Query::renamePartner(const char *_partner) {
	Buffer packet;
	packet.writeU32(id);
	packet.writeStr(_partner);

	server->bouncer->sendToClients(
		Packet::B2C_WINDOW_RENAME, packet);

	partner = _partner;
}
