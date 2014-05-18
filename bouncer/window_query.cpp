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
			outputUserMessage(server->currentNick, args, /*isAction=*/true);

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

	outputUserMessage(server->currentNick, str, /*isAction=*/false);

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

void Query::handlePrivmsg(const UserRef &user, const char *str) {
	outputUserMessage(user.nick.c_str(), str, /*isAction=*/false);
}

void Query::outputUserMessage(const char *nick, const char *str, bool isAction) {
	RichTextBuilder rt;

	if (isAction) {
		rt.foreground(COL_LEVEL_BASE, COL_ACTION);
		rt.append("* ");
	} else {
		rt.writeS8('<');
	}

	rt.bold();
	rt.appendNick(nick);
	rt.endBold();
	rt.append(isAction ? " " : "> ");

	rt.appendIRC(str);

	bool isSelf = (strcmp(nick, server->currentNick) == 0);

	pushMessage(rt.c_str(), isSelf ? 0 : 3);
}

void Query::handleCtcp(const UserRef &user, const char *type, const char *params) {
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

		pushMessage(rt.c_str(), 3);
	}
}

void Query::renamePartner(const char *_partner) {
	partner = _partner;

	notifyWindowRename();
}
