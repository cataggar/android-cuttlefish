#pragma once

#include <utility>
#include "absl/log/check.h"

namespace absl {
namespace log_internal {
template <typename T>
T&& DieIfNull(const char* file, int line, const char* exprtext, T&& t) {
  if (t == nullptr) {
    LogMessageFatal(file, line) << "'" << exprtext << "' Must be non NULL";
  }
  return std::forward<T>(t);
}
}  // namespace log_internal
}  // namespace absl

#define ABSL_DIE_IF_NULL(val) \
  ::absl::log_internal::DieIfNull(__FILE__, __LINE__, #val, (val))
