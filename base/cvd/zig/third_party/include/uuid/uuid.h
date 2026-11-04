#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char uuid_t[16];

#define UUID_STR_LEN 37

void uuid_generate(uuid_t out);
void uuid_generate_random(uuid_t out);
void uuid_unparse(const uuid_t uu, char* out);
void uuid_unparse_lower(const uuid_t uu, char* out);
void uuid_unparse_upper(const uuid_t uu, char* out);
int uuid_parse(const char* in, uuid_t uu);
void uuid_clear(uuid_t uu);
void uuid_copy(uuid_t dst, const uuid_t src);
int uuid_compare(const uuid_t uu1, const uuid_t uu2);
int uuid_is_null(const uuid_t uu);

#ifdef __cplusplus
}
#endif
