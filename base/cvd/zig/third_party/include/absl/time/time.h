#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>

namespace absl {

class Duration {
 public:
  constexpr Duration() noexcept : ns_(0) {}
  explicit constexpr Duration(int64_t ns) noexcept : ns_(ns) {}

  constexpr int64_t ToNanoseconds() const noexcept { return ns_; }

  constexpr Duration& operator+=(Duration rhs) noexcept {
    ns_ += rhs.ns_;
    return *this;
  }
  constexpr Duration& operator-=(Duration rhs) noexcept {
    ns_ -= rhs.ns_;
    return *this;
  }
  constexpr Duration& operator*=(int64_t r) noexcept {
    ns_ *= r;
    return *this;
  }
  constexpr Duration& operator/=(int64_t r) noexcept {
    ns_ /= r;
    return *this;
  }
  constexpr Duration operator-() const noexcept { return Duration(-ns_); }

  friend constexpr Duration operator+(Duration lhs, Duration rhs) noexcept {
    return Duration(lhs.ns_ + rhs.ns_);
  }
  friend constexpr Duration operator-(Duration lhs, Duration rhs) noexcept {
    return Duration(lhs.ns_ - rhs.ns_);
  }
  friend constexpr Duration operator*(Duration lhs, int64_t rhs) noexcept {
    return Duration(lhs.ns_ * rhs);
  }
  friend constexpr Duration operator*(int64_t lhs, Duration rhs) noexcept {
    return Duration(lhs * rhs.ns_);
  }
  friend constexpr Duration operator/(Duration lhs, int64_t rhs) noexcept {
    return Duration(lhs.ns_ / rhs);
  }
  friend constexpr int64_t operator/(Duration lhs, Duration rhs) noexcept {
    return lhs.ns_ / rhs.ns_;
  }
  friend constexpr bool operator==(Duration a, Duration b) noexcept {
    return a.ns_ == b.ns_;
  }
  friend constexpr bool operator!=(Duration a, Duration b) noexcept {
    return a.ns_ != b.ns_;
  }
  friend constexpr bool operator<(Duration a, Duration b) noexcept {
    return a.ns_ < b.ns_;
  }
  friend constexpr bool operator<=(Duration a, Duration b) noexcept {
    return a.ns_ <= b.ns_;
  }
  friend constexpr bool operator>(Duration a, Duration b) noexcept {
    return a.ns_ > b.ns_;
  }
  friend constexpr bool operator>=(Duration a, Duration b) noexcept {
    return a.ns_ >= b.ns_;
  }

 private:
  int64_t ns_;
};

constexpr Duration ZeroDuration() noexcept { return Duration(0); }
constexpr Duration InfiniteDuration() noexcept {
  return Duration(std::numeric_limits<int64_t>::max());
}
constexpr Duration Nanoseconds(int64_t n) noexcept { return Duration(n); }
constexpr Duration Microseconds(int64_t n) noexcept {
  return Duration(n * 1000LL);
}
constexpr Duration Milliseconds(int64_t n) noexcept {
  return Duration(n * 1000000LL);
}
constexpr Duration Seconds(int64_t n) noexcept {
  return Duration(n * 1000000000LL);
}
constexpr Duration Minutes(int64_t n) noexcept {
  return Seconds(n * 60LL);
}
constexpr Duration Hours(int64_t n) noexcept {
  return Minutes(n * 60LL);
}

constexpr int64_t ToInt64Nanoseconds(Duration d) noexcept {
  return d.ToNanoseconds();
}
constexpr int64_t ToInt64Microseconds(Duration d) noexcept {
  return d.ToNanoseconds() / 1000LL;
}
constexpr int64_t ToInt64Milliseconds(Duration d) noexcept {
  return d.ToNanoseconds() / 1000000LL;
}
constexpr int64_t ToInt64Seconds(Duration d) noexcept {
  return d.ToNanoseconds() / 1000000000LL;
}
constexpr int64_t ToInt64Minutes(Duration d) noexcept {
  return ToInt64Seconds(d) / 60LL;
}
constexpr int64_t ToInt64Hours(Duration d) noexcept {
  return ToInt64Minutes(d) / 60LL;
}
constexpr double ToDoubleNanoseconds(Duration d) noexcept {
  return static_cast<double>(d.ToNanoseconds());
}
constexpr double ToDoubleMicroseconds(Duration d) noexcept {
  return static_cast<double>(d.ToNanoseconds()) / 1e3;
}
constexpr double ToDoubleMilliseconds(Duration d) noexcept {
  return static_cast<double>(d.ToNanoseconds()) / 1e6;
}
constexpr double ToDoubleSeconds(Duration d) noexcept {
  return static_cast<double>(d.ToNanoseconds()) / 1e9;
}

inline std::chrono::nanoseconds ToChronoNanoseconds(Duration d) {
  return std::chrono::nanoseconds(ToInt64Nanoseconds(d));
}
inline std::chrono::microseconds ToChronoMicroseconds(Duration d) {
  return std::chrono::microseconds(ToInt64Microseconds(d));
}
inline std::chrono::milliseconds ToChronoMilliseconds(Duration d) {
  return std::chrono::milliseconds(ToInt64Milliseconds(d));
}
inline std::chrono::seconds ToChronoSeconds(Duration d) {
  return std::chrono::seconds(ToInt64Seconds(d));
}
template <typename Rep, typename Period>
constexpr Duration FromChrono(const std::chrono::duration<Rep, Period>& d) {
  return Nanoseconds(
      std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
}

class Time {
 public:
  constexpr Time() noexcept : ns_(0) {}
  explicit constexpr Time(int64_t ns) noexcept : ns_(ns) {}

  constexpr int64_t ToUnixNanos() const noexcept { return ns_; }

  constexpr Time& operator+=(Duration d) noexcept {
    ns_ += d.ToNanoseconds();
    return *this;
  }
  constexpr Time& operator-=(Duration d) noexcept {
    ns_ -= d.ToNanoseconds();
    return *this;
  }

  friend constexpr Time operator+(Time t, Duration d) noexcept {
    return Time(t.ns_ + d.ToNanoseconds());
  }
  friend constexpr Time operator+(Duration d, Time t) noexcept {
    return Time(t.ns_ + d.ToNanoseconds());
  }
  friend constexpr Time operator-(Time t, Duration d) noexcept {
    return Time(t.ns_ - d.ToNanoseconds());
  }
  friend constexpr Duration operator-(Time a, Time b) noexcept {
    return Duration(a.ns_ - b.ns_);
  }
  friend constexpr bool operator==(Time a, Time b) noexcept {
    return a.ns_ == b.ns_;
  }
  friend constexpr bool operator!=(Time a, Time b) noexcept {
    return a.ns_ != b.ns_;
  }
  friend constexpr bool operator<(Time a, Time b) noexcept {
    return a.ns_ < b.ns_;
  }
  friend constexpr bool operator<=(Time a, Time b) noexcept {
    return a.ns_ <= b.ns_;
  }
  friend constexpr bool operator>(Time a, Time b) noexcept {
    return a.ns_ > b.ns_;
  }
  friend constexpr bool operator>=(Time a, Time b) noexcept {
    return a.ns_ >= b.ns_;
  }

 private:
  int64_t ns_;
};

constexpr Time UnixEpoch() noexcept { return Time(0); }
constexpr Time InfiniteFuture() noexcept {
  return Time(std::numeric_limits<int64_t>::max());
}
constexpr Time InfinitePast() noexcept {
  return Time(std::numeric_limits<int64_t>::min());
}

constexpr Time FromUnixNanos(int64_t ns) noexcept { return Time(ns); }
constexpr Time FromUnixMicros(int64_t us) noexcept {
  return Time(us * 1000LL);
}
constexpr Time FromUnixMillis(int64_t ms) noexcept {
  return Time(ms * 1000000LL);
}
constexpr Time FromUnixSeconds(int64_t s) noexcept {
  return Time(s * 1000000000LL);
}

constexpr int64_t ToUnixNanos(Time t) noexcept { return t.ToUnixNanos(); }
constexpr int64_t ToUnixMicros(Time t) noexcept {
  return t.ToUnixNanos() / 1000LL;
}
constexpr int64_t ToUnixMillis(Time t) noexcept {
  return t.ToUnixNanos() / 1000000LL;
}
constexpr int64_t ToUnixSeconds(Time t) noexcept {
  return t.ToUnixNanos() / 1000000000LL;
}

}  // namespace absl
