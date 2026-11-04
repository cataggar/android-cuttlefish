#pragma once

#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace absl {

namespace numbers_internal {

inline std::string_view TrimAsciiWhitespace(std::string_view sv) {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
    sv.remove_suffix(1);
  }
  return sv;
}

template <typename IntType>
bool ParseIntImpl(std::string_view str, IntType* out, int base) {
  if (!out) return false;
  str = TrimAsciiWhitespace(str);
  if (str.empty()) return false;

  bool negative = false;
  if (str.front() == '+') {
    str.remove_prefix(1);
  } else if (str.front() == '-') {
    if constexpr (std::is_unsigned_v<IntType>) {
      return false;
    }
    negative = true;
    str.remove_prefix(1);
  }
  if (str.empty()) return false;

  if (base == 16 && str.size() >= 2 && str[0] == '0' &&
      (str[1] == 'x' || str[1] == 'X')) {
    str.remove_prefix(2);
    if (str.empty()) return false;
  }

  std::string tmp;
  if (negative) {
    tmp.reserve(str.size() + 1);
    tmp.push_back('-');
    tmp.append(str);
    str = tmp;
  }

  IntType val{};
  auto [ptr, ec] =
      std::from_chars(str.data(), str.data() + str.size(), val, base);
  if (ec != std::errc{} || ptr != str.data() + str.size()) {
    return false;
  }
  *out = val;
  return true;
}

}  // namespace numbers_internal

template <typename int_type>
bool SimpleAtoi(std::string_view str, int_type* out) {
  return numbers_internal::ParseIntImpl(str, out, 10);
}

template <typename int_type>
bool SimpleHexAtoi(std::string_view str, int_type* out) {
  return numbers_internal::ParseIntImpl(str, out, 16);
}

inline bool SimpleAtod(std::string_view str, double* out) {
  if (!out) return false;
  str = numbers_internal::TrimAsciiWhitespace(str);
  if (str.empty()) return false;
  std::string s(str);
  char* endptr = nullptr;
  errno = 0;
  double val = std::strtod(s.c_str(), &endptr);
  if (errno == ERANGE || endptr != s.c_str() + s.size()) return false;
  *out = val;
  return true;
}

inline bool SimpleAtof(std::string_view str, float* out) {
  if (!out) return false;
  str = numbers_internal::TrimAsciiWhitespace(str);
  if (str.empty()) return false;
  std::string s(str);
  char* endptr = nullptr;
  errno = 0;
  float val = std::strtof(s.c_str(), &endptr);
  if (errno == ERANGE || endptr != s.c_str() + s.size()) return false;
  *out = val;
  return true;
}

inline bool SimpleAtob(std::string_view str, bool* out) {
  if (!out) return false;
  str = numbers_internal::TrimAsciiWhitespace(str);
  if (str.empty()) return false;
  std::string lower;
  lower.reserve(str.size());
  for (unsigned char c : str) {
    lower.push_back(static_cast<char>(std::tolower(c)));
  }
  if (lower == "true" || lower == "t" || lower == "yes" || lower == "y" ||
      lower == "1") {
    *out = true;
    return true;
  }
  if (lower == "false" || lower == "f" || lower == "no" || lower == "n" ||
      lower == "0") {
    *out = false;
    return true;
  }
  return false;
}

inline bool safe_strto32(std::string_view str, int32_t* out) {
  return SimpleAtoi(str, out);
}
inline bool safe_strtou32(std::string_view str, uint32_t* out) {
  return SimpleAtoi(str, out);
}
inline bool safe_strto64(std::string_view str, int64_t* out) {
  return SimpleAtoi(str, out);
}
inline bool safe_strtou64(std::string_view str, uint64_t* out) {
  return SimpleAtoi(str, out);
}

}  // namespace absl
