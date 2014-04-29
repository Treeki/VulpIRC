#include "core.h"

/*static*/ bool SocketRWCommon::setSocketNonBlocking(int sock) {
#ifdef _WIN32
	u_long m = 1;
	if (ioctlsocket(sock, FIONBIO, &m) != NO_ERROR) {
		printf("ioctlsocket failed");
		return false;
	}
	return true;
#else
	int opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		perror("Could not get fcntl options\n");
		return false;
	}
	opts |= O_NONBLOCK;
	if (fcntl(sock, F_SETFL, opts) == -1) {
		perror("Could not set fcntl options\n");
		return false;
	}
	return true;
#endif
}


SocketRWCommon::SocketRWCommon(NetCore *_netCore) {
	netCore = _netCore;
	sock = -1;
	_state = CS_DISCONNECTED;
#ifdef USE_GNUTLS
	tlsActive = false;
#endif
}
SocketRWCommon::~SocketRWCommon() {
	close();
}


void SocketRWCommon::setState(ConnState v) {
	_state = v;
	connectionStateChangedEvent();
}


#ifdef USE_GNUTLS
bool SocketRWCommon::hasTlsPendingData() const {
	if (tlsActive)
		return (gnutls_record_check_pending(tls) > 0);
	else
		return false;
}

bool SocketRWCommon::tryTLSHandshake() {
	int hsRet = gnutls_handshake(tls);
	if (gnutls_error_is_fatal(hsRet)) {
		printf("[SocketRWCommon::tryTLSHandshake] gnutls_handshake borked\n");
		gnutls_perror(hsRet);
		close();
		return false;
	}

	if (hsRet == GNUTLS_E_SUCCESS) {
		// We're in !!
		setState(CS_CONNECTED);

		inputBuf.clear();
		outputBuf.clear();

		printf("[SocketRWCommon connected via SSL!]\n");
		return true;
	}

	return false;
}
#endif

void SocketRWCommon::close() {
	if (sock != -1) {
#ifdef USE_GNUTLS
		if (tlsActive)
			gnutls_bye(tls, GNUTLS_SHUT_RDWR);
#endif

#ifdef _WIN32
		shutdown(sock, SD_BOTH);
		closesocket(sock);
#else
		shutdown(sock, SHUT_RDWR);
		::close(sock);
#endif
	}

	sock = -1;
	inputBuf.clear();
	outputBuf.clear();
	setState(CS_DISCONNECTED);

#ifdef USE_GNUTLS
	if (tlsActive) {
		gnutls_deinit(tls);
		tlsActive = false;
	}
#endif
}

void SocketRWCommon::readAction() {
	// Ensure we have at least 0x200 bytes space free
	// (Up this, maybe?)
	int bufSize = inputBuf.size();
	int requiredSize = bufSize + 0x200;
	if (requiredSize > inputBuf.capacity())
		inputBuf.setCapacity(requiredSize);

	ssize_t amount;

#ifdef USE_GNUTLS
	if (tlsActive) {
		amount = gnutls_record_recv(tls,
				&inputBuf.data()[bufSize],
				0x200);
	} else
#endif
	{
		amount = recv(sock,
				&inputBuf.data()[bufSize],
				0x200,
				0);
	}


	if (amount > 0) {
		// Yep, we have data
		printf("[fd=%d] Read %" PRIuPTR " bytes\n", sock, amount);
		inputBuf.resize(bufSize + amount);

		processReadBuffer();

	} else if (amount == 0) {
		printf("[fd=%d] Read 0! Socket closing.\n", sock);
		close();

	} else if (amount < 0) {
#ifdef USE_GNUTLS
		if (tlsActive) {
			if (gnutls_error_is_fatal(amount)) {
				printf("Error while reading [gnutls %" PRIuPTR "]!\n", amount);
				close();
			}
		} else
#endif
		{
			perror("Error while reading!");
			close();
		}
	}
}

void SocketRWCommon::writeAction() {
	// What can we get rid of...?
	ssize_t amount;

#ifdef USE_GNUTLS
	if (tlsActive) {
		amount = gnutls_record_send(tls,
				outputBuf.data(),
				outputBuf.size());
	} else
#endif
	{
		amount = send(sock,
				outputBuf.data(),
				outputBuf.size(),
				0);
	}

	if (amount > 0) {
		printf("[fd=%d] Wrote %" PRIuPTR " bytes out of %d\n", sock, amount, outputBuf.size());
		outputBuf.trimFromStart(amount);
	} else if (amount == 0)
		printf("Sent 0!\n");
	else if (amount < 0) {
#ifdef USE_GNUTLS
		if (tlsActive) {
			if (gnutls_error_is_fatal(amount)) {
				printf("Error while sending [gnutls %" PRIuPTR "]!\n", amount);
				close();
			}
		} else
#endif
		{
			perror("Error while sending!");
			close();
		}
	}
}
