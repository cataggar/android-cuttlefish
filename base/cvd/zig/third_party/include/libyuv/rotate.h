#pragma once

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

typedef enum RotationMode {
  kRotate0 = 0,
  kRotate90 = 90,
  kRotate180 = 180,
  kRotate270 = 270,
  kRotateNone = 0,
  kRotateClockwise = 90,
  kRotateCounterClockwise = 270,
} RotationModeEnum;

int I420Rotate(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
               int dst_stride_u, uint8_t* dst_v, int dst_stride_v, int width,
               int height, enum RotationMode mode);

int ARGBRotate(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_argb,
               int dst_stride_argb, int width, int height,
               enum RotationMode mode);

int RotatePlane(const uint8_t* src, int src_stride, uint8_t* dst,
                int dst_stride, int width, int height, enum RotationMode mode);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
