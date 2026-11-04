#pragma once

#include <cstdint>

namespace rtc {

constexpr int64_t kNumMillisecsPerSec = 1000;
constexpr int64_t kNumMicrosecsPerSec = 1000000;
constexpr int64_t kNumNanosecsPerSec = 1000000000;

constexpr int64_t kNumMicrosecsPerMillisec =
    kNumMicrosecsPerSec / kNumMillisecsPerSec;
constexpr int64_t kNumNanosecsPerMillisec =
    kNumNanosecsPerSec / kNumMillisecsPerSec;
constexpr int64_t kNumNanosecsPerMicrosec =
    kNumNanosecsPerSec / kNumMicrosecsPerSec;

int64_t SystemTimeNanos();
int64_t SystemTimeMillis();
int64_t TimeNanos();
int64_t TimeMicros();
int64_t TimeMillis();
uint32_t Time32();
int64_t TimeUTCMillis();
int64_t TimeDiff(int64_t later, int64_t earlier);
int32_t TimeDiff32(uint32_t later, uint32_t earlier);

}  // namespace rtc
