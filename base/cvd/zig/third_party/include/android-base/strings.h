#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace android {
namespace base {

inline std::vector<std::string> Split(const std::string& s,
                                      const std::string& delimiters) {
  std::vector<std::string> result;
  size_t base = 0;
  size_t found;
  while (true) {
    found = s.find_first_of(delimiters, base);
    result.push_back(s.substr(base, found - base));
    if (found == s.npos) break;
    base = found + 1;
  }
  return result;
}

inline std::vector<std::string> Tokenize(const std::string& s,
                                         const std::string& delimiters) {
  std::vector<std::string> result;
  size_t end = 0;
  while (true) {
    size_t base = s.find_first_not_of(delimiters, end);
    if (base == s.npos) break;
    end = s.find_first_of(delimiters, base);
    result.push_back(s.substr(base, end - base));
  }
  return result;
}

template <typename T>
std::string Trim(T&& s) {
  std::string_view sv(s);
  size_t start = 0;
  while (start < sv.size() &&
         std::isspace(static_cast<unsigned char>(sv[start]))) {
    ++start;
  }
  size_t end = sv.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(sv[end - 1]))) {
    --end;
  }
  return std::string(sv.substr(start, end - start));
}

template <typename ContainerT, typename SeparatorT>
std::string Join(const ContainerT& things, SeparatorT separator) {
  if (things.empty()) return "";
  std::ostringstream result;
  auto it = things.begin();
  result << *it++;
  for (; it != things.end(); ++it) {
    result << separator << *it;
  }
  return result.str();
}

inline bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

inline bool StartsWith(std::string_view s, char prefix) {
  return !s.empty() && s.front() == prefix;
}

inline bool EndsWith(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() &&
         s.substr(s.size() - suffix.size()) == suffix;
}

inline bool EndsWith(std::string_view s, char suffix) {
  return !s.empty() && s.back() == suffix;
}

inline bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
        std::tolower(static_cast<unsigned char>(rhs[i]))) {
      return false;
    }
  }
  return true;
}

inline bool StartsWithIgnoreCase(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() &&
         EqualsIgnoreCase(s.substr(0, prefix.size()), prefix);
}

inline bool EndsWithIgnoreCase(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() &&
         EqualsIgnoreCase(s.substr(s.size() - suffix.size()), suffix);
}

inline std::string StringReplace(std::string_view s, std::string_view from,
                                 std::string_view to, bool all) {
  if (from.empty()) return std::string(s);
  std::string out;
  size_t start_pos = 0;
  do {
    size_t pos = s.find(from, start_pos);
    if (pos == std::string_view::npos) break;
    out.append(s.data() + start_pos, pos - start_pos);
    out.append(to.data(), to.size());
    start_pos = pos + from.size();
  } while (all);
  out.append(s.data() + start_pos, s.size() - start_pos);
  return out;
}

inline bool ConsumePrefix(std::string_view* s, std::string_view prefix) {
  if (!StartsWith(*s, prefix)) return false;
  s->remove_prefix(prefix.size());
  return true;
}

inline bool ConsumeSuffix(std::string_view* s, std::string_view suffix) {
  if (!EndsWith(*s, suffix)) return false;
  s->remove_suffix(suffix.size());
  return true;
}

}  // namespace base
}  // namespace android
