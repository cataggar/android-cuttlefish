#pragma once

#include <ostream>
#include <sstream>

namespace rtc {

enum LoggingSeverity {
  LS_VERBOSE,
  LS_INFO,
  LS_WARNING,
  LS_ERROR,
  LS_NONE,
  INFO = LS_INFO,
  WARNING = LS_WARNING,
  LERROR = LS_ERROR
};

class LogMessage {
 public:
  LogMessage(const char* file, int line, LoggingSeverity sev)
      : severity_(sev) {
    (void)file;
    (void)line;
  }
  ~LogMessage() = default;

  std::ostream& stream() { return stream_; }

  static void LogToDebug(LoggingSeverity min_sev);
  static LoggingSeverity GetLogToDebug();

 private:
  LoggingSeverity severity_;
  std::ostringstream stream_;
};

}  // namespace rtc

#ifndef RTC_LOG
#define RTC_LOG(sev) \
  ::rtc::LogMessage(__FILE__, __LINE__, ::rtc::sev).stream()
#endif

#ifndef RTC_DLOG
#define RTC_DLOG(sev) RTC_LOG(sev)
#endif
