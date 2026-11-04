#pragma once

namespace webrtc {

enum VideoCodecType {
  kVideoCodecGeneric = 0,
  kVideoCodecVP8,
  kVideoCodecVP9,
  kVideoCodecAV1,
  kVideoCodecH264,
  kVideoCodecMultiplex,
  kVideoCodecH265,
};

}  // namespace webrtc
