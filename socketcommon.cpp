#include "core.h"

/*static*/ bool SocketRWCommon::setSocketNonBlocking(int sock) {
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
}


SocketRWCommon::SocketRWCommon(NetCore *_netCore) {
	netCore = _netCore;
	sock = -1;
	state = CS_DISCONNECTED;
	tlsActive = false;
}
SocketRWCommon::~SocketRWCommon() {
	close();
}

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
		state = CS_CONNECTED;

		inputBuf.clear();
		outputBuf.clear();

		printf("[SocketRWCommon connected via SSL!]\n");
		return true;
	}

	return false;
}

void SocketRWCommon::close() {
	if (sock != -1) {
		if (tlsActive)
			gnutls_bye(tls, GNUTLS_SHUT_RDWR);
		shutdown(sock, SHUT_RDWR);
		::close(sock);
	}

	sock = -1;
	inputBuf.clear();
	outputBuf.clear();
	state = CS_DISCONNECTED;

	if (tlsActive) {
		gnutls_deinit(tls);
		tlsActive = false;
	}
}

void SocketRWCommon::readAction() {
	// Ensure we have at least 0x200 bytes space free
	// (Up this, maybe?)
	int bufSize = inputBuf.size();
	int requiredSize = bufSize + 0x200;
	if (requiredSize > inputBuf.capacity())
		inputBuf.setCapacity(requiredSize);

	ssize_t amount;
	if (tlsActive) {
		amount = gnutls_record_recv(tls,
				&inputBuf.data()[bufSize],
				0x200);
	} else {

		amount = recv(sock,
				&inputBuf.data()[bufSize],
				0x200,
				0);
	}


	if (amount > 0) {
		// Yep, we have data
		printf("[fd=%d] Read %d bytes\n", sock, amount);
		inputBuf.resize(bufSize + amount);

		processReadBuffer();

	} else if (amount == 0) {
		printf("[fd=%d] Read 0! Socket closing.\n", sock);
		close();

	} else if (amount < 0) {
		if (tlsActive) {
			if (gnutls_error_is_fatal(amount)) {
				printf("Error while reading [gnutls %d]!\n", amount);
				close();
			}
		} else {
			perror("Error while reading!");
			close();
		}
	}
}

void SocketRWCommon::writeAction() {
	// What can we get rid of...?
	ssize_t amount;
	if (tlsActive) {
		amount = gnutls_record_send(tls,
				outputBuf.data(),
				outputBuf.size());
	} else {
		amount = send(sock,
				outputBuf.data(),
				outputBuf.size(),
				0);
	}

	if (amount > 0) {
		printf("[fd=%d] Wrote %d bytes out of %d\n", sock, amount, outputBuf.size());
		outputBuf.trimFromStart(amount);
	} else if (amount == 0)
		printf("Sent 0!\n");
	else if (amount < 0) {
		if (tlsActive) {
			if (gnutls_error_is_fatal(amount)) {
				printf("Error while sending [gnutls %d]!\n", amount);
				close();
			}
		} else {
			perror("Error while sending!");
			close();
		}
	}
}
