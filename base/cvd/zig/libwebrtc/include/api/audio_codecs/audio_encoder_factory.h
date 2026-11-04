#pragma once

#include <memory>
#include <vector>

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

struct AudioCodecPairId {};

class AudioEncoderFactory : public RefCountInterface {
 public:
  virtual std::vector<AudioCodecSpec> GetSupportedEncoders() = 0;
  virtual absl::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) = 0;
  virtual std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id) = 0;
};

}  // namespace webrtc
