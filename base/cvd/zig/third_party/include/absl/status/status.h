#pragma once

#include <cerrno>
#include <cstring>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace absl {

enum class StatusCode : int {
  kOk = 0,
  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
  kInternal = 13,
  kUnavailable = 14,
  kDataLoss = 15,
  kUnauthenticated = 16,
  kDoNotUseReservedForFutureExpansionUseDefaultInSwitchInstead_ = 20
};

inline std::string StatusCodeToString(StatusCode code) {
  switch (code) {
    case StatusCode::kOk: return "OK";
    case StatusCode::kCancelled: return "CANCELLED";
    case StatusCode::kUnknown: return "UNKNOWN";
    case StatusCode::kInvalidArgument: return "INVALID_ARGUMENT";
    case StatusCode::kDeadlineExceeded: return "DEADLINE_EXCEEDED";
    case StatusCode::kNotFound: return "NOT_FOUND";
    case StatusCode::kAlreadyExists: return "ALREADY_EXISTS";
    case StatusCode::kPermissionDenied: return "PERMISSION_DENIED";
    case StatusCode::kResourceExhausted: return "RESOURCE_EXHAUSTED";
    case StatusCode::kFailedPrecondition: return "FAILED_PRECONDITION";
    case StatusCode::kAborted: return "ABORTED";
    case StatusCode::kOutOfRange: return "OUT_OF_RANGE";
    case StatusCode::kUnimplemented: return "UNIMPLEMENTED";
    case StatusCode::kInternal: return "INTERNAL";
    case StatusCode::kUnavailable: return "UNAVAILABLE";
    case StatusCode::kDataLoss: return "DATA_LOSS";
    case StatusCode::kUnauthenticated: return "UNAUTHENTICATED";
    default: return "UNKNOWN";
  }
}

inline std::ostream& operator<<(std::ostream& os, StatusCode code) {
  return os << StatusCodeToString(code);
}

enum class StatusToStringMode : int {
  kWithNoExtraData = 0,
  kWithPayload = 1 << 0,
  kWithEverything = ~kWithNoExtraData,
  kDefault = kWithPayload,
};

class Status {
 public:
  Status() noexcept : code_(StatusCode::kOk) {}
  Status(StatusCode code, std::string_view msg) : code_(code), msg_(msg) {}

  Status(const Status&) = default;
  Status(Status&&) noexcept = default;
  Status& operator=(const Status&) = default;
  Status& operator=(Status&&) noexcept = default;

  bool ok() const noexcept { return code_ == StatusCode::kOk; }
  StatusCode code() const noexcept { return code_; }
  int raw_code() const noexcept { return static_cast<int>(code_); }
  std::string_view message() const noexcept { return msg_; }

  void Update(const Status& new_status) {
    if (ok()) *this = new_status;
  }
  void Update(Status&& new_status) {
    if (ok()) *this = std::move(new_status);
  }

  void IgnoreError() const noexcept {}

  std::string ToString(
      StatusToStringMode = StatusToStringMode::kDefault) const {
    if (ok()) return "OK";
    if (msg_.empty()) return StatusCodeToString(code_);
    return StatusCodeToString(code_) + ": " + msg_;
  }

  friend bool operator==(const Status& lhs, const Status& rhs) {
    return lhs.code_ == rhs.code_ && lhs.msg_ == rhs.msg_;
  }
  friend bool operator!=(const Status& lhs, const Status& rhs) {
    return !(lhs == rhs);
  }
  friend std::ostream& operator<<(std::ostream& os, const Status& s) {
    return os << s.ToString();
  }

 private:
  StatusCode code_;
  std::string msg_;
};

inline Status OkStatus() { return Status(); }
inline Status CancelledError(std::string_view msg = "") {
  return Status(StatusCode::kCancelled, msg);
}
inline Status UnknownError(std::string_view msg = "") {
  return Status(StatusCode::kUnknown, msg);
}
inline Status InvalidArgumentError(std::string_view msg = "") {
  return Status(StatusCode::kInvalidArgument, msg);
}
inline Status DeadlineExceededError(std::string_view msg = "") {
  return Status(StatusCode::kDeadlineExceeded, msg);
}
inline Status NotFoundError(std::string_view msg = "") {
  return Status(StatusCode::kNotFound, msg);
}
inline Status AlreadyExistsError(std::string_view msg = "") {
  return Status(StatusCode::kAlreadyExists, msg);
}
inline Status PermissionDeniedError(std::string_view msg = "") {
  return Status(StatusCode::kPermissionDenied, msg);
}
inline Status ResourceExhaustedError(std::string_view msg = "") {
  return Status(StatusCode::kResourceExhausted, msg);
}
inline Status FailedPreconditionError(std::string_view msg = "") {
  return Status(StatusCode::kFailedPrecondition, msg);
}
inline Status AbortedError(std::string_view msg = "") {
  return Status(StatusCode::kAborted, msg);
}
inline Status OutOfRangeError(std::string_view msg = "") {
  return Status(StatusCode::kOutOfRange, msg);
}
inline Status UnimplementedError(std::string_view msg = "") {
  return Status(StatusCode::kUnimplemented, msg);
}
inline Status InternalError(std::string_view msg = "") {
  return Status(StatusCode::kInternal, msg);
}
inline Status UnavailableError(std::string_view msg = "") {
  return Status(StatusCode::kUnavailable, msg);
}
inline Status DataLossError(std::string_view msg = "") {
  return Status(StatusCode::kDataLoss, msg);
}
inline Status UnauthenticatedError(std::string_view msg = "") {
  return Status(StatusCode::kUnauthenticated, msg);
}

inline StatusCode ErrnoToStatusCode(int error_number) {
  switch (error_number) {
    case 0:
      return StatusCode::kOk;
    case EINVAL:
    case ENAMETOOLONG:
    case E2BIG:
    case EDESTADDRREQ:
    case EDOM:
    case EFAULT:
    case EILSEQ:
    case ENOPROTOOPT:
    case ENOSTR:
    case ENOTSOCK:
    case ENOTTY:
    case EPROTOTYPE:
    case ESPIPE:
      return StatusCode::kInvalidArgument;
    case ETIMEDOUT:
    case ETIME:
      return StatusCode::kDeadlineExceeded;
    case ENODEV:
    case ENOENT:
#ifdef ENOMEDIUM
    case ENOMEDIUM:
#endif
    case ENXIO:
    case ESRCH:
      return StatusCode::kNotFound;
    case EEXIST:
    case EADDRNOTAVAIL:
    case EAFNOSUPPORT:
    case EDQUOT:
    case ENOTEMPTY:
      return StatusCode::kAlreadyExists;
    case EPERM:
    case EACCES:
#ifdef ENOKEY
    case ENOKEY:
#endif
    case EROFS:
      return StatusCode::kPermissionDenied;
    case ENOTSUP:
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
    case EOPNOTSUPP:
#endif
    case ENOSYS:
      return StatusCode::kUnimplemented;
    case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
    case EBUSY:
    case ECONNREFUSED:
    case ECONNRESET:
    case EINTR:
    case ENETDOWN:
    case ENETRESET:
    case ENETUNREACH:
    case ENOLCK:
    case ENOLINK:
      return StatusCode::kUnavailable;
    default:
      return StatusCode::kUnknown;
  }
}

inline Status ErrnoToStatus(int error_number, std::string_view message) {
  if (error_number == 0) return OkStatus();
  std::string full_msg(message);
  if (!full_msg.empty()) full_msg += ": ";
  const char* err_str = std::strerror(error_number);
  if (err_str) full_msg += err_str;
  return Status(ErrnoToStatusCode(error_number), full_msg);
}

}  // namespace absl
