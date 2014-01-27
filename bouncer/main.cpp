#include "core.h"
#include "dns.h"

#ifdef USE_GNUTLS
static gnutls_dh_params_t dh_params;
gnutls_certificate_credentials_t g_serverCreds, g_clientCreds;

bool initTLS() {
	int ret;
	ret = gnutls_global_init();
	if (ret != GNUTLS_E_SUCCESS) {
		printf("gnutls_global_init failure: %s\n", gnutls_strerror(ret));
		return false;
	}

	unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_LEGACY);

	ret = gnutls_dh_params_init(&dh_params);
	if (ret != GNUTLS_E_SUCCESS) {
		printf("dh_params_init failure: %s\n", gnutls_strerror(ret));
		return false;
	}

	ret = gnutls_dh_params_generate2(dh_params, bits);
	if (ret != GNUTLS_E_SUCCESS) {
		printf("dh_params_generate2 failure: %s\n", gnutls_strerror(ret));
		return false;
	}

	gnutls_certificate_allocate_credentials(&g_clientCreds);
	ret = gnutls_certificate_set_x509_key_file(g_clientCreds, "ssl_test.crt", "ssl_test.key", GNUTLS_X509_FMT_PEM);
	if (ret != GNUTLS_E_SUCCESS) {
		printf("set_x509_key_file failure: %s\n", gnutls_strerror(ret));
		return false;
	}
	gnutls_certificate_set_dh_params(g_clientCreds, dh_params);

	gnutls_certificate_allocate_credentials(&g_serverCreds);

	return true;
}
#endif

int main(int argc, char **argv) {
#ifdef USE_GNUTLS
	if (!initTLS())
		return EXIT_FAILURE;
#endif

	DNS::start();

	Bouncer bounce;
	bounce.loadConfig();

	int errcode = bounce.execute();
	if (errcode < 0) {
		printf("(Bouncer::execute failed with %d)\n", errcode);
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}