#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "api/units/time_delta.h"

namespace rtc {

class Thread {
 public:
  Thread();
  virtual ~Thread();

  static std::unique_ptr<Thread> CreateWithSocketServer();
  static std::unique_ptr<Thread> Create();
  static Thread* Current();

  bool SetName(const std::string& name, const void* obj = nullptr);
  const std::string& name() const { return name_; }

  bool Start();
  void Stop();
  bool IsCurrent() const;

  void PostTask(std::function<void()> task);
  void PostDelayedTask(std::function<void()> task, webrtc::TimeDelta delay);

  template <typename Functor,
            typename ReturnT = decltype(std::declval<Functor>()()),
            typename std::enable_if<!std::is_void<ReturnT>::value, int>::type = 0>
  ReturnT BlockingCall(Functor&& functor) {
    if (IsCurrent() || !running_) {
      return functor();
    }
    ReturnT result{};
    std::mutex done_mu;
    std::condition_variable done_cv;
    bool done = false;
    PostTask([&]() {
      result = functor();
      std::lock_guard<std::mutex> lock(done_mu);
      done = true;
      done_cv.notify_one();
    });
    std::unique_lock<std::mutex> lock(done_mu);
    done_cv.wait(lock, [&]() { return done; });
    return result;
  }

  template <typename Functor,
            typename ReturnT = decltype(std::declval<Functor>()()),
            typename std::enable_if<std::is_void<ReturnT>::value, int>::type = 0>
  void BlockingCall(Functor&& functor) {
    if (IsCurrent() || !running_) {
      functor();
      return;
    }
    std::mutex done_mu;
    std::condition_variable done_cv;
    bool done = false;
    PostTask([&]() {
      functor();
      std::lock_guard<std::mutex> lock(done_mu);
      done = true;
      done_cv.notify_one();
    });
    std::unique_lock<std::mutex> lock(done_mu);
    done_cv.wait(lock, [&]() { return done; });
  }

 private:
  void RunLoop();

  std::string name_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  std::thread worker_;
  std::thread::id worker_id_{};
  bool running_ = false;
  bool stop_requested_ = false;
};

}  // namespace rtc

namespace webrtc {
using rtc::Thread;
}  // namespace webrtc
