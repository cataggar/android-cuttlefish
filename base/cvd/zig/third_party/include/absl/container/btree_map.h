#pragma once

#include <functional>
#include <map>
#include <memory>
#include <utility>

namespace absl {

template <typename Key, typename Value, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>>
using btree_map = std::map<Key, Value, Compare, Alloc>;

template <typename Key, typename Value, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>>
using btree_multimap = std::multimap<Key, Value, Compare, Alloc>;

}  // namespace absl
