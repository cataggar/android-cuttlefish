#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

DH* DH_new(void);
void DH_free(DH* dh);

#ifdef __cplusplus
}
#endif
