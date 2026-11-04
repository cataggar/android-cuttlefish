#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace rtc {

class CopyOnWriteBuffer {
 public:
  CopyOnWriteBuffer() = default;
  CopyOnWriteBuffer(const CopyOnWriteBuffer&) = default;
  CopyOnWriteBuffer(CopyOnWriteBuffer&&) noexcept = default;
  CopyOnWriteBuffer& operator=(const CopyOnWriteBuffer&) = default;
  CopyOnWriteBuffer& operator=(CopyOnWriteBuffer&&) noexcept = default;

  explicit CopyOnWriteBuffer(size_t size) : data_(size) {}
  CopyOnWriteBuffer(size_t size, size_t capacity) : data_(size) {
    data_.reserve(capacity);
  }

  template <typename T,
            typename std::enable_if<sizeof(T) == 1 &&
                                        std::is_trivial<T>::value,
                                    int>::type = 0>
  CopyOnWriteBuffer(const T* data, size_t size) {
    SetData(data, size);
  }

  template <typename T,
            typename std::enable_if<sizeof(T) == 1 &&
                                        std::is_trivial<T>::value,
                                    int>::type = 0>
  CopyOnWriteBuffer(const T* data, size_t size, size_t capacity) {
    data_.reserve(capacity);
    SetData(data, size);
  }

  template <typename Container,
            typename = decltype(std::declval<const Container&>().data()),
            typename = decltype(std::declval<const Container&>().size()),
            typename std::enable_if<
                !std::is_same<typename std::decay<Container>::type,
                              CopyOnWriteBuffer>::value,
                int>::type = 0>
  explicit CopyOnWriteBuffer(const Container& c) {
    SetData(c.data(), c.size());
  }

  ~CopyOnWriteBuffer() = default;

  template <typename U = uint8_t>
  const U* cdata() const {
    return reinterpret_cast<const U*>(data_.data());
  }

  template <typename U = uint8_t>
  const U* data() const {
    return reinterpret_cast<const U*>(data_.data());
  }

  template <typename U = uint8_t>
  U* MutableData() {
    return reinterpret_cast<U*>(data_.data());
  }

  size_t size() const { return data_.size(); }
  size_t capacity() const { return data_.capacity(); }
  bool empty() const { return data_.empty(); }

  template <typename T,
            typename std::enable_if<sizeof(T) == 1 &&
                                        std::is_trivial<T>::value,
                                    int>::type = 0>
  void SetData(const T* data, size_t size) {
    if (!data || size == 0) {
      data_.clear();
      return;
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    data_.assign(bytes, bytes + size);
  }

  template <typename Container,
            typename = decltype(std::declval<const Container&>().data()),
            typename = decltype(std::declval<const Container&>().size())>
  void SetData(const Container& c) {
    SetData(c.data(), c.size());
  }

  template <typename T,
            typename std::enable_if<sizeof(T) == 1 &&
                                        std::is_trivial<T>::value,
                                    int>::type = 0>
  void AppendData(const T* data, size_t size) {
    if (!data || size == 0) {
      return;
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    data_.insert(data_.end(), bytes, bytes + size);
  }

  void SetSize(size_t size) { data_.resize(size); }
  void EnsureCapacity(size_t capacity) { data_.reserve(capacity); }
  void Clear() { data_.clear(); }

  bool operator==(const CopyOnWriteBuffer& other) const {
    return data_ == other.data_;
  }
  bool operator!=(const CopyOnWriteBuffer& other) const {
    return data_ != other.data_;
  }

 private:
  std::vector<uint8_t> data_;
};

}  // namespace rtc
