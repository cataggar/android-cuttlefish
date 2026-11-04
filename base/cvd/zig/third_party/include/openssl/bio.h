#pragma once

#include "openssl/base.h"

#ifdef __cplusplus
extern "C" {
#endif

struct buf_mem_st {
  size_t length;
  char* data;
  size_t max;
};
typedef struct buf_mem_st BUF_MEM;

BIO* BIO_new(const BIO_METHOD* method);
int BIO_free(BIO* bio);
void BIO_free_all(BIO* bio);
int BIO_read(BIO* bio, void* data, int len);
int BIO_write(BIO* bio, const void* data, int len);
int BIO_puts(BIO* bio, const char* str);
int BIO_flush(BIO* bio);
long BIO_ctrl(BIO* bio, int cmd, long larg, void* parg);

const BIO_METHOD* BIO_s_mem(void);
BIO* BIO_new_mem_buf(const void* buf, int len);
int BIO_get_mem_ptr(BIO* bio, BUF_MEM** out);
int BIO_mem_contents(const BIO* bio, const uint8_t** out_contents,
                     size_t* out_len);

#define BIO_CTRL_FLUSH 11
#define BIO_C_GET_BUF_MEM_PTR 115

#ifdef __cplusplus
}
#endif
