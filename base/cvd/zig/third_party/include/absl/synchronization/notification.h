#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "absl/time/time.h"

namespace absl {

class Notification {
 public:
  Notification() : notified_(false) {}
  Notification(const Notification&) = delete;
  Notification& operator=(const Notification&) = delete;
  ~Notification() = default;

  bool HasBeenNotified() const {
    return notified_.load(std::memory_order_acquire);
  }

  void WaitForNotification() const {
    if (!HasBeenNotified()) {
      std::unique_lock<std::mutex> lk(mu_);
      cv_.wait(lk, [this]() { return HasBeenNotified(); });
    }
  }

  bool WaitForNotificationWithTimeout(Duration timeout) const {
    if (HasBeenNotified()) return true;
    std::unique_lock<std::mutex> lk(mu_);
    return cv_.wait_for(lk, ToChronoNanoseconds(timeout),
                        [this]() { return HasBeenNotified(); });
  }

  void Notify() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      notified_.store(true, std::memory_order_release);
    }
    cv_.notify_all();
  }

 private:
  mutable std::mutex mu_;
  mutable std::condition_variable cv_;
  std::atomic<bool> notified_;
};

}  // namespace absl
