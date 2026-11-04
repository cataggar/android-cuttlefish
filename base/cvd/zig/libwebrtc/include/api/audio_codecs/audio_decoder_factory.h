#pragma once

#include <memory>
#include <vector>

#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class AudioDecoderFactory : public RefCountInterface {
 public:
  virtual std::vector<AudioCodecSpec> GetSupportedDecoders() = 0;
  virtual bool IsSupportedDecoder(const SdpAudioFormat& format) = 0;
  virtual std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id) = 0;
};

}  // namespace webrtc
