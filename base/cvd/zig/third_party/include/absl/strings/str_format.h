#pragma once

#include <cstdio>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace absl {

namespace str_format_internal {

template <typename T>
auto ToPrintfArg(const T& arg) {
  using Decayed = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<Decayed, std::string>) {
    return arg.c_str();
  } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
    return std::string(arg).c_str();
  } else if constexpr (std::is_enum_v<Decayed>) {
    return static_cast<std::underlying_type_t<Decayed>>(arg);
  } else {
    return arg;
  }
}

template <typename T>
struct ArgHolder {
  T val;
  auto get() const { return val; }
};

template <>
struct ArgHolder<std::string> {
  std::string val;
  const char* get() const { return val.c_str(); }
};

template <>
struct ArgHolder<std::string_view> {
  std::string val;
  explicit ArgHolder(std::string_view sv) : val(sv) {}
  const char* get() const { return val.c_str(); }
};

template <typename T>
auto MakeHolder(const T& v) {
  using Decayed = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<Decayed, std::string>) {
    return ArgHolder<std::string>{v};
  } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
    return ArgHolder<std::string_view>(v);
  } else if constexpr (std::is_enum_v<Decayed>) {
    using U = std::underlying_type_t<Decayed>;
    return ArgHolder<U>{static_cast<U>(v)};
  } else if constexpr (std::is_array_v<Decayed> ||
                       std::is_same_v<Decayed, const char*> ||
                       std::is_same_v<Decayed, char*>) {
    return ArgHolder<const char*>{v};
  } else {
    return ArgHolder<Decayed>{v};
  }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma clang diagnostic ignored "-Wformat-nonliteral"

template <typename... Holders>
std::string FormatWithHolders(const char* fmt, const Holders&... holders) {
  if constexpr (sizeof...(Holders) == 0) {
    char stack_buf[512];
    int n = std::snprintf(stack_buf, sizeof(stack_buf), "%s", fmt);
    if (n < 0) return {};
    return std::string( fmt);
  } else {
    char stack_buf[512];
    int n = std::snprintf(stack_buf, sizeof(stack_buf), fmt, holders.get()...);
    if (n < 0) return {};
    if (static_cast<size_t>(n) < sizeof(stack_buf)) {
      return std::string(stack_buf, static_cast<size_t>(n));
    }
    std::string out(static_cast<size_t>(n) + 1, '\0');
    std::snprintf(out.data(), out.size(), fmt, holders.get()...);
    out.resize(static_cast<size_t>(n));
    return out;
  }
}

#pragma clang diagnostic pop

}  // namespace str_format_internal

template <typename... Args>
using FormatSpec = std::string_view;

template <typename... Args>
std::string StrFormat(std::string_view format_str, const Args&... args) {
  std::string fmt_copy(format_str);
  return str_format_internal::FormatWithHolders(
      fmt_copy.c_str(), str_format_internal::MakeHolder(args)...);
}

template <typename... Args>
std::string& StrAppendFormat(std::string* dst, std::string_view format_str,
                             const Args&... args) {
  *dst += StrFormat(format_str, args...);
  return *dst;
}

template <typename... Args>
std::string StreamFormat(std::string_view format_str, const Args&... args) {
  return StrFormat(format_str, args...);
}

template <typename... Args>
int SNPrintF(char* output, size_t size, std::string_view format_str,
             const Args&... args) {
  std::string s = StrFormat(format_str, args...);
  if (size > 0) {
    size_t copy_len = s.size() < (size - 1) ? s.size() : (size - 1);
    for (size_t i = 0; i < copy_len; ++i) output[i] = s[i];
    output[copy_len] = '\0';
  }
  return static_cast<int>(s.size());
}

template <typename... Args>
int FPrintF(std::FILE* output, std::string_view format_str,
            const Args&... args) {
  std::string s = StrFormat(format_str, args...);
  return static_cast<int>(std::fwrite(s.data(), 1, s.size(), output));
}

template <typename... Args>
int PrintF(std::string_view format_str, const Args&... args) {
  return FPrintF(stdout, format_str, args...);
}

}  // namespace absl
