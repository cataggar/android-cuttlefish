#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "android-base/parsebool.h"
#include "android-base/parseint.h"

namespace android {
namespace base {

inline std::string GetProperty(const std::string& key,
                               const std::string& default_value) {
  const char* val = std::getenv(key.c_str());
  return val ? std::string(val) : default_value;
}

inline bool GetBoolProperty(const std::string& key, bool default_value) {
  switch (ParseBool(GetProperty(key, ""))) {
    case ParseBoolResult::kTrue:
      return true;
    case ParseBoolResult::kFalse:
      return false;
    case ParseBoolResult::kError:
      return default_value;
  }
  return default_value;
}

template <typename T>
T GetIntProperty(const std::string& key, T default_value,
                 T min = std::numeric_limits<T>::min(),
                 T max = std::numeric_limits<T>::max()) {
  T result;
  std::string value = GetProperty(key, "");
  if (!value.empty() && ParseInt(value, &result, min, max)) return result;
  return default_value;
}

template <typename T>
T GetUintProperty(const std::string& key, T default_value,
                  T max = std::numeric_limits<T>::max()) {
  T result;
  std::string value = GetProperty(key, "");
  if (!value.empty() && ParseUint(value, &result, max)) return result;
  return default_value;
}

inline bool SetProperty(const std::string& key, const std::string& value) {
  return setenv(key.c_str(), value.c_str(), 1) == 0;
}

inline bool WaitForProperty(
    const std::string& key, const std::string& expected_value,
    std::chrono::milliseconds relative_timeout = std::chrono::milliseconds::max()) {
  (void)relative_timeout;
  return GetProperty(key, "") == expected_value;
}

}  // namespace base
}  // namespace android
