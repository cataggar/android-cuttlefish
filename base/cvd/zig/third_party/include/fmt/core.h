#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#define FMT_VERSION 100201
#define FMT_STRING(s) s
#define FMT_BEGIN_NAMESPACE namespace fmt {
#define FMT_END_NAMESPACE }
#define FMT_CONSTEXPR constexpr
#define FMT_INLINE inline

namespace fmt {

template <typename Char>
class basic_string_view {
 public:
  using value_type = Char;
  using iterator = const Char*;
  using const_iterator = const Char*;

  constexpr basic_string_view() noexcept : data_(nullptr), size_(0) {}
  constexpr basic_string_view(const Char* s, size_t count) noexcept
      : data_(s), size_(count) {}
  constexpr basic_string_view(const Char* s) noexcept
      : data_(s), size_(s ? std::char_traits<Char>::length(s) : 0) {}
  template <typename Traits, typename Alloc>
  constexpr basic_string_view(
      const std::basic_string<Char, Traits, Alloc>& s) noexcept
      : data_(s.data()), size_(s.size()) {}
  template <typename Traits>
  constexpr basic_string_view(
      std::basic_string_view<Char, Traits> s) noexcept
      : data_(s.data()), size_(s.size()) {}

  constexpr const Char* data() const noexcept { return data_; }
  constexpr size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }
  constexpr const_iterator begin() const noexcept { return data_; }
  constexpr const_iterator end() const noexcept { return data_ + size_; }
  constexpr const Char& operator[](size_t pos) const noexcept {
    return data_[pos];
  }
  constexpr void remove_prefix(size_t n) noexcept {
    data_ += n;
    size_ -= n;
  }
  constexpr operator std::basic_string_view<Char>() const noexcept {
    return std::basic_string_view<Char>(data_, size_);
  }

 private:
  const Char* data_;
  size_t size_;
};

using string_view = basic_string_view<char>;

template <typename Char>
struct runtime_format_string {
  basic_string_view<Char> str;
};

inline runtime_format_string<char> runtime(string_view s) { return {s}; }

template <typename Char, typename... Args>
class basic_format_string {
 public:
  template <typename S,
            std::enable_if_t<std::is_convertible_v<const S&, basic_string_view<Char>>,
                             int> = 0>
  constexpr basic_format_string(const S& s) : str_(s) {}
  constexpr basic_format_string(runtime_format_string<Char> r) : str_(r.str) {}

  constexpr operator basic_string_view<Char>() const noexcept { return str_; }
  constexpr basic_string_view<Char> get() const noexcept { return str_; }

 private:
  basic_string_view<Char> str_;
};

template <typename... Args>
using format_string = basic_format_string<char, std::type_identity_t<Args>...>;

class format_parse_context {
 public:
  using char_type = char;
  using iterator = const char*;
  using const_iterator = const char*;

  constexpr explicit format_parse_context(string_view format_str) noexcept
      : begin_(format_str.begin()), end_(format_str.end()) {}

  constexpr const_iterator begin() const noexcept { return begin_; }
  constexpr const_iterator end() const noexcept { return end_; }
  constexpr void advance_to(const_iterator it) noexcept { begin_ = it; }

 private:
  const_iterator begin_;
  const_iterator end_;
};

class format_context {
 public:
  using char_type = char;
  using iterator = std::back_insert_iterator<std::string>;

  explicit format_context(std::string& buf)
      : buf_(&buf), out_(std::back_inserter(buf)) {}

  iterator out() { return out_; }
  void advance_to(iterator it) { out_ = it; }
  std::string& buf() { return *buf_; }

 private:
  std::string* buf_;
  iterator out_;
};

template <typename T, typename Char = char, typename Enable = void>
struct formatter;

namespace detail {

template <typename T>
concept HasFormatAs = requires(const T& v) { format_as(v); };

template <typename T>
concept Streamable = requires(std::ostream& os, const T& v) { os << v; };

struct ParsedSpec {
  char fill = ' ';
  char align = '\0';  // '<', '>', '^'
  char sign = '\0';   // '+', '-', ' '
  bool alt = false;   // '#'
  bool zero = false;  // '0'
  int width = 0;
  int precision = -1;
  char type = '\0';
  std::string raw;
};

inline ParsedSpec parse_std_spec(format_parse_context& ctx) {
  ParsedSpec spec;
  auto it = ctx.begin();
  auto end = ctx.end();
  const char* start = it;
  while (it != end && *it != '}') {
    ++it;
  }
  spec.raw.assign(start, it);
  ctx.advance_to(it);

  const char* p = start;
  const char* pend = it;
  if (pend - p >= 2 && (p[1] == '<' || p[1] == '>' || p[1] == '^')) {
    spec.fill = p[0];
    spec.align = p[1];
    p += 2;
  } else if (p != pend && (p[0] == '<' || p[0] == '>' || p[0] == '^')) {
    spec.align = p[0];
    p += 1;
  }
  if (p != pend && (p[0] == '+' || p[0] == '-' || p[0] == ' ')) {
    spec.sign = *p++;
  }
  if (p != pend && *p == '#') {
    spec.alt = true;
    ++p;
  }
  if (p != pend && *p == '0') {
    spec.zero = true;
    if (spec.align == '\0') {
      spec.fill = '0';
      spec.align = '>';
    }
    ++p;
  }
  while (p != pend && *p >= '0' && *p <= '9') {
    spec.width = spec.width * 10 + (*p - '0');
    ++p;
  }
  if (p != pend && *p == '.') {
    ++p;
    spec.precision = 0;
    while (p != pend && *p >= '0' && *p <= '9') {
      spec.precision = spec.precision * 10 + (*p - '0');
      ++p;
    }
  }
  if (p != pend) {
    spec.type = *p;
  }
  return spec;
}

inline std::string apply_padding(std::string s, const ParsedSpec& spec,
                                 bool is_numeric) {
  if (spec.width <= 0 || static_cast<int>(s.size()) >= spec.width) {
    return s;
  }
  size_t pad = static_cast<size_t>(spec.width) - s.size();
  char align = spec.align;
  if (align == '\0') {
    align = is_numeric ? '>' : '<';
  }
  if (align == '<') {
    s.append(pad, spec.fill);
    return s;
  } else if (align == '^') {
    size_t left = pad / 2;
    size_t right = pad - left;
    return std::string(left, spec.fill) + s + std::string(right, spec.fill);
  } else {
    if (is_numeric && spec.fill == '0' && !s.empty()) {
      size_t prefix_len = 0;
      if (s[0] == '-' || s[0] == '+' || s[0] == ' ') prefix_len = 1;
      if (s.size() >= prefix_len + 2 && s[prefix_len] == '0' &&
          (s[prefix_len + 1] == 'x' || s[prefix_len + 1] == 'X' ||
           s[prefix_len + 1] == 'b' || s[prefix_len + 1] == 'B')) {
        prefix_len += 2;
      }
      if (prefix_len > 0) {
        return s.substr(0, prefix_len) + std::string(pad, '0') +
               s.substr(prefix_len);
      }
    }
    return std::string(pad, spec.fill) + s;
  }
}

template <typename Int>
std::string format_integer(Int val, const ParsedSpec& spec) {
  if (spec.type == 'c') {
    return std::string(1, static_cast<char>(val));
  }
  using UInt = std::make_unsigned_t<
      std::conditional_t<std::is_same_v<Int, bool>, unsigned int, Int>>;
  bool negative = false;
  UInt uval;
  if constexpr (std::is_signed_v<Int>) {
    if (val < 0 && spec.type != 'x' && spec.type != 'X' && spec.type != 'o' &&
        spec.type != 'b' && spec.type != 'B' && spec.type != 'u') {
      negative = true;
      uval = static_cast<UInt>(-(val + 1)) + 1;
    } else {
      uval = static_cast<UInt>(val);
    }
  } else {
    uval = static_cast<UInt>(val);
  }

  int base = 10;
  bool upper = false;
  std::string prefix;
  if (spec.type == 'x' || spec.type == 'X') {
    base = 16;
    upper = (spec.type == 'X');
    if (spec.alt && uval != 0) prefix = upper ? "0X" : "0x";
  } else if (spec.type == 'o') {
    base = 8;
    if (spec.alt && uval != 0) prefix = "0";
  } else if (spec.type == 'b' || spec.type == 'B') {
    base = 2;
    if (spec.alt && uval != 0) prefix = (spec.type == 'B') ? "0B" : "0b";
  }

  char buf[128];
  char* p = buf + sizeof(buf);
  const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  if (uval == 0) {
    *--p = '0';
  } else {
    while (uval > 0) {
      *--p = digits[uval % static_cast<UInt>(base)];
      uval /= static_cast<UInt>(base);
    }
  }

  std::string sign_str;
  if (negative) {
    sign_str = "-";
  } else if (spec.sign == '+') {
    sign_str = "+";
  } else if (spec.sign == ' ') {
    sign_str = " ";
  }

  std::string s = sign_str + prefix + std::string(p, static_cast<size_t>((buf + sizeof(buf)) - p));
  return apply_padding( std::move(s), spec, true);
}

inline std::string format_float(double val, const ParsedSpec& spec) {
  char fmt_buf[32];
  char* fp = fmt_buf;
  *fp++ = '%';
  if (spec.sign == '+') *fp++ = '+';
  else if (spec.sign == ' ') *fp++ = ' ';
  if (spec.alt) *fp++ = '#';
  if (spec.precision >= 0) {
    fp += std::snprintf(fp, sizeof(fmt_buf) - static_cast<size_t>(fp - fmt_buf),
                        ".%d", spec.precision);
  }
  char t = spec.type ? spec.type : 'g';
  *fp++ = t;
  *fp = '\0';

  char out_buf[256];
  if (spec.type == '\0' && spec.precision < 0) {
    std::ostringstream oss;
    oss << val;
    return apply_padding(oss.str(), spec, true);
  }
  std::snprintf(out_buf, sizeof(out_buf), fmt_buf, val);
  return apply_padding(std::string(out_buf), spec, true);
}

}  // namespace detail

template <typename T, typename Char, typename Enable>
struct formatter {
  detail::ParsedSpec spec_;

  constexpr auto parse(format_parse_context& ctx) {
    spec_ = detail::parse_std_spec(ctx);
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const T& val, FormatContext& ctx) const {
    using Decayed = std::remove_cvref_t<T>;
    std::string s;
    if constexpr (detail::HasFormatAs<Decayed>) {
      auto converted = format_as(val);
      formatter<std::remove_cvref_t<decltype(converted)>, Char> sub;
      sub.spec_ = spec_;
      return sub.format(converted, ctx);
    } else if constexpr (std::is_same_v<Decayed, bool>) {
      if (spec_.type == 'd' || spec_.type == 'x' || spec_.type == 'X' ||
          spec_.type == 'o' || spec_.type == 'b') {
        s = detail::format_integer(static_cast<int>(val), spec_);
      } else {
        s = detail::apply_padding(val ? "true" : "false", spec_, false);
      }
    } else if constexpr (std::is_same_v<Decayed, char>) {
      if (spec_.type == 'd' || spec_.type == 'x' || spec_.type == 'X' ||
          spec_.type == 'o' || spec_.type == 'b' || spec_.type == 'u') {
        s = detail::format_integer(static_cast<int>(val), spec_);
      } else {
        s = detail::apply_padding(std::string(1, val), spec_, false);
      }
    } else if constexpr (std::is_same_v<Decayed, signed char>) {
      s = detail::format_integer(static_cast<int>(val), spec_);
    } else if constexpr (std::is_same_v<Decayed, unsigned char>) {
      s = detail::format_integer(static_cast<unsigned int>(val), spec_);
    } else if constexpr (std::is_integral_v<Decayed>) {
      s = detail::format_integer(val, spec_);
    } else if constexpr (std::is_floating_point_v<Decayed>) {
      s = detail::format_float(static_cast<double>(val), spec_);
    } else if constexpr (std::is_enum_v<Decayed>) {
      if constexpr (detail::Streamable<Decayed>) {
        std::ostringstream oss;
        oss << val;
        s = detail::apply_padding(oss.str(), spec_, false);
      } else {
        using U = std::underlying_type_t<Decayed>;
        s = detail::format_integer(static_cast<U>(val), spec_);
      }
    } else if constexpr (std::is_convertible_v<Decayed, std::string_view>) {
      std::string_view sv(val);
      if (spec_.precision >= 0 &&
          static_cast<size_t>(spec_.precision) < sv.size()) {
        sv = sv.substr(0, static_cast<size_t>(spec_.precision));
      }
      s = detail::apply_padding(std::string(sv), spec_, false);
    } else if constexpr (std::is_same_v<Decayed, const char*> ||
                         std::is_same_v<Decayed, char*>) {
      std::string_view sv = val ? std::string_view(val) : std::string_view("(null)");
      if (spec_.precision >= 0 &&
          static_cast<size_t>(spec_.precision) < sv.size()) {
        sv = sv.substr(0, static_cast<size_t>(spec_.precision));
      }
      s = detail::apply_padding(std::string(sv), spec_, false);
    } else if constexpr (std::is_same_v<Decayed, std::nullptr_t> ||
                         std::is_pointer_v<Decayed>) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%p", static_cast<const void*>(val));
      s = detail::apply_padding(std::string(buf), spec_, true);
    } else if constexpr (detail::Streamable<Decayed>) {
      std::ostringstream oss;
      oss << val;
      s = detail::apply_padding(oss.str(), spec_, false);
    } else {
      s = "<unformattable>";
    }
    auto out = ctx.out();
    out = std::copy(s.begin(), s.end(), out);
    ctx.advance_to(out);
    return out;
  }
};

namespace detail {

template <typename T>
void format_one_arg(std::string& out, string_view spec_with_brace,
                    const T& arg) {
  formatter<std::remove_cvref_t<T>, char> f;
  format_parse_context parse_ctx(spec_with_brace);
  f.parse(parse_ctx);
  format_context fmt_ctx(out);
  f.format(arg, fmt_ctx);
}

template <typename Tuple, size_t... Is>
void dispatch_arg(size_t idx, std::string& out, string_view spec_with_brace,
                  const Tuple& tup, std::index_sequence<Is...>) {
  ((idx == Is ? (format_one_arg(out, spec_with_brace, std::get<Is>(tup)), 0)
              : 0),
   ...);
}

template <typename... Args>
std::string format_impl(string_view fmt_str, const Args&... args) {
  std::string out;
  out.reserve(fmt_str.size() + 32);
  auto tup = std::forward_as_tuple(args...);
  constexpr size_t num_args = sizeof...(Args);
  size_t next_arg = 0;

  const char* p = fmt_str.begin();
  const char* end = fmt_str.end();

  while (p < end) {
    if (*p == '{') {
      if (p + 1 < end && *(p + 1) == '{') {
        out.push_back('{');
        p += 2;
        continue;
      }
      const char* brace_start = p + 1;
      const char* close = brace_start;
      int depth = 1;
      while (close < end && depth > 0) {
        if (*close == '{') ++depth;
        else if (*close == '}') --depth;
        if (depth > 0) ++close;
      }
      if (close == end) {
        out.append(p, end);
        break;
      }
      // Parse optional argument index before ':'
      const char* colon = brace_start;
      while (colon < close && *colon != ':') {
        ++colon;
      }
      size_t arg_idx = next_arg;
      if (colon > brace_start) {
        size_t parsed_idx = 0;
        bool all_digits = true;
        for (const char* d = brace_start; d < colon; ++d) {
          if (*d >= '0' && *d <= '9') {
            parsed_idx = parsed_idx * 10 + static_cast<size_t>(*d - '0');
          } else {
            all_digits = false;
            break;
          }
        }
        if (all_digits) {
          arg_idx = parsed_idx;
        } else {
          ++next_arg;
        }
      } else {
        arg_idx = next_arg++;
      }

      const char* spec_start = (colon < close && *colon == ':') ? (colon + 1) : close;
      // Include the closing '}' so custom formatters checking `*it != '}'` work!
      string_view spec_view(spec_start, static_cast<size_t>((close + 1) - spec_start));
      if constexpr (num_args > 0) {
        if (arg_idx < num_args) {
          dispatch_arg(arg_idx, out, spec_view, tup,
                       std::make_index_sequence<num_args>{});
        }
      }
      p = close + 1;
    } else if (*p == '}') {
      if (p + 1 < end && *(p + 1) == '}') {
        out.push_back('}');
        p += 2;
        continue;
      }
      out.push_back(*p++);
    } else {
      out.push_back(*p++);
    }
  }
  return out;
}

}  // namespace detail

template <typename... Args>
std::string format(format_string<Args...> fmt_str, const Args&... args) {
  return detail::format_impl(fmt_str.get(), args...);
}

template <typename... Args>
std::string format(runtime_format_string<char> fmt_str, const Args&... args) {
  return detail::format_impl(fmt_str.str, args...);
}

template <typename OutputIt, typename... Args>
OutputIt format_to(OutputIt out, format_string<Args...> fmt_str,
                   const Args&... args) {
  std::string s = detail::format_impl(fmt_str.get(), args...);
  return std::copy(s.begin(), s.end(), out);
}

template <typename OutputIt, typename... Args>
OutputIt format_to(OutputIt out, runtime_format_string<char> fmt_str,
                   const Args&... args) {
  std::string s = detail::format_impl(fmt_str.str, args...);
  return std::copy(s.begin(), s.end(), out);
}

template <typename... Args>
size_t formatted_size(format_string<Args...> fmt_str, const Args&... args) {
  return detail::format_impl(fmt_str.get(), args...).size();
}

template <typename... Args>
void print(format_string<Args...> fmt_str, const Args&... args) {
  std::string s = detail::format_impl(fmt_str.get(), args...);
  std::fwrite(s.data(), 1, s.size(), stdout);
}

template <typename... Args>
void print(std::FILE* f, format_string<Args...> fmt_str, const Args&... args) {
  std::string s = detail::format_impl(fmt_str.get(), args...);
  std::fwrite(s.data(), 1, s.size(), f);
}

template <typename... Args>
void println(format_string<Args...> fmt_str, const Args&... args) {
  std::string s = detail::format_impl(fmt_str.get(), args...);
  s.push_back('\n');
  std::fwrite(s.data(), 1, s.size(), stdout);
}

template <typename... Args>
void println(std::FILE* f, format_string<Args...> fmt_str,
             const Args&... args) {
  std::string s = detail::format_impl(fmt_str.get(), args...);
  s.push_back('\n');
  std::fwrite(s.data(), 1, s.size(), f);
}

template <typename T>
std::string to_string(const T& value) {
  return format("{}", value);
}

}  // namespace fmt
