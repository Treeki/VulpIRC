#include "dns.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
#endif

#define DNS_QUERY_COUNT 20
#define DNS_QUERY_NAME_SIZE 256

enum DNSQueryState {
	DQS_FREE = 0,
	DQS_WAITING = 1,
	DQS_COMPLETE = 2,
	DQS_ERROR = 3
};

struct DNSQuery {
	char name[DNS_QUERY_NAME_SIZE];
	in_addr result;
	DNSQueryState status;
	int version;
};

static DNSQuery dnsQueue[DNS_QUERY_COUNT];
static bool dnsQuit = false;

#ifdef _WIN32
static HANDLE dnsThread;
static CRITICAL_SECTION dnsQueueCS;
static HANDLE dnsQueueEvent;
#else
static pthread_t dnsThread;
static pthread_mutex_t dnsQueueMutex;
static pthread_cond_t dnsQueueCond;
#endif

#ifdef _WIN32
static unsigned int __stdcall dnsThreadProc(void *);

static inline void _dnsQueueLock() {
	EnterCriticalSection(&dnsQueueCS);
}
static inline void _dnsQueueUnlock() {
	LeaveCriticalSection(&dnsQueueCS);
}
static inline void _dnsQueueSignal() {
	SetEvent(dnsQueueEvent);
}
#else
static void *dnsThreadProc(void *);

static inline void _dnsQueueLock() {
	pthread_mutex_lock(&dnsQueueMutex);
}
static inline void _dnsQueueUnlock() {
	pthread_mutex_unlock(&dnsQueueMutex);
}
static inline void _dnsQueueSignal() {
	pthread_cond_signal(&dnsQueueCond);
}
#endif


void DNS::start() {
#ifndef _WIN32
	pthread_mutex_init(&dnsQueueMutex, NULL);
	pthread_cond_init(&dnsQueueCond, NULL);

	pthread_create(&dnsThread, NULL, &dnsThreadProc, NULL);
#endif

	for (int i = 0; i < DNS_QUERY_COUNT; i++) {
		dnsQueue[i].status = DQS_FREE;
		dnsQueue[i].version = 0;
	}

#ifdef _WIN32
	InitializeCriticalSection(&dnsQueueCS);
	dnsQueueEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	dnsThread = (HANDLE)_beginthreadex(NULL, 0, &dnsThreadProc, NULL, 0, NULL);
#endif
}

void DNS::stop() {
	_dnsQueueLock();
	dnsQuit = true;
	_dnsQueueUnlock();
	_dnsQueueSignal();

#ifdef _WIN32
	WaitForSingleObject(dnsThread, INFINITE);
	CloseHandle(dnsThread);

	DeleteCriticalSection(&dnsQueueCS);
	CloseHandle(dnsQueueEvent);
#else
	pthread_join(dnsThread, NULL);
#endif
}

int DNS::makeQuery(const char *name) {
	int id = -1;

	_dnsQueueLock();

	for (int i = 0; i < DNS_QUERY_COUNT; i++) {
		if (dnsQueue[i].status == DQS_FREE) {
			id = i;
			break;
		}
	}

	if (id != -1) {
		strncpy(dnsQueue[id].name, name, sizeof(dnsQueue[id].name));
		dnsQueue[id].name[sizeof(dnsQueue[id].name) - 1] = 0;
		dnsQueue[id].status = DQS_WAITING;
		dnsQueue[id].version++;
		printf("[DNS::%d] New query: %s\n", id, dnsQueue[id].name);
	}

	_dnsQueueUnlock();
	_dnsQueueSignal();

	return id;
}

void DNS::closeQuery(int id) {
	if (id < 0 || id >= DNS_QUERY_COUNT)
		return;

	_dnsQueueLock();
	printf("[DNS::%d] Closing query\n", id);
	dnsQueue[id].status = DQS_FREE;
	_dnsQueueUnlock();
}

bool DNS::checkQuery(int id, in_addr *pResult, bool *pIsError) {
	if (id < 0 || id >= DNS_QUERY_COUNT)
		return false;

	_dnsQueueLock();

	bool finalResult = false;
	if (dnsQueue[id].status == DQS_COMPLETE) {
		finalResult = true;
		*pIsError = false;
		memcpy(pResult, &dnsQueue[id].result, sizeof(dnsQueue[id].result));
	} else if (dnsQueue[id].status == DQS_ERROR) {
		finalResult = true;
		*pIsError = true;
	}

	_dnsQueueUnlock();

	return finalResult;
}


#ifdef _WIN32
unsigned int __stdcall dnsThreadProc(void *) {
#else
void *dnsThreadProc(void *) {
#endif
	_dnsQueueLock();

	dnsQuit = false;

	while (!dnsQuit) {
		for (int i = 0; i < DNS_QUERY_COUNT; i++) {
			if (dnsQueue[i].status == DQS_WAITING) {
				char nameCopy[DNS_QUERY_NAME_SIZE];
				memcpy(nameCopy, dnsQueue[i].name, DNS_QUERY_NAME_SIZE);

				int versionCopy = dnsQueue[i].version;

				printf("[DNS::%d] Trying %s...\n", i, nameCopy);

				_dnsQueueUnlock();

				addrinfo hints, *res;

				memset(&hints, 0, sizeof(hints));
				hints.ai_family = AF_INET;
				hints.ai_socktype = SOCK_STREAM;
				hints.ai_protocol = IPPROTO_TCP;
				hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;

				int s = getaddrinfo(nameCopy, NULL, &hints, &res);

				_dnsQueueLock();

				// Before we write to the request, check that it hasn't been
				// closed (and possibly replaced...!) by another thread

				if (dnsQueue[i].status == DQS_WAITING && dnsQueue[i].version == versionCopy) {
					if (s == 0) {
						// Only try the first one for now...
						// Is this safe? Not sure.
						dnsQueue[i].status = DQS_COMPLETE;
						memcpy(&dnsQueue[i].result, &((sockaddr_in*)res->ai_addr)->sin_addr, sizeof(dnsQueue[i].result));

						printf("[DNS::%d] Resolved %s to %x\n", i, dnsQueue[i].name, dnsQueue[i].result.s_addr);
					} else {
						dnsQueue[i].status = DQS_ERROR;
						printf("[DNS::%d] Error condition: %d\n", i, s);
					}
				} else {
					printf("[DNS::%d] Request was cancelled before getaddrinfo completed\n", i);
				}

				if (s == 0)
					freeaddrinfo(res);
			}
		}

#ifdef _WIN32
		_dnsQueueUnlock();
		WaitForSingleObject(dnsQueueEvent, INFINITE);
		_dnsQueueLock();
#else
		pthread_cond_wait(&dnsQueueCond, &dnsQueueMutex);
#endif
	}

	_dnsQueueUnlock();

#ifdef _WIN32
	_endthreadex(0);
#endif
	return NULL;
}
