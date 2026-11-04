#pragma once

#include "absl/log/log.h"

#ifndef CHECK
#define CHECK(condition)                                              \
  (condition)                                                         \
      ? (void)0                                                       \
      : ::absl::log_internal::Voidify() &                             \
            ::absl::log_internal::LogMessageFatal(__FILE__, __LINE__) \
                << "Check failed: " #condition " "
#endif

#define ABSL_CHECK_OP_IMPL(name, op, val1, val2)                      \
  ((val1)op(val2))                                                    \
      ? (void)0                                                       \
      : ::absl::log_internal::Voidify() &                             \
            ::absl::log_internal::LogMessageFatal(__FILE__, __LINE__) \
                << "Check failed: " #val1 " " #op " " #val2 " "

#ifndef CHECK_EQ
#define CHECK_EQ(val1, val2) ABSL_CHECK_OP_IMPL(Check_EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) ABSL_CHECK_OP_IMPL(Check_NE, !=, val1, val2)
#define CHECK_LE(val1, val2) ABSL_CHECK_OP_IMPL(Check_LE, <=, val1, val2)
#define CHECK_LT(val1, val2) ABSL_CHECK_OP_IMPL(Check_LT, <, val1, val2)
#define CHECK_GE(val1, val2) ABSL_CHECK_OP_IMPL(Check_GE, >=, val1, val2)
#define CHECK_GT(val1, val2) ABSL_CHECK_OP_IMPL(Check_GT, >, val1, val2)
#endif

#ifndef CHECK_OK
#define CHECK_OK(status) CHECK((status).ok())
#endif

#ifndef QCHECK
#define QCHECK(condition) CHECK(condition)
#define QCHECK_EQ(val1, val2) CHECK_EQ(val1, val2)
#define QCHECK_NE(val1, val2) CHECK_NE(val1, val2)
#define QCHECK_LE(val1, val2) CHECK_LE(val1, val2)
#define QCHECK_LT(val1, val2) CHECK_LT(val1, val2)
#define QCHECK_GE(val1, val2) CHECK_GE(val1, val2)
#define QCHECK_GT(val1, val2) CHECK_GT(val1, val2)
#define QCHECK_OK(status) CHECK_OK(status)
#endif

#ifndef DCHECK
#define DCHECK(condition) CHECK(condition)
#define DCHECK_EQ(val1, val2) CHECK_EQ(val1, val2)
#define DCHECK_NE(val1, val2) CHECK_NE(val1, val2)
#define DCHECK_LE(val1, val2) CHECK_LE(val1, val2)
#define DCHECK_LT(val1, val2) CHECK_LT(val1, val2)
#define DCHECK_GE(val1, val2) CHECK_GE(val1, val2)
#define DCHECK_GT(val1, val2) CHECK_GT(val1, val2)
#define DCHECK_OK(status) CHECK_OK(status)
#endif

#define ABSL_CHECK(condition) CHECK(condition)
#define ABSL_CHECK_EQ(val1, val2) CHECK_EQ(val1, val2)
#define ABSL_CHECK_NE(val1, val2) CHECK_NE(val1, val2)
#define ABSL_CHECK_LE(val1, val2) CHECK_LE(val1, val2)
#define ABSL_CHECK_LT(val1, val2) CHECK_LT(val1, val2)
#define ABSL_CHECK_GE(val1, val2) CHECK_GE(val1, val2)
#define ABSL_CHECK_GT(val1, val2) CHECK_GT(val1, val2)
#define ABSL_QCHECK(condition) QCHECK(condition)
#define ABSL_DCHECK(condition) DCHECK(condition)
