#pragma once

#include "openssl/base.h"
#include "openssl/bio.h"
#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/x509.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSL_ERROR_NONE 0
#define SSL_ERROR_SSL 1
#define SSL_ERROR_WANT_READ 2
#define SSL_ERROR_WANT_WRITE 3
#define SSL_ERROR_WANT_X509_LOOKUP 4
#define SSL_ERROR_SYSCALL 5
#define SSL_ERROR_ZERO_RETURN 6
#define SSL_ERROR_WANT_CONNECT 7
#define SSL_ERROR_WANT_ACCEPT 8

#define SSL_VERIFY_NONE 0x00
#define SSL_VERIFY_PEER 0x01
#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 0x02

#define SRTP_AES128_CM_SHA1_80 0x0001
#define SRTP_AES128_CM_SHA1_32 0x0002
#define SRTP_AEAD_AES_128_GCM 0x0007
#define SRTP_AEAD_AES_256_GCM 0x0008

typedef struct srtp_protection_profile_st {
  const char* name;
  unsigned long id;
} SRTP_PROTECTION_PROFILE;

int SSL_library_init(void);
void SSL_load_error_strings(void);

const SSL_METHOD* TLS_method(void);
const SSL_METHOD* TLS_client_method(void);
const SSL_METHOD* TLS_server_method(void);
const SSL_METHOD* DTLS_method(void);
const SSL_METHOD* DTLS_client_method(void);
const SSL_METHOD* DTLS_server_method(void);

SSL_CTX* SSL_CTX_new(const SSL_METHOD* method);
void SSL_CTX_free(SSL_CTX* ctx);
int SSL_CTX_use_certificate(SSL_CTX* ctx, X509* x);
int SSL_CTX_use_PrivateKey(SSL_CTX* ctx, EVP_PKEY* pkey);
int SSL_CTX_check_private_key(const SSL_CTX* ctx);
void SSL_CTX_set_verify(SSL_CTX* ctx, int mode,
                        int (*verify_callback)(int, X509_STORE_CTX*));
int SSL_CTX_set_tlsext_use_srtp(SSL_CTX* ctx, const char* profiles);
int SSL_CTX_set_cipher_list(SSL_CTX* ctx, const char* str);

SSL* SSL_new(SSL_CTX* ctx);
void SSL_free(SSL* ssl);
void SSL_set_bio(SSL* ssl, BIO* rbio, BIO* wbio);
void SSL_set_connect_state(SSL* ssl);
void SSL_set_accept_state(SSL* ssl);
int SSL_do_handshake(SSL* ssl);
int SSL_connect(SSL* ssl);
int SSL_accept(SSL* ssl);
int SSL_read(SSL* ssl, void* buf, int num);
int SSL_write(SSL* ssl, const void* buf, int num);
int SSL_shutdown(SSL* ssl);
int SSL_get_error(const SSL* ssl, int ret_code);

X509* SSL_get_peer_certificate(const SSL* ssl);
const SRTP_PROTECTION_PROFILE* SSL_get_selected_srtp_profile(SSL* ssl);
int SSL_export_keying_material(SSL* ssl, uint8_t* out, size_t out_len,
                               const char* label, size_t label_len,
                               const uint8_t* context, size_t context_len,
                               int use_context);

long DTLSv1_get_timeout(const SSL* ssl, void* out);
int DTLSv1_handle_timeout(SSL* ssl);
void SSL_set_mtu(SSL* ssl, unsigned int mtu);

#ifdef __cplusplus
}
#endif
