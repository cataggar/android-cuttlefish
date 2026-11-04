#pragma once

#include <cstdint>
#include <limits>

#include "api/units/time_delta.h"

namespace webrtc {

class Timestamp {
 public:
  constexpr Timestamp() : us_(0) {}

  static constexpr Timestamp Zero() { return Timestamp(0); }
  static constexpr Timestamp PlusInfinity() {
    return Timestamp(std::numeric_limits<int64_t>::max());
  }
  static constexpr Timestamp MinusInfinity() {
    return Timestamp(std::numeric_limits<int64_t>::min());
  }

  static constexpr Timestamp Seconds(int64_t value) {
    return Timestamp(value * 1000000);
  }
  static constexpr Timestamp Millis(int64_t value) {
    return Timestamp(value * 1000);
  }
  static constexpr Timestamp Micros(int64_t value) {
    return Timestamp(value);
  }

  constexpr int64_t seconds() const { return us_ / 1000000; }
  constexpr int64_t ms() const { return us_ / 1000; }
  constexpr int64_t us() const { return us_; }

  constexpr bool IsInfinite() const {
    return us_ == std::numeric_limits<int64_t>::max() ||
           us_ == std::numeric_limits<int64_t>::min();
  }
  constexpr bool IsFinite() const { return !IsInfinite(); }

 private:
  explicit constexpr Timestamp(int64_t us) : us_(us) {}
  int64_t us_;
};

}  // namespace webrtc
