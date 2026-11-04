#pragma once

#include <utility>

namespace android {
namespace base {

template <typename F>
class ScopeGuard {
 public:
  constexpr explicit ScopeGuard(F&& f) : f_(std::forward<F>(f)), active_(true) {}
  constexpr ScopeGuard(ScopeGuard&& that) noexcept
      : f_(std::move(that.f_)), active_(that.active_) {
    that.active_ = false;
  }
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
  ScopeGuard& operator=(ScopeGuard&&) = delete;

  ~ScopeGuard() {
    if (active_) f_();
  }

  void Disable() { active_ = false; }
  bool active() const { return active_; }

 private:
  F f_;
  bool active_;
};

template <typename F>
ScopeGuard<F> make_scope_guard(F&& f) {
  return ScopeGuard<F>(std::forward<F>(f));
}

}  // namespace base
}  // namespace android
