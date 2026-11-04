#pragma once

#include "libyuv/basic_types.h"
#include "libyuv/rotate.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

int I420ToARGB(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_argb, int dst_stride_argb, int width, int height);

int I420ToABGR(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_abgr, int dst_stride_abgr, int width, int height);

int I420ToBGRA(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_bgra, int dst_stride_bgra, int width, int height);

int I420ToRGBA(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_rgba, int dst_stride_rgba, int width, int height);

int I420ToRGB24(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
                int src_stride_u, const uint8_t* src_v, int src_stride_v,
                uint8_t* dst_rgb24, int dst_stride_rgb24, int width,
                int height);

int I420ToRAW(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
              int src_stride_u, const uint8_t* src_v, int src_stride_v,
              uint8_t* dst_raw, int dst_stride_raw, int width, int height);

int I420ToNV12(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_y, int dst_stride_y, uint8_t* dst_uv,
               int dst_stride_uv, int width, int height);

int I420ToNV21(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_y, int dst_stride_y, uint8_t* dst_vu,
               int dst_stride_vu, int width, int height);

int ConvertFromI420(const uint8_t* y, int y_stride, const uint8_t* u,
                    int u_stride, const uint8_t* v, int v_stride,
                    uint8_t* dst_sample, int dst_sample_stride, int width,
                    int height, uint32_t fourcc);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
