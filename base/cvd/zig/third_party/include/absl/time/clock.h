#pragma once

#include <chrono>
#include <thread>

#include "absl/time/time.h"

namespace absl {

inline int64_t GetCurrentTimeNanos() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

inline Time Now() { return FromUnixNanos(GetCurrentTimeNanos()); }

inline void SleepFor(Duration duration) {
  if (duration > ZeroDuration()) {
    std::this_thread::sleep_for(ToChronoNanoseconds(duration));
  }
}

}  // namespace absl
