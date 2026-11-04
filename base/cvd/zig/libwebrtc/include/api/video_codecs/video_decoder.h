#pragma once

#include <cstdint>
#include <string>

#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class DecodedImageCallback {
 public:
  virtual ~DecodedImageCallback() = default;

  virtual int32_t Decoded(VideoFrame& decodedImage) = 0;  // NOLINT
  virtual int32_t Decoded(VideoFrame& decodedImage,       // NOLINT
                          int64_t decode_time_ms) {
    (void)decode_time_ms;
    return Decoded(decodedImage);
  }
  virtual void Decoded(VideoFrame& decodedImage,  // NOLINT
                       absl::optional<int32_t> decode_time_ms,
                       absl::optional<uint8_t> qp) {
    (void)decode_time_ms;
    (void)qp;
    Decoded(decodedImage);
  }
};

class VideoDecoder {
 public:
  struct RenderResolution {
    constexpr RenderResolution() = default;
    constexpr RenderResolution(int width, int height)
        : width_(width), height_(height) {}
    constexpr int Width() const { return width_; }
    constexpr int Height() const { return height_; }

   private:
    int width_ = 0;
    int height_ = 0;
  };

  class Settings {
   public:
    Settings() = default;
    ~Settings() = default;

    int number_of_cores() const { return number_of_cores_; }
    void set_number_of_cores(int value) { number_of_cores_ = value; }

    RenderResolution max_render_resolution() const { return max_resolution_; }
    void set_max_render_resolution(RenderResolution value) {
      max_resolution_ = value;
    }

    VideoCodecType codec_type() const { return codec_type_; }
    void set_codec_type(VideoCodecType value) { codec_type_ = value; }

   private:
    int number_of_cores_ = 1;
    RenderResolution max_resolution_;
    VideoCodecType codec_type_ = kVideoCodecGeneric;
  };

  struct DecoderInfo {
    std::string implementation_name = "slim_decoder";
    bool is_hardware_accelerated = false;
  };

  virtual ~VideoDecoder() = default;

  virtual bool Configure(const Settings& settings) = 0;

  virtual int32_t Decode(const EncodedImage& input_image,
                         bool missing_frames,
                         int64_t render_time_ms) = 0;

  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) = 0;

  virtual int32_t Release() = 0;

  virtual DecoderInfo GetDecoderInfo() const { return DecoderInfo{}; }
  virtual const char* ImplementationName() const { return "slim_decoder"; }
};

}  // namespace webrtc
