#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/color_space.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class EncodedImageBufferInterface : public rtc::RefCountInterface {
 public:
  virtual const uint8_t* data() const = 0;
  virtual uint8_t* data() = 0;
  virtual size_t size() const = 0;
};

class EncodedImageBuffer : public EncodedImageBufferInterface {
 public:
  static rtc::scoped_refptr<EncodedImageBuffer> Create(size_t size = 0);
  static rtc::scoped_refptr<EncodedImageBuffer> Create(const uint8_t* data,
                                                       size_t size);

  const uint8_t* data() const override { return buffer_.data(); }
  uint8_t* data() override { return buffer_.data(); }
  size_t size() const override { return buffer_.size(); }
  void Realloc(size_t size) { buffer_.resize(size); }

 protected:
  explicit EncodedImageBuffer(size_t size);
  EncodedImageBuffer(const uint8_t* data, size_t size);
  ~EncodedImageBuffer() override = default;

 private:
  std::vector<uint8_t> buffer_;
};

class EncodedImage {
 public:
  EncodedImage() = default;
  EncodedImage(const EncodedImage&) = default;
  EncodedImage(EncodedImage&&) = default;
  EncodedImage& operator=(const EncodedImage&) = default;
  EncodedImage& operator=(EncodedImage&&) = default;
  ~EncodedImage() = default;

  void SetTimestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }
  uint32_t Timestamp() const { return timestamp_rtp_; }
  void SetRtpTimestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }
  uint32_t RtpTimestamp() const { return timestamp_rtp_; }

  void SetEncodeTime(int64_t encode_start_ms, int64_t encode_finish_ms) {
    encode_start_ms_ = encode_start_ms;
    encode_finish_ms_ = encode_finish_ms;
  }

  void SetEncodedData(
      rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data) {
    encoded_data_ = std::move(encoded_data);
    size_ = encoded_data_ ? encoded_data_->size() : 0;
  }

  rtc::scoped_refptr<EncodedImageBufferInterface> GetEncodedData() const {
    return encoded_data_;
  }

  const uint8_t* data() const {
    return encoded_data_ ? encoded_data_->data() : nullptr;
  }

  uint8_t* mutable_data() {
    return encoded_data_ ? encoded_data_->data() : nullptr;
  }

  size_t size() const { return size_; }
  void set_size(size_t new_size) { size_ = new_size; }
  void ClearEncodedData() {
    encoded_data_ = nullptr;
    size_ = 0;
  }

  uint32_t _encodedWidth = 0;
  uint32_t _encodedHeight = 0;
  int64_t ntp_time_ms_ = 0;
  int64_t capture_time_ms_ = 0;
  VideoFrameType _frameType = VideoFrameType::kVideoFrameDelta;
  VideoRotation rotation_ = kVideoRotation_0;
  VideoContentType content_type_ = VideoContentType::UNSPECIFIED;
  bool _completeFrame = true;
  int qp_ = -1;

 private:
  rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data_;
  size_t size_ = 0;
  uint32_t timestamp_rtp_ = 0;
  int64_t encode_start_ms_ = 0;
  int64_t encode_finish_ms_ = 0;
};

}  // namespace webrtc
