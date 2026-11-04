#pragma once

#include <ostream>
#include <string_view>

namespace absl {

enum class LogSeverity : int {
  kInfo = 0,
  kWarning = 1,
  kError = 2,
  kFatal = 3,
};

enum class LogSeverityAtLeast : int {
  kInfo = static_cast<int>(LogSeverity::kInfo),
  kWarning = static_cast<int>(LogSeverity::kWarning),
  kError = static_cast<int>(LogSeverity::kError),
  kFatal = static_cast<int>(LogSeverity::kFatal),
  kInfinity = 1000,
};

enum class LogSeverityAtMost : int {
  kNegativeInfinity = -1000,
  kInfo = static_cast<int>(LogSeverity::kInfo),
  kWarning = static_cast<int>(LogSeverity::kWarning),
  kError = static_cast<int>(LogSeverity::kError),
  kFatal = static_cast<int>(LogSeverity::kFatal),
};

constexpr const char* LogSeverityName(LogSeverity s) {
  switch (s) {
    case LogSeverity::kInfo:
      return "INFO";
    case LogSeverity::kWarning:
      return "WARNING";
    case LogSeverity::kError:
      return "ERROR";
    case LogSeverity::kFatal:
      return "FATAL";
  }
  return "UNKNOWN";
}

constexpr LogSeverity NormalizeLogSeverity(LogSeverity s) {
  int v = static_cast<int>(s);
  if (v < static_cast<int>(LogSeverity::kInfo)) return LogSeverity::kInfo;
  if (v > static_cast<int>(LogSeverity::kFatal)) return LogSeverity::kFatal;
  return s;
}

constexpr LogSeverity NormalizeLogSeverity(int s) {
  return NormalizeLogSeverity(static_cast<LogSeverity>(s));
}

inline std::ostream& operator<<(std::ostream& os, LogSeverity s) {
  return os << LogSeverityName(s);
}

constexpr bool operator<(LogSeverity lhs, LogSeverityAtLeast rhs) {
  return static_cast<int>(lhs) < static_cast<int>(rhs);
}
constexpr bool operator>=(LogSeverity lhs, LogSeverityAtLeast rhs) {
  return !(lhs < rhs);
}
constexpr bool operator>(LogSeverity lhs, LogSeverityAtMost rhs) {
  return static_cast<int>(lhs) > static_cast<int>(rhs);
}
constexpr bool operator<=(LogSeverity lhs, LogSeverityAtMost rhs) {
  return !(lhs > rhs);
}

}  // namespace absl
