#pragma once

#include <functional>

namespace absl {

template <typename Sig>
using FunctionRef = std::function<Sig>;

}  // namespace absl
