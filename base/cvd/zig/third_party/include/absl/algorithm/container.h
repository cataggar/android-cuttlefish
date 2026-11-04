#pragma once

#include <algorithm>
#include <iterator>

namespace absl {

template <typename C, typename T>
auto c_find(C& c, const T& value) {
  return std::find(std::begin(c), std::end(c), value);
}

template <typename C, typename Pred>
auto c_find_if(C& c, Pred&& pred) {
  return std::find_if(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename C, typename Pred>
bool c_any_of(const C& c, Pred&& pred) {
  return std::any_of(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename C, typename Pred>
bool c_all_of(const C& c, Pred&& pred) {
  return std::all_of(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename C, typename Pred>
bool c_none_of(const C& c, Pred&& pred) {
  return std::none_of(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename C, typename T>
bool c_linear_search(const C& c, const T& value) {
  return std::find(std::begin(c), std::end(c), value) != std::end(c);
}

template <typename C>
void c_sort(C& c) {
  std::sort(std::begin(c), std::end(c));
}

template <typename C, typename Comp>
void c_sort(C& c, Comp&& comp) {
  std::sort(std::begin(c), std::end(c), std::forward<Comp>(comp));
}

template <typename C1, typename C2>
bool c_equal(const C1& c1, const C2& c2) {
  return std::equal(std::begin(c1), std::end(c1), std::begin(c2), std::end(c2));
}

}  // namespace absl
