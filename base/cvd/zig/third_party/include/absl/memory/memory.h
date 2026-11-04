#pragma once

#include <memory>
#include <type_traits>

namespace absl {

template <typename T>
std::unique_ptr<T> WrapUnique(T* ptr) {
  static_assert(!std::is_array_v<T>, "array types are unsupported");
  static_assert(std::is_object_v<T>, "non-object types are unsupported");
  return std::unique_ptr<T>(ptr);
}

using std::make_unique;

}  // namespace absl
