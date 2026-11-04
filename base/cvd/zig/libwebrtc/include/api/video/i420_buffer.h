#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/video_frame_buffer.h"

namespace webrtc {

class I420Buffer : public I420BufferInterface {
 public:
  static rtc::scoped_refptr<I420Buffer> Create(int width, int height);
  static rtc::scoped_refptr<I420Buffer> Create(int width,
                                               int height,
                                               int stride_y,
                                               int stride_u,
                                               int stride_v);
  static rtc::scoped_refptr<I420Buffer> Copy(const I420BufferInterface& buffer);

  void InitializeData();

  int width() const override { return width_; }
  int height() const override { return height_; }

  const uint8_t* DataY() const override { return data_.data(); }
  const uint8_t* DataU() const override {
    return data_.data() + stride_y_ * height_;
  }
  const uint8_t* DataV() const override {
    return DataU() + stride_u_ * ((height_ + 1) / 2);
  }

  uint8_t* MutableDataY() { return data_.data(); }
  uint8_t* MutableDataU() { return data_.data() + stride_y_ * height_; }
  uint8_t* MutableDataV() {
    return MutableDataU() + stride_u_ * ((height_ + 1) / 2);
  }

  int StrideY() const override { return stride_y_; }
  int StrideU() const override { return stride_u_; }
  int StrideV() const override { return stride_v_; }

  void CropAndScaleFrom(const I420BufferInterface& src,
                        int offset_x,
                        int offset_y,
                        int crop_width,
                        int crop_height);
  void CropAndScaleFrom(const I420BufferInterface& src);
  void ScaleFrom(const I420BufferInterface& src);

 protected:
  I420Buffer(int width, int height);
  I420Buffer(int width,
             int height,
             int stride_y,
             int stride_u,
             int stride_v);
  ~I420Buffer() override = default;

 private:
  const int width_;
  const int height_;
  const int stride_y_;
  const int stride_u_;
  const int stride_v_;
  std::vector<uint8_t> data_;
};

}  // namespace webrtc
