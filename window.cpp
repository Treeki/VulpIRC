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
	if (str[0] == '/') {
	} else {
		server->sendLine(str);
	}
}

void Channel::syncStateForClient(Buffer &output) {
	Window::syncStateForClient(output);
}


void Channel::handleNameReply(const char *str) {
	char copy[4096];
	strncpy(copy, str, 4096);
	copy[4095] = 0;

	char *strtok_var;
	char *name = strtok_r(copy, " ", &strtok_var);

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

		// TODO: push add command

		// Get the next name
		name = strtok_r(NULL, " ", &strtok_var);
	}
}

void Channel::handleJoin(const UserRef &user) {
	if (user.isSelf) {
		users.clear();
		inChannel = true;
		pushMessage("You have joined the channel!");
	} else {
		users[user.nick] = 0;
		// TODO: push add command

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
		// TODO: push remove command
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
	// TODO: push remove command
	pushMessage("Removed from users");

	char buf[1024];

	snprintf(buf, 1024,
		"%s (%s@%s) has quit (%s)",
		user.nick.c_str(),
		user.ident.c_str(),
		user.hostmask.c_str(),
		message);

	pushMessage(buf);
}

void Channel::handleNick(const UserRef &user, const char *newNick) {
	auto i = users.find(user.nick);
	if (i == users.end())
		return;

	users[newNick] = i->second;
	users.erase(i);
	// TODO: push rename command

	char buf[1024];
	snprintf(buf, 1024,
		"%s is now known as %s",
		user.nick.c_str(),
		newNick);

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
				// TODO: push mode change to clients
				uint32_t flags = i->second;
				if (addFlag)
					flags |= flag;
				else
					flags &= ~flag;
				users[target] = flags;
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


char Channel::getEffectivePrefixChar(const char *nick) const {
	auto i = users.find(nick);
	if (i == users.end())
		return 0;

	// Maybe this bit would work best as an IRCServer method?

	uint32_t modes = i->second;
	uint32_t flag = 1;
	char *prefixes = server->serverPrefix;

	while (*prefixes != 0) {
		if (modes & flag)
			return *prefixes;

		++prefixes;
		flag <<= 1;
	}

	return 0;
}


void Channel::disconnected() {
	if (inChannel) {
		inChannel = false;
		pushMessage("You have been disconnected.");
	}
}
