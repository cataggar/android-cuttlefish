#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "json/forwards.h"

namespace Json {

enum ValueType {
  nullValue = 0,
  intValue,
  uintValue,
  realValue,
  stringValue,
  booleanValue,
  arrayValue,
  objectValue
};

enum CommentPlacement {
  commentBefore = 0,
  commentAfterOnSameLine,
  commentAfter,
  numberOfCommentPlacement
};

class StaticString {
 public:
  explicit constexpr StaticString(const char* czstring) : c_str_(czstring) {}
  constexpr operator const char*() const { return c_str_; }
  constexpr const char* c_str() const { return c_str_; }

 private:
  const char* c_str_;
};

class Value {
 public:
  using Members = std::vector<std::string>;
  using ObjectValues = std::map<std::string, Value>;
  using ArrayValues = std::vector<Value>;
  using value_type = Value;
  using Int = Json::Int;
  using UInt = Json::UInt;
  using Int64 = Json::Int64;
  using UInt64 = Json::UInt64;
  using LargestInt = Json::LargestInt;
  using LargestUInt = Json::LargestUInt;
  using ArrayIndex = Json::ArrayIndex;

  static const Value& nullSingleton();
  static const Value null;
  static const Value& nullRef;

  Value(ValueType type = nullValue);
  Value(int value);
  Value(unsigned int value);
  Value(long value);
  Value(unsigned long value);
  Value(long long value);
  Value(unsigned long long value);
  Value(double value);
  Value(const char* value);
  Value(const char* begin, const char* end);
  Value(const StaticString& value);
  Value(const std::string& value);
  Value(std::string_view value);
  Value(bool value);
  Value(const Value& other);
  Value(Value&& other) noexcept;
  ~Value();

  Value& operator=(const Value& other);
  Value& operator=(Value&& other) noexcept;

  void swap(Value& other) noexcept;
  void swapPayload(Value& other) noexcept;
  void copy(const Value& other);
  void copyPayload(const Value& other);

  ValueType type() const noexcept { return type_; }

  bool operator<(const Value& other) const;
  bool operator<=(const Value& other) const;
  bool operator>=(const Value& other) const;
  bool operator>(const Value& other) const;
  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const;
  int compare(const Value& other) const;

  const char* asCString() const;
  std::string asString() const;
  bool getString(char const** begin, char const** end) const;
  Int asInt() const;
  UInt asUInt() const;
  Int64 asInt64() const;
  UInt64 asUInt64() const;
  LargestInt asLargestInt() const;
  LargestUInt asLargestUInt() const;
  float asFloat() const;
  double asDouble() const;
  bool asBool() const;

  bool isNull() const noexcept { return type_ == nullValue; }
  bool isBool() const noexcept { return type_ == booleanValue; }
  bool isInt() const noexcept;
  bool isInt64() const noexcept;
  bool isUInt() const noexcept;
  bool isUInt64() const noexcept;
  bool isIntegral() const noexcept;
  bool isDouble() const noexcept {
    return type_ == intValue || type_ == uintValue || type_ == realValue;
  }
  bool isNumeric() const noexcept { return isDouble(); }
  bool isString() const noexcept { return type_ == stringValue; }
  bool isArray() const noexcept { return type_ == arrayValue; }
  bool isObject() const noexcept {
    return type_ == nullValue || type_ == objectValue;
  }
  bool isConvertibleTo(ValueType other) const noexcept;

  ArrayIndex size() const noexcept;
  bool empty() const noexcept;
  explicit operator bool() const noexcept { return !isNull(); }
  void clear();
  void resize(ArrayIndex newSize);

  Value& operator[](ArrayIndex index);
  Value& operator[](int index);
  const Value& operator[](ArrayIndex index) const;
  const Value& operator[](int index) const;

  Value get(ArrayIndex index, const Value& defaultValue) const;
  bool isValidIndex(ArrayIndex index) const noexcept;
  Value& append(const Value& value);
  Value& append(Value&& value);
  bool insert(ArrayIndex index, const Value& newValue);
  bool insert(ArrayIndex index, Value&& newValue);

  Value& operator[](const char* key);
  const Value& operator[](const char* key) const;
  Value& operator[](const std::string& key);
  const Value& operator[](const std::string& key) const;
  Value& operator[](const StaticString& key);

  Value get(const char* key, const Value& defaultValue) const;
  Value get(const char* begin, const char* end,
            const Value& defaultValue) const;
  Value get(const std::string& key, const Value& defaultValue) const;

  const Value* find(const char* begin, const char* end) const;
  const Value* find(const std::string& key) const {
    return find(key.data(), key.data() + key.size());
  }
  Value* demand(const char* begin, const char* end);

  void removeMember(const char* key);
  void removeMember(const std::string& key);
  bool removeMember(const char* key, Value* removed);
  bool removeMember(const std::string& key, Value* removed);
  bool removeMember(const char* begin, const char* end, Value* removed);
  bool removeIndex(ArrayIndex i, Value* removed);

  bool isMember(const char* key) const;
  bool isMember(const std::string& key) const;
  bool isMember(const char* begin, const char* end) const;

  Members getMemberNames() const;

  std::string toStyledString() const;

  class const_iterator;
  class iterator;

  const_iterator begin() const;
  const_iterator end() const;
  iterator begin();
  iterator end();

 private:
  friend class ValueIteratorBase;
  friend class const_iterator;
  friend class iterator;

  ValueType type_ = nullValue;
  Int64 int_val_ = 0;
  UInt64 uint_val_ = 0;
  double real_val_ = 0.0;
  bool bool_val_ = false;
  std::string str_val_;
  ArrayValues arr_val_;
  ObjectValues obj_val_;
};

class Value::const_iterator {
 public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = const Value;
  using difference_type = ptrdiff_t;
  using pointer = const Value*;
  using reference = const Value&;

  const_iterator() = default;
  const_iterator(ArrayValues::const_iterator it, ArrayIndex idx)
      : is_array_(true), arr_it_(it), idx_(idx) {}
  explicit const_iterator(ObjectValues::const_iterator it)
      : is_object_(true), obj_it_(it) {}

  reference operator*() const {
    if (is_array_) return *arr_it_;
    if (is_object_) return obj_it_->second;
    return Value::nullSingleton();
  }
  pointer operator->() const { return &(operator*()); }

  const_iterator& operator++() {
    if (is_array_) {
      ++arr_it_;
      ++idx_;
    } else if (is_object_) {
      ++obj_it_;
    }
    return *this;
  }
  const_iterator operator++(int) {
    const_iterator tmp = *this;
    ++(*this);
    return tmp;
  }
  const_iterator& operator--() {
    if (is_array_) {
      --arr_it_;
      --idx_;
    } else if (is_object_) {
      --obj_it_;
    }
    return *this;
  }
  const_iterator operator--(int) {
    const_iterator tmp = *this;
    --(*this);
    return tmp;
  }

  bool operator==(const const_iterator& other) const {
    if (is_array_ != other.is_array_ || is_object_ != other.is_object_) {
      return false;
    }
    if (is_array_) return arr_it_ == other.arr_it_;
    if (is_object_) return obj_it_ == other.obj_it_;
    return true;
  }
  bool operator!=(const const_iterator& other) const {
    return !(*this == other);
  }

  Value key() const {
    if (is_object_) return Value(obj_it_->first);
    if (is_array_) return Value(idx_);
    return Value();
  }
  ArrayIndex index() const { return is_array_ ? idx_ : static_cast<ArrayIndex>(-1); }
  std::string name() const { return is_object_ ? obj_it_->first : std::string(); }
  const char* memberName() const {
    return is_object_ ? obj_it_->first.c_str() : "";
  }

 protected:
  bool is_array_ = false;
  bool is_object_ = false;
  ArrayValues::const_iterator arr_it_{};
  ObjectValues::const_iterator obj_it_{};
  ArrayIndex idx_ = 0;
};

class Value::iterator : public Value::const_iterator {
 public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = Value;
  using difference_type = ptrdiff_t;
  using pointer = Value*;
  using reference = Value&;

  iterator() = default;
  iterator(ArrayValues::iterator it, ArrayIndex idx)
      : const_iterator(it, idx), mut_arr_it_(it) {}
  explicit iterator(ObjectValues::iterator it)
      : const_iterator(it), mut_obj_it_(it) {}

  reference operator*() const {
    if (is_array_) return *mut_arr_it_;
    if (is_object_) return mut_obj_it_->second;
    return const_cast<Value&>(Value::nullSingleton());
  }
  pointer operator->() const { return &(operator*()); }

  iterator& operator++() {
    if (is_array_) {
      ++mut_arr_it_;
      ++arr_it_;
      ++idx_;
    } else if (is_object_) {
      ++mut_obj_it_;
      ++obj_it_;
    }
    return *this;
  }
  iterator operator++(int) {
    iterator tmp = *this;
    ++(*this);
    return tmp;
  }
  iterator& operator--() {
    if (is_array_) {
      --mut_arr_it_;
      --arr_it_;
      --idx_;
    } else if (is_object_) {
      --mut_obj_it_;
      --obj_it_;
    }
    return *this;
  }
  iterator operator--(int) {
    iterator tmp = *this;
    --(*this);
    return tmp;
  }

 private:
  ArrayValues::iterator mut_arr_it_{};
  ObjectValues::iterator mut_obj_it_{};
};

using ValueConstIterator = Value::const_iterator;
using ValueIterator = Value::iterator;

std::ostream& operator<<(std::ostream& os, const Value& root);
std::istream& operator>>(std::istream& is, Value& root);

}  // namespace Json
