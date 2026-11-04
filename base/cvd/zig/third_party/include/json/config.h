#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#define JSONCPP_USING_SECURE_MEMORY 0
#define JSON_USE_EXCEPTION 1
#define JSON_HAS_INT64 1

#ifndef JSONCPP_STRING
#define JSONCPP_STRING std::string
#endif

namespace Json {

using Int = int;
using UInt = unsigned int;
using Int64 = int64_t;
using UInt64 = uint64_t;
using LargestInt = Int64;
using LargestUInt = UInt64;
using String = std::string;
using ArrayIndex = unsigned int;

}  // namespace Json
