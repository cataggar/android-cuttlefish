#pragma once

#include "vpx/vpx_image.h"
#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VPX_CODEC_OK,
  VPX_CODEC_ERROR,
  VPX_CODEC_MEM_ERROR,
  VPX_CODEC_ABI_MISMATCH,
  VPX_CODEC_INCAPABLE,
  VPX_CODEC_UNSUP_BITSTREAM,
  VPX_CODEC_UNSUP_FEATURE,
  VPX_CODEC_CORRUPT_FRAME,
  VPX_CODEC_INVALID_PARAM,
  VPX_CODEC_LIST_END
} vpx_codec_err_t;

typedef long vpx_codec_caps_t;
#define VPX_CODEC_CAP_DECODER 0x1
#define VPX_CODEC_CAP_ENCODER 0x2
#define VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER 0x400000

typedef long vpx_codec_flags_t;

typedef const struct vpx_codec_iface vpx_codec_iface_t;
typedef struct vpx_codec_priv vpx_codec_priv_t;
typedef const void* vpx_codec_iter_t;

typedef struct vpx_codec_ctx {
  const char* name;
  vpx_codec_iface_t* iface;
  vpx_codec_err_t err;
  const char* err_detail;
  vpx_codec_flags_t init_flags;
  union {
    const struct vpx_codec_dec_cfg* dec;
    const struct vpx_codec_enc_cfg* enc;
    const void* raw;
  } config;
  vpx_codec_priv_t* priv;
} vpx_codec_ctx_t;

typedef enum vpx_bit_depth {
  VPX_BITS_8 = 8,
  VPX_BITS_10 = 10,
  VPX_BITS_12 = 12
} vpx_bit_depth_t;

int vpx_codec_version(void);
const char* vpx_codec_version_str(void);
const char* vpx_codec_version_extra_str(void);
const char* vpx_codec_build_config(void);
const char* vpx_codec_iface_name(vpx_codec_iface_t* iface);
const char* vpx_codec_err_to_string(vpx_codec_err_t err);
const char* vpx_codec_error(const vpx_codec_ctx_t* ctx);
const char* vpx_codec_error_detail(const vpx_codec_ctx_t* ctx);
vpx_codec_err_t vpx_codec_destroy(vpx_codec_ctx_t* ctx);
vpx_codec_caps_t vpx_codec_get_caps(vpx_codec_iface_t* iface);
vpx_codec_err_t vpx_codec_control_(vpx_codec_ctx_t* ctx, int ctrl_id, ...);

#define vpx_codec_control(ctx, id, ...) \
  vpx_codec_control_(ctx, id, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
