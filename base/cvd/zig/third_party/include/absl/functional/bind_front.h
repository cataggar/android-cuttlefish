#pragma once

#include <functional>
#include <utility>

namespace absl {

template <typename F, typename... BoundArgs>
constexpr auto bind_front(F&& func, BoundArgs&&... args) {
  return std::bind_front(std::forward<F>(func),
                         std::forward<BoundArgs>(args)...);
}

}  // namespace absl
