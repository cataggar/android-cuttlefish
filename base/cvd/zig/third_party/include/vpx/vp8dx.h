#pragma once

#include "vpx/vp8.h"
#include "vpx/vpx_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

vpx_codec_iface_t* vpx_codec_vp8_dx(void);
vpx_codec_iface_t* vpx_codec_vp9_dx(void);

enum vp8_dec_control_id {
  VP8D_GET_LAST_REF_UPDATES = 256,
  VP8D_GET_FRAME_CORRUPTED,
  VP8D_GET_LAST_REF_USED,
  VPXD_SET_DECRYPTOR,
  VP8D_SET_DECRYPTOR = VPXD_SET_DECRYPTOR,
  VP9D_GET_FRAME_SIZE,
  VP9D_GET_DISPLAY_SIZE,
  VP9D_GET_BIT_DEPTH,
  VP9_SET_BYTE_ALIGNMENT,
  VP9_INVERT_TILE_DECODE_ORDER,
  VP9_SET_SKIP_LOOP_FILTER,
  VP9_DECODE_SVC_SPATIAL_LAYER,
  VPXD_GET_LAST_QUANTIZER,
  VP9D_SET_ROW_MT,
  VP9D_SET_LOOP_FILTER_OPT,
  VP8_DECODER_CTRL_ID_MAX
};

#ifdef __cplusplus
}
#endif
