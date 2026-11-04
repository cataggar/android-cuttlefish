#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>

namespace absl {

constexpr bool StrContains(std::string_view haystack,
                           std::string_view needle) noexcept {
  return haystack.find(needle) != std::string_view::npos;
}

constexpr bool StrContains(std::string_view haystack, char needle) noexcept {
  return haystack.find(needle) != std::string_view::npos;
}

constexpr bool StartsWith(std::string_view text,
                          std::string_view prefix) noexcept {
  return text.size() >= prefix.size() &&
         text.substr(0, prefix.size()) == prefix;
}

constexpr bool EndsWith(std::string_view text,
                        std::string_view suffix) noexcept {
  return text.size() >= suffix.size() &&
         text.substr(text.size() - suffix.size()) == suffix;
}

inline bool EqualsIgnoreCase(std::string_view piece1,
                             std::string_view piece2) noexcept {
  if (piece1.size() != piece2.size()) return false;
  for (size_t i = 0; i < piece1.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(piece1[i])) !=
        std::tolower(static_cast<unsigned char>(piece2[i]))) {
      return false;
    }
  }
  return true;
}

inline bool StartsWithIgnoreCase(std::string_view text,
                                 std::string_view prefix) noexcept {
  return text.size() >= prefix.size() &&
         EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

inline bool EndsWithIgnoreCase(std::string_view text,
                               std::string_view suffix) noexcept {
  return text.size() >= suffix.size() &&
         EqualsIgnoreCase(text.substr(text.size() - suffix.size()), suffix);
}

inline bool StrContainsIgnoreCase(std::string_view haystack,
                                  std::string_view needle) noexcept {
  if (needle.empty()) return true;
  if (haystack.size() < needle.size()) return false;
  for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
    if (EqualsIgnoreCase(haystack.substr(i, needle.size()), needle)) {
      return true;
    }
  }
  return false;
}

inline bool StrContainsIgnoreCase(std::string_view haystack,
                                  char needle) noexcept {
  char lower_needle = static_cast<char>(
      std::tolower(static_cast<unsigned char>(needle)));
  for (unsigned char c : haystack) {
    if (static_cast<char>(std::tolower(c)) == lower_needle) return true;
  }
  return false;
}

}  // namespace absl
