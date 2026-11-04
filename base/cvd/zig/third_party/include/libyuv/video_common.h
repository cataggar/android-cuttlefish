#pragma once

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#define FOURCC(a, b, c, d)                                        \
  (((uint32_t)(a)) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | \
   ((uint32_t)(d) << 24))

enum FourCC {
  FOURCC_I420 = FOURCC('I', '4', '2', '0'),
  FOURCC_I422 = FOURCC('I', '4', '2', '2'),
  FOURCC_I444 = FOURCC('I', '4', '4', '4'),
  FOURCC_I400 = FOURCC('I', '4', '0', '0'),
  FOURCC_NV21 = FOURCC('N', 'V', '2', '1'),
  FOURCC_NV12 = FOURCC('N', 'V', '1', '2'),
  FOURCC_YUY2 = FOURCC('Y', 'U', 'Y', '2'),
  FOURCC_YUYV = FOURCC('Y', 'U', 'Y', 'V'),
  FOURCC_UYVY = FOURCC('U', 'Y', 'V', 'Y'),
  FOURCC_ARGB = FOURCC('A', 'R', 'G', 'B'),
  FOURCC_BGRA = FOURCC('B', 'G', 'R', 'A'),
  FOURCC_ABGR = FOURCC('A', 'B', 'G', 'R'),
  FOURCC_RGBA = FOURCC('R', 'G', 'B', 'A'),
  FOURCC_24BG = FOURCC('2', '4', 'B', 'G'),
  FOURCC_RAW = FOURCC('r', 'a', 'w', ' '),
  FOURCC_RGBP = FOURCC('R', 'G', 'B', 'P'),
  FOURCC_RGBO = FOURCC('R', 'G', 'B', 'O'),
  FOURCC_MJPG = FOURCC('M', 'J', 'P', 'G'),
  FOURCC_YV12 = FOURCC('Y', 'V', '1', '2'),
  FOURCC_YV16 = FOURCC('Y', 'V', '1', '6'),
  FOURCC_YV24 = FOURCC('Y', 'V', '2', '4'),
  FOURCC_YU12 = FOURCC_I420,
  FOURCC_J420 = FOURCC('J', '4', '2', '0'),
  FOURCC_J400 = FOURCC('J', '4', '0', '0'),
  FOURCC_H420 = FOURCC('H', '4', '2', '0'),
  FOURCC_ANY = -1,
};

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
