#pragma once

#include <string_view>

#include "absl/strings/match.h"

namespace absl {

inline bool ConsumePrefix(std::string_view* str, std::string_view expected) {
  if (!StartsWith(*str, expected)) return false;
  str->remove_prefix(expected.size());
  return true;
}

inline bool ConsumeSuffix(std::string_view* str, std::string_view expected) {
  if (!EndsWith(*str, expected)) return false;
  str->remove_suffix(expected.size());
  return true;
}

inline std::string_view StripPrefix(std::string_view str,
                                    std::string_view prefix) {
  ConsumePrefix(&str, prefix);
  return str;
}

inline std::string_view StripSuffix(std::string_view str,
                                    std::string_view suffix) {
  ConsumeSuffix(&str, suffix);
  return str;
}

}  // namespace absl
