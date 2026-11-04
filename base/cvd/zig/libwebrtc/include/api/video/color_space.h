#pragma once

#include <cstdint>

namespace webrtc {

class ColorSpace {
 public:
  enum class PrimaryID : uint8_t {
    kInvalid = 0,
    kBT709 = 1,
    kUnspecified = 2,
  };
  enum class TransferID : uint8_t {
    kInvalid = 0,
    kBT709 = 1,
    kUnspecified = 2,
  };
  enum class MatrixID : uint8_t {
    kRGB = 0,
    kBT709 = 1,
    kUnspecified = 2,
  };
  enum class RangeID : uint8_t {
    kInvalid = 0,
    kLimited = 1,
    kFull = 2,
    kDerived = 3,
  };

  ColorSpace() = default;
  PrimaryID primaries() const { return primaries_; }
  TransferID transfer() const { return transfer_; }
  MatrixID matrix() const { return matrix_; }
  RangeID range() const { return range_; }

 private:
  PrimaryID primaries_ = PrimaryID::kUnspecified;
  TransferID transfer_ = TransferID::kUnspecified;
  MatrixID matrix_ = MatrixID::kUnspecified;
  RangeID range_ = RangeID::kLimited;
};

}  // namespace webrtc
