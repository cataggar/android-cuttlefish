#pragma once

#include <cstdint>

namespace absl {

using uint128 = unsigned __int128;
using int128 = __int128;

constexpr uint128 MakeUint128(uint64_t high, uint64_t low) noexcept {
  return (static_cast<uint128>(high) << 64) | static_cast<uint128>(low);
}

constexpr uint64_t Uint128High64(uint128 v) noexcept {
  return static_cast<uint64_t>(v >> 64);
}

constexpr uint64_t Uint128Low64(uint128 v) noexcept {
  return static_cast<uint64_t>(v);
}

}  // namespace absl
