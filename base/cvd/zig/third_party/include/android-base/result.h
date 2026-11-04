#pragma once

#include <cerrno>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "tl/expected.hpp"

namespace android {
namespace base {

struct ErrorCode {
  int value = 0;
  ErrorCode() = default;
  explicit ErrorCode(int v) : value(v) {}
  operator int() const { return value; }
};

class ResultError {
 public:
  ResultError() = default;
  ResultError(std::string msg, int code = 0)
      : msg_(std::move(msg)), code_(code) {}

  const std::string& message() const { return msg_; }
  ErrorCode code() const { return ErrorCode(code_); }

  template <typename T>
  ResultError& operator<<(const T& val) {
    std::ostringstream oss;
    oss << val;
    msg_ += oss.str();
    return *this;
  }

  operator tl::unexpected<ResultError>() const {
    return tl::unexpected<ResultError>(*this);
  }

  friend std::ostream& operator<<(std::ostream& os, const ResultError& e) {
    os << e.msg_;
    if (e.code_ != 0) {
      os << ": " << std::strerror(e.code_);
    }
    return os;
  }

 private:
  std::string msg_;
  int code_ = 0;
};

inline ResultError Error(int code = 0) { return ResultError("", code); }
inline ResultError ErrnoError() { return ResultError("", errno); }

template <typename T>
using Result = tl::expected<T, ResultError>;

}  // namespace base
}  // namespace android
