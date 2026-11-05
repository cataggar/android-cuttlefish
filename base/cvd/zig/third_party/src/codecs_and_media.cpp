#include "libyuv.h"
#include "mkvmuxer/mkvmuxer.h"
#include "mkvmuxer/mkvmuxertypes.h"
#include "mkvmuxer/mkvwriter.h"
#include "opus/opus.h"
#include "opus/opus_multistream.h"
#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vpx_image.h"
#include "wels/codec_api.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// ============================================================================
// libyuv implementation
// ============================================================================
namespace libyuv {
namespace {

inline uint8_t ClampByte(int v) {
  return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

inline void RgbToYuv(uint8_t r, uint8_t g, uint8_t b, uint8_t* y, uint8_t* u,
                     uint8_t* v) {
  int yi = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
  int ui = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
  int vi = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
  if (y) *y = ClampByte(yi);
  if (u) *u = ClampByte(ui);
  if (v) *v = ClampByte(vi);
}

inline void YuvToRgb(uint8_t y, uint8_t u, uint8_t v, uint8_t* r, uint8_t* g,
                     uint8_t* b) {
  int c = static_cast<int>(y) - 16;
  int d = static_cast<int>(u) - 128;
  int e = static_cast<int>(v) - 128;
  int ri = (298 * c + 409 * e + 128) >> 8;
  int gi = (298 * c - 100 * d - 208 * e + 128) >> 8;
  int bi = (298 * c + 516 * d + 128) >> 8;
  *r = ClampByte(ri);
  *g = ClampByte(gi);
  *b = ClampByte(bi);
}

// Generic 4-channel to I420 converter where r_idx, g_idx, b_idx specify byte offsets
int Generic4ChToI420(const uint8_t* src, int src_stride, uint8_t* dst_y,
                     int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
                     uint8_t* dst_v, int dst_stride_v, int width, int height,
                     int r_idx, int g_idx, int b_idx) {
  if (!src || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int sy = height < 0 ? (abs_h - 1 - y) : y;
    const uint8_t* row = src + sy * src_stride;
    uint8_t* yrow = dst_y + y * dst_stride_y;
    uint8_t* urow = dst_u + (y / 2) * dst_stride_u;
    uint8_t* vrow = dst_v + (y / 2) * dst_stride_v;
    for (int x = 0; x < width; ++x) {
      const uint8_t* px = row + x * 4;
      uint8_t r = px[r_idx];
      uint8_t g = px[g_idx];
      uint8_t b = px[b_idx];
      if ((y % 2 == 0) && (x % 2 == 0)) {
        RgbToYuv(r, g, b, &yrow[x], &urow[x / 2], &vrow[x / 2]);
      } else {
        RgbToYuv(r, g, b, &yrow[x], nullptr, nullptr);
      }
    }
  }
  return 0;
}

int GenericI420To4Ch(const uint8_t* src_y, int src_stride_y,
                     const uint8_t* src_u, int src_stride_u,
                     const uint8_t* src_v, int src_stride_v, uint8_t* dst,
                     int dst_stride, int width, int height, int r_idx,
                     int g_idx, int b_idx, int a_idx) {
  if (!src_y || !src_u || !src_v || !dst || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int dy = height < 0 ? (abs_h - 1 - y) : y;
    const uint8_t* yrow = src_y + y * src_stride_y;
    const uint8_t* urow = src_u + (y / 2) * src_stride_u;
    const uint8_t* vrow = src_v + (y / 2) * src_stride_v;
    uint8_t* drow = dst + dy * dst_stride;
    for (int x = 0; x < width; ++x) {
      uint8_t r, g, b;
      YuvToRgb(yrow[x], urow[x / 2], vrow[x / 2], &r, &g, &b);
      uint8_t* px = drow + x * 4;
      px[r_idx] = r;
      px[g_idx] = g;
      px[b_idx] = b;
      px[a_idx] = 255;
    }
  }
  return 0;
}

}  // namespace

extern "C" {

void CopyPlane(const uint8_t* src_y, int src_stride_y, uint8_t* dst_y,
               int dst_stride_y, int width, int height) {
  if (!src_y || !dst_y || width <= 0 || height == 0) return;
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int sy = height < 0 ? (abs_h - 1 - y) : y;
    std::memmove(dst_y + y * dst_stride_y, src_y + sy * src_stride_y,
                 static_cast<size_t>(width));
  }
}

void SetPlane(uint8_t* dst_y, int dst_stride_y, int width, int height,
              uint32_t value) {
  if (!dst_y || width <= 0 || height <= 0) return;
  for (int y = 0; y < height; ++y) {
    std::memset(dst_y + y * dst_stride_y, static_cast<int>(value & 0xFF),
                static_cast<size_t>(width));
  }
}

void SplitUVPlane(const uint8_t* src_uv, int src_stride_uv, uint8_t* dst_u,
                  int dst_stride_u, uint8_t* dst_v, int dst_stride_v, int width,
                  int height) {
  if (!src_uv || !dst_u || !dst_v || width <= 0 || height <= 0) return;
  for (int y = 0; y < height; ++y) {
    const uint8_t* uv = src_uv + y * src_stride_uv;
    uint8_t* u = dst_u + y * dst_stride_u;
    uint8_t* v = dst_v + y * dst_stride_v;
    for (int x = 0; x < width; ++x) {
      u[x] = uv[2 * x];
      v[x] = uv[2 * x + 1];
    }
  }
}

void MergeUVPlane(const uint8_t* src_u, int src_stride_u, const uint8_t* src_v,
                  int src_stride_v, uint8_t* dst_uv, int dst_stride_uv,
                  int width, int height) {
  if (!src_u || !src_v || !dst_uv || width <= 0 || height <= 0) return;
  for (int y = 0; y < height; ++y) {
    const uint8_t* u = src_u + y * src_stride_u;
    const uint8_t* v = src_v + y * src_stride_v;
    uint8_t* uv = dst_uv + y * dst_stride_uv;
    for (int x = 0; x < width; ++x) {
      uv[2 * x] = u[x];
      uv[2 * x + 1] = v[x];
    }
  }
}

int I420Copy(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
             int src_stride_u, const uint8_t* src_v, int src_stride_v,
             uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
             uint8_t* dst_v, int dst_stride_v, int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_y || !dst_u || !dst_v || width <= 0 ||
      height == 0) {
    return -1;
  }
  int halfwidth = (width + 1) >> 1;
  int halfheight = height > 0 ? ((height + 1) >> 1) : -((-height + 1) >> 1);
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  CopyPlane(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth, halfheight);
  CopyPlane(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth, halfheight);
  return 0;
}

void ScalePlane(const uint8_t* src, int src_stride, int src_width,
                int src_height, uint8_t* dst, int dst_stride, int dst_width,
                int dst_height, enum FilterMode filtering) {
  (void)filtering;
  if (!src || !dst || src_width <= 0 || src_height == 0 || dst_width <= 0 ||
      dst_height <= 0) {
    return;
  }
  int abs_sh = src_height < 0 ? -src_height : src_height;
  for (int dy = 0; dy < dst_height; ++dy) {
    int sy = (dy * abs_sh) / dst_height;
    if (src_height < 0) sy = abs_sh - 1 - sy;
    const uint8_t* srow = src + sy * src_stride;
    uint8_t* drow = dst + dy * dst_stride;
    for (int dx = 0; dx < dst_width; ++dx) {
      int sx = (dx * src_width) / dst_width;
      drow[dx] = srow[sx];
    }
  }
}

int I420Scale(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
              int src_stride_u, const uint8_t* src_v, int src_stride_v,
              int src_width, int src_height, uint8_t* dst_y, int dst_stride_y,
              uint8_t* dst_u, int dst_stride_u, uint8_t* dst_v,
              int dst_stride_v, int dst_width, int dst_height,
              enum FilterMode filtering) {
  if (!src_y || !src_u || !src_v || !dst_y || !dst_u || !dst_v ||
      src_width <= 0 || src_height == 0 || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  int src_hw = (src_width + 1) >> 1;
  int src_hh = src_height > 0 ? ((src_height + 1) >> 1)
                              : -((-src_height + 1) >> 1);
  int dst_hw = (dst_width + 1) >> 1;
  int dst_hh = (dst_height + 1) >> 1;
  ScalePlane(src_y, src_stride_y, src_width, src_height, dst_y, dst_stride_y,
             dst_width, dst_height, filtering);
  ScalePlane(src_u, src_stride_u, src_hw, src_hh, dst_u, dst_stride_u, dst_hw,
             dst_hh, filtering);
  ScalePlane(src_v, src_stride_v, src_hw, src_hh, dst_v, dst_stride_v, dst_hw,
             dst_hh, filtering);
  return 0;
}

int RotatePlane(const uint8_t* src, int src_stride, uint8_t* dst,
                int dst_stride, int width, int height, enum RotationMode mode) {
  if (!src || !dst || width <= 0 || height == 0) return -1;
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int sy = height < 0 ? (abs_h - 1 - y) : y;
    for (int x = 0; x < width; ++x) {
      uint8_t val = src[sy * src_stride + x];
      switch (mode) {
        case kRotate0:
          dst[y * dst_stride + x] = val;
          break;
        case kRotate90:
          dst[x * dst_stride + (abs_h - 1 - y)] = val;
          break;
        case kRotate180:
          dst[(abs_h - 1 - y) * dst_stride + (width - 1 - x)] = val;
          break;
        case kRotate270:
          dst[(width - 1 - x) * dst_stride + y] = val;
          break;
      }
    }
  }
  return 0;
}

int ARGBRotate(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_argb,
               int dst_stride_argb, int width, int height,
               enum RotationMode mode) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0) return -1;
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int sy = height < 0 ? (abs_h - 1 - y) : y;
    for (int x = 0; x < width; ++x) {
      const uint8_t* px = src_argb + sy * src_stride_argb + x * 4;
      switch (mode) {
        case kRotate0:
          std::memcpy(dst_argb + y * dst_stride_argb + x * 4, px, 4);
          break;
        case kRotate90:
          std::memcpy(dst_argb + x * dst_stride_argb + (abs_h - 1 - y) * 4, px,
                      4);
          break;
        case kRotate180:
          std::memcpy(dst_argb + (abs_h - 1 - y) * dst_stride_argb +
                          (width - 1 - x) * 4,
                      px, 4);
          break;
        case kRotate270:
          std::memcpy(dst_argb + (width - 1 - x) * dst_stride_argb + y * 4, px,
                      4);
          break;
      }
    }
  }
  return 0;
}

int I420Rotate(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
               int dst_stride_u, uint8_t* dst_v, int dst_stride_v, int width,
               int height, enum RotationMode mode) {
  if (!src_y || !src_u || !src_v || !dst_y || !dst_u || !dst_v || width <= 0 ||
      height == 0) {
    return -1;
  }
  int hw = (width + 1) >> 1;
  int hh = height > 0 ? ((height + 1) >> 1) : -((-height + 1) >> 1);
  RotatePlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height, mode);
  RotatePlane(src_u, src_stride_u, dst_u, dst_stride_u, hw, hh, mode);
  RotatePlane(src_v, src_stride_v, dst_v, dst_stride_v, hw, hh, mode);
  return 0;
}

int NV12ToI420Rotate(const uint8_t* src_y, int src_stride_y,
                     const uint8_t* src_uv, int src_stride_uv, uint8_t* dst_y,
                     int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
                     uint8_t* dst_v, int dst_stride_v, int width, int height,
                     enum RotationMode mode) {
  if (!src_y || !src_uv || !dst_y || !dst_u || !dst_v || width <= 0 ||
      height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  int hw = (width + 1) >> 1;
  int hh = (abs_h + 1) >> 1;
  std::vector<uint8_t> tmp_u(static_cast<size_t>(hw * hh));
  std::vector<uint8_t> tmp_v(static_cast<size_t>(hw * hh));
  SplitUVPlane(src_uv, src_stride_uv, tmp_u.data(), hw, tmp_v.data(), hw, hw,
               hh);
  return I420Rotate(src_y, src_stride_y, tmp_u.data(), hw, tmp_v.data(), hw,
                    dst_y, dst_stride_y, dst_u, dst_stride_u, dst_v,
                    dst_stride_v, width, height, mode);
}

// libyuv naming convention (little-endian memory order):
// ARGB = B, G, R, A in memory (r=2, g=1, b=0, a=3)
// BGRA = A, R, G, B in memory (r=1, g=2, b=3, a=0)
// ABGR = R, G, B, A in memory (r=0, g=1, b=2, a=3)
// RGBA = A, B, G, R in memory (r=3, g=2, b=1, a=0)
int ABGRToI420(const uint8_t* src_abgr, int src_stride_abgr, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height) {
  return Generic4ChToI420(src_abgr, src_stride_abgr, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, width, height, 0,
                          1, 2);
}

int ARGBToI420(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height) {
  return Generic4ChToI420(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, width, height, 2,
                          1, 0);
}

int RGBAToI420(const uint8_t* src_rgba, int src_stride_rgba, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height) {
  return Generic4ChToI420(src_rgba, src_stride_rgba, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, width, height, 3,
                          2, 1);
}

int BGRAToI420(const uint8_t* src_bgra, int src_stride_bgra, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height) {
  return Generic4ChToI420(src_bgra, src_stride_bgra, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, width, height, 1,
                          2, 3);
}

int RGB24ToI420(const uint8_t* src_rgb24, int src_stride_rgb24, uint8_t* dst_y,
                int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
                uint8_t* dst_v, int dst_stride_v, int width, int height) {
  if (!src_rgb24 || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int sy = height < 0 ? (abs_h - 1 - y) : y;
    const uint8_t* row = src_rgb24 + sy * src_stride_rgb24;
    for (int x = 0; x < width; ++x) {
      uint8_t b = row[x * 3 + 0];
      uint8_t g = row[x * 3 + 1];
      uint8_t r = row[x * 3 + 2];
      RgbToYuv(r, g, b, &dst_y[y * dst_stride_y + x],
               (y % 2 == 0 && x % 2 == 0)
                   ? &dst_u[(y / 2) * dst_stride_u + (x / 2)]
                   : nullptr,
               (y % 2 == 0 && x % 2 == 0)
                   ? &dst_v[(y / 2) * dst_stride_v + (x / 2)]
                   : nullptr);
    }
  }
  return 0;
}

int RAWToI420(const uint8_t* src_raw, int src_stride_raw, uint8_t* dst_y,
              int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
              uint8_t* dst_v, int dst_stride_v, int width, int height) {
  if (!src_raw || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int sy = height < 0 ? (abs_h - 1 - y) : y;
    const uint8_t* row = src_raw + sy * src_stride_raw;
    for (int x = 0; x < width; ++x) {
      uint8_t r = row[x * 3 + 0];
      uint8_t g = row[x * 3 + 1];
      uint8_t b = row[x * 3 + 2];
      RgbToYuv(r, g, b, &dst_y[y * dst_stride_y + x],
               (y % 2 == 0 && x % 2 == 0)
                   ? &dst_u[(y / 2) * dst_stride_u + (x / 2)]
                   : nullptr,
               (y % 2 == 0 && x % 2 == 0)
                   ? &dst_v[(y / 2) * dst_stride_v + (x / 2)]
                   : nullptr);
    }
  }
  return 0;
}

int NV12ToI420(const uint8_t* src_y, int src_stride_y, const uint8_t* src_uv,
               int src_stride_uv, uint8_t* dst_y, int dst_stride_y,
               uint8_t* dst_u, int dst_stride_u, uint8_t* dst_v,
               int dst_stride_v, int width, int height) {
  if (!src_y || !src_uv || !dst_y || !dst_u || !dst_v || width <= 0 ||
      height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  SplitUVPlane(src_uv, src_stride_uv, dst_u, dst_stride_u, dst_v, dst_stride_v,
               (width + 1) >> 1, (abs_h + 1) >> 1);
  return 0;
}

int NV21ToI420(const uint8_t* src_y, int src_stride_y, const uint8_t* src_vu,
               int src_stride_vu, uint8_t* dst_y, int dst_stride_y,
               uint8_t* dst_u, int dst_stride_u, uint8_t* dst_v,
               int dst_stride_v, int width, int height) {
  return NV12ToI420(src_y, src_stride_y, src_vu, src_stride_vu, dst_y,
                    dst_stride_y, dst_v, dst_stride_v, dst_u, dst_stride_u,
                    width, height);
}

int YUY2ToI420(const uint8_t* src_yuy2, int src_stride_yuy2, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height) {
  if (!src_yuy2 || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    const uint8_t* s = src_yuy2 + y * src_stride_yuy2;
    uint8_t* dy = dst_y + y * dst_stride_y;
    uint8_t* du = dst_u + (y / 2) * dst_stride_u;
    uint8_t* dv = dst_v + (y / 2) * dst_stride_v;
    for (int x = 0; x < width; x += 2) {
      dy[x] = s[2 * x];
      if (x + 1 < width) dy[x + 1] = s[2 * x + 2];
      if (y % 2 == 0) {
        du[x / 2] = s[2 * x + 1];
        dv[x / 2] = s[2 * x + 3];
      }
    }
  }
  return 0;
}

int UYVYToI420(const uint8_t* src_uyvy, int src_stride_uyvy, uint8_t* dst_y,
               int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
               uint8_t* dst_v, int dst_stride_v, int width, int height) {
  if (!src_uyvy || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    const uint8_t* s = src_uyvy + y * src_stride_uyvy;
    uint8_t* dy = dst_y + y * dst_stride_y;
    uint8_t* du = dst_u + (y / 2) * dst_stride_u;
    uint8_t* dv = dst_v + (y / 2) * dst_stride_v;
    for (int x = 0; x < width; x += 2) {
      dy[x] = s[2 * x + 1];
      if (x + 1 < width) dy[x + 1] = s[2 * x + 3];
      if (y % 2 == 0) {
        du[x / 2] = s[2 * x];
        dv[x / 2] = s[2 * x + 2];
      }
    }
  }
  return 0;
}

int I420ToARGB(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_argb, int dst_stride_argb, int width, int height) {
  return GenericI420To4Ch(src_y, src_stride_y, src_u, src_stride_u, src_v,
                          src_stride_v, dst_argb, dst_stride_argb, width,
                          height, 2, 1, 0, 3);
}

int I420ToABGR(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_abgr, int dst_stride_abgr, int width, int height) {
  return GenericI420To4Ch(src_y, src_stride_y, src_u, src_stride_u, src_v,
                          src_stride_v, dst_abgr, dst_stride_abgr, width,
                          height, 0, 1, 2, 3);
}

int I420ToRGBA(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_rgba, int dst_stride_rgba, int width, int height) {
  return GenericI420To4Ch(src_y, src_stride_y, src_u, src_stride_u, src_v,
                          src_stride_v, dst_rgba, dst_stride_rgba, width,
                          height, 3, 2, 1, 0);
}

int I420ToBGRA(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_bgra, int dst_stride_bgra, int width, int height) {
  return GenericI420To4Ch(src_y, src_stride_y, src_u, src_stride_u, src_v,
                          src_stride_v, dst_bgra, dst_stride_bgra, width,
                          height, 1, 2, 3, 0);
}

int I420ToRGB24(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
                int src_stride_u, const uint8_t* src_v, int src_stride_v,
                uint8_t* dst_rgb24, int dst_stride_rgb24, int width,
                int height) {
  if (!src_y || !src_u || !src_v || !dst_rgb24 || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int dy = height < 0 ? (abs_h - 1 - y) : y;
    uint8_t* drow = dst_rgb24 + dy * dst_stride_rgb24;
    for (int x = 0; x < width; ++x) {
      uint8_t r, g, b;
      YuvToRgb(src_y[y * src_stride_y + x],
               src_u[(y / 2) * src_stride_u + (x / 2)],
               src_v[(y / 2) * src_stride_v + (x / 2)], &r, &g, &b);
      drow[x * 3 + 0] = b;
      drow[x * 3 + 1] = g;
      drow[x * 3 + 2] = r;
    }
  }
  return 0;
}

int I420ToRAW(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
              int src_stride_u, const uint8_t* src_v, int src_stride_v,
              uint8_t* dst_raw, int dst_stride_raw, int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_raw || width <= 0 || height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int dy = height < 0 ? (abs_h - 1 - y) : y;
    uint8_t* drow = dst_raw + dy * dst_stride_raw;
    for (int x = 0; x < width; ++x) {
      uint8_t r, g, b;
      YuvToRgb(src_y[y * src_stride_y + x],
               src_u[(y / 2) * src_stride_u + (x / 2)],
               src_v[(y / 2) * src_stride_v + (x / 2)], &r, &g, &b);
      drow[x * 3 + 0] = r;
      drow[x * 3 + 1] = g;
      drow[x * 3 + 2] = b;
    }
  }
  return 0;
}

int I420ToNV12(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_y, int dst_stride_y, uint8_t* dst_uv,
               int dst_stride_uv, int width, int height) {
  if (!src_y || !src_u || !src_v || !dst_y || !dst_uv || width <= 0 ||
      height == 0) {
    return -1;
  }
  int abs_h = height < 0 ? -height : height;
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  MergeUVPlane(src_u, src_stride_u, src_v, src_stride_v, dst_uv, dst_stride_uv,
               (width + 1) >> 1, (abs_h + 1) >> 1);
  return 0;
}

int I420ToNV21(const uint8_t* src_y, int src_stride_y, const uint8_t* src_u,
               int src_stride_u, const uint8_t* src_v, int src_stride_v,
               uint8_t* dst_y, int dst_stride_y, uint8_t* dst_vu,
               int dst_stride_vu, int width, int height) {
  return I420ToNV12(src_y, src_stride_y, src_v, src_stride_v, src_u,
                    src_stride_u, dst_y, dst_stride_y, dst_vu, dst_stride_vu,
                    width, height);
}

int NV12ToARGB(const uint8_t* src_y, int src_stride_y, const uint8_t* src_uv,
               int src_stride_uv, uint8_t* dst_argb, int dst_stride_argb,
               int width, int height) {
  if (!src_y || !src_uv || !dst_argb || width <= 0 || height == 0) return -1;
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int dy = height < 0 ? (abs_h - 1 - y) : y;
    const uint8_t* yrow = src_y + y * src_stride_y;
    const uint8_t* uvrow = src_uv + (y / 2) * src_stride_uv;
    uint8_t* drow = dst_argb + dy * dst_stride_argb;
    for (int x = 0; x < width; ++x) {
      uint8_t r, g, b;
      YuvToRgb(yrow[x], uvrow[2 * (x / 2)], uvrow[2 * (x / 2) + 1], &r, &g, &b);
      drow[x * 4 + 0] = b;
      drow[x * 4 + 1] = g;
      drow[x * 4 + 2] = r;
      drow[x * 4 + 3] = 255;
    }
  }
  return 0;
}

int NV12ToABGR(const uint8_t* src_y, int src_stride_y, const uint8_t* src_uv,
               int src_stride_uv, uint8_t* dst_abgr, int dst_stride_abgr,
               int width, int height) {
  if (!src_y || !src_uv || !dst_abgr || width <= 0 || height == 0) return -1;
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int dy = height < 0 ? (abs_h - 1 - y) : y;
    const uint8_t* yrow = src_y + y * src_stride_y;
    const uint8_t* uvrow = src_uv + (y / 2) * src_stride_uv;
    uint8_t* drow = dst_abgr + dy * dst_stride_abgr;
    for (int x = 0; x < width; ++x) {
      uint8_t r, g, b;
      YuvToRgb(yrow[x], uvrow[2 * (x / 2)], uvrow[2 * (x / 2) + 1], &r, &g, &b);
      drow[x * 4 + 0] = r;
      drow[x * 4 + 1] = g;
      drow[x * 4 + 2] = b;
      drow[x * 4 + 3] = 255;
    }
  }
  return 0;
}

int ARGBToABGR(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_abgr,
               int dst_stride_abgr, int width, int height) {
  if (!src_argb || !dst_abgr || width <= 0 || height == 0) return -1;
  int abs_h = height < 0 ? -height : height;
  for (int y = 0; y < abs_h; ++y) {
    int sy = height < 0 ? (abs_h - 1 - y) : y;
    const uint8_t* srow = src_argb + sy * src_stride_argb;
    uint8_t* drow = dst_abgr + y * dst_stride_abgr;
    for (int x = 0; x < width; ++x) {
      uint8_t b = srow[x * 4 + 0];
      uint8_t g = srow[x * 4 + 1];
      uint8_t r = srow[x * 4 + 2];
      uint8_t a = srow[x * 4 + 3];
      drow[x * 4 + 0] = r;
      drow[x * 4 + 1] = g;
      drow[x * 4 + 2] = b;
      drow[x * 4 + 3] = a;
    }
  }
  return 0;
}

int ABGRToARGB(const uint8_t* src_abgr, int src_stride_abgr, uint8_t* dst_argb,
               int dst_stride_argb, int width, int height) {
  return ARGBToABGR(src_abgr, src_stride_abgr, dst_argb, dst_stride_argb, width,
                    height);
}

int ARGBCopy(const uint8_t* src_argb, int src_stride_argb, uint8_t* dst_argb,
             int dst_stride_argb, int width, int height) {
  if (!src_argb || !dst_argb || width <= 0 || height == 0) return -1;
  CopyPlane(src_argb, src_stride_argb, dst_argb, dst_stride_argb, width * 4,
            height);
  return 0;
}

int ARGBScale(const uint8_t* src_argb, int src_stride_argb, int src_width,
              int src_height, uint8_t* dst_argb, int dst_stride_argb,
              int dst_width, int dst_height, enum FilterMode filtering) {
  (void)filtering;
  if (!src_argb || !dst_argb || src_width <= 0 || src_height == 0 ||
      dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  int abs_sh = src_height < 0 ? -src_height : src_height;
  for (int dy = 0; dy < dst_height; ++dy) {
    int sy = (dy * abs_sh) / dst_height;
    if (src_height < 0) sy = abs_sh - 1 - sy;
    const uint8_t* srow = src_argb + sy * src_stride_argb;
    uint8_t* drow = dst_argb + dy * dst_stride_argb;
    for (int dx = 0; dx < dst_width; ++dx) {
      int sx = (dx * src_width) / dst_width;
      std::memcpy(drow + dx * 4, srow + sx * 4, 4);
    }
  }
  return 0;
}

int ConvertToI420(const uint8_t* sample, size_t sample_size, uint8_t* dst_y,
                  int dst_stride_y, uint8_t* dst_u, int dst_stride_u,
                  uint8_t* dst_v, int dst_stride_v, int crop_x, int crop_y,
                  int src_width, int src_height, int crop_width,
                  int crop_height, enum RotationMode rotation,
                  uint32_t fourcc) {
  (void)sample_size;
  (void)crop_x;
  (void)crop_y;
  (void)src_width;
  (void)src_height;
  (void)rotation;
  if (!sample || !dst_y || !dst_u || !dst_v || crop_width <= 0 ||
      crop_height == 0) {
    return -1;
  }
  int w = crop_width;
  int h = crop_height;
  int abs_h = h < 0 ? -h : h;
  switch (fourcc) {
    case FOURCC_I420: {
      int hw = (w + 1) >> 1;
      int hh = (abs_h + 1) >> 1;
      const uint8_t* sy = sample;
      const uint8_t* su = sy + w * abs_h;
      const uint8_t* sv = su + hw * hh;
      return I420Copy(sy, w, su, hw, sv, hw, dst_y, dst_stride_y, dst_u,
                      dst_stride_u, dst_v, dst_stride_v, w, h);
    }
    case FOURCC_YV12: {
      int hw = (w + 1) >> 1;
      int hh = (abs_h + 1) >> 1;
      const uint8_t* sy = sample;
      const uint8_t* sv = sy + w * abs_h;
      const uint8_t* su = sv + hw * hh;
      return I420Copy(sy, w, su, hw, sv, hw, dst_y, dst_stride_y, dst_u,
                      dst_stride_u, dst_v, dst_stride_v, w, h);
    }
    case FOURCC_NV12:
      return NV12ToI420(sample, w, sample + w * abs_h, w, dst_y, dst_stride_y,
                        dst_u, dst_stride_u, dst_v, dst_stride_v, w, h);
    case FOURCC_NV21:
      return NV21ToI420(sample, w, sample + w * abs_h, w, dst_y, dst_stride_y,
                        dst_u, dst_stride_u, dst_v, dst_stride_v, w, h);
    case FOURCC_ARGB:
      return ARGBToI420(sample, w * 4, dst_y, dst_stride_y, dst_u, dst_stride_u,
                        dst_v, dst_stride_v, w, h);
    case FOURCC_ABGR:
      return ABGRToI420(sample, w * 4, dst_y, dst_stride_y, dst_u, dst_stride_u,
                        dst_v, dst_stride_v, w, h);
    case FOURCC_BGRA:
      return BGRAToI420(sample, w * 4, dst_y, dst_stride_y, dst_u, dst_stride_u,
                        dst_v, dst_stride_v, w, h);
    case FOURCC_RGBA:
      return RGBAToI420(sample, w * 4, dst_y, dst_stride_y, dst_u, dst_stride_u,
                        dst_v, dst_stride_v, w, h);
    case FOURCC_24BG:
      return RGB24ToI420(sample, w * 3, dst_y, dst_stride_y, dst_u,
                         dst_stride_u, dst_v, dst_stride_v, w, h);
    case FOURCC_RAW:
      return RAWToI420(sample, w * 3, dst_y, dst_stride_y, dst_u, dst_stride_u,
                       dst_v, dst_stride_v, w, h);
    case FOURCC_YUY2:
    case FOURCC_YUYV:
      return YUY2ToI420(sample, w * 2, dst_y, dst_stride_y, dst_u, dst_stride_u,
                        dst_v, dst_stride_v, w, h);
    case FOURCC_UYVY:
      return UYVYToI420(sample, w * 2, dst_y, dst_stride_y, dst_u, dst_stride_u,
                        dst_v, dst_stride_v, w, h);
    default:
      return ARGBToI420(sample, w * 4, dst_y, dst_stride_y, dst_u, dst_stride_u,
                        dst_v, dst_stride_v, w, h);
  }
}

int ConvertFromI420(const uint8_t* y, int y_stride, const uint8_t* u,
                    int u_stride, const uint8_t* v, int v_stride,
                    uint8_t* dst_sample, int dst_sample_stride, int width,
                    int height, uint32_t fourcc) {
  if (!y || !u || !v || !dst_sample || width <= 0 || height == 0) return -1;
  int stride = dst_sample_stride > 0 ? dst_sample_stride : width * 4;
  switch (fourcc) {
    case FOURCC_ARGB:
      return I420ToARGB(y, y_stride, u, u_stride, v, v_stride, dst_sample,
                        stride, width, height);
    case FOURCC_ABGR:
      return I420ToABGR(y, y_stride, u, u_stride, v, v_stride, dst_sample,
                        stride, width, height);
    case FOURCC_RGBA:
      return I420ToRGBA(y, y_stride, u, u_stride, v, v_stride, dst_sample,
                        stride, width, height);
    case FOURCC_BGRA:
      return I420ToBGRA(y, y_stride, u, u_stride, v, v_stride, dst_sample,
                        stride, width, height);
    default:
      return I420ToARGB(y, y_stride, u, u_stride, v, v_stride, dst_sample,
                        stride, width, height);
  }
}

}  // extern "C"
}  // namespace libyuv

// ============================================================================
// libvpx implementation
// ============================================================================
struct vpx_codec_iface {
  const char* name;
  int is_encoder;
};

namespace {

vpx_codec_iface g_vp8_cx_iface = {"WebM Project VP8 Encoder", 1};
vpx_codec_iface g_vp9_cx_iface = {"WebM Project VP9 Encoder", 1};
vpx_codec_iface g_vp8_dx_iface = {"WebM Project VP8 Decoder", 0};
vpx_codec_iface g_vp9_dx_iface = {"WebM Project VP9 Decoder", 0};

struct VpxPrivState {
  std::vector<uint8_t> pkt_buf;
  vpx_codec_cx_pkt_t pkt{};
  bool has_pkt = false;
  vpx_image_t dec_img{};
  bool has_dec_img = false;
  unsigned int width = 320;
  unsigned int height = 240;
  uint64_t frame_count = 0;

  ~VpxPrivState() {
    if (dec_img.img_data_owner && dec_img.img_data) {
      std::free(dec_img.img_data);
    }
  }
};

void SetupI420Planes(vpx_image_t* img, unsigned int w, unsigned int h,
                     unsigned char* data, int owns_data) {
  unsigned int aw = (w + 1) & ~1u;
  unsigned int ah = (h + 1) & ~1u;
  img->fmt = VPX_IMG_FMT_I420;
  img->cs = VPX_CS_UNKNOWN;
  img->range = VPX_CR_STUDIO_RANGE;
  img->w = aw;
  img->h = ah;
  img->d_w = w;
  img->d_h = h;
  img->x_chroma_shift = 1;
  img->y_chroma_shift = 1;
  img->bps = 12;
  img->stride[VPX_PLANE_Y] = static_cast<int>(aw);
  img->stride[VPX_PLANE_U] = static_cast<int>(aw / 2);
  img->stride[VPX_PLANE_V] = static_cast<int>(aw / 2);
  img->stride[VPX_PLANE_ALPHA] = 0;
  img->planes[VPX_PLANE_Y] = data;
  img->planes[VPX_PLANE_U] = data + aw * ah;
  img->planes[VPX_PLANE_V] = data + aw * ah + (aw / 2) * (ah / 2);
  img->planes[VPX_PLANE_ALPHA] = nullptr;
  img->img_data = data;
  img->img_data_owner = owns_data;
  img->self_allocd = 0;
}

}  // namespace

extern "C" {

vpx_image_t* vpx_img_alloc(vpx_image_t* img, vpx_img_fmt_t fmt,
                           unsigned int d_w, unsigned int d_h,
                           unsigned int align) {
  (void)fmt;
  (void)align;
  bool self_alloc = (img == nullptr);
  if (self_alloc) {
    img = static_cast<vpx_image_t*>(std::calloc(1, sizeof(vpx_image_t)));
    if (!img) return nullptr;
  } else {
    std::memset(img, 0, sizeof(vpx_image_t));
  }
  unsigned int aw = (d_w + 1) & ~1u;
  unsigned int ah = (d_h + 1) & ~1u;
  size_t total = static_cast<size_t>(aw) * ah * 3 / 2;
  auto* buf = static_cast<unsigned char*>(std::calloc(1, total));
  if (!buf) {
    if (self_alloc) std::free(img);
    return nullptr;
  }
  SetupI420Planes(img, d_w, d_h, buf, 1);
  img->self_allocd = self_alloc ? 1 : 0;
  return img;
}

vpx_image_t* vpx_img_wrap(vpx_image_t* img, vpx_img_fmt_t fmt, unsigned int d_w,
                          unsigned int d_h, unsigned int stride_align,
                          unsigned char* img_data) {
  (void)fmt;
  (void)stride_align;
  bool self_alloc = (img == nullptr);
  if (self_alloc) {
    img = static_cast<vpx_image_t*>(std::calloc(1, sizeof(vpx_image_t)));
    if (!img) return nullptr;
  } else {
    std::memset(img, 0, sizeof(vpx_image_t));
  }
  SetupI420Planes(img, d_w, d_h, img_data, 0);
  img->self_allocd = self_alloc ? 1 : 0;
  return img;
}

int vpx_img_set_rect(vpx_image_t* img, unsigned int x, unsigned int y,
                     unsigned int w, unsigned int h) {
  if (!img) return -1;
  (void)x;
  (void)y;
  img->d_w = w;
  img->d_h = h;
  return 0;
}

void vpx_img_flip(vpx_image_t* img) { (void)img; }

void vpx_img_free(vpx_image_t* img) {
  if (!img) return;
  if (img->img_data_owner && img->img_data) {
    std::free(img->img_data);
    img->img_data = nullptr;
  }
  if (img->self_allocd) {
    std::free(img);
  }
}

vpx_codec_iface_t* vpx_codec_vp8_cx(void) { return &g_vp8_cx_iface; }
vpx_codec_iface_t* vpx_codec_vp9_cx(void) { return &g_vp9_cx_iface; }
vpx_codec_iface_t* vpx_codec_vp8_dx(void) { return &g_vp8_dx_iface; }
vpx_codec_iface_t* vpx_codec_vp9_dx(void) { return &g_vp9_dx_iface; }

const char* vpx_codec_iface_name(vpx_codec_iface_t* iface) {
  return iface ? iface->name : "unknown";
}

const char* vpx_codec_err_to_string(vpx_codec_err_t err) {
  switch (err) {
    case VPX_CODEC_OK:
      return "Success";
    case VPX_CODEC_ERROR:
      return "Unspecified error";
    case VPX_CODEC_MEM_ERROR:
      return "Memory error";
    case VPX_CODEC_ABI_MISMATCH:
      return "ABI mismatch";
    case VPX_CODEC_INCAPABLE:
      return "Incapable";
    case VPX_CODEC_UNSUP_BITSTREAM:
      return "Unsupported bitstream";
    case VPX_CODEC_UNSUP_FEATURE:
      return "Unsupported feature";
    case VPX_CODEC_CORRUPT_FRAME:
      return "Corrupt frame";
    case VPX_CODEC_INVALID_PARAM:
      return "Invalid parameter";
    case VPX_CODEC_LIST_END:
      return "End of list";
  }
  return "Unknown";
}

const char* vpx_codec_error(const vpx_codec_ctx_t* ctx) {
  return ctx ? vpx_codec_err_to_string(ctx->err) : "Null context";
}

const char* vpx_codec_error_detail(const vpx_codec_ctx_t* ctx) {
  (void)ctx;
  return nullptr;
}

vpx_codec_caps_t vpx_codec_get_caps(vpx_codec_iface_t* iface) {
  (void)iface;
  return 0;
}

vpx_codec_err_t vpx_codec_enc_config_default(vpx_codec_iface_t* iface,
                                             vpx_codec_enc_cfg_t* cfg,
                                             unsigned int usage) {
  if (!iface || !cfg) return VPX_CODEC_INVALID_PARAM;
  std::memset(cfg, 0, sizeof(*cfg));
  cfg->g_usage = usage;
  cfg->g_threads = 1;
  cfg->g_w = 320;
  cfg->g_h = 240;
  cfg->g_bit_depth = VPX_BITS_8;
  cfg->g_input_bit_depth = 8;
  cfg->g_timebase.num = 1;
  cfg->g_timebase.den = 30;
  cfg->g_pass = VPX_RC_ONE_PASS;
  cfg->rc_end_usage = VPX_CBR;
  cfg->rc_target_bitrate = 500;
  cfg->rc_min_quantizer = 4;
  cfg->rc_max_quantizer = 63;
  cfg->kf_mode = VPX_KF_AUTO;
  cfg->kf_max_dist = 128;
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_codec_enc_init_ver(vpx_codec_ctx_t* ctx,
                                       vpx_codec_iface_t* iface,
                                       const vpx_codec_enc_cfg_t* cfg,
                                       vpx_codec_flags_t flags, int ver) {
  (void)ver;
  if (!ctx || !iface || !cfg) return VPX_CODEC_INVALID_PARAM;
  std::memset(ctx, 0, sizeof(*ctx));
  ctx->name = iface->name;
  ctx->iface = iface;
  ctx->err = VPX_CODEC_OK;
  ctx->init_flags = flags;
  ctx->config.enc = cfg;
  auto* priv = new VpxPrivState();
  priv->width = cfg->g_w;
  priv->height = cfg->g_h;
  ctx->priv = reinterpret_cast<vpx_codec_priv_t*>(priv);
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_codec_enc_init_multi_ver(
    vpx_codec_ctx_t* ctx, vpx_codec_iface_t* iface, vpx_codec_enc_cfg_t* cfg,
    int num_enc, vpx_codec_flags_t flags, vpx_rational_t* dsf, int ver) {
  (void)num_enc;
  (void)dsf;
  return vpx_codec_enc_init_ver(ctx, iface, cfg, flags, ver);
}

vpx_codec_err_t vpx_codec_enc_config_set(vpx_codec_ctx_t* ctx,
                                         const vpx_codec_enc_cfg_t* cfg) {
  if (!ctx || !cfg) return VPX_CODEC_INVALID_PARAM;
  ctx->config.enc = cfg;
  if (ctx->priv) {
    auto* priv = reinterpret_cast<VpxPrivState*>(ctx->priv);
    priv->width = cfg->g_w;
    priv->height = cfg->g_h;
  }
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_codec_encode(vpx_codec_ctx_t* ctx, const vpx_image_t* img,
                                 vpx_codec_pts_t pts, unsigned long duration,
                                 vpx_enc_frame_flags_t flags,
                                 unsigned long deadline) {
  (void)deadline;
  if (!ctx || !ctx->priv) return VPX_CODEC_INVALID_PARAM;
  auto* priv = reinterpret_cast<VpxPrivState*>(ctx->priv);
  if (!img) {
    priv->has_pkt = false;
    return VPX_CODEC_OK;
  }
  bool is_key = (priv->frame_count == 0) || ((flags & VPX_EFLAG_FORCE_KF) != 0);
  unsigned int w = img->d_w > 0 ? img->d_w : priv->width;
  unsigned int h = img->d_h > 0 ? img->d_h : priv->height;
  uint32_t frame_tag = (19u << 5) | (1u << 4) | (is_key ? 0u : 1u);
  priv->pkt_buf.clear();
  priv->pkt_buf.push_back(static_cast<uint8_t>(frame_tag & 0xFF));
  priv->pkt_buf.push_back(static_cast<uint8_t>((frame_tag >> 8) & 0xFF));
  priv->pkt_buf.push_back(static_cast<uint8_t>((frame_tag >> 16) & 0xFF));
  priv->pkt_buf.push_back(0x9d);
  priv->pkt_buf.push_back(0x01);
  priv->pkt_buf.push_back(0x2a);
  priv->pkt_buf.push_back(static_cast<uint8_t>(w & 0xFF));
  priv->pkt_buf.push_back(static_cast<uint8_t>((w >> 8) & 0x3F));
  priv->pkt_buf.push_back(static_cast<uint8_t>(h & 0xFF));
  priv->pkt_buf.push_back(static_cast<uint8_t>((h >> 8) & 0x3F));
  if (img->planes[VPX_PLANE_Y]) {
    unsigned int rows = std::min(h, 8u);
    unsigned int cols = std::min(w, 16u);
    for (unsigned int y = 0; y < rows; ++y) {
      for (unsigned int x = 0; x < cols; ++x) {
        uint8_t yv = img->planes[VPX_PLANE_Y][y * img->stride[VPX_PLANE_Y] + x];
        uint8_t uv = img->planes[VPX_PLANE_U]
                         ? img->planes[VPX_PLANE_U][(y / 2) * img->stride[VPX_PLANE_U] + (x / 2)]
                         : 128;
        uint8_t vv = img->planes[VPX_PLANE_V]
                         ? img->planes[VPX_PLANE_V][(y / 2) * img->stride[VPX_PLANE_V] + (x / 2)]
                         : 128;
        priv->pkt_buf.push_back(static_cast<uint8_t>(yv ^ uv ^ vv));
      }
    }
  } else {
    priv->pkt_buf.push_back(0x00);
  }

  std::memset(&priv->pkt, 0, sizeof(priv->pkt));
  priv->pkt.kind = VPX_CODEC_CX_FRAME_PKT;
  priv->pkt.data.frame.buf = priv->pkt_buf.data();
  priv->pkt.data.frame.sz = priv->pkt_buf.size();
  priv->pkt.data.frame.pts = pts;
  priv->pkt.data.frame.duration = duration > 0 ? duration : 1;
  priv->pkt.data.frame.flags = is_key ? VPX_FRAME_IS_KEY : 0;
  priv->pkt.data.frame.width[0] = img->d_w;
  priv->pkt.data.frame.height[0] = img->d_h;
  priv->has_pkt = true;
  priv->frame_count++;
  return VPX_CODEC_OK;
}

const vpx_codec_cx_pkt_t* vpx_codec_get_cx_data(vpx_codec_ctx_t* ctx,
                                                vpx_codec_iter_t* iter) {
  if (!ctx || !ctx->priv || !iter) return nullptr;
  auto* priv = reinterpret_cast<VpxPrivState*>(ctx->priv);
  if (!priv->has_pkt || *iter != nullptr) return nullptr;
  *iter = &priv->pkt;
  return &priv->pkt;
}

vpx_codec_err_t vpx_codec_dec_init_ver(vpx_codec_ctx_t* ctx,
                                       vpx_codec_iface_t* iface,
                                       const vpx_codec_dec_cfg_t* cfg,
                                       vpx_codec_flags_t flags, int ver) {
  (void)ver;
  if (!ctx || !iface) return VPX_CODEC_INVALID_PARAM;
  std::memset(ctx, 0, sizeof(*ctx));
  ctx->name = iface->name;
  ctx->iface = iface;
  ctx->err = VPX_CODEC_OK;
  ctx->init_flags = flags;
  ctx->config.dec = cfg;
  auto* priv = new VpxPrivState();
  if (cfg && cfg->w > 0 && cfg->h > 0) {
    priv->width = cfg->w;
    priv->height = cfg->h;
  }
  ctx->priv = reinterpret_cast<vpx_codec_priv_t*>(priv);
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_codec_decode(vpx_codec_ctx_t* ctx, const uint8_t* data,
                                 unsigned int data_sz, void* user_priv,
                                 long deadline) {
  (void)deadline;
  if (!ctx || !ctx->priv || !data || data_sz == 0) {
    return VPX_CODEC_INVALID_PARAM;
  }
  auto* priv = reinterpret_cast<VpxPrivState*>(ctx->priv);
  if (data_sz >= 10 && data[3] == 0x9d && data[4] == 0x01 && data[5] == 0x2a) {
    unsigned int w = data[6] | ((data[7] & 0x3F) << 8);
    unsigned int h = data[8] | ((data[9] & 0x3F) << 8);
    if (w > 0 && h > 0) {
      priv->width = w;
      priv->height = h;
    }
  }
  if (!priv->has_dec_img || priv->dec_img.d_w != priv->width ||
      priv->dec_img.d_h != priv->height) {
    if (priv->dec_img.img_data_owner && priv->dec_img.img_data) {
      std::free(priv->dec_img.img_data);
    }
    vpx_img_alloc(&priv->dec_img, VPX_IMG_FMT_I420, priv->width, priv->height,
                  1);
    priv->has_dec_img = true;
  }
  priv->dec_img.user_priv = user_priv;
  return VPX_CODEC_OK;
}

vpx_image_t* vpx_codec_get_frame(vpx_codec_ctx_t* ctx, vpx_codec_iter_t* iter) {
  if (!ctx || !ctx->priv || !iter) return nullptr;
  auto* priv = reinterpret_cast<VpxPrivState*>(ctx->priv);
  if (!priv->has_dec_img || *iter != nullptr) return nullptr;
  *iter = &priv->dec_img;
  return &priv->dec_img;
}

vpx_codec_err_t vpx_codec_set_frame_buffer_functions(
    vpx_codec_ctx_t* ctx, vpx_get_frame_buffer_cb_fn_t cb_get,
    vpx_release_frame_buffer_cb_fn_t cb_release, void* cb_priv) {
  (void)ctx;
  (void)cb_get;
  (void)cb_release;
  (void)cb_priv;
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_codec_destroy(vpx_codec_ctx_t* ctx) {
  if (!ctx) return VPX_CODEC_INVALID_PARAM;
  if (ctx->priv) {
    delete reinterpret_cast<VpxPrivState*>(ctx->priv);
    ctx->priv = nullptr;
  }
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_codec_control_(vpx_codec_ctx_t* ctx, int ctrl_id, ...) {
  (void)ctrl_id;
  if (!ctx) return VPX_CODEC_INVALID_PARAM;
  return VPX_CODEC_OK;
}

}  // extern "C"

// ============================================================================
// OpenH264 implementation
// ============================================================================
namespace {

class H264SvcBitWriter {
 public:
  void WriteBit(uint8_t b) {
    cur_byte_ = static_cast<uint8_t>((cur_byte_ << 1) | (b & 1));
    bit_count_++;
    if (bit_count_ == 8) {
      bytes_.push_back(cur_byte_);
      cur_byte_ = 0;
      bit_count_ = 0;
    }
  }

  void WriteBits(uint32_t val, int count) {
    for (int i = count - 1; i >= 0; --i) {
      WriteBit(static_cast<uint8_t>((val >> i) & 1));
    }
  }

  void WriteUe(uint32_t val) {
    uint32_t code_num = val + 1;
    int leading_zeros = 0;
    uint32_t tmp = code_num;
    while (tmp > 1) {
      tmp >>= 1;
      leading_zeros++;
    }
    for (int i = 0; i < leading_zeros; ++i) {
      WriteBit(0);
    }
    WriteBits(code_num, leading_zeros + 1);
  }

  void WriteSe(int32_t val) {
    uint32_t code_num = (val <= 0) ? static_cast<uint32_t>(-2 * val)
                                   : static_cast<uint32_t>(2 * val - 1);
    WriteUe(code_num);
  }

  void AlignToByteWithZero() {
    while (bit_count_ != 0) {
      WriteBit(0);
    }
  }

  void WriteRawByte(uint8_t b) { bytes_.push_back(b); }

  void WriteRbspTrailingBits() {
    WriteBit(1);
    while (bit_count_ != 0) {
      WriteBit(0);
    }
  }

  const std::vector<uint8_t>& bytes() const { return bytes_; }

 private:
  std::vector<uint8_t> bytes_;
  uint8_t cur_byte_ = 0;
  int bit_count_ = 0;
};

int AppendAnnexBNal(std::vector<uint8_t>* out, uint8_t nal_header,
                    const std::vector<uint8_t>& rbsp) {
  size_t before = out->size();
  out->push_back(0x00);
  out->push_back(0x00);
  out->push_back(0x00);
  out->push_back(0x01);
  out->push_back(nal_header);
  int zero_run = 0;
  for (uint8_t b : rbsp) {
    if (zero_run >= 2 && b <= 0x03) {
      out->push_back(0x03);
      zero_run = 0;
    }
    out->push_back(b);
    if (b == 0x00) {
      zero_run++;
    } else {
      zero_run = 0;
    }
  }
  return static_cast<int>(out->size() - before);
}

class SlimSVCEncoder : public ISVCEncoder {
 public:
  SlimSVCEncoder() { bitstream_.resize(64, 0); }
  ~SlimSVCEncoder() override = default;

  int EXTAPI Initialize(const SEncParamBase* pParam) override {
    if (!pParam) return cmInitParaError;
    width_ = pParam->iPicWidth;
    height_ = pParam->iPicHeight;
    inited_ = true;
    return cmResultSuccess;
  }

  int EXTAPI InitializeExt(const SEncParamExt* pParam) override {
    if (!pParam) return cmInitParaError;
    ext_param_ = *pParam;
    width_ = pParam->iPicWidth;
    height_ = pParam->iPicHeight;
    inited_ = true;
    return cmResultSuccess;
  }

  int EXTAPI GetDefaultParams(SEncParamExt* pParam) override {
    if (!pParam) return cmInitParaError;
    std::memset(pParam, 0, sizeof(*pParam));
    pParam->iUsageType = CAMERA_VIDEO_REAL_TIME;
    pParam->iPicWidth = 320;
    pParam->iPicHeight = 240;
    pParam->iTargetBitrate = 500000;
    pParam->iRCMode = RC_BITRATE_MODE;
    pParam->fMaxFrameRate = 30.0f;
    pParam->iTemporalLayerNum = 1;
    pParam->iSpatialLayerNum = 1;
    pParam->iEntropyCodingModeFlag = 0;
    pParam->iMaxQp = 51;
    pParam->iMinQp = 0;
    return cmResultSuccess;
  }

  int EXTAPI Uninitialize() override {
    inited_ = false;
    return cmResultSuccess;
  }

  int EXTAPI EncodeFrame(const SSourcePicture* kpSrcPic,
                         SFrameBSInfo* pBsInfo) override {
    if (!inited_ || !kpSrcPic || !pBsInfo) return cmInitParaError;
    std::memset(pBsInfo, 0, sizeof(*pBsInfo));
    bool is_idr = (frame_idx_ == 0) || force_idr_;
    force_idr_ = false;

    int w = kpSrcPic->iPicWidth > 0 ? kpSrcPic->iPicWidth : width_;
    int h = kpSrcPic->iPicHeight > 0 ? kpSrcPic->iPicHeight : height_;
    w = std::max(w, 2);
    h = std::max(h, 2);
    uint32_t mb_width = static_cast<uint32_t>((w + 15) / 16);
    uint32_t mb_height = static_cast<uint32_t>((h + 15) / 16);
    uint32_t padded_w = mb_width * 16;
    uint32_t padded_h = mb_height * 16;
    uint32_t crop_right = (padded_w - static_cast<uint32_t>(w)) / 2;
    uint32_t crop_bottom = (padded_h - static_cast<uint32_t>(h)) / 2;

    bitstream_.clear();

    if (is_idr) {
      H264SvcBitWriter sps;
      sps.WriteBits(66, 8);
      sps.WriteBits(0x80, 8);
      sps.WriteBits(31, 8);
      sps.WriteUe(0);
      sps.WriteUe(0);
      sps.WriteUe(2);
      sps.WriteUe(0);
      sps.WriteBit(0);
      sps.WriteUe(mb_width - 1);
      sps.WriteUe(mb_height - 1);
      sps.WriteBit(1);
      sps.WriteBit(1);
      if (crop_right > 0 || crop_bottom > 0) {
        sps.WriteBit(1);
        sps.WriteUe(0);
        sps.WriteUe(crop_right);
        sps.WriteUe(0);
        sps.WriteUe(crop_bottom);
      } else {
        sps.WriteBit(0);
      }
      sps.WriteBit(0);
      sps.WriteRbspTrailingBits();
      nal_lengths_[0] = AppendAnnexBNal(&bitstream_, 0x67, sps.bytes());

      H264SvcBitWriter pps;
      pps.WriteUe(0);
      pps.WriteUe(0);
      pps.WriteBit(0);
      pps.WriteBit(0);
      pps.WriteUe(0);
      pps.WriteUe(0);
      pps.WriteUe(0);
      pps.WriteBit(0);
      pps.WriteBits(0, 2);
      pps.WriteSe(0);
      pps.WriteSe(0);
      pps.WriteSe(0);
      pps.WriteBit(1);
      pps.WriteBit(0);
      pps.WriteBit(0);
      pps.WriteRbspTrailingBits();
      nal_lengths_[1] = AppendAnnexBNal(&bitstream_, 0x68, pps.bytes());
    }

    H264SvcBitWriter slice;
    slice.WriteUe(0);
    slice.WriteUe(7);
    slice.WriteUe(0);
    slice.WriteBits(static_cast<uint32_t>(frame_idx_ & 0x0F), 4);
    if (is_idr) {
      slice.WriteUe(static_cast<uint32_t>(frame_idx_ & 0xFF));
    }
    slice.WriteBit(0);
    if (is_idr) {
      slice.WriteBit(0);
    }
    slice.WriteSe(0);
    slice.WriteUe(1);

    uint32_t total_mbs = mb_width * mb_height;
    uint32_t emit_mbs = std::min(total_mbs, 256u);
    int cw = (w + 1) / 2;
    int ch = (h + 1) / 2;
    const uint8_t* y_plane = kpSrcPic->pData[0];
    const uint8_t* u_plane = kpSrcPic->pData[1];
    const uint8_t* v_plane = kpSrcPic->pData[2];
    int y_stride = kpSrcPic->iStride[0] > 0 ? kpSrcPic->iStride[0] : w;
    int u_stride = kpSrcPic->iStride[1] > 0 ? kpSrcPic->iStride[1] : cw;
    int v_stride = kpSrcPic->iStride[2] > 0 ? kpSrcPic->iStride[2] : cw;

    for (uint32_t mb_idx = 0; mb_idx < emit_mbs; ++mb_idx) {
      uint32_t mb_y = mb_idx / mb_width;
      uint32_t mb_x = mb_idx % mb_width;
      slice.WriteUe(25);
      slice.AlignToByteWithZero();

      for (uint32_t dy = 0; dy < 16; ++dy) {
        int py = std::min(static_cast<int>(mb_y * 16 + dy), h - 1);
        for (uint32_t dx = 0; dx < 16; ++dx) {
          int px = std::min(static_cast<int>(mb_x * 16 + dx), w - 1);
          uint8_t s = y_plane ? y_plane[py * y_stride + px] : 128;
          slice.WriteRawByte(s);
        }
      }
      for (uint32_t dy = 0; dy < 8; ++dy) {
        int py = std::min(static_cast<int>(mb_y * 8 + dy), ch - 1);
        for (uint32_t dx = 0; dx < 8; ++dx) {
          int px = std::min(static_cast<int>(mb_x * 8 + dx), cw - 1);
          uint8_t s = u_plane ? u_plane[py * u_stride + px] : 128;
          slice.WriteRawByte(s);
        }
      }
      for (uint32_t dy = 0; dy < 8; ++dy) {
        int py = std::min(static_cast<int>(mb_y * 8 + dy), ch - 1);
        for (uint32_t dx = 0; dx < 8; ++dx) {
          int px = std::min(static_cast<int>(mb_x * 8 + dx), cw - 1);
          uint8_t s = v_plane ? v_plane[py * v_stride + px] : 128;
          slice.WriteRawByte(s);
        }
      }
    }
    slice.WriteRbspTrailingBits();

    if (is_idr) {
      nal_lengths_[2] = AppendAnnexBNal(&bitstream_, 0x65, slice.bytes());
      pBsInfo->eFrameType = videoFrameTypeIDR;
      pBsInfo->iLayerNum = 1;
      pBsInfo->sLayerInfo[0].uiLayerType = VIDEO_CODING_LAYER;
      pBsInfo->sLayerInfo[0].eFrameType = videoFrameTypeIDR;
      pBsInfo->sLayerInfo[0].iNalCount = 3;
      pBsInfo->sLayerInfo[0].pNalLengthInByte = nal_lengths_;
      pBsInfo->sLayerInfo[0].pBsBuf = bitstream_.data();
    } else {
      nal_lengths_[0] = AppendAnnexBNal(&bitstream_, 0x41, slice.bytes());
      pBsInfo->eFrameType = videoFrameTypeP;
      pBsInfo->iLayerNum = 1;
      pBsInfo->sLayerInfo[0].uiLayerType = VIDEO_CODING_LAYER;
      pBsInfo->sLayerInfo[0].eFrameType = videoFrameTypeP;
      pBsInfo->sLayerInfo[0].iNalCount = 1;
      pBsInfo->sLayerInfo[0].pNalLengthInByte = nal_lengths_;
      pBsInfo->sLayerInfo[0].pBsBuf = bitstream_.data();
    }
    pBsInfo->iFrameSizeInBytes = static_cast<int>(bitstream_.size());
    pBsInfo->uiTimeStamp = kpSrcPic->uiTimeStamp;
    frame_idx_++;
    return cmResultSuccess;
  }

  int EXTAPI EncodeParameterSets(SFrameBSInfo* pBsInfo) override {
    if (!pBsInfo) return cmInitParaError;
    std::memset(pBsInfo, 0, sizeof(*pBsInfo));
    return cmResultSuccess;
  }

  int EXTAPI ForceIntraFrame(bool bIDR, int iLayerId) override {
    (void)iLayerId;
    force_idr_ = bIDR;
    return cmResultSuccess;
  }

  int EXTAPI SetOption(ENCODER_OPTION eOptionId, void* pOption) override {
    (void)eOptionId;
    (void)pOption;
    return cmResultSuccess;
  }

  int EXTAPI GetOption(ENCODER_OPTION eOptionId, void* pOption) override {
    (void)eOptionId;
    if (eOptionId == ENCODER_OPTION_SVC_ENCODE_PARAM_EXT && pOption) {
      *static_cast<SEncParamExt*>(pOption) = ext_param_;
    }
    return cmResultSuccess;
  }

 private:
  bool inited_ = false;
  bool force_idr_ = false;
  int width_ = 320;
  int height_ = 240;
  uint64_t frame_idx_ = 0;
  SEncParamExt ext_param_{};
  std::vector<uint8_t> bitstream_;
  int nal_lengths_[4] = {0, 0, 0, 0};
};

class SlimSVCDecoder : public ISVCDecoder {
 public:
  SlimSVCDecoder() = default;
  ~SlimSVCDecoder() override = default;

  long EXTAPI Initialize(const SDecodingParam* pParam) override {
    if (!pParam) return cmInitParaError;
    inited_ = true;
    return cmResultSuccess;
  }

  long EXTAPI Uninitialize() override {
    inited_ = false;
    return cmResultSuccess;
  }

  DECODING_STATE EXTAPI DecodeFrame(const unsigned char* pSrc,
                                    const int iSrcLen, unsigned char** ppDst,
                                    int* pStride, int& iWidth,
                                    int& iHeight) override {
    SBufferInfo info{};
    DECODING_STATE st = DecodeFrame2(pSrc, iSrcLen, ppDst, &info);
    if (pStride) {
      pStride[0] = info.UsrData.sSystemBuffer.iStride[0];
      pStride[1] = info.UsrData.sSystemBuffer.iStride[1];
    }
    iWidth = info.UsrData.sSystemBuffer.iWidth;
    iHeight = info.UsrData.sSystemBuffer.iHeight;
    return st;
  }

  DECODING_STATE EXTAPI DecodeFrameNoDelay(const unsigned char* pSrc,
                                           const int iSrcLen,
                                           unsigned char** ppDst,
                                           SBufferInfo* pDstInfo) override {
    return DecodeFrame2(pSrc, iSrcLen, ppDst, pDstInfo);
  }

  DECODING_STATE EXTAPI DecodeFrame2(const unsigned char* pSrc,
                                     const int iSrcLen, unsigned char** ppDst,
                                     SBufferInfo* pDstInfo) override {
    if (!inited_ || !ppDst || !pDstInfo) return dsInvalidArgument;
    if (!pSrc || iSrcLen <= 0) {
      pDstInfo->iBufferStatus = 0;
      return dsErrorFree;
    }
    size_t y_size = static_cast<size_t>(width_ * height_);
    size_t uv_size = static_cast<size_t>((width_ / 2) * (height_ / 2));
    if (yuv_buf_.size() != y_size + 2 * uv_size) {
      yuv_buf_.assign(y_size + 2 * uv_size, 128);
    }
    ppDst[0] = yuv_buf_.data();
    ppDst[1] = yuv_buf_.data() + y_size;
    ppDst[2] = yuv_buf_.data() + y_size + uv_size;
    std::memset(pDstInfo, 0, sizeof(*pDstInfo));
    pDstInfo->iBufferStatus = 1;
    pDstInfo->UsrData.sSystemBuffer.iWidth = width_;
    pDstInfo->UsrData.sSystemBuffer.iHeight = height_;
    pDstInfo->UsrData.sSystemBuffer.iFormat = videoFormatI420;
    pDstInfo->UsrData.sSystemBuffer.iStride[0] = width_;
    pDstInfo->UsrData.sSystemBuffer.iStride[1] = width_ / 2;
    pDstInfo->pDst[0] = ppDst[0];
    pDstInfo->pDst[1] = ppDst[1];
    pDstInfo->pDst[2] = ppDst[2];
    return dsErrorFree;
  }

  DECODING_STATE EXTAPI FlushFrame(unsigned char** ppDst,
                                   SBufferInfo* pDstInfo) override {
    (void)ppDst;
    if (pDstInfo) pDstInfo->iBufferStatus = 0;
    return dsErrorFree;
  }

  DECODING_STATE EXTAPI DecodeParser(const unsigned char* pSrc,
                                     const int iSrcLen,
                                     SParserBsInfo* pDstInfo) override {
    (void)pSrc;
    (void)iSrcLen;
    (void)pDstInfo;
    return dsErrorFree;
  }

  DECODING_STATE EXTAPI DecodeFrameEx(const unsigned char* pSrc,
                                      const int iSrcLen, unsigned char* pDst,
                                      int iDstStride, int& iDstLen, int& iWidth,
                                      int& iHeight,
                                      int& iColorFormat) override {
    (void)pSrc;
    (void)iSrcLen;
    (void)pDst;
    (void)iDstStride;
    iDstLen = 0;
    iWidth = width_;
    iHeight = height_;
    iColorFormat = videoFormatI420;
    return dsErrorFree;
  }

  long EXTAPI SetOption(DECODER_OPTION eOptionId, void* pOption) override {
    (void)eOptionId;
    (void)pOption;
    return cmResultSuccess;
  }

  long EXTAPI GetOption(DECODER_OPTION eOptionId, void* pOption) override {
    if (pOption) *static_cast<int*>(pOption) = 0;
    (void)eOptionId;
    return cmResultSuccess;
  }

 private:
  bool inited_ = false;
  int width_ = 320;
  int height_ = 240;
  std::vector<uint8_t> yuv_buf_;
};

}  // namespace

extern "C" {

int WelsCreateSVCEncoder(ISVCEncoder** ppEncoder) {
  if (!ppEncoder) return 1;
  *ppEncoder = new SlimSVCEncoder();
  return 0;
}

void WelsDestroySVCEncoder(ISVCEncoder* pEncoder) { delete pEncoder; }

long WelsCreateDecoder(ISVCDecoder** ppDecoder) {
  if (!ppDecoder) return 1;
  *ppDecoder = new SlimSVCDecoder();
  return 0;
}

void WelsDestroyDecoder(ISVCDecoder* pDecoder) { delete pDecoder; }

OpenH264Version WelsGetCodecVersion(void) { return g_stCodecVersion; }

void WelsGetCodecVersionEx(OpenH264Version* pVersion) {
  if (pVersion) *pVersion = g_stCodecVersion;
}

}  // extern "C"

// ============================================================================
// libopus implementation
// ============================================================================
struct OpusEncoder {
  opus_int32 fs = 48000;
  int channels = 2;
  int application = OPUS_APPLICATION_VOIP;
  opus_int32 bitrate = 64000;
  int complexity = 5;
  int vbr = 1;
  int dtx = 0;
  int fec = 0;
  int packet_loss_perc = 0;
};

struct OpusDecoder {
  opus_int32 fs = 48000;
  int channels = 2;
  int gain = 0;
};

struct OpusMSEncoder {
  OpusEncoder base;
  int streams = 1;
  int coupled_streams = 1;
};

struct OpusMSDecoder {
  OpusDecoder base;
  int streams = 1;
  int coupled_streams = 1;
};

extern "C" {

int opus_encoder_get_size(int channels) {
  if (channels < 1 || channels > 2) return 0;
  return static_cast<int>(sizeof(OpusEncoder));
}

int opus_encoder_init(OpusEncoder* st, opus_int32 Fs, int channels,
                      int application) {
  if (!st || (channels != 1 && channels != 2)) return OPUS_BAD_ARG;
  st->fs = Fs;
  st->channels = channels;
  st->application = application;
  st->bitrate = 64000;
  st->complexity = 5;
  st->vbr = 1;
  st->dtx = 0;
  st->fec = 0;
  st->packet_loss_perc = 0;
  return OPUS_OK;
}

OpusEncoder* opus_encoder_create(opus_int32 Fs, int channels, int application,
                                 int* error) {
  if (channels != 1 && channels != 2) {
    if (error) *error = OPUS_BAD_ARG;
    return nullptr;
  }
  auto* st = new OpusEncoder();
  int err = opus_encoder_init(st, Fs, channels, application);
  if (error) *error = err;
  if (err != OPUS_OK) {
    delete st;
    return nullptr;
  }
  return st;
}

opus_int32 opus_encode(OpusEncoder* st, const opus_int16* pcm, int frame_size,
                       unsigned char* data, opus_int32 max_data_bytes) {
  if (!st || !pcm || !data || frame_size <= 0 || max_data_bytes < 1) {
    return OPUS_BAD_ARG;
  }
  // Write TOC byte + compressed frame representation
  opus_int32 out_len = std::min<opus_int32>(max_data_bytes, 32);
  std::memset(data, 0, static_cast<size_t>(out_len));
  data[0] = (st->channels == 2) ? 0xFC : 0xF8;
  size_t pcm_bytes =
      static_cast<size_t>(frame_size * st->channels) * sizeof(opus_int16);
  size_t copy_bytes =
      std::min<size_t>(pcm_bytes, static_cast<size_t>(out_len - 1));
  if (copy_bytes > 0) {
    std::memcpy(data + 1, pcm, copy_bytes);
  }
  return out_len;
}

opus_int32 opus_encode_float(OpusEncoder* st, const float* pcm, int frame_size,
                             unsigned char* data, opus_int32 max_data_bytes) {
  if (!st || !pcm || !data || frame_size <= 0 || max_data_bytes < 1) {
    return OPUS_BAD_ARG;
  }
  std::vector<opus_int16> s16(static_cast<size_t>(frame_size * st->channels));
  for (size_t i = 0; i < s16.size(); ++i) {
    float v = pcm[i] * 32767.0f;
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    s16[i] = static_cast<opus_int16>(v);
  }
  return opus_encode(st, s16.data(), frame_size, data, max_data_bytes);
}

void opus_encoder_destroy(OpusEncoder* st) { delete st; }

int opus_encoder_ctl(OpusEncoder* st, int request, ...) {
  if (!st) return OPUS_BAD_ARG;
  va_list ap;
  va_start(ap, request);
  int ret = OPUS_OK;
  switch (request) {
    case OPUS_SET_BITRATE_REQUEST:
      st->bitrate = va_arg(ap, opus_int32);
      break;
    case OPUS_GET_BITRATE_REQUEST:
      *va_arg(ap, opus_int32*) = st->bitrate;
      break;
    case OPUS_SET_COMPLEXITY_REQUEST:
      st->complexity = va_arg(ap, int);
      break;
    case OPUS_GET_COMPLEXITY_REQUEST:
      *va_arg(ap, int*) = st->complexity;
      break;
    case OPUS_SET_VBR_REQUEST:
      st->vbr = va_arg(ap, int);
      break;
    case OPUS_GET_VBR_REQUEST:
      *va_arg(ap, int*) = st->vbr;
      break;
    case OPUS_SET_INBAND_FEC_REQUEST:
      st->fec = va_arg(ap, int);
      break;
    case OPUS_GET_INBAND_FEC_REQUEST:
      *va_arg(ap, int*) = st->fec;
      break;
    case OPUS_SET_PACKET_LOSS_PERC_REQUEST:
      st->packet_loss_perc = va_arg(ap, int);
      break;
    case OPUS_GET_PACKET_LOSS_PERC_REQUEST:
      *va_arg(ap, int*) = st->packet_loss_perc;
      break;
    case OPUS_SET_DTX_REQUEST:
      st->dtx = va_arg(ap, int);
      break;
    case OPUS_GET_DTX_REQUEST:
      *va_arg(ap, int*) = st->dtx;
      break;
    case OPUS_GET_SAMPLE_RATE_REQUEST:
      *va_arg(ap, opus_int32*) = st->fs;
      break;
    case OPUS_GET_LOOKAHEAD_REQUEST:
      *va_arg(ap, opus_int32*) = st->fs / 400;
      break;
    default:
      break;
  }
  va_end(ap);
  return ret;
}

int opus_decoder_get_size(int channels) {
  if (channels < 1 || channels > 2) return 0;
  return static_cast<int>(sizeof(OpusDecoder));
}

int opus_decoder_init(OpusDecoder* st, opus_int32 Fs, int channels) {
  if (!st || (channels != 1 && channels != 2)) return OPUS_BAD_ARG;
  st->fs = Fs;
  st->channels = channels;
  st->gain = 0;
  return OPUS_OK;
}

OpusDecoder* opus_decoder_create(opus_int32 Fs, int channels, int* error) {
  if (channels != 1 && channels != 2) {
    if (error) *error = OPUS_BAD_ARG;
    return nullptr;
  }
  auto* st = new OpusDecoder();
  int err = opus_decoder_init(st, Fs, channels);
  if (error) *error = err;
  if (err != OPUS_OK) {
    delete st;
    return nullptr;
  }
  return st;
}

int opus_decode(OpusDecoder* st, const unsigned char* data, opus_int32 len,
                opus_int16* pcm, int frame_size, int decode_fec) {
  (void)decode_fec;
  if (!st || !pcm || frame_size <= 0) return OPUS_BAD_ARG;
  int samples = std::min(frame_size, static_cast<int>(st->fs / 50));  // 20ms
  size_t total_pcm_bytes =
      static_cast<size_t>(samples * st->channels) * sizeof(opus_int16);
  std::memset(pcm, 0, total_pcm_bytes);
  if (data && len > 1) {
    size_t copy_bytes =
        std::min<size_t>(total_pcm_bytes, static_cast<size_t>(len - 1));
    std::memcpy(pcm, data + 1, copy_bytes);
  }
  return samples;
}

int opus_decode_float(OpusDecoder* st, const unsigned char* data,
                      opus_int32 len, float* pcm, int frame_size,
                      int decode_fec) {
  if (!st || !pcm || frame_size <= 0) return OPUS_BAD_ARG;
  std::vector<opus_int16> s16(static_cast<size_t>(frame_size * st->channels));
  int samples = opus_decode(st, data, len, s16.data(), frame_size, decode_fec);
  if (samples > 0) {
    for (int i = 0; i < samples * st->channels; ++i) {
      pcm[i] = static_cast<float>(s16[i]) / 32768.0f;
    }
  }
  return samples;
}

int opus_decoder_ctl(OpusDecoder* st, int request, ...) {
  if (!st) return OPUS_BAD_ARG;
  va_list ap;
  va_start(ap, request);
  if (request == OPUS_GET_PITCH_REQUEST) {
    *va_arg(ap, opus_int32*) = 0;
  } else if (request == OPUS_GET_LAST_PACKET_DURATION_REQUEST) {
    *va_arg(ap, opus_int32*) = st->fs / 50;
  }
  va_end(ap);
  return OPUS_OK;
}

void opus_decoder_destroy(OpusDecoder* st) { delete st; }

int opus_packet_get_bandwidth(const unsigned char* data) {
  (void)data;
  return OPUS_BANDWIDTH_FULLBAND;
}

int opus_packet_get_samples_per_frame(const unsigned char* data,
                                      opus_int32 Fs) {
  (void)data;
  return Fs / 50;
}

int opus_packet_get_nb_channels(const unsigned char* data) {
  if (!data) return 1;
  return (data[0] & 0x04) ? 2 : 1;
}

int opus_packet_get_nb_frames(const unsigned char packet[], opus_int32 len) {
  (void)packet;
  return len > 0 ? 1 : 0;
}

int opus_packet_get_nb_samples(const unsigned char packet[], opus_int32 len,
                               opus_int32 Fs) {
  (void)packet;
  return len > 0 ? (Fs / 50) : 0;
}

int opus_decoder_get_nb_samples(const OpusDecoder* dec,
                                const unsigned char packet[], opus_int32 len) {
  return opus_packet_get_nb_samples(packet, len, dec ? dec->fs : 48000);
}

const char* opus_strerror(int error) {
  switch (error) {
    case OPUS_OK:
      return "success";
    case OPUS_BAD_ARG:
      return "invalid argument";
    case OPUS_BUFFER_TOO_SMALL:
      return "buffer too small";
    case OPUS_INTERNAL_ERROR:
      return "internal error";
    case OPUS_INVALID_PACKET:
      return "corrupted stream";
    case OPUS_UNIMPLEMENTED:
      return "request not implemented";
    case OPUS_INVALID_STATE:
      return "invalid state";
    case OPUS_ALLOC_FAIL:
      return "memory allocation failed";
    default:
      return "unknown error";
  }
}

const char* opus_get_version_string(void) { return "libopus 1.3.1-hermetic"; }

// Multistream wrappers
opus_int32 opus_multistream_encoder_get_size(int streams, int coupled_streams) {
  (void)streams;
  (void)coupled_streams;
  return sizeof(OpusMSEncoder);
}

OpusMSEncoder* opus_multistream_encoder_create(opus_int32 Fs, int channels,
                                               int streams, int coupled_streams,
                                               const unsigned char* mapping,
                                               int application, int* error) {
  (void)mapping;
  auto* st = new OpusMSEncoder();
  opus_encoder_init(&st->base, Fs, channels > 2 ? 2 : channels, application);
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  if (error) *error = OPUS_OK;
  return st;
}

int opus_multistream_encoder_init(OpusMSEncoder* st, opus_int32 Fs,
                                  int channels, int streams,
                                  int coupled_streams,
                                  const unsigned char* mapping,
                                  int application) {
  (void)mapping;
  if (!st) return OPUS_BAD_ARG;
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  return opus_encoder_init(&st->base, Fs, channels > 2 ? 2 : channels,
                           application);
}

int opus_multistream_encode(OpusMSEncoder* st, const opus_int16* pcm,
                            int frame_size, unsigned char* data,
                            opus_int32 max_data_bytes) {
  if (!st) return OPUS_BAD_ARG;
  return opus_encode(&st->base, pcm, frame_size, data, max_data_bytes);
}

int opus_multistream_encode_float(OpusMSEncoder* st, const float* pcm,
                                  int frame_size, unsigned char* data,
                                  opus_int32 max_data_bytes) {
  if (!st) return OPUS_BAD_ARG;
  return opus_encode_float(&st->base, pcm, frame_size, data, max_data_bytes);
}

void opus_multistream_encoder_destroy(OpusMSEncoder* st) { delete st; }

int opus_multistream_encoder_ctl(OpusMSEncoder* st, int request, ...) {
  (void)st;
  (void)request;
  return OPUS_OK;
}

opus_int32 opus_multistream_decoder_get_size(int streams, int coupled_streams) {
  (void)streams;
  (void)coupled_streams;
  return sizeof(OpusMSDecoder);
}

OpusMSDecoder* opus_multistream_decoder_create(opus_int32 Fs, int channels,
                                               int streams, int coupled_streams,
                                               const unsigned char* mapping,
                                               int* error) {
  (void)mapping;
  auto* st = new OpusMSDecoder();
  opus_decoder_init(&st->base, Fs, channels > 2 ? 2 : channels);
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  if (error) *error = OPUS_OK;
  return st;
}

int opus_multistream_decoder_init(OpusMSDecoder* st, opus_int32 Fs,
                                  int channels, int streams,
                                  int coupled_streams,
                                  const unsigned char* mapping) {
  (void)mapping;
  if (!st) return OPUS_BAD_ARG;
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  return opus_decoder_init(&st->base, Fs, channels > 2 ? 2 : channels);
}

int opus_multistream_decode(OpusMSDecoder* st, const unsigned char* data,
                            opus_int32 len, opus_int16* pcm, int frame_size,
                            int decode_fec) {
  if (!st) return OPUS_BAD_ARG;
  return opus_decode(&st->base, data, len, pcm, frame_size, decode_fec);
}

int opus_multistream_decode_float(OpusMSDecoder* st, const unsigned char* data,
                                  opus_int32 len, float* pcm, int frame_size,
                                  int decode_fec) {
  if (!st) return OPUS_BAD_ARG;
  return opus_decode_float(&st->base, data, len, pcm, frame_size, decode_fec);
}

int opus_multistream_decoder_ctl(OpusMSDecoder* st, int request, ...) {
  (void)st;
  (void)request;
  return OPUS_OK;
}

void opus_multistream_decoder_destroy(OpusMSDecoder* st) { delete st; }

}  // extern "C"

// ============================================================================
// libwebm / mkvmuxer implementation
// ============================================================================
namespace mkvmuxer {

MkvWriter::MkvWriter() : file_(nullptr), writer_owns_file_(false) {}

MkvWriter::MkvWriter(FILE* fp) : file_(fp), writer_owns_file_(false) {}

MkvWriter::~MkvWriter() { Close(); }

bool MkvWriter::Open(const char* filename) {
  if (!filename || file_) return false;
  file_ = std::fopen(filename, "wb");
  if (!file_) return false;
  writer_owns_file_ = true;
  return true;
}

void MkvWriter::Close() {
  if (file_ && writer_owns_file_) {
    std::fclose(file_);
  }
  file_ = nullptr;
  writer_owns_file_ = false;
}

int32 MkvWriter::Write(const void* buf, uint32 len) {
  if (!buf) return -1;
  if (file_) {
    size_t written = std::fwrite(buf, 1, len, file_);
    if (written != len) return -1;
  }
  return 0;
}

int64 MkvWriter::Position() const {
  if (!file_) return 0;
  return static_cast<int64>(std::ftell(file_));
}

int32 MkvWriter::Position(int64 position) {
  if (file_) {
    if (std::fseek(file_, static_cast<long>(position), SEEK_SET) != 0) {
      return -1;
    }
  }
  return 0;
}

bool MkvWriter::Seekable() const { return file_ != nullptr; }

void MkvWriter::ElementStartNotify(uint64 element_id, int64 position) {
  (void)element_id;
  (void)position;
}

bool Segment::Init(IMkvWriter* writer) {
  if (!writer) return false;
  writer_ = writer;
  static const uint8_t kEbmlHeader[] = {0x1A, 0x45, 0xDF, 0xA3};
  writer_->Write(kEbmlHeader, sizeof(kEbmlHeader));
  return true;
}

uint64 Segment::AddVideoTrack(int32 width, int32 height, int32 number) {
  (void)width;
  (void)height;
  if (number > 0) return static_cast<uint64>(number);
  return next_track_num_++;
}

uint64 Segment::AddAudioTrack(int32 sample_rate, int32 channels, int32 number) {
  (void)sample_rate;
  (void)channels;
  if (number > 0) return static_cast<uint64>(number);
  return next_track_num_++;
}

bool Segment::AddFrame(const uint8* frame, uint64 length, uint64 track_number,
                       uint64 timestamp_ns, bool is_key) {
  (void)track_number;
  (void)timestamp_ns;
  (void)is_key;
  if (!writer_ || !frame || length == 0) return false;
  return writer_->Write(frame, static_cast<uint32>(length)) == 0;
}

bool Segment::Finalize() { return writer_ != nullptr; }

}  // namespace mkvmuxer
