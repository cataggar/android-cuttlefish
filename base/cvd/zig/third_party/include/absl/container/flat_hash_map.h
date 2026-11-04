#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

namespace absl {

template <class K, class V, class Hash = std::hash<K>,
          class Eq = std::equal_to<K>,
          class Allocator = std::allocator<std::pair<const K, V>>>
using flat_hash_map = std::unordered_map<K, V, Hash, Eq, Allocator>;

}  // namespace absl
