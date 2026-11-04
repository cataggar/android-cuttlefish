#pragma once

#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace android {
namespace base {

template <typename T>
bool ParseUint(const char* s, T* out,
               T max = std::numeric_limits<T>::max(),
               bool allow_suffixes = false) {
  static_assert(std::is_unsigned_v<T>, "ParseUint requires unsigned type");
  if (s == nullptr) return false;
  while (std::isspace(static_cast<unsigned char>(*s))) {
    s++;
  }
  if (*s == '\0' || *s == '-') return false;

  int base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 16 : 10;
  errno = 0;
  char* end = nullptr;
  unsigned long long result = std::strtoull(s, &end, base);
  if (errno != 0 || end == s) return false;
  if (*end != '\0') {
    const char* suffixes = "bkmgtpe";
    const char* suffix = nullptr;
    if (!allow_suffixes ||
        (suffix = std::strchr(suffixes, std::tolower(static_cast<unsigned char>(*end)))) ==
            nullptr ||
        *(end + 1) != '\0') {
      return false;
    }
    unsigned long long mul = 1ULL << (10 * (suffix - suffixes));
    if (__builtin_umulll_overflow(result, mul, &result)) {
      return false;
    }
  }
  if (max < result) return false;
  if (out != nullptr) *out = static_cast<T>(result);
  return true;
}

template <typename T>
bool ParseUint(const std::string& s, T* out,
               T max = std::numeric_limits<T>::max(),
               bool allow_suffixes = false) {
  return ParseUint(s.c_str(), out, max, allow_suffixes);
}

template <typename T>
bool ParseUint(std::string_view s, T* out,
               T max = std::numeric_limits<T>::max(),
               bool allow_suffixes = false) {
  return ParseUint(std::string(s), out, max, allow_suffixes);
}

template <typename T>
bool ParseByteCount(const char* s, T* out,
                    T max = std::numeric_limits<T>::max()) {
  return ParseUint(s, out, max, true);
}

template <typename T>
bool ParseByteCount(const std::string& s, T* out,
                    T max = std::numeric_limits<T>::max()) {
  return ParseByteCount(s.c_str(), out, max);
}

template <typename T>
bool ParseInt(const char* s, T* out,
              T min = std::numeric_limits<T>::min(),
              T max = std::numeric_limits<T>::max()) {
  static_assert(std::is_signed_v<T>, "ParseInt requires signed type");
  if (s == nullptr) return false;
  while (std::isspace(static_cast<unsigned char>(*s))) {
    s++;
  }
  if (*s == '\0') return false;

  int base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 16 : 10;
  if ((s[0] == '-' || s[0] == '+') && s[1] == '0' &&
      (s[2] == 'x' || s[2] == 'X')) {
    base = 16;
  }
  errno = 0;
  char* end = nullptr;
  long long result = std::strtoll(s, &end, base);
  if (errno != 0 || s == end || *end != '\0') return false;
  if (result < min || max < result) return false;
  if (out != nullptr) *out = static_cast<T>(result);
  return true;
}

template <typename T>
bool ParseInt(const std::string& s, T* out,
              T min = std::numeric_limits<T>::min(),
              T max = std::numeric_limits<T>::max()) {
  return ParseInt(s.c_str(), out, min, max);
}

template <typename T>
bool ParseInt(std::string_view s, T* out,
              T min = std::numeric_limits<T>::min(),
              T max = std::numeric_limits<T>::max()) {
  return ParseInt(std::string(s), out, min, max);
}

}  // namespace base
}  // namespace android
