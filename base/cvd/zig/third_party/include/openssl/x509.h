#pragma once

#include "openssl/base.h"
#include "openssl/evp.h"
#include "openssl/sha.h"

#ifdef __cplusplus
extern "C" {
#endif

X509* X509_new(void);
void X509_free(X509* x);
int X509_up_ref(X509* x);

int X509_digest(const X509* data, const EVP_MD* type, unsigned char* md,
                unsigned int* len);
int X509_sign(X509* x, EVP_PKEY* pkey, const EVP_MD* md);
int X509_set_pubkey(X509* x, EVP_PKEY* pkey);
EVP_PKEY* X509_get_pubkey(X509* x);
int X509_set_version(X509* x, long version);
ASN1_INTEGER* X509_get_serialNumber(X509* x);
X509_NAME* X509_get_subject_name(const X509* a);
X509_NAME* X509_get_issuer_name(const X509* a);
int X509_set_subject_name(X509* x, X509_NAME* name);
int X509_set_issuer_name(X509* x, X509_NAME* name);
ASN1_TIME* X509_getm_notBefore(const X509* x);
ASN1_TIME* X509_getm_notAfter(const X509* x);

int i2d_X509(X509* a, unsigned char** out);
X509* d2i_X509(X509** a, const unsigned char** in, long len);

#ifdef __cplusplus
}
#endif
