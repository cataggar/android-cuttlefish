#pragma once

#include <cstdint>
#include <memory>

#include "api/units/timestamp.h"

namespace webrtc {

class Clock {
 public:
  virtual ~Clock() = default;
  virtual Timestamp CurrentTime() = 0;
  virtual int64_t TimeInMilliseconds() { return CurrentTime().ms(); }
  virtual int64_t TimeInMicroseconds() { return CurrentTime().us(); }
  virtual int64_t CurrentNtpInMilliseconds() = 0;

  static Clock* GetRealTimeClock();
};

}  // namespace webrtc
