#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

namespace android {
namespace base {

inline void StringAppendV(std::string* dst, const char* format, va_list ap) {
  va_list ap_copy;
  va_copy(ap_copy, ap);
  char buf[1024];
  int result = std::vsnprintf(buf, sizeof(buf), format, ap_copy);
  va_end(ap_copy);
  if (result < 0) return;
  if (static_cast<size_t>(result) < sizeof(buf)) {
    dst->append(buf, static_cast<size_t>(result));
    return;
  }
  size_t old_size = dst->size();
  dst->resize(old_size + static_cast<size_t>(result));
  va_copy(ap_copy, ap);
  std::vsnprintf(dst->data() + old_size, static_cast<size_t>(result) + 1,
                 format, ap_copy);
  va_end(ap_copy);
}

inline std::string StringPrintf(const char* fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));

inline std::string StringPrintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::string result;
  StringAppendV(&result, fmt, ap);
  va_end(ap);
  return result;
}

inline void StringAppendF(std::string* dst, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));

inline void StringAppendF(std::string* dst, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  StringAppendV(dst, fmt, ap);
  va_end(ap);
}

}  // namespace base
}  // namespace android
