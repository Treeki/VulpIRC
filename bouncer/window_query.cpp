#include "core.h"
#include "richtext.h"

Query::Query(IRCServer *_server, const char *_partner) :
	IRCWindow(_server),
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

void Query::handleCommand(const char *cmd, const char *args) {
	char msgBuf[16384];

	if (strcmp(cmd, "me") == 0) {
		if (args[0] != 0) {
			// The duplication of code between here and
			// handlePrivmsg is ugly. TODO: fixme.

			snprintf(msgBuf, sizeof(msgBuf),
				"* %s %s",
				server->currentNick,
				args);
			pushMessage(msgBuf);

			snprintf(msgBuf, sizeof(msgBuf),
				"PRIVMSG %s :\x01" "ACTION %s\x01",
				partner.c_str(),
				args);
			server->sendLine(msgBuf);
		}
	} else {
		IRCWindow::handleCommand(cmd, args);
	}
}

void Query::handleUserInput(const char *str) {
	char msgBuf[16384];

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
