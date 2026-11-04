#pragma once

#include <string>
#include <utility>

#include "rtc_base/ref_count.h"

namespace webrtc {

enum class RTCErrorType {
  NONE,
  UNSUPPORTED_OPERATION,
  UNSUPPORTED_PARAMETER,
  INVALID_PARAMETER,
  INVALID_RANGE,
  SYNTAX_ERROR,
  INVALID_STATE,
  INVALID_MODIFICATION,
  NETWORK_ERROR,
  RESOURCE_EXHAUSTED,
  INTERNAL_ERROR,
  OPERATION_ERROR_WITH_DATA,
};

const char* ToString(RTCErrorType error);

class RTCError {
 public:
  RTCError() : type_(RTCErrorType::NONE) {}
  explicit RTCError(RTCErrorType type) : type_(type) {}
  RTCError(RTCErrorType type, std::string message)
      : type_(type), message_(std::move(message)) {}

  static RTCError OK() { return RTCError(); }

  bool ok() const { return type_ == RTCErrorType::NONE; }
  RTCErrorType type() const { return type_; }
  void set_type(RTCErrorType type) { type_ = type; }

  const char* message() const { return message_.c_str(); }
  void set_message(std::string message) { message_ = std::move(message); }

 private:
  RTCErrorType type_;
  std::string message_;
};

template <typename T>
class RTCErrorOr {
 public:
  RTCErrorOr() : error_(RTCErrorType::INTERNAL_ERROR) {}
  RTCErrorOr(RTCError&& error) : error_(std::move(error)) {}      // NOLINT
  RTCErrorOr(const RTCError& error) : error_(error) {}            // NOLINT
  RTCErrorOr(T&& value)                                           // NOLINT
      : error_(RTCError::OK()), value_(std::move(value)) {}
  RTCErrorOr(const T& value)                                      // NOLINT
      : error_(RTCError::OK()), value_(value) {}

  template <typename U>
  RTCErrorOr(U&& value)  // NOLINT
      : error_(RTCError::OK()), value_(std::forward<U>(value)) {}

  bool ok() const { return error_.ok(); }
  explicit operator bool() const { return ok(); }

  const RTCError& error() const { return error_; }
  RTCError MoveError() { return std::move(error_); }

  const T& value() const { return *value_; }
  T& value() { return *value_; }
  T MoveValue() { return std::move(*value_); }

 private:
  RTCError error_;
  absl::optional<T> value_;
};

}  // namespace webrtc
