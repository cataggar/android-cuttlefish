#pragma once

#include "absl/log/log_entry.h"

namespace absl {

class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void Send(const absl::LogEntry& entry) = 0;
  virtual void Flush() {}
};

}  // namespace absl
