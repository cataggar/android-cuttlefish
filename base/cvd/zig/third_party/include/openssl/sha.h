#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHA_DIGEST_LENGTH 20
#define SHA224_DIGEST_LENGTH 28
#define SHA256_DIGEST_LENGTH 32
#define SHA384_DIGEST_LENGTH 48
#define SHA512_DIGEST_LENGTH 64

struct sha_state_st {
  uint32_t h[5];
  uint32_t Nl;
  uint32_t Nh;
  uint8_t data[64];
  unsigned int num;
};

struct sha256_state_st {
  uint32_t h[8];
  uint32_t Nl;
  uint32_t Nh;
  uint8_t data[64];
  unsigned int num;
  unsigned int md_len;
};

struct sha512_state_st {
  uint64_t h[8];
  uint64_t Nl;
  uint64_t Nh;
  uint8_t p[128];
  unsigned int num;
  unsigned int md_len;
};

int SHA1_Init(SHA_CTX* sha);
int SHA1_Update(SHA_CTX* sha, const void* data, size_t len);
int SHA1_Final(uint8_t out[SHA_DIGEST_LENGTH], SHA_CTX* sha);
uint8_t* SHA1(const uint8_t* data, size_t len, uint8_t out[SHA_DIGEST_LENGTH]);

int SHA256_Init(SHA256_CTX* sha);
int SHA256_Update(SHA256_CTX* sha, const void* data, size_t len);
int SHA256_Final(uint8_t out[SHA256_DIGEST_LENGTH], SHA256_CTX* sha);
uint8_t* SHA256(const uint8_t* data, size_t len,
                uint8_t out[SHA256_DIGEST_LENGTH]);

int SHA384_Init(SHA512_CTX* sha);
int SHA384_Update(SHA512_CTX* sha, const void* data, size_t len);
int SHA384_Final(uint8_t out[SHA384_DIGEST_LENGTH], SHA512_CTX* sha);
uint8_t* SHA384(const uint8_t* data, size_t len,
                uint8_t out[SHA384_DIGEST_LENGTH]);

int SHA512_Init(SHA512_CTX* sha);
int SHA512_Update(SHA512_CTX* sha, const void* data, size_t len);
int SHA512_Final(uint8_t out[SHA512_DIGEST_LENGTH], SHA512_CTX* sha);
uint8_t* SHA512(const uint8_t* data, size_t len,
                uint8_t out[SHA512_DIGEST_LENGTH]);

#ifdef __cplusplus
}
#endif
