#pragma once

#include <cstdint>
#include <cstdio>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "absl/strings/string_view.h"

namespace absl {

enum PadSpec : uint8_t {
  kNoPad = 1,
  kZeroPad2,
  kZeroPad3,
  kZeroPad4,
  kZeroPad5,
  kZeroPad6,
  kZeroPad7,
  kZeroPad8,
  kZeroPad9,
  kZeroPad10,
  kZeroPad11,
  kZeroPad12,
  kZeroPad13,
  kZeroPad14,
  kZeroPad15,
  kZeroPad16,
  kSpacePad2 = kZeroPad2 + 64,
  kSpacePad3,
  kSpacePad4,
  kSpacePad5,
  kSpacePad6,
  kSpacePad7,
  kSpacePad8,
  kSpacePad9,
  kSpacePad10,
  kSpacePad11,
  kSpacePad12,
  kSpacePad13,
  kSpacePad14,
  kSpacePad15,
  kSpacePad16,
};

struct Hex {
  uint64_t value;
  uint8_t width;
  char fill;

  template <typename Int>
  explicit Hex(Int v, PadSpec spec = kNoPad) : value(0), width(0), fill('0') {
    if constexpr (sizeof(Int) == 1) {
      value = static_cast<uint8_t>(v);
    } else if constexpr (sizeof(Int) == 2) {
      value = static_cast<uint16_t>(v);
    } else if constexpr (sizeof(Int) == 4) {
      value = static_cast<uint32_t>(v);
    } else {
      value = static_cast<uint64_t>(v);
    }
    if (spec >= kZeroPad2 && spec <= kZeroPad16) {
      width = static_cast<uint8_t>(spec - kZeroPad2 + 2);
      fill = '0';
    } else if (spec >= kSpacePad2 && spec <= kSpacePad16) {
      width = static_cast<uint8_t>(spec - kSpacePad2 + 2);
      fill = ' ';
    }
  }
};

struct Dec {
  int64_t value;
  uint8_t width;
  char fill;
  bool neg;

  template <typename Int>
  explicit Dec(Int v, PadSpec spec = kNoPad)
      : value(static_cast<int64_t>(v)),
        width(0),
        fill('0'),
        neg(v < 0) {
    if (spec >= kZeroPad2 && spec <= kZeroPad16) {
      width = static_cast<uint8_t>(spec - kZeroPad2 + 2);
      fill = '0';
    } else if (spec >= kSpacePad2 && spec <= kSpacePad16) {
      width = static_cast<uint8_t>(spec - kSpacePad2 + 2);
      fill = ' ';
    }
  }
};

namespace strings_internal {

struct StringifySink {
  std::string buffer;
  void Append(std::string_view sv) { buffer.append(sv.data(), sv.size()); }
  void Append(size_t count, char c) { buffer.append(count, c); }
};

template <typename T>
concept HasAbslStringify = requires(StringifySink& s, const T& v) {
  AbslStringify(s, v);
};

}  // namespace strings_internal

class AlphaNum {
 public:
  AlphaNum(int x) : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(unsigned int x) : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(long x) : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(unsigned long x) : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(long long x) : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(unsigned long long x)
      : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(short x) : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(unsigned short x) : storage_(std::to_string(x)), piece_(storage_) {}
  AlphaNum(signed char x)
      : storage_(std::to_string(static_cast<int>(x))), piece_(storage_) {}
  AlphaNum(unsigned char x)
      : storage_(std::to_string(static_cast<unsigned int>(x))),
        piece_(storage_) {}
  AlphaNum(bool b) : piece_(b ? "1" : "0") {}
  AlphaNum(char c) : storage_(1, c), piece_(storage_) {}
  AlphaNum(float f) {
    std::ostringstream oss;
    oss << f;
    storage_ = oss.str();
    piece_ = storage_;
  }
  AlphaNum(double d) {
    std::ostringstream oss;
    oss << d;
    storage_ = oss.str();
    piece_ = storage_;
  }
  AlphaNum(const char* c_str)
      : piece_(c_str ? std::string_view(c_str) : std::string_view()) {}
  AlphaNum(std::string_view pc) : piece_(pc) {}
  AlphaNum(const std::string& str) : piece_(str) {}
  AlphaNum(std::string&& str) : storage_(std::move(str)), piece_(storage_) {}

  template <typename T,
            std::enable_if_t<std::is_enum_v<T> &&
                                 !strings_internal::HasAbslStringify<T>,
                             int> = 0>
  AlphaNum(T e)
      : storage_(std::to_string(static_cast<std::underlying_type_t<T>>(e))),
        piece_(storage_) {}

  template <typename T,
            std::enable_if_t<strings_internal::HasAbslStringify<T>, int> = 0>
  AlphaNum(const T& v) {
    strings_internal::StringifySink sink;
    AbslStringify(sink, v);
    storage_ = std::move(sink.buffer);
    piece_ = storage_;
  }

  AlphaNum(Hex h) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llx",
                  static_cast<unsigned long long>(h.value));
    std::string s(buf);
    if (s.size() < h.width) {
      s = std::string(h.width - s.size(), h.fill) + s;
    }
    storage_ = std::move(s);
    piece_ = storage_;
  }

  AlphaNum(Dec d) {
    uint64_t u = d.neg ? static_cast<uint64_t>(-d.value)
                       : static_cast<uint64_t>(d.value);
    std::string s = std::to_string(u);
    size_t target = d.width > (d.neg ? 1 : 0) ? d.width - (d.neg ? 1 : 0) : 0;
    if (s.size() < target) {
      if (d.fill == '0') {
        s = (d.neg ? "-" : "") + std::string(target - s.size(), '0') + s;
      } else {
        s = std::string(target - s.size(), d.fill) + (d.neg ? "-" : "") + s;
      }
    } else if (d.neg) {
      s = "-" + s;
    }
    storage_ = std::move(s);
    piece_ = storage_;
  }

  std::string_view Piece() const { return piece_; }
  const char* data() const { return piece_.data(); }
  size_t size() const { return piece_.size(); }

 private:
  std::string storage_;
  std::string_view piece_;
};

inline std::string StrCat() { return std::string(); }

inline std::string StrCat(const AlphaNum& a) {
  return std::string(a.Piece());
}

inline std::string StrCat(const AlphaNum& a, const AlphaNum& b) {
  std::string out;
  out.reserve(a.size() + b.size());
  out.append(a.Piece());
  out.append(b.Piece());
  return out;
}

inline std::string StrCat(const AlphaNum& a, const AlphaNum& b,
                          const AlphaNum& c) {
  std::string out;
  out.reserve(a.size() + b.size() + c.size());
  out.append(a.Piece());
  out.append(b.Piece());
  out.append(c.Piece());
  return out;
}

inline std::string StrCat(const AlphaNum& a, const AlphaNum& b,
                          const AlphaNum& c, const AlphaNum& d) {
  std::string out;
  out.reserve(a.size() + b.size() + c.size() + d.size());
  out.append(a.Piece());
  out.append(b.Piece());
  out.append(c.Piece());
  out.append(d.Piece());
  return out;
}

template <typename... AV>
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
                   const AlphaNum& d, const AlphaNum& e, const AV&... args) {
  std::string out;
  auto append_one = [&out](const AlphaNum& x) { out.append(x.Piece()); };
  append_one(a);
  append_one(b);
  append_one(c);
  append_one(d);
  append_one(e);
  (append_one(AlphaNum(args)), ...);
  return out;
}

inline void StrAppend(std::string*) {}

template <typename... AV>
void StrAppend(std::string* dest, const AlphaNum& a, const AV&... args) {
  dest->append(a.Piece());
  (dest->append(AlphaNum(args).Piece()), ...);
}

}  // namespace absl
