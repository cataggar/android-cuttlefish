#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"

namespace absl {

class Condition {
 public:
  Condition(bool (*func)(void*), void* arg)
      : eval_([func, arg]() { return func(arg); }) {}

  template <typename T>
  Condition(bool (*func)(T*), T* arg)
      : eval_([func, arg]() { return func(arg); }) {}

  template <typename T>
  Condition(T* obj, bool (T::*method)())
      : eval_([obj, method]() { return (obj->*method)(); }) {}

  template <typename T>
  Condition(const T* obj, bool (T::*method)() const)
      : eval_([obj, method]() { return (obj->*method)(); }) {}

  explicit Condition(const bool* cond) : eval_([cond]() { return *cond; }) {}

  bool Eval() const { return eval_(); }

 private:
  std::function<bool()> eval_;
};

class ABSL_LOCKABLE Mutex {
 public:
  Mutex() = default;
  ~Mutex() = default;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() { mu_.lock(); }
  void Unlock() ABSL_UNLOCK_FUNCTION() {
    cv_.notify_all();
    mu_.unlock();
  }
  bool TryLock() ABSL_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return mu_.try_lock();
  }

  void ReaderLock() ABSL_SHARED_LOCK_FUNCTION() { Lock(); }
  void ReaderUnlock() ABSL_UNLOCK_FUNCTION() { Unlock(); }
  bool ReaderTryLock() ABSL_SHARED_TRYLOCK_FUNCTION(true) { return TryLock(); }

  void WriterLock() ABSL_EXCLUSIVE_LOCK_FUNCTION() { Lock(); }
  void WriterUnlock() ABSL_UNLOCK_FUNCTION() { Unlock(); }

  void AssertHeld() const ABSL_ASSERT_EXCLUSIVE_LOCK() {}
  void AssertReaderHeld() const ABSL_ASSERT_SHARED_LOCK() {}
  void AssertNotHeld() const {}

  void Await(const Condition& cond) {
    std::unique_lock<std::mutex> lk(mu_, std::adopt_lock);
    cv_.wait(lk, [&]() { return cond.Eval(); });
    lk.release();
  }

  bool AwaitWithTimeout(const Condition& cond, Duration timeout) {
    std::unique_lock<std::mutex> lk(mu_, std::adopt_lock);
    bool ok = cv_.wait_for(lk, ToChronoNanoseconds(timeout),
                           [&]() { return cond.Eval(); });
    lk.release();
    return ok;
  }

  void LockWhen(const Condition& cond) ABSL_EXCLUSIVE_LOCK_FUNCTION() {
    Lock();
    Await(cond);
  }

  bool LockWhenWithTimeout(const Condition& cond, Duration timeout)
      ABSL_EXCLUSIVE_LOCK_FUNCTION() {
    Lock();
    return AwaitWithTimeout(cond, timeout);
  }

 private:
  friend class CondVar;
  std::mutex mu_;
  std::condition_variable cv_;
};

class ABSL_SCOPED_LOCKABLE MutexLock {
 public:
  explicit MutexLock(Mutex* mu) ABSL_EXCLUSIVE_LOCK_FUNCTION(mu) : mu_(mu) {
    mu_->Lock();
  }
  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;
  ~MutexLock() ABSL_UNLOCK_FUNCTION() { mu_->Unlock(); }

 private:
  Mutex* const mu_;
};

class ABSL_SCOPED_LOCKABLE ReaderMutexLock {
 public:
  explicit ReaderMutexLock(Mutex* mu) ABSL_SHARED_LOCK_FUNCTION(mu) : mu_(mu) {
    mu_->ReaderLock();
  }
  ReaderMutexLock(const ReaderMutexLock&) = delete;
  ReaderMutexLock& operator=(const ReaderMutexLock&) = delete;
  ~ReaderMutexLock() ABSL_UNLOCK_FUNCTION() { mu_->ReaderUnlock(); }

 private:
  Mutex* const mu_;
};

class ABSL_SCOPED_LOCKABLE WriterMutexLock {
 public:
  explicit WriterMutexLock(Mutex* mu) ABSL_EXCLUSIVE_LOCK_FUNCTION(mu)
      : mu_(mu) {
    mu_->WriterLock();
  }
  WriterMutexLock(const WriterMutexLock&) = delete;
  WriterMutexLock& operator=(const WriterMutexLock&) = delete;
  ~WriterMutexLock() ABSL_UNLOCK_FUNCTION() { mu_->WriterUnlock(); }

 private:
  Mutex* const mu_;
};

class CondVar {
 public:
  CondVar() = default;
  ~CondVar() = default;

  void Wait(Mutex* mu) {
    std::unique_lock<std::mutex> lk(mu->mu_, std::adopt_lock);
    cv_.wait(lk);
    lk.release();
  }

  bool WaitWithTimeout(Mutex* mu, Duration timeout) {
    std::unique_lock<std::mutex> lk(mu->mu_, std::adopt_lock);
    bool timed_out =
        (cv_.wait_for(lk, ToChronoNanoseconds(timeout)) ==
         std::cv_status::timeout);
    lk.release();
    return timed_out;
  }

  void Signal() { cv_.notify_one(); }
  void SignalAll() { cv_.notify_all(); }

 private:
  std::condition_variable cv_;
};

}  // namespace absl
