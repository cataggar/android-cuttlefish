#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char Byte;
typedef Byte Bytef;
typedef unsigned int uInt;
typedef unsigned long uLong;
typedef uLong uLongf;
typedef void* voidpf;
typedef void* voidp;
typedef const void* voidpc;

#define Z_OK 0
#define Z_STREAM_END 1
#define Z_NEED_DICT 2
#define Z_ERRNO (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR (-3)
#define Z_MEM_ERROR (-4)
#define Z_BUF_ERROR (-5)
#define Z_VERSION_ERROR (-6)

#define Z_NO_COMPRESSION 0
#define Z_BEST_SPEED 1
#define Z_BEST_COMPRESSION 9
#define Z_DEFAULT_COMPRESSION (-1)

uLong crc32(uLong crc, const Bytef* buf, uInt len);
uLong adler32(uLong adler, const Bytef* buf, uInt len);
uLong compressBound(uLong sourceLen);
int compress(Bytef* dest, uLongf* destLen, const Bytef* source,
             uLong sourceLen);
int compress2(Bytef* dest, uLongf* destLen, const Bytef* source,
              uLong sourceLen, int level);
int uncompress(Bytef* dest, uLongf* destLen, const Bytef* source,
               uLong sourceLen);

#ifdef __cplusplus
}
#endif
