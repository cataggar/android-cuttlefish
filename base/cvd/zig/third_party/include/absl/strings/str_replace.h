#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace absl {

template <typename StrToStrMapping>
int StrReplaceAll(const StrToStrMapping& replacements, std::string* target) {
  int substitutions = 0;
  std::string result;
  result.reserve(target->size());
  size_t pos = 0;
  while (pos < target->size()) {
    size_t best_pos = std::string::npos;
    std::string_view best_old;
    std::string_view best_new;
    for (const auto& rep : replacements) {
      std::string_view old_s(rep.first);
      std::string_view new_s(rep.second);
      if (old_s.empty()) continue;
      size_t found = target->find(old_s, pos);
      if (found != std::string::npos &&
          (best_pos == std::string::npos || found < best_pos)) {
        best_pos = found;
        best_old = old_s;
        best_new = new_s;
      }
    }
    if (best_pos == std::string::npos) {
      result.append(*target, pos, std::string::npos);
      break;
    }
    result.append(*target, pos, best_pos - pos);
    result.append(best_new);
    pos = best_pos + best_old.size();
    ++substitutions;
  }
  *target = std::move(result);
  return substitutions;
}

inline int StrReplaceAll(
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        replacements,
    std::string* target) {
  return StrReplaceAll<
      std::initializer_list<std::pair<std::string_view, std::string_view>>>(
      replacements, target);
}

template <typename StrToStrMapping>
std::string StrReplaceAll(std::string_view s,
                          const StrToStrMapping& replacements) {
  std::string result(s);
  StrReplaceAll(replacements, &result);
  return result;
}

inline std::string StrReplaceAll(
    std::string_view s,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        replacements) {
  std::string result(s);
  StrReplaceAll(replacements, &result);
  return result;
}

}  // namespace absl
