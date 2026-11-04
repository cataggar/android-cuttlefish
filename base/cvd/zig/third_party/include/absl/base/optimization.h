#pragma once

#define ABSL_PREDICT_FALSE(x) (__builtin_expect(false || (x), false))
#define ABSL_PREDICT_TRUE(x) (__builtin_expect(false || (x), true))
#define ABSL_INTERNAL_ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
