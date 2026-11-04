#pragma once

#include <new>
#include <utility>

namespace android {
namespace base {

template <typename T>
class NoDestructor {
 public:
  template <typename... Args>
  explicit NoDestructor(Args&&... args) {
    new (storage_) T(std::forward<Args>(args)...);
  }

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;
  ~NoDestructor() = default;

  T* get() { return reinterpret_cast<T*>(storage_); }
  const T* get() const { return reinterpret_cast<const T*>(storage_); }
  T& operator*() { return *get(); }
  const T& operator*() const { return *get(); }
  T* operator->() { return get(); }
  const T* operator->() const { return get(); }

 private:
  alignas(T) unsigned char storage_[sizeof(T)];
};

}  // namespace base
}  // namespace android
