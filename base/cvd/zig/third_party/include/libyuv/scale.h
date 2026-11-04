#pragma once

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

typedef enum FilterMode {
  kFilterNone = 0,
  kFilterLinear = 1,
  kFilterBilinear = 2,
  kFilterBox = 3
} FilterModeEnum;

int I420Scale(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
              int src_stride_u, const uint8_t* src_v, int src_stride_v,
              int src_width, int src_height, uint8_t* dst_y, int dst_stride_y,
              uint8_t* dst_u, int dst_stride_u, uint8_t* dst_v,
              int dst_stride_v, int dst_width, int dst_height,
              enum FilterMode filtering);

void ScalePlane(const uint8_t* src, int src_stride, int src_width,
                int src_height, uint8_t* dst, int dst_stride, int dst_width,
                int dst_height, enum FilterMode filtering);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
