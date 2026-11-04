#pragma once

#include <cstddef>
#include <cstdint>

namespace webrtc {

enum class ScalabilityMode : uint8_t {
  kL1T1 = 0,
  kL1T2,
  kL1T3,
  kL2T1,
  kL2T2,
  kL2T3,
  kL3T1,
  kL3T2,
  kL3T3,
};

constexpr size_t kScalabilityModeCount = 9;

}  // namespace webrtc
