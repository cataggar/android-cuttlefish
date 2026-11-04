#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/base/log_severity.h"

namespace absl {

class LogEntry {
 public:
  static constexpr int kNoVerbosityLevel = -1;

  LogEntry() = default;
  LogEntry(LogSeverity severity, int verbosity, std::string_view full_filename,
           int line, bool prefix, uint32_t tid, std::string_view text_msg,
           std::string_view formatted_with_prefix,
           std::string_view formatted_with_newline)
      : severity_(severity),
        verbosity_(verbosity),
        full_filename_(full_filename),
        base_filename_(Basename(full_filename)),
        line_(line),
        prefix_(prefix),
        tid_(tid),
        text_message_(text_msg),
        with_prefix_(formatted_with_prefix),
        with_newline_(formatted_with_newline) {}

  LogSeverity log_severity() const { return severity_; }
  int verbosity() const { return verbosity_; }
  std::string_view source_filename() const { return full_filename_; }
  std::string_view source_basename() const { return base_filename_; }
  int source_line() const { return line_; }
  bool prefix() const { return prefix_; }
  uint32_t tid() const { return tid_; }
  std::string_view text_message() const { return text_message_; }
  std::string_view text_message_with_prefix() const { return with_prefix_; }
  std::string_view text_message_with_prefix_and_newline() const {
    return with_newline_;
  }
  std::string_view encoded_message() const { return text_message_; }

 private:
  static std::string_view Basename(std::string_view path) {
    auto pos = path.find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.substr(pos + 1);
  }

  LogSeverity severity_ = LogSeverity::kInfo;
  int verbosity_ = kNoVerbosityLevel;
  std::string_view full_filename_;
  std::string_view base_filename_;
  int line_ = 0;
  bool prefix_ = true;
  uint32_t tid_ = 0;
  std::string_view text_message_;
  std::string_view with_prefix_;
  std::string_view with_newline_;
};

}  // namespace absl
