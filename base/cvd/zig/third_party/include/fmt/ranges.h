#pragma once

#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "fmt/core.h"

namespace fmt {

template <typename It, typename Sentinel, typename Char = char>
struct join_view {
  It begin;
  Sentinel end;
  basic_string_view<Char> sep;

  join_view(It b, Sentinel e, basic_string_view<Char> s)
      : begin(std::move(b)), end(std::move(e)), sep(s) {}
};

template <typename It, typename Sentinel>
join_view<It, Sentinel, char> join(It begin, Sentinel end, string_view sep) {
  return join_view<It, Sentinel, char>(std::move(begin), std::move(end), sep);
}

template <typename Range>
auto join(Range&& range, string_view sep)
    -> join_view<decltype(std::begin(range)), decltype(std::end(range)), char> {
  return join(std::begin(range), std::end(range), sep);
}

template <typename It, typename Sentinel, typename Char>
struct formatter<join_view<It, Sentinel, Char>, Char> {
  using value_type = std::remove_cvref_t<decltype(*std::declval<It>())>;
  formatter<value_type, Char> value_formatter_;

  constexpr auto parse(format_parse_context& ctx) {
    return value_formatter_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(const join_view<It, Sentinel, Char>& view,
              FormatContext& ctx) const {
    auto it = view.begin;
    auto out = ctx.out();
    if (it != view.end) {
      out = value_formatter_.format(*it, ctx);
      ++it;
      while (it != view.end) {
        out = std::copy(view.sep.begin(), view.sep.end(), out);
        ctx.advance_to(out);
        out = value_formatter_.format(*it, ctx);
        ++it;
      }
    }
    return out;
  }
};

namespace detail {
template <typename T>
concept RangeNotString = requires(const T& r) {
  std::begin(r);
  std::end(r);
} && !std::is_convertible_v<T, std::string_view> &&
    !std::is_same_v<std::remove_cvref_t<T>, std::string>;
}  // namespace detail

template <typename Range, typename Char>
struct formatter<Range, Char,
                 std::enable_if_t<detail::RangeNotString<Range>>> {
  constexpr auto parse(format_parse_context& ctx) {
    auto it = ctx.begin();
    while (it != ctx.end() && *it != '}') ++it;
    ctx.advance_to(it);
    return it;
  }

  template <typename FormatContext>
  auto format(const Range& range, FormatContext& ctx) const {
    auto out = ctx.out();
    *out++ = '[';
    ctx.advance_to(out);
    bool first = true;
    for (const auto& elem : range) {
      if (!first) {
        *out++ = ',';
        *out++ = ' ';
        ctx.advance_to(out);
      }
      first = false;
      using ElemT = std::remove_cvref_t<decltype(elem)>;
      formatter<ElemT, Char> ef;
      out = ef.format(elem, ctx);
    }
    *out++ = ']';
    ctx.advance_to(out);
    return out;
  }
};

}  // namespace fmt
