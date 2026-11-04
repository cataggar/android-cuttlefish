#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VPX_MAXIMUM_WORK_BUFFERS 8
#define VP9_MAXIMUM_REF_BUFFERS 8

typedef struct vpx_codec_frame_buffer {
  uint8_t* data;
  size_t size;
  void* priv;
} vpx_codec_frame_buffer_t;

typedef int (*vpx_get_frame_buffer_cb_fn_t)(void* priv, size_t min_size,
                                            vpx_codec_frame_buffer_t* fb);

typedef int (*vpx_release_frame_buffer_cb_fn_t)(void* priv,
                                                vpx_codec_frame_buffer_t* fb);

#ifdef __cplusplus
}
#endif
