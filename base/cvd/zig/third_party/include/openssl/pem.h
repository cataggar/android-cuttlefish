#pragma once

#include "openssl/base.h"
#include "openssl/bio.h"
#include "openssl/evp.h"
#include "openssl/x509.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int pem_password_cb(char* buf, int size, int rwflag, void* userdata);

X509* PEM_read_bio_X509(BIO* bp, X509** x, pem_password_cb* cb, void* u);
int PEM_write_bio_X509(BIO* bp, X509* x);
EVP_PKEY* PEM_read_bio_PrivateKey(BIO* bp, EVP_PKEY** x, pem_password_cb* cb,
                                  void* u);
int PEM_write_bio_PrivateKey(BIO* bp, EVP_PKEY* x, const EVP_CIPHER* enc,
                             unsigned char* kstr, int klen,
                             pem_password_cb* cb, void* u);
int PEM_write_bio_PUBKEY(BIO* bp, EVP_PKEY* x);

#ifdef __cplusplus
}
#endif
