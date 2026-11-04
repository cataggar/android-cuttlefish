#pragma once

#include <cstddef>
#include <cstdint>

namespace webrtc {

class VideoBitrateAllocation {
 public:
  static constexpr size_t kMaxSpatialLayers = 5;
  static constexpr size_t kMaxTemporalStreams = 4;

  VideoBitrateAllocation() = default;

  bool SetBitrate(size_t spatial_index,
                  size_t temporal_index,
                  uint32_t bitrate_bps) {
    if (spatial_index >= kMaxSpatialLayers ||
        temporal_index >= kMaxTemporalStreams) {
      return false;
    }
    uint32_t old = bitrates_[spatial_index][temporal_index];
    bitrates_[spatial_index][temporal_index] = bitrate_bps;
    sum_ += bitrate_bps;
    sum_ -= old;
    return true;
  }

  uint32_t GetBitrate(size_t spatial_index, size_t temporal_index) const {
    if (spatial_index >= kMaxSpatialLayers ||
        temporal_index >= kMaxTemporalStreams) {
      return 0;
    }
    return bitrates_[spatial_index][temporal_index];
  }

  uint32_t get_sum_bps() const { return sum_; }
  uint32_t get_sum_kbps() const { return (sum_ + 500) / 1000; }

  bool is_bw_limited() const { return is_bw_limited_; }
  void set_bw_limited(bool limited) { is_bw_limited_ = limited; }

 private:
  uint32_t sum_ = 0;
  uint32_t bitrates_[kMaxSpatialLayers][kMaxTemporalStreams] = {};
  bool is_bw_limited_ = false;
};

}  // namespace webrtc
