#pragma once

#include <atomic>
#include <type_traits>
#include <utility>

#include "rtc_base/ref_count.h"

namespace rtc {

template <class T>
class RefCountedObject : public T {
 public:
  RefCountedObject() = default;

  template <class P0>
  explicit RefCountedObject(P0&& p0) : T(std::forward<P0>(p0)) {}

  template <class P0, class P1, class... Args>
  RefCountedObject(P0&& p0, P1&& p1, Args&&... args)
      : T(std::forward<P0>(p0),
          std::forward<P1>(p1),
          std::forward<Args>(args)...) {}

  RefCountedObject(const RefCountedObject&) = delete;
  RefCountedObject& operator=(const RefCountedObject&) = delete;

  void AddRef() const override {
    ref_count_.fetch_add(1, std::memory_order_relaxed);
  }

  RefCountReleaseStatus Release() const override {
    if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      delete this;
      return RefCountReleaseStatus::kDroppedLastRef;
    }
    return RefCountReleaseStatus::kOtherRefsRemained;
  }

  virtual bool HasOneRef() const {
    return ref_count_.load(std::memory_order_acquire) == 1;
  }

 protected:
  ~RefCountedObject() override = default;

  mutable std::atomic<int> ref_count_{0};
};

template <class T>
class FinalRefCountedObject final : public T {
 public:
  using T::T;

  FinalRefCountedObject(const FinalRefCountedObject&) = delete;
  FinalRefCountedObject& operator=(const FinalRefCountedObject&) = delete;

  void AddRef() const {
    ref_count_.fetch_add(1, std::memory_order_relaxed);
  }

  RefCountReleaseStatus Release() const {
    if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      delete this;
      return RefCountReleaseStatus::kDroppedLastRef;
    }
    return RefCountReleaseStatus::kOtherRefsRemained;
  }

  bool HasOneRef() const {
    return ref_count_.load(std::memory_order_acquire) == 1;
  }

 private:
  ~FinalRefCountedObject() = default;
  mutable std::atomic<int> ref_count_{0};
};

}  // namespace rtc

namespace webrtc {
using rtc::FinalRefCountedObject;
using rtc::RefCountedObject;
}  // namespace webrtc
