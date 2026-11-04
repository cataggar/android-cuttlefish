#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t ERR_get_error(void);
uint32_t ERR_peek_error(void);
uint32_t ERR_peek_last_error(void);
void ERR_clear_error(void);
char* ERR_error_string(uint32_t packed_error, char* buf);
void ERR_error_string_n(uint32_t packed_error, char* buf, size_t len);
const char* ERR_reason_error_string(uint32_t packed_error);

#define ERR_GET_LIB(packed_error) ((int)(((packed_error) >> 24) & 0xff))
#define ERR_GET_REASON(packed_error) ((int)((packed_error) & 0xfff))

#ifdef __cplusplus
}
#endif
