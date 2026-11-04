#pragma once

#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace tl {

template <class T, class E>
class expected;

template <class E>
class unexpected {
 public:
  static_assert(!std::is_same_v<E, void>, "E must not be void");

  constexpr unexpected() = delete;
  constexpr explicit unexpected(const E& e) : val_(e) {}
  constexpr explicit unexpected(E&& e) : val_(std::move(e)) {}

  template <class... Args,
            std::enable_if_t<std::is_constructible_v<E, Args...>, int> = 0>
  constexpr explicit unexpected(std::in_place_t, Args&&... args)
      : val_(std::forward<Args>(args)...) {}

  template <
      class U,
      std::enable_if_t<!std::is_same_v<std::remove_cvref_t<U>, unexpected> &&
                           !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
                           std::is_constructible_v<E, U>,
                       int> = 0>
  constexpr explicit(!std::is_convertible_v<U, E>) unexpected(U&& u)
      : val_(std::forward<U>(u)) {}

  constexpr const E& value() const& noexcept { return val_; }
  constexpr E& value() & noexcept { return val_; }
  constexpr E&& value() && noexcept { return std::move(val_); }
  constexpr const E&& value() const&& noexcept { return std::move(val_); }

  constexpr const E& error() const& noexcept { return val_; }
  constexpr E& error() & noexcept { return val_; }
  constexpr E&& error() && noexcept { return std::move(val_); }
  constexpr const E&& error() const&& noexcept { return std::move(val_); }

 private:
  E val_;
};

template <class E>
unexpected(E) -> unexpected<std::decay_t<E>>;

template <class E1, class E2>
constexpr bool operator==(const unexpected<E1>& x, const unexpected<E2>& y) {
  return x.value() == y.value();
}

template <class E1, class E2>
constexpr bool operator!=(const unexpected<E1>& x, const unexpected<E2>& y) {
  return !(x == y);
}

template <class E>
constexpr unexpected<std::decay_t<E>> make_unexpected(E&& e) {
  return unexpected<std::decay_t<E>>(std::forward<E>(e));
}

struct unexpect_t {
  explicit constexpr unexpect_t() = default;
};
inline constexpr unexpect_t unexpect{};

using in_place_t = std::in_place_t;
inline constexpr std::in_place_t in_place{};

template <class E>
class bad_expected_access;

template <>
class bad_expected_access<void> : public std::exception {
 public:
  explicit bad_expected_access() = default;
  const char* what() const noexcept override { return "bad expected access"; }
};

template <class E>
class bad_expected_access : public bad_expected_access<void> {
 public:
  explicit bad_expected_access(E e) : val_(std::move(e)) {}
  const E& error() const& noexcept { return val_; }
  E& error() & noexcept { return val_; }
  E&& error() && noexcept { return std::move(val_); }
  const E&& error() const&& noexcept { return std::move(val_); }

 private:
  E val_;
};

namespace detail {
template <class T>
struct is_unexpected : std::false_type {};
template <class E>
struct is_unexpected<unexpected<E>> : std::true_type {};

template <class T>
struct is_expected : std::false_type {};
template <class T, class E>
struct is_expected<expected<T, E>> : std::true_type {};
}  // namespace detail

template <class T, class E>
class expected {
 public:
  using value_type = T;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  template <class U = T,
            std::enable_if_t<std::is_default_constructible_v<U>, int> = 0>
  constexpr expected() : has_val_(true) {
    std::construct_at(std::addressof(val_), T());
  }

  constexpr expected(const expected& other) : has_val_(other.has_val_) {
    if (has_val_) {
      std::construct_at(std::addressof(val_), other.val_);
    } else {
      std::construct_at(std::addressof(err_), other.err_);
    }
  }

  constexpr expected(expected&& other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_constructible_v<E>)
      : has_val_(other.has_val_) {
    if (has_val_) {
      std::construct_at(std::addressof(val_), std::move(other.val_));
    } else {
      std::construct_at(std::addressof(err_), std::move(other.err_));
    }
  }

  template <class U, class G,
            std::enable_if_t<std::is_constructible_v<T, const U&> &&
                                 std::is_constructible_v<E, const G&>,
                             int> = 0>
  constexpr explicit(!std::is_convertible_v<const U&, T> ||
                     !std::is_convertible_v<const G&, E>)
      expected(const expected<U, G>& other)
      : has_val_(other.has_value()) {
    if (has_val_) {
      std::construct_at(std::addressof(val_), *other);
    } else {
      std::construct_at(std::addressof(err_), other.error());
    }
  }

  template <class U, class G,
            std::enable_if_t<std::is_constructible_v<T, U&&> &&
                                 std::is_constructible_v<E, G&&>,
                             int> = 0>
  constexpr explicit(!std::is_convertible_v<U&&, T> ||
                     !std::is_convertible_v<G&&, E>)
      expected(expected<U, G>&& other)
      : has_val_(other.has_value()) {
    if (has_val_) {
      std::construct_at(std::addressof(val_), std::move(*other));
    } else {
      std::construct_at(std::addressof(err_), std::move(other.error()));
    }
  }

  template <
      class U = T,
      std::enable_if_t<!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
                           !std::is_same_v<std::remove_cvref_t<U>, unexpect_t> &&
                           !std::is_same_v<std::remove_cvref_t<U>, expected> &&
                           !detail::is_unexpected<std::remove_cvref_t<U>>::value &&
                           !detail::is_expected<std::remove_cvref_t<U>>::value &&
                           std::is_constructible_v<T, U>,
                       int> = 0>
  constexpr explicit(!std::is_convertible_v<U, T>) expected(U&& v)
      : has_val_(true) {
    std::construct_at(std::addressof(val_), std::forward<U>(v));
  }

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, const G&>, int> = 0>
  constexpr explicit(!std::is_convertible_v<const G&, E>)
      expected(const unexpected<G>& u)
      : has_val_(false) {
    std::construct_at(std::addressof(err_), u.value());
  }

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, G&&>, int> = 0>
  constexpr explicit(!std::is_convertible_v<G&&, E>) expected(unexpected<G>&& u)
      : has_val_(false) {
    std::construct_at(std::addressof(err_), std::move(u.value()));
  }

  template <class... Args,
            std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  constexpr explicit expected(std::in_place_t, Args&&... args)
      : has_val_(true) {
    std::construct_at(std::addressof(val_), std::forward<Args>(args)...);
  }

  template <class... Args,
            std::enable_if_t<std::is_constructible_v<E, Args...>, int> = 0>
  constexpr explicit expected(unexpect_t, Args&&... args) : has_val_(false) {
    std::construct_at(std::addressof(err_), std::forward<Args>(args)...);
  }

  ~expected() { destroy(); }

  expected& operator=(const expected& other) {
    if (this != &other) {
      destroy();
      has_val_ = other.has_val_;
      if (has_val_) {
        std::construct_at(std::addressof(val_), other.val_);
      } else {
        std::construct_at(std::addressof(err_), other.err_);
      }
    }
    return *this;
  }

  expected& operator=(expected&& other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_constructible_v<E>) {
    if (this != &other) {
      destroy();
      has_val_ = other.has_val_;
      if (has_val_) {
        std::construct_at(std::addressof(val_), std::move(other.val_));
      } else {
        std::construct_at(std::addressof(err_), std::move(other.err_));
      }
    }
    return *this;
  }

  template <
      class U = T,
      std::enable_if_t<!std::is_same_v<std::remove_cvref_t<U>, expected> &&
                           !detail::is_unexpected<std::remove_cvref_t<U>>::value &&
                           std::is_constructible_v<T, U>,
                       int> = 0>
  expected& operator=(U&& v) {
    destroy();
    has_val_ = true;
    std::construct_at(std::addressof(val_), std::forward<U>(v));
    return *this;
  }

  template <class G>
  expected& operator=(const unexpected<G>& u) {
    destroy();
    has_val_ = false;
    std::construct_at(std::addressof(err_), u.value());
    return *this;
  }

  template <class G>
  expected& operator=(unexpected<G>&& u) {
    destroy();
    has_val_ = false;
    std::construct_at(std::addressof(err_), std::move(u.value()));
    return *this;
  }

  template <class... Args>
  T& emplace(Args&&... args) {
    destroy();
    has_val_ = true;
    std::construct_at(std::addressof(val_), std::forward<Args>(args)...);
    return val_;
  }

  constexpr const T* operator->() const noexcept { return std::addressof(val_); }
  constexpr T* operator->() noexcept { return std::addressof(val_); }

  constexpr const T& operator*() const& noexcept { return val_; }
  constexpr T& operator*() & noexcept { return val_; }
  constexpr const T&& operator*() const&& noexcept { return std::move(val_); }
  constexpr T&& operator*() && noexcept { return std::move(val_); }

  constexpr explicit operator bool() const noexcept { return has_val_; }
  constexpr bool has_value() const noexcept { return has_val_; }

  constexpr T& value() & {
    if (!has_val_) throw bad_expected_access<E>(err_);
    return val_;
  }
  constexpr const T& value() const& {
    if (!has_val_) throw bad_expected_access<E>(err_);
    return val_;
  }
  constexpr T&& value() && {
    if (!has_val_) throw bad_expected_access<E>(std::move(err_));
    return std::move(val_);
  }
  constexpr const T&& value() const&& {
    if (!has_val_) throw bad_expected_access<E>(err_);
    return std::move(val_);
  }

  constexpr const E& error() const& noexcept { return err_; }
  constexpr E& error() & noexcept { return err_; }
  constexpr const E&& error() const&& noexcept { return std::move(err_); }
  constexpr E&& error() && noexcept { return std::move(err_); }

  template <class U>
  constexpr T value_or(U&& default_value) const& {
    return has_val_ ? val_ : static_cast<T>(std::forward<U>(default_value));
  }
  template <class U>
  constexpr T value_or(U&& default_value) && {
    return has_val_ ? std::move(val_)
                    : static_cast<T>(std::forward<U>(default_value));
  }

  template <class F>
  constexpr auto transform(F&& f) & {
    using U = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
    if constexpr (std::is_void_v<U>) {
      if (has_val_) {
        std::invoke(std::forward<F>(f), val_);
        return expected<void, E>();
      }
      return expected<void, E>(unexpect, err_);
    } else {
      if (has_val_) {
        return expected<U, E>(std::in_place, std::invoke(std::forward<F>(f), val_));
      }
      return expected<U, E>(unexpect, err_);
    }
  }

  template <class F>
  constexpr auto transform(F&& f) const& {
    using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
    if constexpr (std::is_void_v<U>) {
      if (has_val_) {
        std::invoke(std::forward<F>(f), val_);
        return expected<void, E>();
      }
      return expected<void, E>(unexpect, err_);
    } else {
      if (has_val_) {
        return expected<U, E>(std::in_place, std::invoke(std::forward<F>(f), val_));
      }
      return expected<U, E>(unexpect, err_);
    }
  }

  template <class F>
  constexpr auto transform(F&& f) && {
    using U = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
    if constexpr (std::is_void_v<U>) {
      if (has_val_) {
        std::invoke(std::forward<F>(f), std::move(val_));
        return expected<void, E>();
      }
      return expected<void, E>(unexpect, std::move(err_));
    } else {
      if (has_val_) {
        return expected<U, E>(std::in_place, std::invoke(std::forward<F>(f), std::move(val_)));
      }
      return expected<U, E>(unexpect, std::move(err_));
    }
  }

  template <class F>
  constexpr auto map(F&& f) & { return transform(std::forward<F>(f)); }
  template <class F>
  constexpr auto map(F&& f) const& { return transform(std::forward<F>(f)); }
  template <class F>
  constexpr auto map(F&& f) && { return std::move(*this).transform(std::forward<F>(f)); }

  template <class F>
  constexpr auto and_then(F&& f) & {
    using Ret = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
    if (has_val_) return std::invoke(std::forward<F>(f), val_);
    return Ret(unexpect, err_);
  }

  template <class F>
  constexpr auto and_then(F&& f) const& {
    using Ret = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
    if (has_val_) return std::invoke(std::forward<F>(f), val_);
    return Ret(unexpect, err_);
  }

  template <class F>
  constexpr auto and_then(F&& f) && {
    using Ret = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
    if (has_val_) return std::invoke(std::forward<F>(f), std::move(val_));
    return Ret(unexpect, std::move(err_));
  }

 private:
  void destroy() {
    if (has_val_) {
      std::destroy_at(std::addressof(val_));
    } else {
      std::destroy_at(std::addressof(err_));
    }
  }

  union {
    T val_;
    E err_;
  };
  bool has_val_;
};

template <class E>
class expected<void, E> {
 public:
  using value_type = void;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  constexpr expected() noexcept : err_(), has_val_(true) {}
  constexpr expected(const expected& other) = default;
  constexpr expected(expected&& other) noexcept = default;
  constexpr explicit expected(std::in_place_t) noexcept : err_(), has_val_(true) {}

  template <class G,
            std::enable_if_t<std::is_constructible_v<E, const G&>, int> = 0>
  constexpr explicit(!std::is_convertible_v<const G&, E>)
      expected(const expected<void, G>& other)
      : has_val_(other.has_value()) {
    if (!has_val_) err_.emplace(other.error());
  }

  template <class G,
            std::enable_if_t<std::is_constructible_v<E, G&&>, int> = 0>
  constexpr explicit(!std::is_convertible_v<G&&, E>)
      expected(expected<void, G>&& other)
      : has_val_(other.has_value()) {
    if (!has_val_) err_.emplace(std::move(other.error()));
  }

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, const G&>, int> = 0>
  constexpr explicit(!std::is_convertible_v<const G&, E>)
      expected(const unexpected<G>& u)
      : err_(u.value()), has_val_(false) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, G&&>, int> = 0>
  constexpr explicit(!std::is_convertible_v<G&&, E>) expected(unexpected<G>&& u)
      : err_(std::move(u.value())), has_val_(false) {}

  template <class... Args,
            std::enable_if_t<std::is_constructible_v<E, Args...>, int> = 0>
  constexpr explicit expected(unexpect_t, Args&&... args)
      : err_(std::in_place, std::forward<Args>(args)...), has_val_(false) {}

  expected& operator=(const expected&) = default;
  expected& operator=(expected&&) noexcept = default;

  template <class G>
  expected& operator=(const unexpected<G>& u) {
    has_val_ = false;
    err_ = u.value();
    return *this;
  }

  template <class G>
  expected& operator=(unexpected<G>&& u) {
    has_val_ = false;
    err_ = std::move(u.value());
    return *this;
  }

  void emplace() noexcept {
    has_val_ = true;
    err_.reset();
  }

  constexpr explicit operator bool() const noexcept { return has_val_; }
  constexpr bool has_value() const noexcept { return has_val_; }
  constexpr void operator*() const noexcept {}
  constexpr void value() const {
    if (!has_val_) throw bad_expected_access<E>(*err_);
  }

  constexpr const E& error() const& noexcept { return *err_; }
  constexpr E& error() & noexcept { return *err_; }
  constexpr const E&& error() const&& noexcept { return std::move(*err_); }
  constexpr E&& error() && noexcept { return std::move(*err_); }

 private:
  std::optional<E> err_;
  bool has_val_;
};

template <class T, class E>
class expected<T&, E> {
 public:
  using value_type = T&;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  constexpr expected(T& v) noexcept : ptr_(std::addressof(v)), has_val_(true) {}
  constexpr expected(const expected&) = default;
  constexpr expected(expected&&) noexcept = default;

  template <class G = E>
  constexpr expected(const unexpected<G>& u)
      : ptr_(nullptr), err_(u.value()), has_val_(false) {}

  template <class G = E>
  constexpr expected(unexpected<G>&& u)
      : ptr_(nullptr), err_(std::move(u.value())), has_val_(false) {}

  expected& operator=(const expected&) = default;
  expected& operator=(expected&&) noexcept = default;

  constexpr T* operator->() const noexcept { return ptr_; }
  constexpr T& operator*() const noexcept { return *ptr_; }
  constexpr explicit operator bool() const noexcept { return has_val_; }
  constexpr bool has_value() const noexcept { return has_val_; }
  constexpr T& value() const {
    if (!has_val_) throw bad_expected_access<E>(*err_);
    return *ptr_;
  }
  constexpr const E& error() const& noexcept { return *err_; }
  constexpr E& error() & noexcept { return *err_; }
  constexpr E&& error() && noexcept { return std::move(*err_); }

 private:
  T* ptr_ = nullptr;
  std::optional<E> err_;
  bool has_val_ = false;
};

template <class T1, class E1, class T2, class E2>
constexpr bool operator==(const expected<T1, E1>& x, const expected<T2, E2>& y) {
  if (x.has_value() != y.has_value()) return false;
  if (!x.has_value()) return x.error() == y.error();
  if constexpr (std::is_void_v<T1> && std::is_void_v<T2>) {
    return true;
  } else {
    return *x == *y;
  }
}

template <class T1, class E1, class T2, class E2>
constexpr bool operator!=(const expected<T1, E1>& x, const expected<T2, E2>& y) {
  return !(x == y);
}

template <class T, class E, class U>
constexpr bool operator==(const expected<T, E>& x, const U& v) {
  return x.has_value() ? *x == v : false;
}

template <class T, class E, class U>
constexpr bool operator==(const U& v, const expected<T, E>& x) {
  return x.has_value() ? *x == v : false;
}

template <class T, class E, class U>
constexpr bool operator!=(const expected<T, E>& x, const U& v) {
  return !(x == v);
}

template <class T, class E, class U>
constexpr bool operator!=(const U& v, const expected<T, E>& x) {
  return !(v == x);
}

template <class T, class E, class G>
constexpr bool operator==(const expected<T, E>& x, const unexpected<G>& u) {
  return !x.has_value() ? x.error() == u.value() : false;
}

template <class T, class E, class G>
constexpr bool operator==(const unexpected<G>& u, const expected<T, E>& x) {
  return !x.has_value() ? x.error() == u.value() : false;
}

template <class T, class E, class G>
constexpr bool operator!=(const expected<T, E>& x, const unexpected<G>& u) {
  return !(x == u);
}

template <class T, class E, class G>
constexpr bool operator!=(const unexpected<G>& u, const expected<T, E>& x) {
  return !(u == x);
}

}  // namespace tl
