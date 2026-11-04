#pragma once

#include <cstdint>
#include <limits>

namespace webrtc {

class Frequency {
 public:
  constexpr Frequency() : mhz_(0) {}

  static constexpr Frequency Zero() { return Frequency(0); }
  static constexpr Frequency PlusInfinity() {
    return Frequency(std::numeric_limits<int64_t>::max());
  }
  static constexpr Frequency MinusInfinity() {
    return Frequency(std::numeric_limits<int64_t>::min());
  }
  static constexpr Frequency MilliHertz(int64_t value) {
    return Frequency(value);
  }
  static constexpr Frequency Hertz(int64_t value) {
    return Frequency(value * 1000);
  }
  static constexpr Frequency Kilohertz(int64_t value) {
    return Frequency(value * 1000000);
  }

  constexpr int64_t hertz() const { return mhz_ / 1000; }
  constexpr int64_t millihertz() const { return mhz_; }
  constexpr double hertz_double() const {
    return static_cast<double>(mhz_) / 1000.0;
  }

 private:
  explicit constexpr Frequency(int64_t mhz) : mhz_(mhz) {}
  int64_t mhz_;
};

}  // namespace webrtc
