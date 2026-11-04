#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <string>
#include <string_view>

extern "C" {
#endif

uint32_t crc32c_extend(uint32_t crc, const uint8_t* data, size_t count);
uint32_t crc32c_value(const uint8_t* data, size_t count);

#ifdef __cplusplus
}  // extern "C"

namespace crc32c {

uint32_t Extend(uint32_t crc, const uint8_t* data, size_t count);

inline uint32_t Crc32c(const uint8_t* data, size_t count) {
  return Extend(0, data, count);
}

inline uint32_t Crc32c(const char* data, size_t count) {
  return Extend(0, reinterpret_cast<const uint8_t*>(data), count);
}

inline uint32_t Crc32c(const std::string& string) {
  return Crc32c(reinterpret_cast<const uint8_t*>(string.data()), string.size());
}

inline uint32_t Crc32c(const std::string_view& sv) {
  return Crc32c(reinterpret_cast<const uint8_t*>(sv.data()), sv.size());
}

}  // namespace crc32c
#endif
