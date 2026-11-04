#pragma once

#include <functional>
#include <ostream>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "android-base/macros.h"

namespace android {
namespace base {

enum LogSeverity {
  VERBOSE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL_WITHOUT_ABORT,
  FATAL,
};

enum LogId {
  DEFAULT,
  MAIN,
  SYSTEM,
  RADIO,
  EVENTS,
  CRASH,
};

using LogFunction = std::function<void(LogId, LogSeverity, const char*,
                                       const char*, unsigned int, const char*)>;
using AbortFunction = std::function<void(const char*)>;

void InitLogging(char* argv[], LogFunction&& logger = {},
                 AbortFunction&& aborter = {});
void SetLogger(LogFunction&& logger);
void SetAborter(AbortFunction&& aborter);
LogSeverity SetMinimumLogSeverity(LogSeverity new_severity);
LogSeverity GetMinimumLogSeverity();
bool ShouldLog(LogSeverity severity, const char* tag);

void StderrLogger(LogId id, LogSeverity severity, const char* tag,
                  const char* file, unsigned int line, const char* message);
void StdioLogger(LogId id, LogSeverity severity, const char* tag,
                 const char* file, unsigned int line, const char* message);
void DefaultAborter(const char* abort_message);

std::string GetDefaultTag();
void SetDefaultTag(const std::string& tag);

class ScopedLogSeverity {
 public:
  explicit ScopedLogSeverity(LogSeverity new_severity)
      : old_(SetMinimumLogSeverity(new_severity)) {}
  ~ScopedLogSeverity() { SetMinimumLogSeverity(old_); }

 private:
  LogSeverity old_;
};

}  // namespace base
}  // namespace android
