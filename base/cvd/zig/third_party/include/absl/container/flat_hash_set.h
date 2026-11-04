#pragma once

#include <functional>
#include <memory>
#include <unordered_set>

namespace absl {

template <class T, class Hash = std::hash<T>, class Eq = std::equal_to<T>,
          class Allocator = std::allocator<T>>
using flat_hash_set = std::unordered_set<T, Hash, Eq, Allocator>;

}  // namespace absl
