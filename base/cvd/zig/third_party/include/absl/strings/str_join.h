#pragma once

#include <initializer_list>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "absl/strings/str_cat.h"

namespace absl {

struct AlphaNumFormatterImpl {
  template <typename T>
  void operator()(std::string* out, const T& t) const {
    StrAppend(out, AlphaNum(t));
  }
};

inline AlphaNumFormatterImpl AlphaNumFormatter() {
  return AlphaNumFormatterImpl{};
}

struct StreamFormatterImpl {
  template <typename T>
  void operator()(std::string* out, const T& t) const {
    std::ostringstream oss;
    oss << t;
    out->append(oss.str());
  }
};

inline StreamFormatterImpl StreamFormatter() { return StreamFormatterImpl{}; }

template <typename F1, typename F2>
class PairFormatterImpl {
 public:
  PairFormatterImpl(F1 f1, std::string_view sep, F2 f2)
      : f1_(std::move(f1)), sep_(sep), f2_(std::move(f2)) {}

  template <typename Pair>
  void operator()(std::string* out, const Pair& p) const {
    f1_(out, p.first);
    out->append(sep_);
    f2_(out, p.second);
  }

 private:
  F1 f1_;
  std::string sep_;
  F2 f2_;
};

inline PairFormatterImpl<AlphaNumFormatterImpl, AlphaNumFormatterImpl>
PairFormatter(std::string_view sep) {
  return {AlphaNumFormatter(), sep, AlphaNumFormatter()};
}

template <typename F1, typename F2>
PairFormatterImpl<F1, F2> PairFormatter(F1 f1, std::string_view sep, F2 f2) {
  return {std::move(f1), sep, std::move(f2)};
}

template <typename Iterator, typename Formatter>
std::string StrJoin(Iterator start, Iterator end, std::string_view sep,
                    Formatter&& fmt) {
  std::string result;
  if (start != end) {
    fmt(&result, *start);
    ++start;
    while (start != end) {
      result.append(sep);
      fmt(&result, *start);
      ++start;
    }
  }
  return result;
}

template <typename Iterator>
std::string StrJoin(Iterator start, Iterator end, std::string_view sep) {
  return StrJoin(start, end, sep, AlphaNumFormatter());
}

template <typename Range, typename Formatter>
std::string StrJoin(const Range& range, std::string_view sep,
                    Formatter&& fmt) {
  return StrJoin(std::begin(range), std::end(range), sep,
                 std::forward<Formatter>(fmt));
}

template <typename Range>
std::string StrJoin(const Range& range, std::string_view sep) {
  return StrJoin(std::begin(range), std::end(range), sep, AlphaNumFormatter());
}

template <typename T, typename Formatter>
std::string StrJoin(std::initializer_list<T> il, std::string_view sep,
                    Formatter&& fmt) {
  return StrJoin(il.begin(), il.end(), sep, std::forward<Formatter>(fmt));
}

template <typename T>
std::string StrJoin(std::initializer_list<T> il, std::string_view sep) {
  return StrJoin(il.begin(), il.end(), sep, AlphaNumFormatter());
}

template <typename... T, typename Formatter>
std::string StrJoin(const std::tuple<T...>& value, std::string_view separator,
                    Formatter&& fmt) {
  std::string result;
  bool first = true;
  std::apply(
      [&](const auto&... elems) {
        auto append_elem = [&](const auto& e) {
          if (!first) result.append(separator);
          first = false;
          fmt(&result, e);
        };
        (append_elem(elems), ...);
      },
      value);
  return result;
}

template <typename... T>
std::string StrJoin(const std::tuple<T...>& value, std::string_view separator) {
  return StrJoin(value, separator, AlphaNumFormatter());
}

}  // namespace absl
