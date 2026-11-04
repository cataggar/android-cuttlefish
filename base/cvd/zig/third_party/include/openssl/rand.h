#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

int RAND_bytes(uint8_t* buf, size_t len);
int RAND_pseudo_bytes(uint8_t* buf, size_t len);
void RAND_seed(const void* buf, int num);
int RAND_status(void);

#ifdef __cplusplus
}
#endif
