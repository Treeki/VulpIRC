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
	Window(_server->bouncer)
{
	server = _server;
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
