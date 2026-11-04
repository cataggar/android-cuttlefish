#pragma once

#include "openssl/base.h"
#include "openssl/base64.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EVP_MAX_MD_SIZE 64
#define EVP_MAX_KEY_LENGTH 64
#define EVP_MAX_IV_LENGTH 16
#define EVP_MAX_BLOCK_LENGTH 32

#define EVP_PKEY_RSA 6
#define EVP_PKEY_EC 408

const EVP_MD* EVP_md5(void);
const EVP_MD* EVP_sha1(void);
const EVP_MD* EVP_sha224(void);
const EVP_MD* EVP_sha256(void);
const EVP_MD* EVP_sha384(void);
const EVP_MD* EVP_sha512(void);
size_t EVP_MD_size(const EVP_MD* md);

EVP_MD_CTX* EVP_MD_CTX_new(void);
EVP_MD_CTX* EVP_MD_CTX_create(void);
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
void EVP_MD_CTX_destroy(EVP_MD_CTX* ctx);
void EVP_MD_CTX_init(EVP_MD_CTX* ctx);
int EVP_MD_CTX_cleanup(EVP_MD_CTX* ctx);

int EVP_DigestInit(EVP_MD_CTX* ctx, const EVP_MD* type);
int EVP_DigestInit_ex(EVP_MD_CTX* ctx, const EVP_MD* type, void* impl);
int EVP_DigestUpdate(EVP_MD_CTX* ctx, const void* data, size_t len);
int EVP_DigestFinal(EVP_MD_CTX* ctx, uint8_t* md_out, unsigned int* out_size);
int EVP_DigestFinal_ex(EVP_MD_CTX* ctx, uint8_t* md_out,
                       unsigned int* out_size);
int EVP_Digest(const void* data, size_t count, uint8_t* md, unsigned int* size,
               const EVP_MD* type, void* impl);

const EVP_CIPHER* EVP_aes_128_ecb(void);
const EVP_CIPHER* EVP_aes_128_cbc(void);
const EVP_CIPHER* EVP_aes_128_ctr(void);
const EVP_CIPHER* EVP_aes_128_gcm(void);
const EVP_CIPHER* EVP_aes_256_ecb(void);
const EVP_CIPHER* EVP_aes_256_cbc(void);
const EVP_CIPHER* EVP_aes_256_ctr(void);
const EVP_CIPHER* EVP_aes_256_gcm(void);

EVP_CIPHER_CTX* EVP_CIPHER_CTX_new(void);
void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX* ctx);
void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX* ctx);
int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX* ctx);

int EVP_EncryptInit_ex(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* cipher,
                       void* impl, const uint8_t* key, const uint8_t* iv);
int EVP_EncryptUpdate(EVP_CIPHER_CTX* ctx, uint8_t* out, int* out_len,
                      const uint8_t* in, int in_len);
int EVP_EncryptFinal_ex(EVP_CIPHER_CTX* ctx, uint8_t* out, int* out_len);

int EVP_DecryptInit_ex(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* cipher,
                       void* impl, const uint8_t* key, const uint8_t* iv);
int EVP_DecryptUpdate(EVP_CIPHER_CTX* ctx, uint8_t* out, int* out_len,
                      const uint8_t* in, int in_len);
int EVP_DecryptFinal_ex(EVP_CIPHER_CTX* ctx, uint8_t* out, int* out_len);

EVP_PKEY* EVP_PKEY_new(void);
void EVP_PKEY_free(EVP_PKEY* pkey);
int EVP_PKEY_id(const EVP_PKEY* pkey);
int EVP_PKEY_assign_RSA(EVP_PKEY* pkey, RSA* key);
int EVP_PKEY_assign_EC_KEY(EVP_PKEY* pkey, EC_KEY* key);
int EVP_PKEY_set1_EC_KEY(EVP_PKEY* pkey, EC_KEY* key);

#ifdef __cplusplus
}
#endif
