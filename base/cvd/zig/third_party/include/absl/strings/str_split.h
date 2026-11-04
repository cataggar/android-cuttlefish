#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <deque>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"

namespace absl {

class ByChar {
 public:
  explicit constexpr ByChar(char c) : c_(c) {}
  std::string_view Find(std::string_view text, size_t pos) const {
    size_t found = text.find(c_, pos);
    if (found == std::string_view::npos) {
      return std::string_view(text.data() + text.size(), 0);
    }
    return text.substr(found, 1);
  }

 private:
  char c_;
};

class ByString {
 public:
  explicit ByString(std::string_view sp) : delimiter_(sp) {}
  std::string_view Find(std::string_view text, size_t pos) const {
    if (delimiter_.empty()) {
      if (pos + 1 < text.size()) {
        return std::string_view(text.data() + pos + 1, 0);
      }
      return std::string_view(text.data() + text.size(), 0);
    }
    size_t found = text.find(delimiter_, pos);
    if (found == std::string_view::npos) {
      return std::string_view(text.data() + text.size(), 0);
    }
    return text.substr(found, delimiter_.size());
  }

 private:
  std::string delimiter_;
};

class ByAnyChar {
 public:
  explicit ByAnyChar(std::string_view sp) : delimiters_(sp) {}
  std::string_view Find(std::string_view text, size_t pos) const {
    size_t found = text.find_first_of(delimiters_, pos);
    if (found == std::string_view::npos) {
      return std::string_view(text.data() + text.size(), 0);
    }
    return text.substr(found, 1);
  }

 private:
  std::string delimiters_;
};

class ByLength {
 public:
  explicit ByLength(ptrdiff_t length) : length_(length) {}
  std::string_view Find(std::string_view text, size_t pos) const {
    size_t next = pos + static_cast<size_t>(length_);
    if (next < text.size()) {
      return std::string_view(text.data() + next, 0);
    }
    return std::string_view(text.data() + text.size(), 0);
  }

 private:
  ptrdiff_t length_;
};

template <typename Delimiter>
class MaxSplitsImpl {
 public:
  MaxSplitsImpl(Delimiter d, int limit)
      : delimiter_(std::move(d)), limit_(limit) {}
  std::string_view Find(std::string_view text, size_t pos, int* count) const {
    if (*count >= limit_) {
      return std::string_view(text.data() + text.size(), 0);
    }
    ++(*count);
    return delimiter_.Find(text, pos);
  }

 private:
  Delimiter delimiter_;
  int limit_;
};

struct AllowEmpty {
  bool operator()(std::string_view) const { return true; }
};

struct SkipEmpty {
  bool operator()(std::string_view sp) const { return !sp.empty(); }
};

struct SkipWhitespace {
  bool operator()(std::string_view sp) const {
    for (unsigned char c : sp) {
      if (!std::isspace(c)) return true;
    }
    return false;
  }
};

namespace strings_internal {

inline auto MakeDelimiter(char c) { return ByChar(c); }
inline auto MakeDelimiter(const char* s) { return ByString(s ? s : ""); }
inline auto MakeDelimiter(std::string_view s) { return ByString(s); }
inline auto MakeDelimiter(const std::string& s) { return ByString(s); }
template <typename D>
D MakeDelimiter(D d) {
  return d;
}

class SplitResult {
 public:
  using value_type = std::string_view;
  using const_iterator = std::vector<std::string_view>::const_iterator;
  using iterator = const_iterator;

  template <typename Delim, typename Pred>
  SplitResult(std::string_view text, bool own_storage, Delim delim, Pred pred) {
    if (own_storage) {
      owned_ = std::string(text);
      text = owned_;
    }
    DoSplit(text, delim, pred);
  }

  SplitResult(const SplitResult& other) : owned_(other.owned_) {
    if (!owned_.empty()) {
      const char* old_base = other.owned_.data();
      const char* new_base = owned_.data();
      parts_.reserve(other.parts_.size());
      for (auto sv : other.parts_) {
        parts_.emplace_back(new_base + (sv.data() - old_base), sv.size());
      }
    } else {
      parts_ = other.parts_;
    }
  }

  SplitResult(SplitResult&&) = default;

  const_iterator begin() const { return parts_.begin(); }
  const_iterator end() const { return parts_.end(); }
  bool empty() const { return parts_.empty(); }
  size_t size() const { return parts_.size(); }

  template <typename Container,
            std::enable_if_t<
                !std::is_same_v<std::remove_cvref_t<Container>, SplitResult> &&
                    !std::is_same_v<std::remove_cvref_t<Container>, std::string> &&
                    !std::is_same_v<std::remove_cvref_t<Container>,
                                    std::string_view> &&
                    requires { typename Container::value_type; },
                int> = 0>
  operator Container() const {
    using ValueType = typename Container::value_type;
    Container c;
    for (auto sv : parts_) {
      c.insert(c.end(), ValueType(sv.data(), sv.size()));
    }
    return c;
  }

  template <typename First, typename Second>
  operator std::pair<First, Second>() const {
    First f = parts_.size() > 0 ? First(parts_[0]) : First();
    Second s = parts_.size() > 1 ? Second(parts_[1]) : Second();
    return {std::move(f), std::move(s)};
  }

 private:
  template <typename Delim, typename Pred>
  void DoSplit(std::string_view text, Delim delim, Pred pred) {
    size_t pos = 0;
    int split_count = 0;
    while (true) {
      std::string_view sep;
      if constexpr (requires { delim.Find(text, pos, &split_count); }) {
        sep = delim.Find(text, pos, &split_count);
      } else {
        sep = delim.Find(text, pos);
      }
      size_t sep_pos = static_cast<size_t>(sep.data() - text.data());
      std::string_view piece = text.substr(pos, sep_pos - pos);
      if (pred(piece)) {
        parts_.push_back(piece);
      }
      if (sep_pos == text.size() && sep.empty()) {
        break;
      }
      pos = sep_pos + sep.size();
    }
  }

  std::string owned_;
  std::vector<std::string_view> parts_;
};

}  // namespace strings_internal

template <typename Delimiter>
auto MaxSplits(Delimiter delimiter, int limit) {
  auto d = strings_internal::MakeDelimiter(delimiter);
  return MaxSplitsImpl<decltype(d)>(std::move(d), limit);
}

template <typename Delimiter>
strings_internal::SplitResult StrSplit(std::string_view text, Delimiter d) {
  return strings_internal::SplitResult(
      text, true, strings_internal::MakeDelimiter(d), AllowEmpty());
}

template <typename Delimiter, typename Predicate>
strings_internal::SplitResult StrSplit(std::string_view text, Delimiter d,
                                       Predicate p) {
  return strings_internal::SplitResult(
      text, true, strings_internal::MakeDelimiter(d), p);
}

}  // namespace absl
