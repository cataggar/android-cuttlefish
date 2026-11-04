#pragma once

#include "openssl/base.h"
#include "openssl/bn.h"

#ifdef __cplusplus
extern "C" {
#endif

RSA* RSA_new(void);
void RSA_free(RSA* rsa);
int RSA_generate_key_ex(RSA* rsa, int bits, BIGNUM* e, void* cb);

#ifdef __cplusplus
}
#endif
