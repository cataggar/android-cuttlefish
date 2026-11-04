#pragma once

#include <ostream>
#include <sstream>

#include "fmt/format.h"

namespace fmt {

template <typename Char = char>
struct basic_ostream_formatter : formatter<basic_string_view<Char>, Char> {
  template <typename T, typename FormatContext>
  auto format(const T& value, FormatContext& ctx) const {
    std::basic_ostringstream<Char> oss;
    oss << value;
    auto str = oss.str();
    return formatter<basic_string_view<Char>, Char>::format(str, ctx);
  }
};

using ostream_formatter = basic_ostream_formatter<char>;

template <typename T>
struct streamed_view {
  const T& value;
};

template <typename T>
streamed_view<T> streamed(const T& v) {
  return {v};
}

template <typename T, typename Char>
struct formatter<streamed_view<T>, Char> : basic_ostream_formatter<Char> {
  template <typename FormatContext>
  auto format(streamed_view<T> view, FormatContext& ctx) const {
    return basic_ostream_formatter<Char>::format(view.value, ctx);
  }
};

}  // namespace fmt
