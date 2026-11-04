#pragma once

#include "libyuv/basic_types.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

int ARGBToI420(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height);

int ABGRToI420(const uint8_t* src_abgr, int src_stride_abgr, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height);

int BGRAToI420(const uint8_t* src_bgra, int src_stride_bgra, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height);

int RGBAToI420(const uint8_t* src_rgba, int src_stride_rgba, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height);

int RGB24ToI420(const uint8_t* src_rgb24, int src_stride_rgb24, uint8_t* dst_y,
                int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
                uint8_t* dst_v, int dst_stride_v, int width, int height);

int RAWToI420(const uint8_t* src_raw, int src_stride_raw, uint8_t* dst_y,
              int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
              uint8_t* dst_v, int dst_stride_v, int width, int height);

int NV12ToI420(const uint8_t* src_y, int src_stride_y, const uint8_t* src_uv,
               int src_stride_uv, uint8_t* dst_y, int dst_stride_y,
               uint8_t* dst_u, int dst_stride_u, uint8_t* dst_v,
               int dst_stride_v, int width, int height);

int NV21ToI420(const uint8_t* src_y, int src_stride_y, const uint8_t* src_vu,
               int src_stride_vu, uint8_t* dst_y, int dst_stride_y,
               uint8_t* dst_u, int dst_stride_u, uint8_t* dst_v,
               int dst_stride_v, int width, int height);

int ConvertToI420(const uint8_t* sample, size_t sample_size, uint8_t* dst_y,
                  int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
                  uint8_t* dst_v, int dst_stride_v, int crop_x, int crop_y,
                  int src_width, int src_height, int crop_width,
                  int crop_height, enum RotationMode rotation, uint32_t fourcc);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
