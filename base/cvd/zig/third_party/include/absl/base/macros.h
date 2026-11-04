#pragma once

#include <cstddef>
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/optimization.h"

#define ABSL_ARRAYSIZE(array) (sizeof(::absl::macros_internal::ArraySizeHelper(array)))

namespace absl {
namespace macros_internal {
template <typename T, size_t N>
auto ArraySizeHelper(const T (&array)[N]) -> char (&)[N];
}  // namespace macros_internal
}  // namespace absl
