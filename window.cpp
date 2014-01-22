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



void Channel::handleJoin(const UserRef &user) {
	if (user.isSelf) {
		users.clear();
		inChannel = true;
		pushMessage("You have joined the channel!");
	} else {
		char buf[1024];
		snprintf(buf, 1024,
			"%s (%s@%s) has joined",
			user.nick.c_str(),
			user.ident.c_str(),
			user.hostmask.c_str());

		pushMessage(buf);
	}
}
void Channel::handlePrivmsg(const UserRef &user, const char *str) {
	char buf[15000];
	snprintf(buf, 15000,
		"<%s> %s",
		user.nick.c_str(),
		str);
	
	pushMessage(buf);
}
