#pragma once

#include <memory>
#include <string>
#include <vector>

#include "api/units/data_rate.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class VideoEncoderFactory {
 public:
  struct CodecSupport {
    bool is_supported = false;
    bool is_power_efficient = false;
  };

  class EncoderSelectorInterface {
   public:
    struct RenderResolution {
      int width = 0;
      int height = 0;
    };

    virtual ~EncoderSelectorInterface() = default;
    virtual void OnCurrentEncoder(const SdpVideoFormat& format) = 0;
    virtual absl::optional<SdpVideoFormat> OnAvailableBitrate(
        const DataRate& rate) = 0;
    virtual absl::optional<SdpVideoFormat> OnRenderResolution(
        const RenderResolution& resolution) {
      (void)resolution;
      return absl::nullopt;
    }
    virtual absl::optional<SdpVideoFormat> OnEncoderBroken() = 0;
  };

  virtual ~VideoEncoderFactory() = default;

  virtual std::vector<SdpVideoFormat> GetSupportedFormats() const = 0;

  virtual std::vector<SdpVideoFormat> GetImplementations() const {
    return GetSupportedFormats();
  }

  virtual CodecSupport QueryCodecSupport(
      const SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const {
    (void)scalability_mode;
    for (const auto& supported : GetSupportedFormats()) {
      if (format.IsSameCodec(supported)) {
        return {.is_supported = true, .is_power_efficient = false};
      }
    }
    return {.is_supported = false, .is_power_efficient = false};
  }

  virtual std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) = 0;

  virtual std::unique_ptr<EncoderSelectorInterface> GetEncoderSelector() const {
    return nullptr;
  }
};

}  // namespace webrtc
