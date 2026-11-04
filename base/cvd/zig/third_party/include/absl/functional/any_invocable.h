#pragma once

#include <functional>

namespace absl {

template <typename Sig>
using AnyInvocable = std::function<Sig>;

}  // namespace absl
