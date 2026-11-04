#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace absl {

template <typename T, size_t N, typename A = std::allocator<T>>
using InlinedVector = std::vector<T, A>;

}  // namespace absl
