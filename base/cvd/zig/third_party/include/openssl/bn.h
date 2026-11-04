#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t BN_ULONG;
#define BN_F4 0x10001L

BIGNUM* BN_new(void);
void BN_free(BIGNUM* bn);
int BN_set_word(BIGNUM* bn, BN_ULONG value);
BIGNUM* BN_bin2bn(const uint8_t* in, size_t len, BIGNUM* ret);
size_t BN_bn2bin(const BIGNUM* in, uint8_t* out);
int BN_num_bytes(const BIGNUM* bn);

#ifdef __cplusplus
}
#endif
