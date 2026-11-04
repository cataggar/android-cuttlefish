#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace absl {

inline std::string CEscape(std::string_view src) {
  std::string dest;
  dest.reserve(src.size() * 2);
  for (unsigned char c : src) {
    switch (c) {
      case '\n': dest += "\\n"; break;
      case '\r': dest += "\\r"; break;
      case '\t': dest += "\\t"; break;
      case '\"': dest += "\\\""; break;
      case '\'': dest += "\\'"; break;
      case '\\': dest += "\\\\"; break;
      default:
        if (c >= 32 && c < 127) {
          dest.push_back(static_cast<char>(c));
        } else {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\%03o", c);
          dest += buf;
        }
        break;
    }
  }
  return dest;
}

inline std::string CHexEscape(std::string_view src) {
  return CEscape(src);
}

inline std::string Utf8SafeCEscape(std::string_view src) {
  return CEscape(src);
}

inline bool CUnescape(std::string_view source, std::string* dest,
                      std::string* error = nullptr) {
  if (error) error->clear();
  if (!dest) return false;
  dest->clear();
  dest->reserve(source.size());
  for (size_t i = 0; i < source.size(); ++i) {
    if (source[i] == '\\' && i + 1 < source.size()) {
      ++i;
      switch (source[i]) {
        case 'n': dest->push_back('\n'); break;
        case 'r': dest->push_back('\r'); break;
        case 't': dest->push_back('\t'); break;
        case '\\': dest->push_back('\\'); break;
        case '"': dest->push_back('"'); break;
        case '\'': dest->push_back('\''); break;
        default: dest->push_back(source[i]); break;
      }
    } else {
      dest->push_back(source[i]);
    }
  }
  return true;
}

inline void Base64Escape(std::string_view src, std::string* dest) {
  static constexpr char kTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  dest->clear();
  dest->reserve(((src.size() + 2) / 3) * 4);
  size_t i = 0;
  while (i + 3 <= src.size()) {
    uint32_t n = (static_cast<uint32_t>(static_cast<uint8_t>(src[i])) << 16) |
                 (static_cast<uint32_t>(static_cast<uint8_t>(src[i + 1])) << 8) |
                 static_cast<uint32_t>(static_cast<uint8_t>(src[i + 2]));
    dest->push_back(kTable[(n >> 18) & 0x3F]);
    dest->push_back(kTable[(n >> 12) & 0x3F]);
    dest->push_back(kTable[(n >> 6) & 0x3F]);
    dest->push_back(kTable[n & 0x3F]);
    i += 3;
  }
  if (i < src.size()) {
    uint32_t n = static_cast<uint32_t>(static_cast<uint8_t>(src[i])) << 16;
    if (i + 1 < src.size()) {
      n |= static_cast<uint32_t>(static_cast<uint8_t>(src[i + 1])) << 8;
    }
    dest->push_back(kTable[(n >> 18) & 0x3F]);
    dest->push_back(kTable[(n >> 12) & 0x3F]);
    dest->push_back((i + 1 < src.size()) ? kTable[(n >> 6) & 0x3F] : '=');
    dest->push_back('=');
  }
}

inline std::string Base64Escape(std::string_view src) {
  std::string dest;
  Base64Escape(src, &dest);
  return dest;
}

inline bool Base64Unescape(std::string_view src, std::string* dest) {
  if (!dest) return false;
  dest->clear();
  auto decode_char = [](unsigned char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
  };
  uint32_t val = 0;
  int bits = -8;
  for (unsigned char c : src) {
    if (c == '=' || c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
    int d = decode_char(c);
    if (d < 0) return false;
    val = (val << 6) | static_cast<uint32_t>(d);
    bits += 6;
    if (bits >= 0) {
      dest->push_back(static_cast<char>((val >> bits) & 0xFF));
      bits -= 8;
    }
  }
  return true;
}

inline std::string BytesToHexString(std::string_view from) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(from.size() * 2);
  for (unsigned char c : from) {
    out.push_back(kHex[c >> 4]);
    out.push_back(kHex[c & 0xF]);
  }
  return out;
}

inline std::string HexStringToBytes(std::string_view from) {
  std::string out;
  out.reserve(from.size() / 2);
  auto hex_val = [](unsigned char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  };
  for (size_t i = 0; i + 1 < from.size(); i += 2) {
    int hi = hex_val(static_cast<unsigned char>(from[i]));
    int lo = hex_val(static_cast<unsigned char>(from[i + 1]));
    out.push_back(static_cast<char>((hi << 4) | lo));
  }
  return out;
}

}  // namespace absl
