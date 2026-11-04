#pragma once

#include <cstdint>

#include "api/scoped_refptr.h"
#include "api/video/color_space.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class VideoFrame {
 public:
  class Builder {
   public:
    Builder() = default;
    ~Builder() = default;

    VideoFrame build();
    Builder& set_video_frame_buffer(
        const rtc::scoped_refptr<VideoFrameBuffer>& buffer);
    Builder& set_timestamp_ms(int64_t timestamp_ms);
    Builder& set_timestamp_us(int64_t timestamp_us);
    Builder& set_timestamp_rtp(uint32_t timestamp_rtp);
    Builder& set_ntp_time_ms(int64_t ntp_time_ms);
    Builder& set_rotation(VideoRotation rotation);
    Builder& set_color_space(const absl::optional<ColorSpace>& color_space);
    Builder& set_color_space(const ColorSpace* color_space);
    Builder& set_id(uint16_t id);

   private:
    uint16_t id_ = 0;
    rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer_;
    int64_t timestamp_us_ = 0;
    uint32_t timestamp_rtp_ = 0;
    int64_t ntp_time_ms_ = 0;
    VideoRotation rotation_ = kVideoRotation_0;
    absl::optional<ColorSpace> color_space_;
  };

  VideoFrame() = default;
  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             VideoRotation rotation,
             int64_t timestamp_us);
  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             uint32_t timestamp_rtp,
             int64_t render_time_ms,
             VideoRotation rotation);
  ~VideoFrame() = default;

  VideoFrame(const VideoFrame&) = default;
  VideoFrame(VideoFrame&&) = default;
  VideoFrame& operator=(const VideoFrame&) = default;
  VideoFrame& operator=(VideoFrame&&) = default;

  int width() const {
    return video_frame_buffer_ ? video_frame_buffer_->width() : 0;
  }
  int height() const {
    return video_frame_buffer_ ? video_frame_buffer_->height() : 0;
  }
  uint32_t size() const {
    return static_cast<uint32_t>(width()) * static_cast<uint32_t>(height());
  }

  uint16_t id() const { return id_; }
  void set_id(uint16_t id) { id_ = id; }

  int64_t timestamp_us() const { return timestamp_us_; }
  void set_timestamp_us(int64_t timestamp_us) { timestamp_us_ = timestamp_us; }

  uint32_t timestamp() const { return timestamp_rtp_; }
  void set_timestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }
  uint32_t rtp_timestamp() const { return timestamp_rtp_; }
  void set_rtp_timestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }

  int64_t ntp_time_ms() const { return ntp_time_ms_; }
  void set_ntp_time_ms(int64_t ntp_time_ms) { ntp_time_ms_ = ntp_time_ms; }

  int64_t render_time_ms() const { return timestamp_us_ / 1000; }

  VideoRotation rotation() const { return rotation_; }
  void set_rotation(VideoRotation rotation) { rotation_ = rotation; }

  const absl::optional<ColorSpace>& color_space() const { return color_space_; }
  void set_color_space(const absl::optional<ColorSpace>& color_space) {
    color_space_ = color_space;
  }

  rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer() const {
    return video_frame_buffer_;
  }
  void set_video_frame_buffer(
      const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {
    video_frame_buffer_ = buffer;
  }

  bool is_texture() const {
    return video_frame_buffer_ &&
           video_frame_buffer_->type() == VideoFrameBuffer::Type::kNative;
  }

 private:
  uint16_t id_ = 0;
  rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer_;
  int64_t timestamp_us_ = 0;
  uint32_t timestamp_rtp_ = 0;
  int64_t ntp_time_ms_ = 0;
  VideoRotation rotation_ = kVideoRotation_0;
  absl::optional<ColorSpace> color_space_;
};

}  // namespace webrtc
