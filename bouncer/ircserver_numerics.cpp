#include "core.h"

static const int numericDefCount = 141;
IRCServer::Numeric IRCServer::numericDefs[numericDefCount] = {
	{1, &IRCServer::rpl_generic},			// RPL_WELCOME
	{2, &IRCServer::rpl_generic},			// RPL_YOURHOST
	{3, &IRCServer::rpl_generic},			// RPL_CREATED
	{4, &IRCServer::rpl_generic},			// RPL_MYINFO
	{5, &IRCServer::rpl_ISUPPORT},
	{200, &IRCServer::rpl_generic},		// RPL_TRACELINK
	{201, &IRCServer::rpl_generic},		// RPL_TRACECONNECTING
	{202, &IRCServer::rpl_generic},		// RPL_TRACEHANDSHAKE
	{203, &IRCServer::rpl_generic},		// RPL_TRACEUNKNOWN
	{204, &IRCServer::rpl_generic},		// RPL_TRACEOPERATOR
	{205, &IRCServer::rpl_generic},		// RPL_TRACEUSER
	{206, &IRCServer::rpl_generic},		// RPL_TRACESERVER
	{207, &IRCServer::rpl_generic},		// RPL_TRACESERVICE
	{208, &IRCServer::rpl_generic},		// RPL_TRACENEWTYPE
	{209, &IRCServer::rpl_generic},		// RPL_TRACECLASS
	{210, &IRCServer::rpl_generic},		// RPL_TRACERECONNECT
	{211, &IRCServer::rpl_generic},		// RPL_STATSLINKINFO
	{212, &IRCServer::rpl_generic},		// RPL_STATSCOMMANDS
	{219, &IRCServer::rpl_generic},		// RPL_ENDOFSTATS
	{221, &IRCServer::RPL_UMODEIS},
	{234, &IRCServer::rpl_generic},		// RPL_SERVLIST
	{235, &IRCServer::rpl_generic},		// RPL_SERVLISTEND
	{242, &IRCServer::rpl_generic},		// RPL_STATSUPTIME
	{243, &IRCServer::RPL_STATSOLINE},
	{250, &IRCServer::rpl_generic},		// Connection stats (Freenode)
	{251, &IRCServer::rpl_generic},		// RPL_LUSERCLIENT
	{252, &IRCServer::rpl_generic},		// RPL_LUSEROP
	{253, &IRCServer::rpl_generic},		// RPL_LUSERUNKNOWN
	{254, &IRCServer::rpl_generic},		// RPL_LUSERCHANNELS
	{255, &IRCServer::rpl_generic},		// RPL_LUSERME
	{256, &IRCServer::rpl_generic},		// RPL_ADMINME
	{257, &IRCServer::rpl_generic},		// RPL_ADMINLOC1
	{258, &IRCServer::rpl_generic},		// RPL_ADMINLOC2
	{259, &IRCServer::rpl_generic},		// RPL_ADMINEMAIL
	{261, &IRCServer::rpl_generic},		// RPL_TRACELOG
	{262, &IRCServer::rpl_generic},		// RPL_TRACEEND
	{263, &IRCServer::RPL_TRYAGAIN},
	{265, &IRCServer::rpl_generic},		// Local user count
	{266, &IRCServer::rpl_generic},		// Global user count
	{301, &IRCServer::RPL_AWAY},
	{302, &IRCServer::RPL_USERHOST},
	{303, &IRCServer::RPL_ISON},
	{305, &IRCServer::RPL_UNAWAY},
	{306, &IRCServer::RPL_NOWAWAY},
	{311, &IRCServer::RPL_WHOISUSER},
	{312, &IRCServer::RPL_WHOISSERVER},
	{313, &IRCServer::RPL_WHOISOPERATOR},
	{314, &IRCServer::RPL_WHOWASUSER},
	{315, &IRCServer::RPL_ENDOFWHO},
	{317, &IRCServer::RPL_WHOISIDLE},
	{318, &IRCServer::RPL_ENDOFWHOIS},
	{319, &IRCServer::RPL_WHOISCHANNELS},
	{321, &IRCServer::RPL_LISTSTART},
	{322, &IRCServer::RPL_LIST},
	{323, &IRCServer::RPL_LISTEND},
	{324, &IRCServer::RPL_CHANNELMODEIS},
	{325, &IRCServer::RPL_UNIQOPIS},
	{331, &IRCServer::RPL_NOTOPIC},
	{332, &IRCServer::RPL_TOPIC},
	{333, &IRCServer::rpl_topicDetails},
	{341, &IRCServer::RPL_INVITING},
	{342, &IRCServer::RPL_SUMMONING},
	{346, &IRCServer::rpl_chanGeneric},	// RPL_INVITELIST
	{347, &IRCServer::rpl_chanGeneric},	// RPL_ENDOFINVITELIST
	{348, &IRCServer::rpl_chanGeneric},	// RPL_EXCEPTLIST
	{349, &IRCServer::rpl_chanGeneric},	// RPL_ENDOFEXCEPTLIST
	{351, &IRCServer::rpl_generic},		// RPL_VERSION
	{352, &IRCServer::RPL_WHOREPLY},
	{353, &IRCServer::RPL_NAMREPLY},
	{364, &IRCServer::rpl_generic},		// RPL_LINKS
	{365, &IRCServer::rpl_generic},		// RPL_ENDOFLINKS
	{366, &IRCServer::RPL_ENDOFNAMES},
	{367, &IRCServer::rpl_chanGeneric},	// RPL_BANLIST
	{368, &IRCServer::rpl_chanGeneric},	// RPL_ENDOFBANLIST
	{369, &IRCServer::RPL_ENDOFWHOWAS},
	{371, &IRCServer::rpl_generic},		// RPL_INFO
	{372, &IRCServer::rpl_generic},		// RPL_MOTD
	{374, &IRCServer::rpl_generic},		// RPL_ENDOFINFO
	{375, &IRCServer::rpl_generic},		// RPL_MOTDSTART
	{376, &IRCServer::rpl_generic},		// RPL_ENDOFMOTD
	{381, &IRCServer::rpl_generic},		// RPL_YOUREOPER
	{382, &IRCServer::rpl_generic},		// RPL_REHASHING
	{383, &IRCServer::rpl_generic},		// RPL_YOURESERVICE
	{391, &IRCServer::RPL_TIME},
	{392, &IRCServer::RPL_USERSSTART},
	{393, &IRCServer::RPL_USERS},
	{394, &IRCServer::RPL_ENDOFUSERS},
	{395, &IRCServer::RPL_NOUSERS},
	{401, &IRCServer::ERR_NOSUCHNICK},
	{402, &IRCServer::ERR_NOSUCHSERVER},
	{403, &IRCServer::ERR_NOSUCHCHANNEL},
	{404, &IRCServer::ERR_CANNOTSENDTOCHAN},
	{405, &IRCServer::ERR_TOOMANYCHANNELS},
	{406, &IRCServer::ERR_WASNOSUCHNICK},
	{407, &IRCServer::rpl_generic},		// ERR_TOOMANYTARGETS
	{408, &IRCServer::rpl_generic},		// ERR_NOSUCHSERVICE
	{409, &IRCServer::rpl_generic},		// ERR_NOORIGIN
	{411, &IRCServer::rpl_generic},		// ERR_NORECIPIENT
	{412, &IRCServer::rpl_generic},		// ERR_NOTEXTTOSEND
	{413, &IRCServer::rpl_generic},		// ERR_NOTOPLEVEL
	{414, &IRCServer::rpl_generic},		// ERR_WILDTOPLEVEL
	{415, &IRCServer::rpl_generic},		// ERR_BADMASK
	{421, &IRCServer::rpl_generic},		// ERR_UNKNOWNCOMMAND
	{422, &IRCServer::rpl_generic},		// ERR_NOMOTD
	{423, &IRCServer::rpl_generic},		// ERR_NOADMININFO
	{424, &IRCServer::rpl_generic},		// ERR_FILEERROR
	{431, &IRCServer::rpl_generic},		// ERR_NONICKNAMEGIVEN
	{432, &IRCServer::rpl_generic},		// ERR_ERRONEUSNICKNAME
	{433, &IRCServer::rpl_generic},		// ERR_NICKNAMEINUSE
	{436, &IRCServer::rpl_generic},		// ERR_NICKCOLLISION
	{437, &IRCServer::rpl_generic},		// ERR_UNAVAILRESOURCE
	{441, &IRCServer::ERR_USERNOTINCHANNEL},
	{442, &IRCServer::rpl_chanGeneric},	// ERR_NOTONCHANNEL
	{443, &IRCServer::ERR_USERONCHANNEL},
	{444, &IRCServer::rpl_generic},		// ERR_NOLOGIN
	{445, &IRCServer::rpl_generic},		// ERR_SUMMONDISABLED
	{446, &IRCServer::rpl_generic},		// ERR_USERSDISABLED
	{451, &IRCServer::rpl_generic},		// ERR_NOTREGISTERED
	{461, &IRCServer::ERR_NEEDMOREPARAMS},
	{462, &IRCServer::rpl_generic},		// ERR_ALREADYREGISTRED
	{463, &IRCServer::rpl_generic},		// ERR_NOPERMFORHOST
	{464, &IRCServer::rpl_generic},		// ERR_PASSWDMISMATCH
	{465, &IRCServer::rpl_generic},		// ERR_YOUREBANNEDCREEP
	{466, &IRCServer::rpl_generic},		// ERR_YOUWILLBEBANNED
	{467, &IRCServer::rpl_chanGeneric},	// ERR_KEYSET
	{471, &IRCServer::rpl_chanGeneric},	// ERR_CHANNELISFULL
	{472, &IRCServer::rpl_generic},		// ERR_UNKNOWNMODE
	{473, &IRCServer::rpl_chanGeneric},	// ERR_INVITEONLYCHAN
	{474, &IRCServer::rpl_chanGeneric},	// ERR_BANNEDFROMCHAN
	{475, &IRCServer::rpl_chanGeneric},	// ERR_BADCHANNELKEY
	{476, &IRCServer::rpl_chanGeneric},	// ERR_BADCHANMASK
	{477, &IRCServer::rpl_chanGeneric},	// ERR_NOCHANMODES
	{478, &IRCServer::rpl_chanGeneric},	// ERR_BANLISTFULL
	{481, &IRCServer::rpl_generic},		// ERR_NOPRIVILEGES
	{482, &IRCServer::rpl_chanGeneric},	// ERR_CHANOPRIVSNEEDED
	{483, &IRCServer::rpl_generic},		// ERR_CANTKILLSERVER
	{484, &IRCServer::rpl_generic},		// ERR_RESTRICTED
	{485, &IRCServer::rpl_generic},		// ERR_UNIQOPPRIVSNEEDED
	{491, &IRCServer::rpl_generic},		// ERR_NOOPERHOST
	{501, &IRCServer::rpl_generic},		// ERR_UMODEUNKNOWNFLAG
	{502, &IRCServer::rpl_generic},		// ERR_USERSDONTMATCH
};

const IRCServer::Numeric *IRCServer::resolveNumeric(int num) const {
	int min, max, i;

	min = 0;
	max = numericDefCount;

	while (min < max) {
		i = min + ((max - min) / 2);
		int check = numericDefs[i].numeric;

		if (num == check)
			break;
		if (num < check)
			max = i;
		if (num > check)
			min = i + 1;
	}

	if (num == numericDefs[i].numeric)
		return &numericDefs[i];
	else
		return NULL;
}

bool IRCServer::dispatchNumeric(int numeric, char *args) {
	const Numeric *n = resolveNumeric(numeric);
	if (n != NULL)
		return (this->*(n->func))(args);
	else
		return false;
}


bool IRCServer::rpl_generic(char *args) {
	status.pushMessage(args);
	return true;
}

bool IRCServer::rpl_chanGeneric(char *args) {
	char *space = strchr(args, ' ');
	if (space == NULL)
		return false;

	*space = 0;
	char *msg = space + 1;
	if (*msg == ':')
		++msg;

	Channel *c = findChannel(args, false);
	if (c != NULL) {
		c->pushMessage(msg);
		return true;
	}

	return false;
}


bool IRCServer::rpl_ISUPPORT(char *line) {
	while (*line != 0) {
		char keyBuf[512], valueBuf[512];
		int keyPos = 0, valuePos = 0;
		int phase = 0;

		// This means we've reached the end
		if (*line == ':')
			return true;

		while ((*line != 0) && (*line != ' ')) {
			if (phase == 0) {
				if (*line == '=')
					phase = 1;
				else if (keyPos < 511)
					keyBuf[keyPos++] = *line;
			} else {
				if (valuePos < 511)
					valueBuf[valuePos++] = *line;
			}

			++line;
		}

		if (*line == ' ')
			++line;

		keyBuf[keyPos] = 0;
		valueBuf[valuePos] = 0;


		// Now process the thing

		if (strcmp(keyBuf, "PREFIX") == 0) {
			int prefixCount = (valuePos - 2) / 2;

			if (valueBuf[0] == '(' && valueBuf[1+prefixCount] == ')') {
				if (prefixCount < 32) {
					strncpy(serverPrefixMode, &valueBuf[1], prefixCount);
					strncpy(serverPrefix, &valueBuf[2+prefixCount], prefixCount);

					serverPrefixMode[prefixCount] = 0;
					serverPrefix[prefixCount] = 0;
				}
			}
		} else if (strcmp(keyBuf, "CHANMODES") == 0) {
			char *proc = &valueBuf[0];

			for (int index = 0; index < 4; index++) {
				if (*proc == 0)
					break;

				char *start = proc;
				char *end = proc;

				while ((*end != ',') && (*end != 0))
					++end;

				// If this is a zero, we can't read any more
				bool endsHere = (*end == 0);
				*end = 0;

				serverChannelModes[index] = start;
				char moof[1000];
				sprintf(moof, "set chanmodes %d to [%s]", index, serverChannelModes[index].c_str());
				status.pushMessage(moof);

				if (endsHere)
					break;
				else
					proc = end + 1;
			}
		} else if (strcmp(keyBuf, "CHANTYPES") == 0) {
			if (strlen(valueBuf) > 0)
				strncpy(chanTypes, valueBuf, 31);
			chanTypes[31] = 0;
		}
	}

	return true;
}

bool IRCServer::RPL_UMODEIS(char *args) { return false; }
bool IRCServer::RPL_STATSOLINE(char *args) { return false; }
bool IRCServer::RPL_TRYAGAIN(char *args) { return false; }
bool IRCServer::RPL_AWAY(char *args) { return false; }
bool IRCServer::RPL_USERHOST(char *args) { return false; }
bool IRCServer::RPL_ISON(char *args) { return false; }
bool IRCServer::RPL_UNAWAY(char *args) { return false; }
bool IRCServer::RPL_NOWAWAY(char *args) { return false; }
bool IRCServer::RPL_WHOISUSER(char *args) { return false; }
bool IRCServer::RPL_WHOISSERVER(char *args) { return false; }
bool IRCServer::RPL_WHOISOPERATOR(char *args) { return false; }
bool IRCServer::RPL_WHOWASUSER(char *args) { return false; }
bool IRCServer::RPL_ENDOFWHO(char *args) { return false; }
bool IRCServer::RPL_WHOISIDLE(char *args) { return false; }
bool IRCServer::RPL_ENDOFWHOIS(char *args) { return false; }
bool IRCServer::RPL_WHOISCHANNELS(char *args) { return false; }
bool IRCServer::RPL_LISTSTART(char *args) { return false; }
bool IRCServer::RPL_LIST(char *args) { return false; }
bool IRCServer::RPL_LISTEND(char *args) { return false; }
bool IRCServer::RPL_CHANNELMODEIS(char *args) { return false; }
bool IRCServer::RPL_UNIQOPIS(char *args) { return false; }

bool IRCServer::RPL_NOTOPIC(char *args) {
	// Params: Channel name, *maybe* text we can ignore

	char *space = strchr(args, ' ');
	if (space)
		*space = 0;

	Channel *c = findChannel(args, false);
	if (c) {
		c->handleTopic(UserRef(), "");
		return true;
	}
	return false;
}

bool IRCServer::RPL_TOPIC(char *args) {
	// Params: Channel name, text

	char *space = strchr(args, ' ');
	if (space) {
		*space = 0;

		char *topic = space + 1;
		if (*topic == ':')
			++topic;

		Channel *c = findChannel(args, false);
		if (c) {
			c->handleTopic(UserRef(), topic);
			return true;
		}
	}
	return false;
}

bool IRCServer::rpl_topicDetails(char *args) {
	char *strtok_var;
	char *chanName = strtok_r(args, " ", &strtok_var);
	char *setBy = strtok_r(NULL, " ", &strtok_var);
	char *when = strtok_r(NULL, " ", &strtok_var);

	if (chanName && setBy && when) {
		Channel *c = findChannel(chanName, false);

		if (c) {
			c->handleTopicInfo(setBy, atoi(when));
			return true;
		}
	}

	return false;
}

bool IRCServer::RPL_INVITING(char *args) { return false; }
bool IRCServer::RPL_SUMMONING(char *args) { return false; }
bool IRCServer::RPL_WHOREPLY(char *args) { return false; }

bool IRCServer::RPL_NAMREPLY(char *args) {
	// Params: Channel privacy flag, channel, user list

	char *space1 = strchr(args, ' ');
	if (space1) {
		char *space2 = strchr(space1 + 1, ' ');
		if (space2) {
			char *chanName = space1 + 1;
			*space2 = 0;

			char *userNames = space2 + 1;
			if (*userNames == ':')
				++userNames;

			Channel *c = findChannel(chanName, false);

			if (c) {
				c->handleNameReply(userNames);
				return true;
			}
		}
	}

	return false;
}

bool IRCServer::RPL_ENDOFNAMES(char *args) {
	// Can ignore this, I suppose
	return true;
}

bool IRCServer::RPL_ENDOFWHOWAS(char *args) { return false; }
bool IRCServer::RPL_TIME(char *args) { return false; }
bool IRCServer::RPL_USERSSTART(char *args) { return false; }
bool IRCServer::RPL_USERS(char *args) { return false; }
bool IRCServer::RPL_ENDOFUSERS(char *args) { return false; }
bool IRCServer::RPL_NOUSERS(char *args) { return false; }
bool IRCServer::ERR_NOSUCHNICK(char *args) { return false; }
bool IRCServer::ERR_NOSUCHSERVER(char *args) { return false; }
bool IRCServer::ERR_NOSUCHCHANNEL(char *args) { return false; }
bool IRCServer::ERR_CANNOTSENDTOCHAN(char *args) { return false; }
bool IRCServer::ERR_TOOMANYCHANNELS(char *args) { return false; }
bool IRCServer::ERR_WASNOSUCHNICK(char *args) { return false; }
bool IRCServer::ERR_USERNOTINCHANNEL(char *args) { return false; }
bool IRCServer::ERR_USERONCHANNEL(char *args) { return false; }
bool IRCServer::ERR_NEEDMOREPARAMS(char *args) { return false; }
