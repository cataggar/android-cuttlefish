#include "json/json.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace Json {

const Value& Value::nullSingleton() {
  static const Value kNull;
  return kNull;
}

const Value Value::null;
const Value& Value::nullRef = Value::null;

// Value constructors
Value::Value(ValueType type) : type_(type) {}

Value::Value(int value)
    : type_(intValue), int_val_(value), uint_val_(static_cast<LargestUInt>(value)), real_val_(value) {}

Value::Value(unsigned int value)
    : type_(uintValue), int_val_(static_cast<LargestInt>(value)), uint_val_(value), real_val_(value) {}

Value::Value(long value)
    : type_(intValue), int_val_(value), uint_val_(static_cast<LargestUInt>(value)), real_val_(static_cast<double>(value)) {}

Value::Value(unsigned long value)
    : type_(uintValue), int_val_(static_cast<LargestInt>(value)), uint_val_(value), real_val_(static_cast<double>(value)) {}

Value::Value(long long value)
    : type_(intValue), int_val_(value), uint_val_(static_cast<LargestUInt>(value)), real_val_(static_cast<double>(value)) {}

Value::Value(unsigned long long value)
    : type_(uintValue), int_val_(static_cast<LargestInt>(value)), uint_val_(value), real_val_(static_cast<double>(value)) {}

Value::Value(double value)
    : type_(realValue),
      int_val_(static_cast<LargestInt>(value)),
      uint_val_(static_cast<LargestUInt>(value)),
      real_val_(value) {}

Value::Value(const char* value)
    : type_(stringValue), str_val_(value ? value : "") {}

Value::Value(const char* begin, const char* end)
    : type_(stringValue), str_val_(begin, end) {}

Value::Value(const StaticString& value)
    : type_(stringValue), str_val_(value.c_str() ? value.c_str() : "") {}

Value::Value(const std::string& value)
    : type_(stringValue), str_val_(value) {}

Value::Value(std::string_view value)
    : type_(stringValue), str_val_(value) {}

Value::Value(bool value)
    : type_(booleanValue),
      int_val_(value ? 1 : 0),
      uint_val_(value ? 1 : 0),
      real_val_(value ? 1.0 : 0.0),
      bool_val_(value) {}

Value::Value(const Value& other) = default;
Value::Value(Value&& other) noexcept = default;
Value::~Value() = default;

Value& Value::operator=(const Value& other) = default;
Value& Value::operator=(Value&& other) noexcept = default;

void Value::swap(Value& other) noexcept {
  std::swap(type_, other.type_);
  std::swap(int_val_, other.int_val_);
  std::swap(uint_val_, other.uint_val_);
  std::swap(real_val_, other.real_val_);
  std::swap(bool_val_, other.bool_val_);
  std::swap(str_val_, other.str_val_);
  std::swap(arr_val_, other.arr_val_);
  std::swap(obj_val_, other.obj_val_);
}

void Value::swapPayload(Value& other) noexcept { swap(other); }
void Value::copy(const Value& other) { *this = other; }
void Value::copyPayload(const Value& other) { *this = other; }

bool Value::operator<(const Value& other) const {
  if (type_ != other.type_) return type_ < other.type_;
  switch (type_) {
    case nullValue:
      return false;
    case intValue:
      return int_val_ < other.int_val_;
    case uintValue:
      return uint_val_ < other.uint_val_;
    case realValue:
      return real_val_ < other.real_val_;
    case booleanValue:
      return bool_val_ < other.bool_val_;
    case stringValue:
      return str_val_ < other.str_val_;
    case arrayValue:
      return arr_val_ < other.arr_val_;
    case objectValue:
      return obj_val_ < other.obj_val_;
  }
  return false;
}

bool Value::operator<=(const Value& other) const { return !(other < *this); }
bool Value::operator>=(const Value& other) const { return !(*this < other); }
bool Value::operator>(const Value& other) const { return other < *this; }

bool Value::operator==(const Value& other) const {
  if (type_ == other.type_) {
    switch (type_) {
      case nullValue:
        return true;
      case intValue:
        return int_val_ == other.int_val_;
      case uintValue:
        return uint_val_ == other.uint_val_;
      case realValue:
        return real_val_ == other.real_val_;
      case booleanValue:
        return bool_val_ == other.bool_val_;
      case stringValue:
        return str_val_ == other.str_val_;
      case arrayValue:
        return arr_val_ == other.arr_val_;
      case objectValue:
        return obj_val_ == other.obj_val_;
    }
  }
  if ((type_ == intValue || type_ == uintValue || type_ == realValue) &&
      (other.type_ == intValue || other.type_ == uintValue ||
       other.type_ == realValue)) {
    return asDouble() == other.asDouble();
  }
  return false;
}

bool Value::operator!=(const Value& other) const { return !(*this == other); }

int Value::compare(const Value& other) const {
  if (*this < other) return -1;
  if (*this > other) return 1;
  return 0;
}

const char* Value::asCString() const { return str_val_.c_str(); }

std::string Value::asString() const {
  switch (type_) {
    case nullValue:
      return "";
    case stringValue:
      return str_val_;
    case booleanValue:
      return bool_val_ ? "true" : "false";
    case intValue:
      return std::to_string(int_val_);
    case uintValue:
      return std::to_string(uint_val_);
    case realValue: {
      std::ostringstream oss;
      oss << real_val_;
      return oss.str();
    }
    default:
      return "";
  }
}

bool Value::getString(char const** begin, char const** end) const {
  if (type_ != stringValue) return false;
  if (begin) *begin = str_val_.data();
  if (end) *end = str_val_.data() + str_val_.size();
  return true;
}

Int Value::asInt() const { return static_cast<Int>(asLargestInt()); }
UInt Value::asUInt() const { return static_cast<UInt>(asLargestUInt()); }
Int64 Value::asInt64() const { return static_cast<Int64>(asLargestInt()); }
UInt64 Value::asUInt64() const { return static_cast<UInt64>(asLargestUInt()); }

LargestInt Value::asLargestInt() const {
  switch (type_) {
    case nullValue:
      return 0;
    case intValue:
      return int_val_;
    case uintValue:
      return static_cast<LargestInt>(uint_val_);
    case realValue:
      return static_cast<LargestInt>(real_val_);
    case booleanValue:
      return bool_val_ ? 1 : 0;
    case stringValue:
      return std::strtoll(str_val_.c_str(), nullptr, 10);
    default:
      return 0;
  }
}

LargestUInt Value::asLargestUInt() const {
  switch (type_) {
    case nullValue:
      return 0;
    case intValue:
      return static_cast<LargestUInt>(int_val_);
    case uintValue:
      return uint_val_;
    case realValue:
      return static_cast<LargestUInt>(real_val_);
    case booleanValue:
      return bool_val_ ? 1 : 0;
    case stringValue:
      return std::strtoull(str_val_.c_str(), nullptr, 10);
    default:
      return 0;
  }
}

float Value::asFloat() const { return static_cast<float>(asDouble()); }

double Value::asDouble() const {
  switch (type_) {
    case nullValue:
      return 0.0;
    case intValue:
      return static_cast<double>(int_val_);
    case uintValue:
      return static_cast<double>(uint_val_);
    case realValue:
      return real_val_;
    case booleanValue:
      return bool_val_ ? 1.0 : 0.0;
    case stringValue:
      return std::strtod(str_val_.c_str(), nullptr);
    default:
      return 0.0;
  }
}

bool Value::asBool() const {
  switch (type_) {
    case nullValue:
      return false;
    case booleanValue:
      return bool_val_;
    case intValue:
      return int_val_ != 0;
    case uintValue:
      return uint_val_ != 0;
    case realValue:
      return real_val_ != 0.0;
    case stringValue:
      return !str_val_.empty();
    case arrayValue:
      return !arr_val_.empty();
    case objectValue:
      return !obj_val_.empty();
  }
  return false;
}

bool Value::isInt() const noexcept {
  if (type_ == intValue) {
    return int_val_ >= std::numeric_limits<Int>::min() &&
           int_val_ <= std::numeric_limits<Int>::max();
  }
  if (type_ == uintValue) {
    return uint_val_ <= static_cast<LargestUInt>(std::numeric_limits<Int>::max());
  }
  if (type_ == realValue) {
    double integral = 0.0;
    if (std::modf(real_val_, &integral) == 0.0) {
      return integral >= std::numeric_limits<Int>::min() &&
             integral <= std::numeric_limits<Int>::max();
    }
  }
  return false;
}

bool Value::isInt64() const noexcept {
  if (type_ == intValue) return true;
  if (type_ == uintValue) {
    return uint_val_ <= static_cast<LargestUInt>(std::numeric_limits<Int64>::max());
  }
  if (type_ == realValue) {
    double integral = 0.0;
    return std::modf(real_val_, &integral) == 0.0 &&
           integral >= static_cast<double>(std::numeric_limits<Int64>::min()) &&
           integral < static_cast<double>(std::numeric_limits<Int64>::max());
  }
  return false;
}

bool Value::isUInt() const noexcept {
  if (type_ == uintValue) {
    return uint_val_ <= std::numeric_limits<UInt>::max();
  }
  if (type_ == intValue) {
    return int_val_ >= 0 &&
           static_cast<LargestUInt>(int_val_) <= std::numeric_limits<UInt>::max();
  }
  if (type_ == realValue) {
    double integral = 0.0;
    if (std::modf(real_val_, &integral) == 0.0) {
      return integral >= 0.0 && integral <= std::numeric_limits<UInt>::max();
    }
  }
  return false;
}

bool Value::isUInt64() const noexcept {
  if (type_ == uintValue) return true;
  if (type_ == intValue) return int_val_ >= 0;
  if (type_ == realValue) {
    double integral = 0.0;
    return std::modf(real_val_, &integral) == 0.0 && integral >= 0.0 &&
           integral < static_cast<double>(std::numeric_limits<UInt64>::max());
  }
  return false;
}

bool Value::isIntegral() const noexcept {
  if (type_ == intValue || type_ == uintValue) return true;
  if (type_ == realValue) {
    double integral = 0.0;
    return std::modf(real_val_, &integral) == 0.0;
  }
  return false;
}

bool Value::isConvertibleTo(ValueType other) const noexcept {
  if (other == type_) return true;
  if (type_ == nullValue) return true;
  switch (other) {
    case nullValue:
      return false;
    case intValue:
      return isInt() || type_ == booleanValue;
    case uintValue:
      return isUInt() || type_ == booleanValue;
    case realValue:
      return isNumeric() || type_ == booleanValue;
    case booleanValue:
      return isNumeric() || type_ == booleanValue;
    case stringValue:
      return isNumeric() || type_ == booleanValue || type_ == stringValue;
    case arrayValue:
      return type_ == arrayValue;
    case objectValue:
      return type_ == objectValue;
  }
  return false;
}

ArrayIndex Value::size() const noexcept {
  if (type_ == arrayValue) {
    return static_cast<ArrayIndex>(arr_val_.size());
  }
  if (type_ == objectValue) {
    return static_cast<ArrayIndex>(obj_val_.size());
  }
  return 0;
}

bool Value::empty() const noexcept {
  if (type_ == nullValue) return true;
  if (type_ == arrayValue) return arr_val_.empty();
  if (type_ == objectValue) return obj_val_.empty();
  return false;
}

void Value::clear() {
  arr_val_.clear();
  obj_val_.clear();
  if (type_ == intValue || type_ == uintValue || type_ == realValue) {
    int_val_ = 0;
    uint_val_ = 0;
    real_val_ = 0.0;
  } else if (type_ == booleanValue) {
    bool_val_ = false;
  } else if (type_ == stringValue) {
    str_val_.clear();
  }
}

void Value::resize(ArrayIndex newSize) {
  if (type_ == nullValue) type_ = arrayValue;
  if (newSize == 0) {
    arr_val_.clear();
  } else {
    arr_val_.resize(newSize);
  }
}

Value& Value::operator[](ArrayIndex index) {
  if (type_ == nullValue) type_ = arrayValue;
  if (index >= arr_val_.size()) {
    arr_val_.resize(index + 1);
  }
  return arr_val_[index];
}

Value& Value::operator[](int index) {
  return (*this)[static_cast<ArrayIndex>(index < 0 ? 0 : index)];
}

const Value& Value::operator[](ArrayIndex index) const {
  if (type_ != arrayValue || index >= arr_val_.size()) {
    return nullSingleton();
  }
  return arr_val_[index];
}

const Value& Value::operator[](int index) const {
  if (index < 0) return nullSingleton();
  return (*this)[static_cast<ArrayIndex>(index)];
}

Value Value::get(ArrayIndex index, const Value& defaultValue) const {
  if (type_ != arrayValue || index >= arr_val_.size()) {
    return defaultValue;
  }
  return arr_val_[index];
}

bool Value::isValidIndex(ArrayIndex index) const noexcept {
  return type_ == arrayValue && index < arr_val_.size();
}

Value& Value::append(const Value& value) {
  if (type_ == nullValue) type_ = arrayValue;
  arr_val_.push_back(value);
  return arr_val_.back();
}

Value& Value::append(Value&& value) {
  if (type_ == nullValue) type_ = arrayValue;
  arr_val_.push_back(std::move(value));
  return arr_val_.back();
}

bool Value::insert(ArrayIndex index, const Value& newValue) {
  if (type_ == nullValue) type_ = arrayValue;
  if (type_ != arrayValue || index > arr_val_.size()) return false;
  arr_val_.insert(arr_val_.begin() + index, newValue);
  return true;
}

bool Value::insert(ArrayIndex index, Value&& newValue) {
  if (type_ == nullValue) type_ = arrayValue;
  if (type_ != arrayValue || index > arr_val_.size()) return false;
  arr_val_.insert(arr_val_.begin() + index, std::move(newValue));
  return true;
}

Value& Value::operator[](const char* key) {
  return (*this)[std::string(key ? key : "")];
}

const Value& Value::operator[](const char* key) const {
  return (*this)[std::string(key ? key : "")];
}

Value& Value::operator[](const std::string& key) {
  if (type_ == nullValue) type_ = objectValue;
  return obj_val_[key];
}

const Value& Value::operator[](const std::string& key) const {
  if (type_ != objectValue) return nullSingleton();
  auto it = obj_val_.find(key);
  if (it == obj_val_.end()) return nullSingleton();
  return it->second;
}

Value& Value::operator[](const StaticString& key) {
  return (*this)[std::string(key.c_str() ? key.c_str() : "")];
}

Value Value::get(const char* key, const Value& defaultValue) const {
  return get(std::string(key ? key : ""), defaultValue);
}

Value Value::get(const char* begin, const char* end,
                 const Value& defaultValue) const {
  return get(std::string(begin, end), defaultValue);
}

Value Value::get(const std::string& key, const Value& defaultValue) const {
  const Value* found = find(key.data(), key.data() + key.size());
  return found ? *found : defaultValue;
}

const Value* Value::find(char const* begin, char const* end) const {
  if (type_ != objectValue) return nullptr;
  auto it = obj_val_.find(std::string(begin, end));
  if (it == obj_val_.end()) return nullptr;
  return &it->second;
}

Value* Value::demand(char const* begin, char const* end) {
  if (type_ == nullValue) type_ = objectValue;
  if (type_ != objectValue) return nullptr;
  return &obj_val_[std::string(begin, end)];
}

void Value::removeMember(const char* key) {
  removeMember(std::string(key ? key : ""));
}

void Value::removeMember(const std::string& key) {
  if (type_ == objectValue) {
    obj_val_.erase(key);
  }
}

bool Value::removeMember(const char* key, Value* removed) {
  return removeMember(std::string(key ? key : ""), removed);
}

bool Value::removeMember(std::string const& key, Value* removed) {
  if (type_ != objectValue) return false;
  auto it = obj_val_.find(key);
  if (it == obj_val_.end()) return false;
  if (removed) *removed = std::move(it->second);
  obj_val_.erase(it);
  return true;
}

bool Value::removeMember(const char* begin, const char* end, Value* removed) {
  return removeMember(std::string(begin, end), removed);
}

bool Value::removeIndex(ArrayIndex index, Value* removed) {
  if (type_ != arrayValue || index >= arr_val_.size()) return false;
  if (removed) *removed = std::move(arr_val_[index]);
  arr_val_.erase(arr_val_.begin() + index);
  return true;
}

bool Value::isMember(const char* key) const {
  return isMember(std::string(key ? key : ""));
}

bool Value::isMember(const std::string& key) const {
  if (type_ != objectValue) return false;
  return obj_val_.find(key) != obj_val_.end();
}

bool Value::isMember(const char* begin, const char* end) const {
  return isMember(std::string(begin, end));
}

Value::Members Value::getMemberNames() const {
  Members names;
  if (type_ == objectValue) {
    names.reserve(obj_val_.size());
    for (const auto& kv : obj_val_) {
      names.push_back(kv.first);
    }
  }
  return names;
}

std::string Value::toStyledString() const {
  StreamWriterBuilder builder;
  builder["indentation"] = "   ";
  return writeString(builder, *this);
}

Value::const_iterator Value::begin() const {
  if (type_ == arrayValue) {
    return const_iterator(arr_val_.begin(), 0);
  }
  return const_iterator(obj_val_.begin());
}

Value::const_iterator Value::end() const {
  if (type_ == arrayValue) {
    return const_iterator(arr_val_.end(),
                          static_cast<ArrayIndex>(arr_val_.size()));
  }
  return const_iterator(obj_val_.end());
}

Value::iterator Value::begin() {
  if (type_ == arrayValue) {
    return iterator(arr_val_.begin(), 0);
  }
  return iterator(obj_val_.begin());
}

Value::iterator Value::end() {
  if (type_ == arrayValue) {
    return iterator(arr_val_.end(), static_cast<ArrayIndex>(arr_val_.size()));
  }
  return iterator(obj_val_.end());
}

// Recursive descent JSON Parser
namespace {

class JsonParser {
 public:
  JsonParser(const char* begin, const char* end)
      : cur_(begin), end_(end), begin_(begin) {}

  bool parse(Value* root, std::string* errs) {
    skipWhitespaceAndComments();
    if (cur_ >= end_) {
      if (errs) *errs = "Empty JSON document";
      return false;
    }
    if (!parseValue(root, errs)) {
      return false;
    }
    skipWhitespaceAndComments();
    if (cur_ < end_) {
      if (errs) *errs = "Extra non-whitespace after JSON value";
      return false;
    }
    return true;
  }

 private:
  void skipWhitespaceAndComments() {
    while (cur_ < end_) {
      char c = *cur_;
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++cur_;
      } else if (c == '/' && cur_ + 1 < end_) {
        if (cur_[1] == '/') {
          cur_ += 2;
          while (cur_ < end_ && *cur_ != '\n') ++cur_;
        } else if (cur_[1] == '*') {
          cur_ += 2;
          while (cur_ + 1 < end_ && !(cur_[0] == '*' && cur_[1] == '/')) {
            ++cur_;
          }
          if (cur_ + 1 < end_) cur_ += 2;
        } else {
          break;
        }
      } else if (c == '#') {
        ++cur_;
        while (cur_ < end_ && *cur_ != '\n') ++cur_;
      } else {
        break;
      }
    }
  }

  bool parseValue(Value* out, std::string* errs) {
    skipWhitespaceAndComments();
    if (cur_ >= end_) {
      if (errs) *errs = "Unexpected end of input";
      return false;
    }
    char c = *cur_;
    if (c == '{') return parseObject(out, errs);
    if (c == '[') return parseArray(out, errs);
    if (c == '"') {
      std::string s;
      if (!parseString(&s, errs)) return false;
      *out = Value(s);
      return true;
    }
    if (c == 't' && end_ - cur_ >= 4 && std::memcmp(cur_, "true", 4) == 0) {
      cur_ += 4;
      *out = Value(true);
      return true;
    }
    if (c == 'f' && end_ - cur_ >= 5 && std::memcmp(cur_, "false", 5) == 0) {
      cur_ += 5;
      *out = Value(false);
      return true;
    }
    if (c == 'n' && end_ - cur_ >= 4 && std::memcmp(cur_, "null", 4) == 0) {
      cur_ += 4;
      *out = Value(nullValue);
      return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parseNumber(out, errs);
    }
    if (errs) {
      *errs = std::string("Unexpected character '") + c + "' at offset " +
              std::to_string(cur_ - begin_);
    }
    return false;
  }

  bool parseObject(Value* out, std::string* errs) {
    ++cur_;  // skip '{'
    *out = Value(objectValue);
    skipWhitespaceAndComments();
    if (cur_ < end_ && *cur_ == '}') {
      ++cur_;
      return true;
    }
    while (cur_ < end_) {
      skipWhitespaceAndComments();
      if (cur_ >= end_ || *cur_ != '"') {
        if (errs) *errs = "Expected string key in object";
        return false;
      }
      std::string key;
      if (!parseString(&key, errs)) return false;
      skipWhitespaceAndComments();
      if (cur_ >= end_ || *cur_ != ':') {
        if (errs) *errs = "Expected ':' after object key";
        return false;
      }
      ++cur_;
      Value val;
      if (!parseValue(&val, errs)) return false;
      (*out)[key] = std::move(val);
      skipWhitespaceAndComments();
      if (cur_ < end_ && *cur_ == ',') {
        ++cur_;
        skipWhitespaceAndComments();
        if (cur_ < end_ && *cur_ == '}') {
          ++cur_;
          return true;
        }
        continue;
      }
      if (cur_ < end_ && *cur_ == '}') {
        ++cur_;
        return true;
      }
      if (errs) *errs = "Expected ',' or '}' in object";
      return false;
    }
    if (errs) *errs = "Unterminated object";
    return false;
  }

  bool parseArray(Value* out, std::string* errs) {
    ++cur_;  // skip '['
    *out = Value(arrayValue);
    skipWhitespaceAndComments();
    if (cur_ < end_ && *cur_ == ']') {
      ++cur_;
      return true;
    }
    while (cur_ < end_) {
      Value elem;
      if (!parseValue(&elem, errs)) return false;
      out->append(std::move(elem));
      skipWhitespaceAndComments();
      if (cur_ < end_ && *cur_ == ',') {
        ++cur_;
        skipWhitespaceAndComments();
        if (cur_ < end_ && *cur_ == ']') {
          ++cur_;
          return true;
        }
        continue;
      }
      if (cur_ < end_ && *cur_ == ']') {
        ++cur_;
        return true;
      }
      if (errs) *errs = "Expected ',' or ']' in array";
      return false;
    }
    if (errs) *errs = "Unterminated array";
    return false;
  }

  static void appendUtf8(std::string* out, uint32_t cp) {
    if (cp <= 0x7F) {
      out->push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
      out->push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
      out->push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out->push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  bool parseHex4(uint32_t* out) {
    if (end_ - cur_ < 4) return false;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      char c = cur_[i];
      v <<= 4;
      if (c >= '0' && c <= '9') {
        v |= static_cast<uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        v |= static_cast<uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        v |= static_cast<uint32_t>(c - 'A' + 10);
      } else {
        return false;
      }
    }
    cur_ += 4;
    *out = v;
    return true;
  }

  bool parseString(std::string* out, std::string* errs) {
    ++cur_;  // skip '"'
    out->clear();
    while (cur_ < end_) {
      char c = *cur_++;
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (cur_ >= end_) {
          if (errs) *errs = "Unterminated escape sequence in string";
          return false;
        }
        char esc = *cur_++;
        switch (esc) {
          case '"':
            out->push_back('"');
            break;
          case '\\':
            out->push_back('\\');
            break;
          case '/':
            out->push_back('/');
            break;
          case 'b':
            out->push_back('\b');
            break;
          case 'f':
            out->push_back('\f');
            break;
          case 'n':
            out->push_back('\n');
            break;
          case 'r':
            out->push_back('\r');
            break;
          case 't':
            out->push_back('\t');
            break;
          case 'u': {
            uint32_t cp = 0;
            if (!parseHex4(&cp)) {
              if (errs) *errs = "Invalid \\uXXXX escape in string";
              return false;
            }
            if (cp >= 0xD800 && cp <= 0xDBFF && end_ - cur_ >= 6 &&
                cur_[0] == '\\' && cur_[1] == 'u') {
              cur_ += 2;
              uint32_t low = 0;
              if (!parseHex4(&low)) {
                if (errs) *errs = "Invalid surrogate pair in string";
                return false;
              }
              if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
              }
            }
            appendUtf8(out, cp);
            break;
          }
          default:
            out->push_back(esc);
            break;
        }
      } else {
        out->push_back(c);
      }
    }
    if (errs) *errs = "Unterminated string literal";
    return false;
  }

  bool parseNumber(Value* out, std::string* errs) {
    const char* start = cur_;
    bool is_negative = false;
    bool is_float = false;
    if (*cur_ == '-') {
      is_negative = true;
      ++cur_;
    }
    if (cur_ >= end_ || !(*cur_ >= '0' && *cur_ <= '9')) {
      if (errs) *errs = "Invalid number";
      return false;
    }
    while (cur_ < end_ && (*cur_ >= '0' && *cur_ <= '9')) ++cur_;
    if (cur_ < end_ && *cur_ == '.') {
      is_float = true;
      ++cur_;
      if (cur_ >= end_ || !(*cur_ >= '0' && *cur_ <= '9')) {
        if (errs) *errs = "Invalid fractional part in number";
        return false;
      }
      while (cur_ < end_ && (*cur_ >= '0' && *cur_ <= '9')) ++cur_;
    }
    if (cur_ < end_ && (*cur_ == 'e' || *cur_ == 'E')) {
      is_float = true;
      ++cur_;
      if (cur_ < end_ && (*cur_ == '+' || *cur_ == '-')) ++cur_;
      if (cur_ >= end_ || !(*cur_ >= '0' && *cur_ <= '9')) {
        if (errs) *errs = "Invalid exponent in number";
        return false;
      }
      while (cur_ < end_ && (*cur_ >= '0' && *cur_ <= '9')) ++cur_;
    }
    std::string num_str(start, cur_);
    if (is_float) {
      char* endptr = nullptr;
      double d = std::strtod(num_str.c_str(), &endptr);
      *out = Value(d);
      return true;
    }
    if (is_negative) {
      char* endptr = nullptr;
      long long v = std::strtoll(num_str.c_str(), &endptr, 10);
      *out = Value(v);
      return true;
    } else {
      char* endptr = nullptr;
      unsigned long long u = std::strtoull(num_str.c_str(), &endptr, 10);
      if (u <= static_cast<unsigned long long>(
                   std::numeric_limits<LargestInt>::max())) {
        *out = Value(static_cast<long long>(u));
      } else {
        *out = Value(u);
      }
      return true;
    }
  }

  const char* cur_;
  const char* end_;
  const char* begin_;
};

class DefaultCharReader : public CharReader {
 public:
  bool parse(char const* beginDoc, char const* endDoc, Value* root,
             std::string* errs) override {
    JsonParser parser(beginDoc, endDoc);
    return parser.parse(root, errs);
  }
};

void escapeJsonString(const std::string& s, std::ostream& sout) {
  sout << '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        sout << "\\\"";
        break;
      case '\\':
        sout << "\\\\";
        break;
      case '\b':
        sout << "\\b";
        break;
      case '\f':
        sout << "\\f";
        break;
      case '\n':
        sout << "\\n";
        break;
      case '\r':
        sout << "\\r";
        break;
      case '\t':
        sout << "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          sout << buf;
        } else {
          sout << static_cast<char>(c);
        }
        break;
    }
  }
  sout << '"';
}

void writeJsonValue(const Value& val, std::ostream& sout,
                    const std::string& indent_step, int depth) {
  const bool pretty = !indent_step.empty();
  switch (val.type()) {
    case nullValue:
      sout << "null";
      break;
    case intValue:
      sout << val.asLargestInt();
      break;
    case uintValue:
      sout << val.asLargestUInt();
      break;
    case realValue: {
      double d = val.asDouble();
      if (std::isnan(d) || std::isinf(d)) {
        sout << "null";
      } else {
        std::ostringstream oss;
        oss << std::setprecision(16) << d;
        std::string s = oss.str();
        if (s.find('.') == std::string::npos &&
            s.find('e') == std::string::npos &&
            s.find('E') == std::string::npos) {
          s += ".0";
        }
        sout << s;
      }
      break;
    }
    case booleanValue:
      sout << (val.asBool() ? "true" : "false");
      break;
    case stringValue:
      escapeJsonString(val.asString(), sout);
      break;
    case arrayValue: {
      if (val.empty()) {
        sout << "[]";
        break;
      }
      sout << '[';
      if (pretty) sout << '\n';
      for (ArrayIndex i = 0; i < val.size(); ++i) {
        if (pretty) {
          for (int d = 0; d < depth + 1; ++d) sout << indent_step;
        }
        writeJsonValue(val[i], sout, indent_step, depth + 1);
        if (i + 1 < val.size()) sout << ',';
        if (pretty) sout << '\n';
      }
      if (pretty) {
        for (int d = 0; d < depth; ++d) sout << indent_step;
      }
      sout << ']';
      break;
    }
    case objectValue: {
      if (val.empty()) {
        sout << "{}";
        break;
      }
      sout << '{';
      if (pretty) sout << '\n';
      auto names = val.getMemberNames();
      for (size_t i = 0; i < names.size(); ++i) {
        if (pretty) {
          for (int d = 0; d < depth + 1; ++d) sout << indent_step;
        }
        escapeJsonString(names[i], sout);
        sout << (pretty ? " : " : ":");
        writeJsonValue(val[names[i]], sout, indent_step, depth + 1);
        if (i + 1 < names.size()) sout << ',';
        if (pretty) sout << '\n';
      }
      if (pretty) {
        for (int d = 0; d < depth; ++d) sout << indent_step;
      }
      sout << '}';
      break;
    }
  }
}

class DefaultStreamWriter : public StreamWriter {
 public:
  explicit DefaultStreamWriter(std::string indentation)
      : indentation_(std::move(indentation)) {}

  int write(Value const& root, std::ostream* sout) override {
    if (!sout) return -1;
    writeJsonValue(root, *sout, indentation_, 0);
    return 0;
  }

 private:
  std::string indentation_;
};

}  // namespace

// CharReaderBuilder
CharReaderBuilder::CharReaderBuilder() : settings_(objectValue) {
  setDefaults(&settings_);
}
CharReaderBuilder::~CharReaderBuilder() = default;

CharReader* CharReaderBuilder::newCharReader() const {
  return new DefaultCharReader();
}

bool CharReaderBuilder::validate(Json::Value* invalid) const {
  (void)invalid;
  return true;
}

Value& CharReaderBuilder::operator[](const std::string& key) {
  return settings_[key];
}

void CharReaderBuilder::setDefaults(Json::Value* settings) {
  if (!settings) return;
  (*settings)["collectComments"] = true;
  (*settings)["allowComments"] = true;
}

void CharReaderBuilder::strictMode(Json::Value* settings) {
  if (!settings) return;
  (*settings)["allowComments"] = false;
  (*settings)["strictRoot"] = true;
}

bool parseFromStream(CharReader::Factory const& fact, std::istream& sin,
                     Value* root, std::string* errs) {
  std::ostringstream sstr;
  sstr << sin.rdbuf();
  std::string doc = sstr.str();
  std::unique_ptr<CharReader> const reader(fact.newCharReader());
  return reader->parse(doc.data(), doc.data() + doc.size(), root, errs);
}

std::istream& operator>>(std::istream& sin, Value& root) {
  CharReaderBuilder b;
  std::string errs;
  if (!parseFromStream(b, sin, &root, &errs)) {
    sin.setstate(std::ios::failbit);
  }
  return sin;
}

// Legacy Reader
bool Reader::parse(const std::string& document, Value& root,
                   bool collectComments) {
  (void)collectComments;
  return parse(document.data(), document.data() + document.size(), root,
               collectComments);
}

bool Reader::parse(const char* beginDoc, const char* endDoc, Value& root,
                   bool collectComments) {
  (void)collectComments;
  errors_.clear();
  JsonParser parser(beginDoc, endDoc);
  return parser.parse(&root, &errors_);
}

bool Reader::parse(std::istream& is, Value& root, bool collectComments) {
  std::ostringstream sstr;
  sstr << is.rdbuf();
  return parse(sstr.str(), root, collectComments);
}

// StreamWriter & Builders
StreamWriterBuilder::StreamWriterBuilder() : settings_(objectValue) {
  setDefaults(&settings_);
}
StreamWriterBuilder::~StreamWriterBuilder() = default;

StreamWriter* StreamWriterBuilder::newStreamWriter() const {
  std::string indent = "   ";
  if (settings_.isMember("indentation")) {
    indent = settings_["indentation"].asString();
  }
  return new DefaultStreamWriter(indent);
}

bool StreamWriterBuilder::validate(Json::Value* invalid) const {
  (void)invalid;
  return true;
}

Value& StreamWriterBuilder::operator[](const std::string& key) {
  return settings_[key];
}

void StreamWriterBuilder::setDefaults(Json::Value* settings) {
  if (!settings) return;
  (*settings)["indentation"] = "   ";
}

std::string writeString(StreamWriter::Factory const& builder,
                        Value const& root) {
  std::ostringstream sout;
  std::unique_ptr<StreamWriter> const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout.str();
}

std::string FastWriter::write(const Value& root) {
  std::ostringstream sout;
  writeJsonValue(root, sout, "", 0);
  if (!omitEndingLineFeed_) sout << '\n';
  return sout.str();
}

std::string StyledWriter::write(const Value& root) {
  std::ostringstream sout;
  writeJsonValue(root, sout, "   ", 0);
  sout << '\n';
  return sout.str();
}

std::string valueToString(Int value) { return std::to_string(value); }
std::string valueToString(UInt value) { return std::to_string(value); }
std::string valueToString(LargestInt value) { return std::to_string(value); }
std::string valueToString(LargestUInt value) { return std::to_string(value); }
std::string valueToString(double value) {
  std::ostringstream oss;
  oss << std::setprecision(16) << value;
  return oss.str();
}
std::string valueToString(bool value) { return value ? "true" : "false"; }
std::string valueToQuotedString(const char* value) {
  std::ostringstream oss;
  escapeJsonString(value ? value : "", oss);
  return oss.str();
}

std::ostream& operator<<(std::ostream& sout, Value const& root) {
  StreamWriterBuilder builder;
  std::unique_ptr<StreamWriter> const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout;
}

}  // namespace Json
