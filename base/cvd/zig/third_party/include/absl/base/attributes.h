#pragma once

#define ABSL_ATTRIBUTE_UNUSED [[maybe_unused]]
#define ABSL_MUST_USE_RESULT [[nodiscard]]
#define ABSL_ATTRIBUTE_NORETURN [[noreturn]]
#define ABSL_ATTRIBUTE_NOINLINE __attribute__((noinline))
#define ABSL_ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline))
#define ABSL_ATTRIBUTE_WEAK __attribute__((weak))
#define ABSL_ATTRIBUTE_PACKED __attribute__((packed))
#define ABSL_PRINTF_ATTRIBUTE(string_index, first_to_check) \
  __attribute__((__format__(__printf__, string_index, first_to_check)))
#define ABSL_SCANF_ATTRIBUTE(string_index, first_to_check) \
  __attribute__((__format__(__scanf__, string_index, first_to_check)))
#define ABSL_CONST_INIT constinit
#define ABSL_FALLTHROUGH [[fallthrough]]
#define ABSL_DEPRECATED(message) [[deprecated(message)]]
#define ABSL_ATTRIBUTE_LIFETIME_BOUND
