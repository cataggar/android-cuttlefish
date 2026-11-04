#pragma once

#include <cstdint>

#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class I420BufferInterface;
class I420ABufferInterface;
class I422BufferInterface;
class I444BufferInterface;
class I010BufferInterface;
class I210BufferInterface;
class NV12BufferInterface;

class VideoFrameBuffer : public rtc::RefCountInterface {
 public:
  enum class Type {
    kNative,
    kI420,
    kI420A,
    kI422,
    kI444,
    kI010,
    kI210,
    kNV12,
  };

  virtual Type type() const = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual rtc::scoped_refptr<I420BufferInterface> ToI420() = 0;

  virtual const I420BufferInterface* GetI420() const;

  virtual rtc::scoped_refptr<VideoFrameBuffer> CropAndScale(
      int offset_x,
      int offset_y,
      int crop_width,
      int crop_height,
      int scaled_width,
      int scaled_height);

 protected:
  ~VideoFrameBuffer() override = default;
};

class PlanarYuvBuffer : public VideoFrameBuffer {
 public:
  virtual int ChromaWidth() const = 0;
  virtual int ChromaHeight() const = 0;
  virtual int StrideY() const = 0;
  virtual int StrideU() const = 0;
  virtual int StrideV() const = 0;

 protected:
  ~PlanarYuvBuffer() override = default;
};

class PlanarYuv8Buffer : public PlanarYuvBuffer {
 public:
  virtual const uint8_t* DataY() const = 0;
  virtual const uint8_t* DataU() const = 0;
  virtual const uint8_t* DataV() const = 0;

 protected:
  ~PlanarYuv8Buffer() override = default;
};

class I420BufferInterface : public PlanarYuv8Buffer {
 public:
  Type type() const override { return Type::kI420; }

  int ChromaWidth() const final { return (width() + 1) / 2; }
  int ChromaHeight() const final { return (height() + 1) / 2; }

  rtc::scoped_refptr<I420BufferInterface> ToI420() final {
    return rtc::scoped_refptr<I420BufferInterface>(this);
  }
  const I420BufferInterface* GetI420() const final { return this; }

 protected:
  ~I420BufferInterface() override = default;
};

}  // namespace webrtc
