#pragma once

#include <cstdlib>
#include <sstream>

namespace rtc {
class FatalCheckStream {
 public:
  FatalCheckStream() = default;
  ~FatalCheckStream() { std::abort(); }
  std::ostream& stream() { return stream_; }

 private:
  std::ostringstream stream_;
};

class NullCheckStream {
 public:
  std::ostream& stream() { return stream_; }

 private:
  std::ostringstream stream_;
};
}  // namespace rtc

#define RTC_CHECK(condition) \
  while (!(condition)) ::rtc::FatalCheckStream().stream()
#define RTC_CHECK_EQ(a, b) RTC_CHECK((a) == (b))
#define RTC_CHECK_NE(a, b) RTC_CHECK((a) != (b))
#define RTC_CHECK_LT(a, b) RTC_CHECK((a) < (b))
#define RTC_CHECK_LE(a, b) RTC_CHECK((a) <= (b))
#define RTC_CHECK_GT(a, b) RTC_CHECK((a) > (b))
#define RTC_CHECK_GE(a, b) RTC_CHECK((a) >= (b))

#define RTC_DCHECK(condition) \
  while (false && !(condition)) ::rtc::NullCheckStream().stream()
#define RTC_DCHECK_EQ(a, b) RTC_DCHECK((a) == (b))
#define RTC_DCHECK_NE(a, b) RTC_DCHECK((a) != (b))
#define RTC_DCHECK_LT(a, b) RTC_DCHECK((a) < (b))
#define RTC_DCHECK_LE(a, b) RTC_DCHECK((a) <= (b))
#define RTC_DCHECK_GT(a, b) RTC_DCHECK((a) > (b))
#define RTC_DCHECK_GE(a, b) RTC_DCHECK((a) >= (b))
#define RTC_DCHECK_RUN_ON(x) (void)(x)
