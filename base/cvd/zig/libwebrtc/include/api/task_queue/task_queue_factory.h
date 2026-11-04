#pragma once

#include <memory>

#include "api/units/time_delta.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class TaskQueueBase {
 public:
  enum class DelayPrecision { kLow, kHigh };
  virtual ~TaskQueueBase() = default;
  virtual void Delete() = 0;
};

class TaskQueueFactory {
 public:
  enum class Priority { NORMAL = 0, HIGH, LOW };
  virtual ~TaskQueueFactory() = default;
  virtual std::unique_ptr<TaskQueueBase> CreateTaskQueue(
      absl::string_view name, Priority priority) const = 0;
};

}  // namespace webrtc
