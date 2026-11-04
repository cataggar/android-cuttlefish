#pragma once

#include <cstdint>
#include <limits>

namespace webrtc {

class DataRate {
 public:
  constexpr DataRate() : bps_(0) {}

  static constexpr DataRate Zero() { return DataRate(0); }
  static constexpr DataRate Infinity() {
    return DataRate(std::numeric_limits<int64_t>::max());
  }
  static constexpr DataRate BitsPerSec(int64_t value) {
    return DataRate(value);
  }
  static constexpr DataRate BytesPerSec(int64_t value) {
    return DataRate(value * 8);
  }
  static constexpr DataRate KilobitsPerSec(int64_t value) {
    return DataRate(value * 1000);
  }

  constexpr int64_t bps() const { return bps_; }
  constexpr int64_t bytes_per_sec() const { return bps_ / 8; }
  constexpr int64_t kbps() const { return bps_ / 1000; }
  constexpr int64_t bps_or(int64_t fallback) const {
    return IsFinite() ? bps_ : fallback;
  }

  constexpr bool IsZero() const { return bps_ == 0; }
  constexpr bool IsInfinite() const {
    return bps_ == std::numeric_limits<int64_t>::max();
  }
  constexpr bool IsFinite() const { return !IsInfinite(); }

 private:
  explicit constexpr DataRate(int64_t bps) : bps_(bps) {}
  int64_t bps_;
};

}  // namespace webrtc
