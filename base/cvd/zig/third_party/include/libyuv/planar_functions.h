#pragma once

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void CopyPlane(const uint8_t* src_y, int src_stride_y, uint8_t* dst_y,
               int dst_stride_y, int width, int height);

void SetPlane(uint8_t* dst_y, int dst_stride_y, int width, int height,
              uint32_t value);

void SplitUVPlane(const uint8_t* src_uv, int src_stride_uv, uint8_t* dst_u,
                  int dst_stride_u, uint8_t* dst_v, int dst_stride_v, int width,
                  int height);

void MergeUVPlane(const uint8_t* src_u, int src_stride_u, const uint8_t* src_v,
                  int src_stride_v, uint8_t* dst_uv, int dst_stride_uv,
                  int width, int height);

int I420Copy(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
             int src_stride_u, const uint8_t* src_v, int src_stride_v,
             uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
             int dst_stride_u, uint8_t* dst_v, int dst_stride_v, int width,
             int height);

int I420Rect(uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
             uint8_t* dst_v, int dst_stride_v, int x, int y, int width,
             int height, int value_y, int value_u, int value_v);

int ARGBCopy(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_argb,
             int dst_stride_argb, int width, int height);

int ARGBToRAW(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_raw,
              int dst_stride_raw, int width, int height);

int ABGRToRAW(const uint8_t* src_abgr, int src_stride_abgr, uint8_t* dst_raw,
              int dst_stride_raw, int width, int height);

int ARGBToRGB24(const uint8_t* src_argb, int src_stride_argb,
                uint8_t* dst_rgb24, int dst_stride_rgb24, int width,
                int height);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
