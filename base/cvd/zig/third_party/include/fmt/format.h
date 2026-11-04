#pragma once

#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "fmt/core.h"

namespace fmt {

class format_error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class memory_buffer {
 public:
  using value_type = char;
  using const_reference = const char&;

  void push_back(char c) { data_.push_back(c); }
  void append(const char* begin, const char* end) { data_.append(begin, end); }
  char* data() noexcept { return data_.data(); }
  const char* data() const noexcept { return data_.data(); }
  size_t size() const noexcept { return data_.size(); }
  void clear() noexcept { data_.clear(); }
  void reserve(size_t n) { data_.reserve(n); }
  std::string str() const { return data_; }

 private:
  std::string data_;
};

inline std::string to_string(const memory_buffer& buf) { return buf.str(); }

template <typename T>
const void* ptr(T p) {
  static_assert(std::is_pointer_v<T>, "ptr requires pointer");
  return static_cast<const void*>(p);
}

template <typename T>
const void* ptr(const std::unique_ptr<T>& p) {
  return p.get();
}

template <typename T>
const void* ptr(const std::shared_ptr<T>& p) {
  return p.get();
}

template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, int> = 0>
constexpr auto underlying(Enum e) noexcept -> std::underlying_type_t<Enum> {
  return static_cast<std::underlying_type_t<Enum>>(e);
}

}  // namespace fmt

#include "fmt/ranges.h"
