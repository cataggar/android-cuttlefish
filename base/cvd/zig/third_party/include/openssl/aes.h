#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AES_ENCRYPT 1
#define AES_DECRYPT 0
#define AES_MAXNR 14
#define AES_BLOCK_SIZE 16

struct aes_key_st {
  uint32_t rd_key[4 * (AES_MAXNR + 1)];
  unsigned int rounds;
};
typedef struct aes_key_st AES_KEY;

int AES_set_encrypt_key(const uint8_t* userKey, const int bits, AES_KEY* key);
int AES_set_decrypt_key(const uint8_t* userKey, const int bits, AES_KEY* key);
void AES_encrypt(const uint8_t* in, uint8_t* out, const AES_KEY* key);
void AES_decrypt(const uint8_t* in, uint8_t* out, const AES_KEY* key);

#ifdef __cplusplus
}
#endif
