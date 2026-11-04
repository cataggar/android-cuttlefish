#pragma once

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class VideoDecoderFactory {
 public:
  struct CodecSupport {
    bool is_supported = false;
    bool is_power_efficient = false;
  };

  virtual ~VideoDecoderFactory() = default;

  virtual std::vector<SdpVideoFormat> GetSupportedFormats() const = 0;

  virtual CodecSupport QueryCodecSupport(const SdpVideoFormat& format,
                                         bool reference_scaling) const {
    (void)reference_scaling;
    for (const auto& supported : GetSupportedFormats()) {
      if (format.IsSameCodec(supported)) {
        return {.is_supported = true, .is_power_efficient = false};
      }
    }
    return {.is_supported = false, .is_power_efficient = false};
  }

  virtual std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) = 0;
};

}  // namespace webrtc
