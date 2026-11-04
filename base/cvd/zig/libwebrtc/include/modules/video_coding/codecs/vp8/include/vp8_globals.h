#pragma once

#include <cstddef>
#include <cstdint>

namespace webrtc {

constexpr uint8_t kNoTemporalIdx = 0xFF;

struct CodecSpecificInfoVP8 {
  bool nonReference = false;
  uint8_t temporalIdx = kNoTemporalIdx;
  bool layerSync = false;
  int8_t keyIdx = -1;
  bool useExplicitDependencies = false;
  static constexpr size_t kMaxBuffersCount = 3;
  size_t referencedBuffers[kMaxBuffersCount] = {};
  size_t referencedBuffersCount = 0;
  size_t updatedBuffers[kMaxBuffersCount] = {};
  size_t updatedBuffersCount = 0;
};

}  // namespace webrtc
