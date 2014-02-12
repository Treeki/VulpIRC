#ifndef DNS_H
#define DNS_H 

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace DNS {
	void start();
	void stop();
	int makeQuery(const char *name);
	void closeQuery(int id);
	bool checkQuery(int id, in_addr *pResult, bool *pIsError);
}

#endif /* DNS_H */
