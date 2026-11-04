#pragma once

#include <type_traits>
#include <vector>

#include "fruit/component.h"

namespace fruit {

template <typename... Params>
class Injector {
 public:
  template <typename CompFn, typename... Args>
  explicit Injector(CompFn&& fn, Args&&... args) {
    auto comp = fn(std::forward<Args>(args)...);
    container_.mergeFrom(comp.container());
  }

  template <typename T>
  T get() {
    using Raw = impl::RawType<T>;
    Raw* ptr = container_.getOrCreate<Raw>();
    if constexpr (std::is_pointer_v<T>) {
      return ptr;
    } else {
      return *ptr;
    }
  }

  template <typename I>
  const std::vector<I*>& getMultibindings() {
    return container_.getMultibindings<I>();
  }

 private:
  impl::Container container_;
};

}  // namespace fruit
