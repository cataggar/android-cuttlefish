#pragma once

#include <cstdint>
#include <string>

namespace webrtc {

enum class H264PacketizationMode {
  NonInterleaved = 0,  // Mode 1 - STAP-A, FU-A is allowed
  SingleNalUnit        // Mode 0 - only single NALU allowed
};

struct H264PacketizationTypes {
  static constexpr H264PacketizationMode kH264SingleNalUnit =
      H264PacketizationMode::SingleNalUnit;
  static constexpr H264PacketizationMode kH264NonInterleaved =
      H264PacketizationMode::NonInterleaved;
};

struct CodecSpecificInfoH264 {
  H264PacketizationMode packetization_mode =
      H264PacketizationMode::NonInterleaved;
  uint8_t temporal_idx = 0;
  bool base_layer_sync = false;
  bool idr_frame = false;
};

}  // namespace webrtc
