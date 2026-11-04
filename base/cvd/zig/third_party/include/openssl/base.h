#pragma once

#include <stddef.h>
#include <stdint.h>

#define OPENSSL_IS_BORINGSSL 1
#define BORINGSSL_API_VERSION 18
#define OPENSSL_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

typedef int ASN1_BOOLEAN;
typedef struct ASN1_INTEGER_st ASN1_INTEGER;
typedef struct ASN1_OCTET_STRING_st ASN1_OCTET_STRING;
typedef struct ASN1_STRING_st ASN1_STRING;
typedef struct ASN1_TIME_st ASN1_TIME;
typedef struct bio_st BIO;
typedef struct bio_method_st BIO_METHOD;
typedef struct bignum_st BIGNUM;
typedef struct bignum_ctx BN_CTX;
typedef struct cbb_st CBB;
typedef struct cbs_st CBS;
typedef struct dh_st DH;
typedef struct dsa_st DSA;
typedef struct ec_group_st EC_GROUP;
typedef struct ec_key_st EC_KEY;
typedef struct ec_point_st EC_POINT;
typedef struct env_md_ctx_st EVP_MD_CTX;
typedef struct env_md_st EVP_MD;
typedef struct evp_aead_st EVP_AEAD;
typedef struct evp_aead_ctx_st EVP_AEAD_CTX;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef struct evp_cipher_st EVP_CIPHER;
typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct hmac_ctx_st HMAC_CTX;
typedef struct rsa_st RSA;
typedef struct sha_state_st SHA_CTX;
typedef struct sha256_state_st SHA256_CTX;
typedef struct sha512_state_st SHA512_CTX;
typedef struct ssl_cipher_st SSL_CIPHER;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_session_st SSL_SESSION;
typedef struct ssl_st SSL;
typedef struct x509_st X509;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct x509_store_st X509_STORE;
typedef struct X509_name_st X509_NAME;

#ifdef __cplusplus
}
#endif
