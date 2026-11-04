#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace absl {

inline constexpr size_t dynamic_extent = static_cast<size_t>(-1);

template <typename T>
class Span {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  static constexpr size_type npos = ~(size_type(0));

  constexpr Span() noexcept : ptr_(nullptr), len_(0) {}
  constexpr Span(pointer array, size_type length) noexcept
      : ptr_(array), len_(length) {}

  template <size_t N>
  constexpr Span(T (&a)[N]) noexcept : ptr_(a), len_(N) {}

  template <typename V,
            std::enable_if_t<
                !std::is_same_v<std::remove_cvref_t<V>, Span> &&
                    !std::is_array_v<std::remove_cvref_t<V>> &&
                    requires(V& v) {
                      { v.data() } -> std::convertible_to<T*>;
                      { v.size() } -> std::convertible_to<size_t>;
                    },
                int> = 0>
  constexpr Span(V& v) noexcept : ptr_(v.data()), len_(v.size()) {}

  template <typename V,
            std::enable_if_t<
                !std::is_same_v<std::remove_cvref_t<V>, Span> &&
                    !std::is_array_v<std::remove_cvref_t<V>> &&
                    std::is_const_v<T> &&
                    requires(const V& v) {
                      { v.data() } -> std::convertible_to<T*>;
                      { v.size() } -> std::convertible_to<size_t>;
                    },
                int> = 0>
  constexpr Span(const V& v) noexcept : ptr_(v.data()), len_(v.size()) {}

  template <typename LazyT = T,
            std::enable_if_t<std::is_const_v<LazyT>, int> = 0>
  constexpr Span(std::initializer_list<value_type> v) noexcept
      : ptr_(v.begin()), len_(v.size()) {}

  constexpr pointer data() const noexcept { return ptr_; }
  constexpr size_type size() const noexcept { return len_; }
  constexpr size_type length() const noexcept { return len_; }
  constexpr size_type size_bytes() const noexcept { return len_ * sizeof(T); }
  constexpr bool empty() const noexcept { return len_ == 0; }

  constexpr iterator begin() const noexcept { return ptr_; }
  constexpr const_iterator cbegin() const noexcept { return ptr_; }
  constexpr iterator end() const noexcept { return ptr_ + len_; }
  constexpr const_iterator cend() const noexcept { return ptr_ + len_; }
  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(cbegin());
  }

  constexpr reference operator[](size_type i) const noexcept { return ptr_[i]; }
  constexpr reference at(size_type i) const {
    if (i >= len_) throw std::out_of_range("Span::at");
    return ptr_[i];
  }
  constexpr reference front() const noexcept { return ptr_[0]; }
  constexpr reference back() const noexcept { return ptr_[len_ - 1]; }

  constexpr void remove_prefix(size_type n) noexcept {
    ptr_ += n;
    len_ -= n;
  }
  constexpr void remove_suffix(size_type n) noexcept { len_ -= n; }

  constexpr Span subspan(size_type pos = 0, size_type len = npos) const {
    if (pos > len_) throw std::out_of_range("Span::subspan");
    return Span(ptr_ + pos, std::min(len_ - pos, len));
  }

  constexpr Span first(size_type len) const { return subspan(0, len); }
  constexpr Span last(size_type len) const { return subspan(len_ - len, len); }

 private:
  pointer ptr_;
  size_type len_;
};

template <typename T>
constexpr bool operator==(Span<T> a, Span<T> b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

template <typename T>
constexpr bool operator!=(Span<T> a, Span<T> b) {
  return !(a == b);
}

template <int&... ExplicitArgumentBarrier, typename T>
constexpr Span<T> MakeSpan(T* ptr, size_t size) noexcept {
  return Span<T>(ptr, size);
}

template <int&... ExplicitArgumentBarrier, typename T>
constexpr Span<T> MakeSpan(T* begin, T* end) noexcept {
  return Span<T>(begin, static_cast<size_t>(end - begin));
}

template <int&... ExplicitArgumentBarrier, typename C>
constexpr auto MakeSpan(C& c) noexcept
    -> Span<std::remove_pointer_t<decltype(c.data())>> {
  return Span<std::remove_pointer_t<decltype(c.data())>>(c.data(), c.size());
}

template <int&... ExplicitArgumentBarrier, typename T, size_t N>
constexpr Span<T> MakeSpan(T (&array)[N]) noexcept {
  return Span<T>(array, N);
}

template <int&... ExplicitArgumentBarrier, typename T>
constexpr Span<const T> MakeConstSpan(T* ptr, size_t size) noexcept {
  return Span<const T>(ptr, size);
}

template <int&... ExplicitArgumentBarrier, typename T>
constexpr Span<const T> MakeConstSpan(T* begin, T* end) noexcept {
  return Span<const T>(begin, static_cast<size_t>(end - begin));
}

template <int&... ExplicitArgumentBarrier, typename C>
constexpr auto MakeConstSpan(const C& c) noexcept
    -> Span<const std::remove_cv_t<std::remove_pointer_t<decltype(c.data())>>> {
  return Span<
      const std::remove_cv_t<std::remove_pointer_t<decltype(c.data())>>>(
      c.data(), c.size());
}

template <int&... ExplicitArgumentBarrier, typename T, size_t N>
constexpr Span<const T> MakeConstSpan(const T (&array)[N]) noexcept {
  return Span<const T>(array, N);
}

}  // namespace absl
