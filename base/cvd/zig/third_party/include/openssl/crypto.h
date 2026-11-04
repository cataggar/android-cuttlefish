#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

void OPENSSL_cleanse(void* ptr, size_t len);
int CRYPTO_memcmp(const void* a, const void* b, size_t len);
int CRYPTO_library_init(void);
void* OPENSSL_malloc(size_t size);
void OPENSSL_free(void* ptr);

#ifdef __cplusplus
}
#endif
