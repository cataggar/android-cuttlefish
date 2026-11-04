#pragma once

#include <string_view>

namespace android {
namespace base {

enum class ParseBoolResult {
  kError,
  kFalse,
  kTrue,
};

inline ParseBoolResult ParseBool(std::string_view s) {
  if (s == "1" || s == "y" || s == "yes" || s == "on" || s == "true") {
    return ParseBoolResult::kTrue;
  }
  if (s == "0" || s == "n" || s == "no" || s == "off" || s == "false") {
    return ParseBoolResult::kFalse;
  }
  return ParseBoolResult::kError;
}

}  // namespace base
}  // namespace android
