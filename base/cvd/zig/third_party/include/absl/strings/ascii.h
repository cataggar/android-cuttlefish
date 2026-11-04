#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace absl {

inline bool ascii_isalpha(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
inline bool ascii_isdigit(unsigned char c) { return c >= '0' && c <= '9'; }
inline bool ascii_isalnum(unsigned char c) {
  return ascii_isalpha(c) || ascii_isdigit(c);
}
inline bool ascii_isspace(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}
inline bool ascii_ispunct(unsigned char c) {
  return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) ||
         (c >= 123 && c <= 126);
}
inline bool ascii_isblank(unsigned char c) { return c == ' ' || c == '\t'; }
inline bool ascii_iscntrl(unsigned char c) { return c < 32 || c == 127; }
inline bool ascii_isxdigit(unsigned char c) {
  return ascii_isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
inline bool ascii_isprint(unsigned char c) { return c >= 32 && c <= 126; }
inline bool ascii_isgraph(unsigned char c) { return c >= 33 && c <= 126; }
inline bool ascii_isupper(unsigned char c) { return c >= 'A' && c <= 'Z'; }
inline bool ascii_islower(unsigned char c) { return c >= 'a' && c <= 'z'; }
inline bool ascii_isascii(unsigned char c) { return c <= 127; }

inline char ascii_tolower(unsigned char c) {
  return ascii_isupper(c) ? static_cast<char>(c + ('a' - 'A'))
                          : static_cast<char>(c);
}
inline char ascii_toupper(unsigned char c) {
  return ascii_islower(c) ? static_cast<char>(c - ('a' - 'A'))
                          : static_cast<char>(c);
}

inline void AsciiStrToLower(std::string* s) {
  for (char& ch : *s) {
    ch = ascii_tolower(static_cast<unsigned char>(ch));
  }
}

inline std::string AsciiStrToLower(std::string_view s) {
  std::string result(s);
  AsciiStrToLower(&result);
  return result;
}

inline void AsciiStrToUpper(std::string* s) {
  for (char& ch : *s) {
    ch = ascii_toupper(static_cast<unsigned char>(ch));
  }
}

inline std::string AsciiStrToUpper(std::string_view s) {
  std::string result(s);
  AsciiStrToUpper(&result);
  return result;
}

inline std::string_view StripLeadingAsciiWhitespace(std::string_view str) {
  size_t i = 0;
  while (i < str.size() && ascii_isspace(static_cast<unsigned char>(str[i]))) {
    ++i;
  }
  return str.substr(i);
}

inline void StripLeadingAsciiWhitespace(std::string* str) {
  auto sv = StripLeadingAsciiWhitespace(*str);
  str->erase(0, str->size() - sv.size());
}

inline std::string_view StripTrailingAsciiWhitespace(std::string_view str) {
  size_t len = str.size();
  while (len > 0 && ascii_isspace(static_cast<unsigned char>(str[len - 1]))) {
    --len;
  }
  return str.substr(0, len);
}

inline void StripTrailingAsciiWhitespace(std::string* str) {
  auto sv = StripTrailingAsciiWhitespace(*str);
  str->resize(sv.size());
}

inline std::string_view StripAsciiWhitespace(std::string_view str) {
  return StripTrailingAsciiWhitespace(StripLeadingAsciiWhitespace(str));
}

inline void StripAsciiWhitespace(std::string* str) {
  StripTrailingAsciiWhitespace(str);
  StripLeadingAsciiWhitespace(str);
}

inline void RemoveExtraAsciiWhitespace(std::string* str) {
  std::string out;
  out.reserve(str->size());
  bool in_space = true;
  for (unsigned char c : *str) {
    if (ascii_isspace(c)) {
      if (!in_space) {
        out.push_back(' ');
        in_space = true;
      }
    } else {
      out.push_back(static_cast<char>(c));
      in_space = false;
    }
  }
  if (!out.empty() && out.back() == ' ') out.pop_back();
  *str = std::move(out);
}

}  // namespace absl
