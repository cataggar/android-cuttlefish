#pragma once

#include "vpx/vpx_codec.h"
#include "vpx/vpx_image.h"

#ifdef __cplusplus
extern "C" {
#endif

enum vp8_com_control_id {
  VP8_SET_REFERENCE = 1,
  VP8_COPY_REFERENCE = 2,
  VP8_SET_POSTPROC = 3,
  VP8_SET_DBG_COLOR_REF_FRAME = 4,
  VP8_SET_DBG_COLOR_MB_MODES = 5,
  VP8_SET_DBG_COLOR_B_MODES = 6,
  VP8_SET_DBG_DISPLAY_MV = 7,
  VP9_GET_REFERENCE = 128,
  VP8_COMMON_CTRL_ID_MAX
};

enum vp8_postproc_level {
  VP8_NOFILTERING = 0,
  VP8_DEBLOCK = 1 << 0,
  VP8_DEMACROBLOCK = 1 << 1,
  VP8_ADDNOISE = 1 << 2,
  VP8_MFQE = 1 << 3
};

typedef struct vp8_postproc_cfg {
  int post_proc_flag;
  int deblocking_level;
  int noise_level;
} vp8_postproc_cfg_t;

typedef enum vpx_ref_frame_type {
  VP8_LAST_FRAME = 1,
  VP8_GOLD_FRAME = 2,
  VP8_ALTR_FRAME = 4
} vpx_ref_frame_type_t;

typedef struct vpx_ref_frame {
  vpx_ref_frame_type_t frame_type;
  vpx_image_t img;
} vpx_ref_frame_t;

typedef struct vp9_ref_frame {
  int idx;
  vpx_image_t img;
} vp9_ref_frame_t;

#ifdef __cplusplus
}
#endif
