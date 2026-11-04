#pragma once

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

namespace android {
namespace base {

inline bool ParseDouble(const char* s, double* out,
                        double min = std::numeric_limits<double>::lowest(),
                        double max = std::numeric_limits<double>::max()) {
  if (!s || *s == '\0') return false;
  errno = 0;
  char* end = nullptr;
  double result = std::strtod(s, &end);
  if (errno != 0 || s == end || *end != '\0') return false;
  if (result < min || max < result) return false;
  if (out) *out = result;
  return true;
}

inline bool ParseDouble(const std::string& s, double* out,
                        double min = std::numeric_limits<double>::lowest(),
                        double max = std::numeric_limits<double>::max()) {
  return ParseDouble(s.c_str(), out, min, max);
}

inline bool ParseFloat(const char* s, float* out,
                       float min = std::numeric_limits<float>::lowest(),
                       float max = std::numeric_limits<float>::max()) {
  if (!s || *s == '\0') return false;
  errno = 0;
  char* end = nullptr;
  float result = std::strtof(s, &end);
  if (errno != 0 || s == end || *end != '\0') return false;
  if (result < min || max < result) return false;
  if (out) *out = result;
  return true;
}

inline bool ParseFloat(const std::string& s, float* out,
                       float min = std::numeric_limits<float>::lowest(),
                       float max = std::numeric_limits<float>::max()) {
  return ParseFloat(s.c_str(), out, min, max);
}

}  // namespace base
}  // namespace android
