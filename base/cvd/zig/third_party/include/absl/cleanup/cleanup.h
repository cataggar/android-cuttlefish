#pragma once

#include <type_traits>
#include <utility>

namespace absl {

template <typename Callback, typename Tag = void>
class Cleanup {
 public:
  Cleanup(Callback cb) noexcept(std::is_nothrow_move_constructible_v<Callback>)
      : callback_(std::move(cb)), released_(false) {}

  Cleanup(Cleanup&& other) noexcept(
      std::is_nothrow_move_constructible_v<Callback>)
      : callback_(std::move(other.callback_)), released_(other.released_) {
    other.released_ = true;
  }

  Cleanup(const Cleanup&) = delete;
  Cleanup& operator=(const Cleanup&) = delete;
  Cleanup& operator=(Cleanup&&) = delete;

  ~Cleanup() {
    if (!released_) {
      std::move(callback_)();
    }
  }

  void Cancel() && noexcept { released_ = true; }
  void Cancel() & noexcept { released_ = true; }

  void Invoke() && {
    if (!released_) {
      released_ = true;
      std::move(callback_)();
    }
  }

  void Invoke() & {
    if (!released_) {
      released_ = true;
      std::move(callback_)();
    }
  }

 private:
  Callback callback_;
  bool released_;
};

template <typename Callback>
Cleanup(Callback) -> Cleanup<std::decay_t<Callback>, void>;

template <int&... ExplicitParameterBarrier, typename Callback>
Cleanup<std::decay_t<Callback>, void> MakeCleanup(Callback&& callback) {
  return Cleanup<std::decay_t<Callback>, void>(std::forward<Callback>(callback));
}

}  // namespace absl
