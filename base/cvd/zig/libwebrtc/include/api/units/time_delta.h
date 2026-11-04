#pragma once

#include <cstdint>
#include <limits>

namespace webrtc {

class TimeDelta {
 public:
  constexpr TimeDelta() : us_(0) {}

  static constexpr TimeDelta Zero() { return TimeDelta(0); }
  static constexpr TimeDelta PlusInfinity() {
    return TimeDelta(std::numeric_limits<int64_t>::max());
  }
  static constexpr TimeDelta MinusInfinity() {
    return TimeDelta(std::numeric_limits<int64_t>::min());
  }

  static constexpr TimeDelta Seconds(int64_t value) {
    return TimeDelta(value * 1000000);
  }
  static constexpr TimeDelta Millis(int64_t value) {
    return TimeDelta(value * 1000);
  }
  static constexpr TimeDelta Micros(int64_t value) {
    return TimeDelta(value);
  }
  static constexpr TimeDelta Nanos(int64_t value) {
    return TimeDelta(value / 1000);
  }

  constexpr int64_t seconds() const { return us_ / 1000000; }
  constexpr int64_t ms() const { return us_ / 1000; }
  constexpr int64_t us() const { return us_; }
  constexpr int64_t ns() const { return us_ * 1000; }

  constexpr bool IsZero() const { return us_ == 0; }
  constexpr bool IsFinite() const {
    return !IsPlusInfinity() && !IsMinusInfinity();
  }
  constexpr bool IsPlusInfinity() const {
    return us_ == std::numeric_limits<int64_t>::max();
  }
  constexpr bool IsMinusInfinity() const {
    return us_ == std::numeric_limits<int64_t>::min();
  }

  constexpr bool operator==(const TimeDelta& o) const { return us_ == o.us_; }
  constexpr bool operator!=(const TimeDelta& o) const { return us_ != o.us_; }
  constexpr bool operator<(const TimeDelta& o) const { return us_ < o.us_; }
  constexpr bool operator<=(const TimeDelta& o) const { return us_ <= o.us_; }
  constexpr bool operator>(const TimeDelta& o) const { return us_ > o.us_; }
  constexpr bool operator>=(const TimeDelta& o) const { return us_ >= o.us_; }

 private:
  explicit constexpr TimeDelta(int64_t us) : us_(us) {}
  int64_t us_;
};

}  // namespace webrtc
