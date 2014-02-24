#include "core.h"
#include "richtext.h"

#ifdef USE_ZLIB
#include "zlibwrapper.h"
#endif

MobileClient::MobileClient(Bouncer *_bouncer) : Client(_bouncer) {
	bouncer = _bouncer;
}

void MobileClient::sessionStartEvent() {
	char tmp[1024];

	printf("{Session started}\n");

	// Welcome message
	gethostname(tmp, sizeof(tmp));
	tmp[sizeof(tmp) - 1] = 0;

	RichTextBuilder rt;
	rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);
	rt.bold();
	rt.append("Welcome to " VULPIRC_VERSION_STRING " on ");
	rt.append(tmp);

	snprintf(tmp, sizeof(tmp),
		" - %d client%s connected",
		bouncer->clientCount,
		(bouncer->clientCount == 1) ? "" : "s");
	rt.append(tmp);

	sendDebugMessage(rt.c_str());


	// Prepare sync
	Buffer syncPacket;
	syncPacket.writeU32(bouncer->windows.size());

	std::list<Window *>::iterator
		i = bouncer->windows.begin(),
		e = bouncer->windows.end();

	for (; i != e; ++i)
		(*i)->syncStateForClient(syncPacket);

#ifdef USE_ZLIB
	Buffer deflatedSync;
	bool didCompress = ZlibWrapper::compress(syncPacket, deflatedSync);
#endif

	// Send info about the sync
	snprintf(tmp, sizeof(tmp),
		"Synchronising %d window%s, %d byte%s total...",
		bouncer->windows.size(),
		(bouncer->windows.size() == 1) ? "" : "s",
		syncPacket.size(),
		(syncPacket.size() == 1) ? "" : "s");

	rt.clear();
	rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);
	rt.append(tmp);

#ifdef USE_ZLIB
	if (didCompress) {
		snprintf(tmp, sizeof(tmp),
			" (compressed to %d byte%s)",
			deflatedSync.size(),
			(deflatedSync.size() == 1) ? "" : "s");
		rt.append(tmp);
	} else {
		rt.append(" (not compressed due to an error)");
	}
#endif

	sendDebugMessage(rt.c_str());

	// and finally, send the sync data over
#ifdef USE_ZLIB
	sendPacket(Packet::B2C_WINDOW_ADD_COMPRESSED, deflatedSync);
#else
	sendPacket(Packet::B2C_WINDOW_ADD, syncPacket);
#endif

	rt.clear();
	rt.foreground(COL_LEVEL_BASE, COL_CHANNEL_NOTICE);
	rt.append("Synchronisation complete!");

	sendDebugMessage(rt.c_str());
}

void MobileClient::sessionEndEvent() {
	printf("{Session ended}\n");
}



void MobileClient::packetReceivedEvent(Packet::Type type, Buffer &pkt) {
	if (type == Packet::C2B_COMMAND) {
		char cmd[2048];
		pkt.readStr(cmd, sizeof(cmd));
		handleDebugCommand(cmd, strlen(cmd));

	} else if (type == Packet::C2B_WINDOW_INPUT) {
		int winID = pkt.readU32();
		Window *window = bouncer->findWindow(winID);
		if (!window) {
			printf("[MobileClient:%p] Message for unknown window %d\n", this, winID);
			return;
		}

		int ackID = pkt.readU8();

		char text[8192];
		pkt.readStr(text, sizeof(text));

		window->handleRawUserInput(text, this, ackID);

	} else if (type == Packet::C2B_WINDOW_CLOSE) {
		int winID = pkt.readU32();
		Window *window = bouncer->findWindow(winID);
		if (!window) {
			printf("[MobileClient:%p] Close request for unknown window %d\n", this, winID);
			return;
		}

		window->handleUserClosed();

	} else {
		printf("[MobileClient:%p] Unrecognised packet for MobileClient: type %d, size %d\n",
			this, type, pkt.size());
	}
}

void MobileClient::handleDebugCommand(char *line, int size) {
	// This is a terrible mess that will be replaced shortly
	if (strncmp(line, "all ", 4) == 0) {
		Buffer pkt;
		pkt.writeStr(&line[4]);
		for (int i = 0; i < bouncer->clientCount; i++)
			bouncer->clients[i]->sendPacket(Packet::B2C_STATUS, pkt);

	} else if (strcmp(line, "quit") == 0) {
		bouncer->quitFlag = true;
	} else if (strcmp(line, "addsrv") == 0) {
		IRCServer *srv = new IRCServer(bouncer);
		bouncer->registerServer(srv);
	} else if (strcmp(line, "save") == 0) {
		bouncer->saveConfig();
		sendDebugMessage("Bouncer configuration saved.");
	}
}

void MobileClient::sendDebugMessage(const char *msg) {
	Buffer pkt;
	pkt.writeStr(msg);
	sendPacket(Packet::B2C_STATUS, pkt);
}
