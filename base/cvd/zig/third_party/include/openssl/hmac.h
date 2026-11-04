#pragma once

#include "openssl/base.h"
#include "openssl/evp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hmac_ctx_st;

HMAC_CTX* HMAC_CTX_new(void);
void HMAC_CTX_free(HMAC_CTX* ctx);
void HMAC_CTX_init(HMAC_CTX* ctx);
void HMAC_CTX_cleanup(HMAC_CTX* ctx);

int HMAC_Init_ex(HMAC_CTX* ctx, const void* key, size_t key_len,
                 const EVP_MD* md, void* impl);
int HMAC_Update(HMAC_CTX* ctx, const uint8_t* data, size_t data_len);
int HMAC_Final(HMAC_CTX* ctx, uint8_t* out, unsigned int* out_len);
uint8_t* HMAC(const EVP_MD* evp_md, const void* key, int key_len,
              const uint8_t* data, size_t data_len, uint8_t* out,
              unsigned int* out_len);

#ifdef __cplusplus
}
#endif
