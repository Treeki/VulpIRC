#include "core.h"
#include "richtext.h"

IRCWindow::IRCWindow(IRCServer *_server) :
	Window(_server->bouncer),
	server(_server)
{
}




static char _chanNameBuf[256];
void parseChannelCommand(const char *inChanName, const char *inArgs, const char **outChanName, const char **outArgs) {
	*outChanName = inChanName;
	*outArgs = NULL;

	if ((inArgs != NULL) && (inArgs[0] != 0)) {
		// We have arguments, but what..?
		*outArgs = inArgs;

		// TODO: check for all channel prefixes here
		if (inArgs[0] == '#') {
			int i = 0;
			while ((inArgs[i] != ' ') && (i < 255)) {
				_chanNameBuf[i] = inArgs[i];
				++i;
			}
			_chanNameBuf[i] = 0;

			*outChanName = _chanNameBuf;

			// Any more arguments after this?
			const char *space = strchr(inArgs, ' ');
			*outArgs = (space == NULL) ? NULL : (space + 1);
		}
	}
}

IRCWindow::CommandRef IRCWindow::commands[] = {
	{"join", &IRCWindow::commandJoin},
	{"part", &IRCWindow::commandPart},
	{"quit", &IRCWindow::commandQuit},
	{"topic", &IRCWindow::commandTopic},
	{"mode", &IRCWindow::commandMode},
	{"kick", &IRCWindow::commandKick},
	{"query", &IRCWindow::commandQuery},
	{"msg", &IRCWindow::commandQuery},
	{"", NULL}
};

void IRCWindow::commandJoin(const char *args) {
	const char *chanName, *extra;
	parseChannelCommand(getChannelName(), args, &chanName, &extra);

	if (chanName) {
		char buf[1024];
		if ((extra == NULL) || (extra[0] == 0))
			snprintf(buf, sizeof(buf), "JOIN %s", chanName);
		else
			snprintf(buf, sizeof(buf), "JOIN %s %s", chanName, extra);
		server->sendLine(buf);
	} else {
		pushMessage("You must enter a channel name to join");
	}
}

void IRCWindow::commandPart(const char *args) {
	const char *chanName, *message;
	parseChannelCommand(getChannelName(), args, &chanName, &message);

	if (chanName) {
		char buf[1024];
		if ((message == NULL) || (message[0] == 0))
			message = "Leaving";
		snprintf(buf, sizeof(buf), "PART %s :%s", chanName, message);
		server->sendLine(buf);
	} else {
		pushMessage("This is not a channel buffer");
	}
}

void IRCWindow::commandQuit(const char *args) {
	char buf[1024];
	if (args[0] == 0) {
		snprintf(buf, sizeof(buf), "QUIT :Leaving - %s", VULPIRC_VERSION_STRING);
	} else {
		snprintf(buf, sizeof(buf), "QUIT :%s", args);
	}
	server->sendLine(buf);
}

void IRCWindow::commandTopic(const char *args) {
	const char *chanName, *topic;
	parseChannelCommand(getChannelName(), args, &chanName, &topic);

	if (chanName) {
		char buf[1024];
		if ((topic == NULL) || (topic[0] == 0))
			snprintf(buf, sizeof(buf), "TOPIC %s", chanName);
		else
			snprintf(buf, sizeof(buf), "TOPIC %s :%s", chanName, topic);
		server->sendLine(buf);
	} else {
		pushMessage("This is not a channel buffer");
	}
}

void IRCWindow::commandMode(const char *args) {
	char buf[1024];
	const char *chanName, *modes;
	parseChannelCommand(getChannelName(), args, &chanName, &modes);

	if (chanName) {
		if ((modes == NULL) || (modes[0] == 0))
			snprintf(buf, sizeof(buf), "MODE %s", chanName);
		else
			snprintf(buf, sizeof(buf), "MODE %s %s", chanName, modes);
	} else {
		if ((modes == NULL) || (modes[0] == 0)) {
			snprintf(buf, sizeof(buf), "MODE %s", server->currentNick);
		} else if ((modes[0] == '+') || (modes[0] == '-')) {
			snprintf(buf, sizeof(buf), "MODE %s %s", server->currentNick, modes);
		} else {
			snprintf(buf, sizeof(buf), "MODE %s", modes);
		}
	}
	server->sendLine(buf);
}

void IRCWindow::commandKick(const char *args) {
	char buf[1024];
	const char *chanName, *subArgs;
	parseChannelCommand(getChannelName(), args, &chanName, &subArgs);

	const char *space = strchr(subArgs, ' ');

	if (space) {
		char name[256];
		int nameSize = space - subArgs;
		if (nameSize > 255)
			nameSize = 255;
		memcpy(name, subArgs, nameSize);
		name[nameSize] = 0;

		snprintf(buf, sizeof(buf), "KICK %s %s :%s", chanName, name, space+1);
	} else {
		snprintf(buf, sizeof(buf), "KICK %s %s", chanName, subArgs);
	}

	server->sendLine(buf);
}

void IRCWindow::commandQuery(const char *args) {
	if (args[0] == 0 || args[0] == ' ') {
		pushMessage("No name supplied for /query");
		return;
	}

	const char *space = strchr(args, ' ');

	if (space) {
		char name[256];
		int nameSize = space - args;
		if (nameSize > 255)
			nameSize = 255;
		memcpy(name, args, nameSize);
		name[nameSize] = 0;

		Query *q = server->createQuery(name);
		q->handleRawUserInput(space + 1);
	} else {
		server->createQuery(args);
	}
}

void IRCWindow::handleCommand(const char *cmd, const char *args) {
	char buf[1024];

	for (int i = 0; commands[i].func != NULL; i++) {
		if (strcmp(cmd, commands[i].cmd) == 0) {
			(this->*(commands[i].func))(args);
			return;
		}
	}

	snprintf(buf, sizeof(buf), "Unknown command: %s", cmd);
	pushMessage(buf);
}



StatusWindow::StatusWindow(IRCServer *_server) : IRCWindow(_server)
{
}

const char *StatusWindow::getTitle() const {
	if (server->config.title.size() != 0)
		return server->config.title.c_str();
	else if (server->config.hostname.size() != 0)
		return server->config.hostname.c_str();
	else
		return "<New Server>";
}

int StatusWindow::getType() const {
	return 1;
}

void StatusWindow::handleCommand(const char *cmd, const char *args) {
	char buf[1024];

	if (strcmp(cmd, "connect") == 0) {
		// Check if we have everything needed...
		if (server->config.nickname.size() == 0) {
			pushMessage("Use /defaultnick <name> to set a nickname");
		} else if (server->config.altNick.size() == 0) {
			pushMessage("Use /altnick <name> to set an alternate nickname");
		} else if (server->config.hostname.size() == 0) {
			pushMessage("Use /server <name> to set an IRC server to connect to");
		} else {
			server->requestConnect();
		}

	} else if (strcmp(cmd, "disconnect") == 0) {
		server->requestDisconnect();

	} else if (strcmp(cmd, "defaultnick") == 0) {
		server->config.nickname = args;

		// generate a default altnick if we don't have one already
		if (server->config.altNick.size() == 0) {
			server->config.altNick = server->config.nickname + "_";
		}

		snprintf(buf, sizeof(buf),
			"Default nickname changed to: %s",
			server->config.nickname.c_str());
		pushMessage(buf);

	} else if (strcmp(cmd, "altnick") == 0) {
		server->config.altNick = args;

		snprintf(buf, sizeof(buf),
			"Alternate nickname changed to: %s",
			server->config.altNick.c_str());
		pushMessage(buf);

	} else if (strcmp(cmd, "server") == 0) {
		server->config.hostname = args;

		snprintf(buf, sizeof(buf),
			"Server address changed to: %s",
			server->config.hostname.c_str());
		pushMessage(buf);

		notifyWindowRename();

	} else if (strcmp(cmd, "port") == 0) {
		if (args[0] == '+') {
			server->config.useTls = true;
			server->config.port = atoi(&args[1]);
		} else {
			server->config.useTls = false;
			server->config.port = atoi(args);
		}

		snprintf(buf, sizeof(buf),
			"Server port changed to %d, TLS %s",
			server->config.port,
			server->config.useTls ? "on" : "off");
		pushMessage(buf);

	} else if (strcmp(cmd, "username") == 0) {
		server->config.username = args;

		snprintf(buf, sizeof(buf),
			"Username changed to: %s",
			server->config.username.c_str());
		pushMessage(buf);

	} else if (strcmp(cmd, "realname") == 0) {
		server->config.realname = args;

		snprintf(buf, sizeof(buf),
			"Real name changed to: %s",
			server->config.username.c_str());
		pushMessage(buf);

	} else if (strcmp(cmd, "password") == 0) {
		server->config.password = args;

		if (server->config.password.size() > 0)
			pushMessage("Server password changed.");
		else
			pushMessage("Server password cleared.");

	} else if (strcmp(cmd, "title") == 0) {
		server->config.title = args;

		if (server->config.title.size() > 0)
			pushMessage("Server title changed.");
		else
			pushMessage("Server title cleared.");

		notifyWindowRename();

	} else {
		IRCWindow::handleCommand(cmd, args);
	}
}

void StatusWindow::handleUserInput(const char *str) {
	server->sendLine(str);
}

