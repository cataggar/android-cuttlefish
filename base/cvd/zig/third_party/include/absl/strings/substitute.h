#pragma once

#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"

namespace absl {

inline std::string Substitute(
    std::string_view format, const AlphaNum& a0 = "", const AlphaNum& a1 = "",
    const AlphaNum& a2 = "", const AlphaNum& a3 = "", const AlphaNum& a4 = "",
    const AlphaNum& a5 = "", const AlphaNum& a6 = "", const AlphaNum& a7 = "",
    const AlphaNum& a8 = "", const AlphaNum& a9 = "") {
  const std::string_view args[10] = {
      a0.Piece(), a1.Piece(), a2.Piece(), a3.Piece(), a4.Piece(),
      a5.Piece(), a6.Piece(), a7.Piece(), a8.Piece(), a9.Piece()};
  std::string out;
  out.reserve(format.size() + 32);
  for (size_t i = 0; i < format.size(); ++i) {
    if (format[i] == '$' && i + 1 < format.size()) {
      char next = format[i + 1];
      if (next == '$') {
        out.push_back('$');
        ++i;
      } else if (next >= '0' && next <= '9') {
        out.append(args[next - '0']);
        ++i;
      } else {
        out.push_back(format[i]);
      }
    } else {
      out.push_back(format[i]);
    }
  }
  return out;
}

}  // namespace absl
