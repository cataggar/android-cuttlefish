#pragma once

#include <cstdlib>
#include <optional>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"

namespace absl {

template <typename T>
class StatusOr {
 public:
  using value_type = T;

  StatusOr() : status_(StatusCode::kUnknown, "Uninitialized StatusOr") {}

  StatusOr(const Status& status) : status_(status) {}
  StatusOr(Status&& status) : status_(std::move(status)) {}

  template <typename U = T,
            std::enable_if_t<!std::is_same_v<std::remove_cvref_t<U>, StatusOr> &&
                                 !std::is_same_v<std::remove_cvref_t<U>, Status> &&
                                 std::is_constructible_v<T, U>,
                             int> = 0>
  StatusOr(U&& v) : status_(OkStatus()), value_(std::forward<U>(v)) {}

  StatusOr(const StatusOr&) = default;
  StatusOr(StatusOr&&) noexcept = default;
  StatusOr& operator=(const StatusOr&) = default;
  StatusOr& operator=(StatusOr&&) noexcept = default;

  bool ok() const noexcept { return status_.ok() && value_.has_value(); }
  const Status& status() const& noexcept { return status_; }
  Status status() && { return std::move(status_); }

  const T& value() const& {
    if (!ok()) std::abort();
    return *value_;
  }
  T& value() & {
    if (!ok()) std::abort();
    return *value_;
  }
  T&& value() && {
    if (!ok()) std::abort();
    return std::move(*value_);
  }
  const T&& value() const&& {
    if (!ok()) std::abort();
    return std::move(*value_);
  }

  const T& operator*() const& { return *value_; }
  T& operator*() & { return *value_; }
  T&& operator*() && { return std::move(*value_); }
  const T&& operator*() const&& { return std::move(*value_); }

  const T* operator->() const { return std::addressof(*value_); }
  T* operator->() { return std::addressof(*value_); }

  template <typename U>
  T value_or(U&& default_value) const& {
    return ok() ? *value_ : static_cast<T>(std::forward<U>(default_value));
  }
  template <typename U>
  T value_or(U&& default_value) && {
    return ok() ? std::move(*value_)
                : static_cast<T>(std::forward<U>(default_value));
  }

 private:
  Status status_;
  std::optional<T> value_;
};

}  // namespace absl
