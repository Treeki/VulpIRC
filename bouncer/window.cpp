#include "core.h"
#include "richtext.h"

Window::Window(NetCore *_core) {
	core = _core;

	currentAckClient = 0;
	currentAckID = 0;
}

void Window::syncStateForClient(Buffer &output) {
	output.writeU32(getType());
	output.writeU32(id);
	output.writeStr(getTitle());

	output.writeU32(messages.size());

	std::list<Message>::iterator
		i = messages.begin(),
		e = messages.end();

	for (; i != e; ++i) {
		output.writeU32((uint32_t)i->time);
		output.writeStr(i->text.c_str());
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

	time_t now;
#ifdef _WIN32
	now = time(NULL);
#else
	timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	now = t.tv_sec;
#endif

	Message m;
	m.time = now;
	m.text = str;
	messages.push_back(m);

	bool createdPacket = false;
	int ackPosition = 0;
	Buffer packet;

	for (int i = 0; i < core->clientCount; i++) {
		if (core->clients[i]->isAuthed()) {
			if (!createdPacket) {
				packet.writeU32(id);
				packet.writeU32((uint32_t)m.time);
				packet.writeU8(priority);
				ackPosition = packet.size();
				packet.writeU8(0); // ACK response ID
				packet.writeStr(str);
				createdPacket = true;
			}

			if (core->clients[i] == currentAckClient) {
				packet.data()[ackPosition] = currentAckID;
				currentAckClient = 0;
				currentAckID = 0;
			} else {
				packet.data()[ackPosition] = 0;
			}

			core->clients[i]->sendPacket(Packet::B2C_WINDOW_MESSAGE, packet);
		}
	}
}

void Window::handleUserClosed() {
	// Do nothing. (For now?)
}

void Window::handleRawUserInput(const char *str, Client *sender, int ackID) {
	if (str[0] == 0)
		return;

	// Store the acknowledgement info, to be used by any
	// calls to pushMessage
	currentAckClient = sender;
	currentAckID = ackID;

	if (str[0] == '/') {
		const char *space = strchr(str, ' ');

		if (space != NULL) {
			int i;
			char cmd[200];

			for (i = 0; (i < 199) && (str[i+1] != ' '); i++)
				cmd[i] = str[i+1];
			cmd[i] = 0;

			handleCommand(cmd, space+1);
		} else {
			handleCommand(&str[1], "");
		}
	} else {
		handleUserInput(str);
	}

	// If we didn't push anything here, then we need to make sure
	// we still acknowledge the client's request
	// Assemble a dummy message packet
	if (currentAckClient != 0 && currentAckClient->isAuthed()) {
		time_t now;
#ifdef _WIN32
		now = time(NULL);
#else
		timespec t;
		clock_gettime(CLOCK_REALTIME, &t);
		now = t.tv_sec;
#endif

		Buffer p;
		p.writeU32(id);
		p.writeU32((uint32_t)now);
		p.writeU8(0);
		p.writeU8(currentAckID);
		p.writeStr("");

		currentAckClient->sendPacket(Packet::B2C_WINDOW_MESSAGE, p);

		currentAckClient = 0;
		currentAckID = 0;
	}
}






