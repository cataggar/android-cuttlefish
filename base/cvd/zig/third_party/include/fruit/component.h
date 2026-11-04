#pragma once

#include <any>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "fruit/fruit_forward_decls.h"
#include "fruit/macro.h"

namespace fruit {

namespace impl {

template <typename T>
struct UnwrapAnnotated {
  using Type = T;
};

template <typename Annotation, typename T>
struct UnwrapAnnotated<Annotated<Annotation, T>> {
  using Type = T;
};

template <typename T>
using RawType = std::remove_cv_t<
    std::remove_pointer_t<std::remove_reference_t<typename UnwrapAnnotated<T>::Type>>>;

class Container {
 public:
  using FactoryFn = std::function<void*(Container&)>;

  struct State {
    std::unordered_map<std::type_index, void*> instances;
    std::unordered_map<std::type_index, FactoryFn> bindings;
    std::unordered_map<std::type_index, std::vector<FactoryFn>> multibindings;
    std::unordered_map<std::type_index, std::vector<void*>> resolved_multibindings;
    std::vector<std::shared_ptr<void>> owned;
  };

  Container() : state_(std::make_shared<State>()) {}

  void mergeFrom(const Container& other) {
    for (const auto& [k, v] : other.state_->instances) {
      state_->instances[k] = v;
    }
    for (const auto& [k, v] : other.state_->bindings) {
      state_->bindings[k] = v;
    }
    for (const auto& [k, vec] : other.state_->multibindings) {
      auto& dst = state_->multibindings[k];
      dst.insert(dst.end(), vec.begin(), vec.end());
    }
    for (const auto& o : other.state_->owned) {
      state_->owned.push_back(o);
    }
  }

  template <typename KeyType, typename ValueType>
  void bindInstance(ValueType& instance) {
    state_->instances[std::type_index(typeid(KeyType))] =
        const_cast<void*>(static_cast<const void*>(std::addressof(instance)));
    using Raw = RawType<KeyType>;
    state_->instances[std::type_index(typeid(Raw))] =
        const_cast<void*>(static_cast<const void*>(std::addressof(instance)));
  }

  template <typename I, typename C>
  void bindInterface() {
    using RawI = RawType<I>;
    using RawC = RawType<C>;
    state_->bindings[std::type_index(typeid(RawI))] = [](Container& c) -> void* {
      RawC* impl_ptr = c.getOrCreate<RawC>();
      RawI* iface_ptr = static_cast<RawI*>(impl_ptr);
      return static_cast<void*>(iface_ptr);
    };
  }

  template <typename I, typename C>
  void addMultibinding() {
    using RawI = RawType<I>;
    using RawC = RawType<C>;
    state_->multibindings[std::type_index(typeid(RawI))].push_back(
        [](Container& c) -> void* {
          RawC* impl_ptr = c.getOrCreate<RawC>();
          RawI* iface_ptr = static_cast<RawI*>(impl_ptr);
          return static_cast<void*>(iface_ptr);
        });
  }

  template <typename I>
  void addInstanceMultibinding(I& instance) {
    using RawI = RawType<I>;
    RawI* ptr = std::addressof(instance);
    state_->multibindings[std::type_index(typeid(RawI))].push_back(
        [ptr](Container&) -> void* { return static_cast<void*>(ptr); });
  }

  void ownShared(std::shared_ptr<void> sp) {
    state_->owned.push_back(std::move(sp));
  }

  template <typename T>
  void registerProviderFn(std::function<T*(Container&)> fn) {
    using RawT = RawType<T>;
    state_->bindings[std::type_index(typeid(RawT))] =
        [fn = std::move(fn)](Container& c) -> void* {
      return static_cast<void*>(fn(c));
    };
  }

  template <typename Spec>
  auto resolveArg() -> std::conditional_t<
      std::is_pointer_v<typename UnwrapAnnotated<Spec>::Type>,
      RawType<Spec>*, RawType<Spec>&> {
    using Unwrapped = typename UnwrapAnnotated<Spec>::Type;
    using Raw = RawType<Spec>;
    auto key_exact = std::type_index(typeid(Spec));
    auto it_exact = state_->instances.find(key_exact);
    Raw* ptr = nullptr;
    if (it_exact != state_->instances.end()) {
      ptr = static_cast<Raw*>(it_exact->second);
    } else {
      ptr = getOrCreate<Raw>();
    }
    if constexpr (std::is_pointer_v<Unwrapped>) {
      return ptr;
    } else {
      return *ptr;
    }
  }

  template <typename T>
  T* getOrCreate() {
    using Raw = RawType<T>;
    auto key = std::type_index(typeid(Raw));
    auto it = state_->instances.find(key);
    if (it != state_->instances.end()) {
      return static_cast<Raw*>(it->second);
    }
    auto bit = state_->bindings.find(key);
    if (bit != state_->bindings.end()) {
      void* p = bit->second(*this);
      state_->instances[key] = p;
      return static_cast<Raw*>(p);
    }
    if constexpr (requires { typename Raw::FruitInjectSignature; }) {
      Raw* created = constructFromSignature<Raw>(
          static_cast<typename Raw::FruitInjectSignature*>(nullptr));
      state_->instances[key] = created;
      return created;
    } else if constexpr (std::is_default_constructible_v<Raw> &&
                         !std::is_abstract_v<Raw>) {
      auto sp = std::make_shared<Raw>();
      Raw* created = sp.get();
      state_->owned.push_back(std::move(sp));
      state_->instances[key] = created;
      return created;
    } else {
      return nullptr;
    }
  }

  template <typename I>
  const std::vector<I*>& getMultibindings() {
    using RawI = RawType<I>;
    auto key = std::type_index(typeid(RawI));
    auto& resolved = state_->resolved_multibindings[key];
    auto& factories = state_->multibindings[key];
    while (resolved.size() < factories.size()) {
      size_t idx = resolved.size();
      resolved.push_back(factories[idx](*this));
    }
    return reinterpret_cast<const std::vector<I*>&>(resolved);
  }

 private:
  template <typename C, typename R, typename... Args>
  C* constructFromSignature(R (*)(Args...)) {
    auto sp = std::make_shared<C>(resolveArg<Args>()...);
    C* ptr = sp.get();
    state_->owned.push_back(std::move(sp));
    return ptr;
  }

  std::shared_ptr<State> state_;
};

template <typename T>
struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

template <typename R, typename... Args>
struct FunctionTraits<R (*)(Args...)> {
  using ReturnType = R;
  template <typename F>
  static R invoke(F& f, Container& c) {
    return f(c.resolveArg<Args>()...);
  }
};

template <typename ClassType, typename R, typename... Args>
struct FunctionTraits<R (ClassType::*)(Args...) const> {
  using ReturnType = R;
  template <typename F>
  static R invoke(const F& f, Container& c) {
    return f(c.resolveArg<Args>()...);
  }
};

template <typename ClassType, typename R, typename... Args>
struct FunctionTraits<R (ClassType::*)(Args...)> {
  using ReturnType = R;
  template <typename F>
  static R invoke(F& f, Container& c) {
    return f(c.resolveArg<Args>()...);
  }
};

}  // namespace impl

class PartialComponent {
 public:
  PartialComponent() = default;
  explicit PartialComponent(impl::Container c) : container_(std::move(c)) {}

  template <typename I, typename C>
  PartialComponent& bind() {
    container_.bindInterface<I, C>();
    return *this;
  }

  template <typename T>
  PartialComponent& bindInstance(T& instance) {
    container_.bindInstance<T, T>(instance);
    return *this;
  }

  template <typename AnnotatedT, typename T>
  PartialComponent& bindInstance(T& instance) {
    container_.bindInstance<AnnotatedT, T>(instance);
    return *this;
  }

  template <typename I, typename C>
  PartialComponent& addMultibinding() {
    container_.addMultibinding<I, C>();
    return *this;
  }

  template <typename I>
  PartialComponent& addInstanceMultibinding(I& instance) {
    container_.addInstanceMultibinding<I>(instance);
    return *this;
  }

  template <typename CompFn, typename... Args>
  PartialComponent& install(CompFn&& fn, Args&&... args) {
    auto comp = fn(std::forward<Args>(args)...);
    container_.mergeFrom(comp.container());
    return *this;
  }

  template <typename Provider>
  PartialComponent& registerProvider(Provider provider) {
    using Traits = impl::FunctionTraits<std::decay_t<Provider>>;
    using Ret = typename Traits::ReturnType;
    using RawRet = impl::RawType<Ret>;
    container_.registerProviderFn<RawRet>(
        [provider = std::move(provider)](impl::Container& c) mutable -> RawRet* {
          if constexpr (std::is_pointer_v<Ret>) {
            Ret ptr = Traits::invoke(provider, c);
            return ptr;
          } else {
            auto sp = std::make_shared<RawRet>(Traits::invoke(provider, c));
            RawRet* ptr = sp.get();
            c.ownShared(std::move(sp));
            return ptr;
          }
        });
    return *this;
  }

  const impl::Container& container() const { return container_; }
  impl::Container& container() { return container_; }

 private:
  impl::Container container_;
};

template <typename... Params>
class Component {
 public:
  Component() = default;
  Component(const PartialComponent& pc) : container_(pc.container()) {}
  Component(PartialComponent&& pc) : container_(std::move(pc.container())) {}

  template <typename... OtherParams>
  Component(const Component<OtherParams...>& other)
      : container_(other.container()) {}

  const impl::Container& container() const { return container_; }
  impl::Container& container() { return container_; }

 private:
  impl::Container container_;
};

inline PartialComponent createComponent() { return PartialComponent(); }

}  // namespace fruit
