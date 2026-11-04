#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

int EVP_EncodedLength(size_t* out_len, size_t len);
size_t EVP_EncodeBlock(uint8_t* dst, const uint8_t* src, size_t src_len);
int EVP_DecodedLength(size_t* out_len, size_t len);
int EVP_DecodeBase64(uint8_t* out, size_t* out_len, size_t max_out,
                     const uint8_t* in, size_t in_len);
int EVP_DecodeBlock(uint8_t* dst, const uint8_t* src, size_t src_len);

#ifdef __cplusplus
}
#endif
