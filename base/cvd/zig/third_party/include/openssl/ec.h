#pragma once

#include "openssl/base.h"
#include "openssl/nid.h"

#ifdef __cplusplus
extern "C" {
#endif

EC_KEY* EC_KEY_new_by_curve_name(int nid);
void EC_KEY_free(EC_KEY* key);
int EC_KEY_generate_key(EC_KEY* key);

#ifdef __cplusplus
}
#endif
