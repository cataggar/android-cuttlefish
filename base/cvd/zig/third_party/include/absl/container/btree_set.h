#pragma once

#include <functional>
#include <memory>
#include <set>

namespace absl {

template <typename Key, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<Key>>
using btree_set = std::set<Key, Compare, Alloc>;

template <typename Key, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<Key>>
using btree_multiset = std::multiset<Key, Compare, Alloc>;

}  // namespace absl
