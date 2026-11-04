#pragma once

#include <cstddef>

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete
#endif

#ifndef DISALLOW_IMPLICIT_CONSTRUCTORS
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)
#endif

#ifndef arraysize
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))
#endif

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp)            \
  ({                                       \
    decltype(exp) _rc;                     \
    do {                                   \
      _rc = (exp);                         \
    } while (_rc == -1 && errno == EINTR); \
    _rc;                                   \
  })
#endif

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED [[maybe_unused]]
#endif

#ifndef UNUSED
#define UNUSED(...)                   \
  do {                                \
    (void)sizeof(__VA_ARGS__, 0);     \
  } while (0)
#endif

#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED [[fallthrough]]
#endif
