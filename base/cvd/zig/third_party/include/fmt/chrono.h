#pragma once

#include <chrono>
#include <ctime>

#include "fmt/format.h"

namespace fmt {

template <typename Rep, typename Period, typename Char>
struct formatter<std::chrono::duration<Rep, Period>, Char> {
  constexpr auto parse(format_parse_context& ctx) {
    auto it = ctx.begin();
    while (it != ctx.end() && *it != '}') ++it;
    ctx.advance_to(it);
    return it;
  }

  template <typename FormatContext>
  auto format(const std::chrono::duration<Rep, Period>& d,
              FormatContext& ctx) const {
    return format_to(ctx.out(), "{}", d.count());
  }
};

}  // namespace fmt
