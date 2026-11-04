#pragma once

#include "absl/log/log_sink.h"

namespace absl {

void AddLogSink(absl::LogSink* sink);
void RemoveLogSink(absl::LogSink* sink);
void FlushLogSinks();

}  // namespace absl
