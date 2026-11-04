#pragma once

#include <string_view>

#include "absl/base/log_severity.h"

namespace absl {

void SetStderrThreshold(absl::LogSeverityAtLeast threshold);
void SetStderrThreshold(absl::LogSeverity threshold);
absl::LogSeverityAtLeast StderrThreshold();

void SetMinLogLevel(absl::LogSeverityAtLeast severity);
absl::LogSeverityAtLeast MinLogLevel();

int SetGlobalVLogLevel(int v);
int GlobalVLogLevel();
int SetVLogLevel(std::string_view module_pattern, int log_level);

void EnableLogPrefix(bool on_off);
bool ShouldPrependLogPrefix();

}  // namespace absl
