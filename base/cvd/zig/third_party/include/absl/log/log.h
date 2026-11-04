#pragma once

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"

namespace absl {
namespace log_internal {

struct StringifySink {
  std::ostream& os;
  void Append(std::string_view sv) { os.write(sv.data(), static_cast<std::streamsize>(sv.size())); }
  void Append(size_t count, char c) {
    for (size_t i = 0; i < count; ++i) os.put(c);
  }
};

template <typename T>
concept HasAbslStringify = requires(StringifySink& s, const T& v) {
  AbslStringify(s, v);
};

void DispatchLogMessage(LogSeverity severity, int verbosity, const char* file,
                        int line, bool prefix, const std::string& msg);

class LogMessage {
 public:
  LogMessage(const char* file, int line, LogSeverity severity,
             int verbosity = LogEntry::kNoVerbosityLevel, bool enabled = true,
             int saved_errno = -1)
      : file_(file),
        line_(line),
        severity_(severity),
        verbosity_(verbosity),
        enabled_(enabled),
        saved_errno_(saved_errno) {}

  ~LogMessage() {
    if (enabled_) {
      std::string text = stream_.str();
      if (saved_errno_ >= 0) {
        text += ": ";
        text += std::strerror(saved_errno_);
        text += " [";
        text += std::to_string(saved_errno_);
        text += "]";
      }
      DispatchLogMessage(severity_, verbosity_, file_, line_, prefix_, text);
    }
    if (severity_ == LogSeverity::kFatal) {
      std::abort();
    }
  }

  LogMessage& AtLocation(std::string_view file, int line) {
    file_storage_.assign(file.data(), file.size());
    file_ = file_storage_.c_str();
    line_ = line;
    return *this;
  }

  LogMessage& NoPrefix() {
    prefix_ = false;
    return *this;
  }

  LogMessage& WithVerbosity(int v) {
    verbosity_ = v;
    return *this;
  }

  LogMessage& WithPerror() {
    saved_errno_ = errno;
    return *this;
  }

  LogMessage& InternalStream() { return *this; }

  std::ostream& stream() { return stream_; }

  template <typename T>
  LogMessage& operator<<(const T& val) {
    if (!enabled_) return *this;
    if constexpr (HasAbslStringify<T> &&
                  !requires(std::ostream& os, const T& v) { os << v; }) {
      StringifySink sink{stream_};
      AbslStringify(sink, val);
    } else {
      stream_ << val;
    }
    return *this;
  }

  LogMessage& operator<<(std::ostream& (*manip)(std::ostream&)) {
    if (enabled_) manip(stream_);
    return *this;
  }

 protected:
  const char* file_;
  std::string file_storage_;
  int line_;
  LogSeverity severity_;
  int verbosity_;
  bool prefix_ = true;
  bool enabled_ = true;
  int saved_errno_ = -1;
  std::ostringstream stream_;
};

class LogMessageFatal : public LogMessage {
 public:
  LogMessageFatal(const char* file, int line, int saved_errno = -1)
      : LogMessage(file, line, LogSeverity::kFatal, LogEntry::kNoVerbosityLevel,
                   true, saved_errno) {}
  [[noreturn]] ~LogMessageFatal() {
    std::string text = stream_.str();
    if (saved_errno_ >= 0) {
      text += ": ";
      text += std::strerror(saved_errno_);
      text += " [";
      text += std::to_string(saved_errno_);
      text += "]";
    }
    DispatchLogMessage(LogSeverity::kFatal, verbosity_, file_, line_, prefix_,
                       text);
    std::abort();
  }
};

class Voidify {
 public:
  void operator&(const LogMessage&) const {}
  void operator&(const std::ostream&) const {}
};

}  // namespace log_internal
}  // namespace absl

#define ABSL_LOG_INTERNAL_SEV_INFO ::absl::LogSeverity::kInfo
#define ABSL_LOG_INTERNAL_SEV_WARNING ::absl::LogSeverity::kWarning
#define ABSL_LOG_INTERNAL_SEV_WARN ::absl::LogSeverity::kWarning
#define ABSL_LOG_INTERNAL_SEV_ERROR ::absl::LogSeverity::kError
#define ABSL_LOG_INTERNAL_SEV_FATAL ::absl::LogSeverity::kFatal
#define ABSL_LOG_INTERNAL_SEV_DFATAL ::absl::LogSeverity::kError
#define ABSL_LOG_INTERNAL_SEV_VERBOSE ::absl::LogSeverity::kInfo
#define ABSL_LOG_INTERNAL_SEV_DEBUG ::absl::LogSeverity::kInfo
#define ABSL_LOG_INTERNAL_SEV_LEVEL(s) static_cast<::absl::LogSeverity>(s)

#define ABSL_LOG_INTERNAL_LOG_INFO \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kInfo)
#define ABSL_LOG_INTERNAL_LOG_WARNING \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kWarning)
#define ABSL_LOG_INTERNAL_LOG_WARN \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kWarning)
#define ABSL_LOG_INTERNAL_LOG_ERROR \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kError)
#define ABSL_LOG_INTERNAL_LOG_FATAL \
  ::absl::log_internal::LogMessageFatal(__FILE__, __LINE__)
#define ABSL_LOG_INTERNAL_LOG_DFATAL \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kError)
#define ABSL_LOG_INTERNAL_LOG_VERBOSE \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kInfo, 1)
#define ABSL_LOG_INTERNAL_LOG_DEBUG \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kInfo, 1)
#define ABSL_LOG_INTERNAL_LOG_LEVEL(s) \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, static_cast<::absl::LogSeverity>(s))

#define ABSL_LOG_INTERNAL_PLOG_INFO \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kInfo, -1, true, errno)
#define ABSL_LOG_INTERNAL_PLOG_WARNING \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kWarning, -1, true, errno)
#define ABSL_LOG_INTERNAL_PLOG_WARN \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kWarning, -1, true, errno)
#define ABSL_LOG_INTERNAL_PLOG_ERROR \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kError, -1, true, errno)
#define ABSL_LOG_INTERNAL_PLOG_FATAL \
  ::absl::log_internal::LogMessageFatal(__FILE__, __LINE__, errno)
#define ABSL_LOG_INTERNAL_PLOG_DFATAL \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kError, -1, true, errno)
#define ABSL_LOG_INTERNAL_PLOG_VERBOSE \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kInfo, 1, true, errno)
#define ABSL_LOG_INTERNAL_PLOG_DEBUG \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kInfo, 1, true, errno)
#define ABSL_LOG_INTERNAL_PLOG_LEVEL(s) \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, static_cast<::absl::LogSeverity>(s), -1, true, errno)

#ifndef LOG
#define LOG(severity) ABSL_LOG_INTERNAL_LOG_##severity
#endif

#ifndef PLOG
#define PLOG(severity) ABSL_LOG_INTERNAL_PLOG_##severity
#endif

#ifndef VLOG_IS_ON
#define VLOG_IS_ON(verbose_level) ((verbose_level) <= ::absl::GlobalVLogLevel())
#endif

#ifndef VLOG
#define VLOG(verbose_level)                                                  \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__,                       \
                                   ::absl::LogSeverity::kInfo,               \
                                   static_cast<int>(verbose_level),          \
                                   VLOG_IS_ON(verbose_level))
#endif

#ifndef DLOG
#define DLOG(severity) ABSL_LOG_INTERNAL_LOG_##severity
#endif

#ifndef DPLOG
#define DPLOG(severity) ABSL_LOG_INTERNAL_PLOG_##severity
#endif

#ifndef DVLOG
#define DVLOG(verbose_level) VLOG(verbose_level)
#endif

#ifndef LOG_IF
#define LOG_IF(severity, condition) \
  !(condition) ? (void)0 : ::absl::log_internal::Voidify() & LOG(severity)
#endif

#ifndef PLOG_IF
#define PLOG_IF(severity, condition) \
  !(condition) ? (void)0 : ::absl::log_internal::Voidify() & PLOG(severity)
#endif

#ifndef DLOG_IF
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#endif

#ifndef LOG_FIRST_N
#define LOG_FIRST_N(severity, n) LOG(severity)
#endif

#ifndef LOG_EVERY_N
#define LOG_EVERY_N(severity, n) LOG(severity)
#endif

#ifndef LOG_EVERY_N_SEC
#define LOG_EVERY_N_SEC(severity, n) LOG(severity)
#endif

#ifndef LOG_EVERY_POW_2
#define LOG_EVERY_POW_2(severity) LOG(severity)
#endif

#define ABSL_LOG(severity) ABSL_LOG_INTERNAL_LOG_##severity
#define ABSL_PLOG(severity) ABSL_LOG_INTERNAL_PLOG_##severity
#define ABSL_VLOG(verbose_level) VLOG(verbose_level)
#define ABSL_DLOG(severity) DLOG(severity)
#define ABSL_LOG_IF(severity, condition) LOG_IF(severity, condition)
#define ABSL_PLOG_IF(severity, condition) PLOG_IF(severity, condition)
