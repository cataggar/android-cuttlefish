#pragma once

#include <string_view>

namespace absl {

using string_view = std::string_view;

constexpr string_view NullSafeStringView(const char* p) noexcept {
  return p ? string_view(p) : string_view();
}

}  // namespace absl
