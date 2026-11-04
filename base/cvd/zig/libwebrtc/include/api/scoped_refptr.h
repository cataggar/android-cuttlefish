#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"

namespace rtc {

template <class T>
class scoped_refptr {
 public:
  typedef T element_type;

  scoped_refptr() : ptr_(nullptr) {}
  scoped_refptr(std::nullptr_t) : ptr_(nullptr) {}  // NOLINT

  scoped_refptr(T* p) : ptr_(p) {  // NOLINT
    if (ptr_) {
      ptr_->AddRef();
    }
  }

  scoped_refptr(const scoped_refptr<T>& r) : ptr_(r.ptr_) {
    if (ptr_) {
      ptr_->AddRef();
    }
  }

  template <typename U,
            typename std::enable_if<std::is_convertible<U*, T*>::value,
                                    int>::type = 0>
  scoped_refptr(const scoped_refptr<U>& r) : ptr_(r.get()) {  // NOLINT
    if (ptr_) {
      ptr_->AddRef();
    }
  }

  scoped_refptr(scoped_refptr<T>&& r) noexcept : ptr_(r.release()) {}

  template <typename U,
            typename std::enable_if<std::is_convertible<U*, T*>::value,
                                    int>::type = 0>
  scoped_refptr(scoped_refptr<U>&& r) noexcept : ptr_(r.release()) {}  // NOLINT

  ~scoped_refptr() {
    if (ptr_) {
      ptr_->Release();
    }
  }

  T* get() const { return ptr_; }
  operator T*() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  T* release() {
    T* retVal = ptr_;
    ptr_ = nullptr;
    return retVal;
  }

  scoped_refptr<T>& operator=(T* p) {
    if (p) {
      p->AddRef();
    }
    if (ptr_) {
      ptr_->Release();
    }
    ptr_ = p;
    return *this;
  }

  scoped_refptr<T>& operator=(const scoped_refptr<T>& r) {
    return *this = r.ptr_;
  }

  template <typename U>
  scoped_refptr<T>& operator=(const scoped_refptr<U>& r) {
    return *this = r.get();
  }

  scoped_refptr<T>& operator=(scoped_refptr<T>&& r) noexcept {
    scoped_refptr<T>(std::move(r)).swap(*this);
    return *this;
  }

  template <typename U>
  scoped_refptr<T>& operator=(scoped_refptr<U>&& r) noexcept {
    scoped_refptr<T>(std::move(r)).swap(*this);
    return *this;
  }

  void swap(T** pp) noexcept {
    T* p = ptr_;
    ptr_ = *pp;
    *pp = p;
  }

  void swap(scoped_refptr<T>& r) noexcept { swap(&r.ptr_); }

 protected:
  T* ptr_;
};

template <typename T, typename... Args>
scoped_refptr<T> make_ref_counted(Args&&... args) {
  return scoped_refptr<T>(new RefCountedObject<T>(std::forward<Args>(args)...));
}

}  // namespace rtc

namespace webrtc {
using rtc::make_ref_counted;
using rtc::scoped_refptr;
}  // namespace webrtc
