#pragma once

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>

namespace android {
namespace base {

inline uint64_t GetThreadId() {
#if defined(__linux__)
  return static_cast<uint64_t>(syscall(__NR_gettid));
#else
  return static_cast<uint64_t>(getpid());
#endif
}

}  // namespace base
}  // namespace android
