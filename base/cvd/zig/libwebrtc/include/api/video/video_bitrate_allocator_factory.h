#pragma once

#include <cstdint>
#include <memory>

#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

struct VideoBitrateAllocationParameters {
  VideoBitrateAllocationParameters(uint32_t total_bitrate_bps,
                                   uint32_t framerate)
      : total_bitrate_bps(total_bitrate_bps),
        framerate(static_cast<double>(framerate)) {}
  VideoBitrateAllocationParameters(DataRate total_bitrate, double framerate)
      : total_bitrate_bps(static_cast<uint32_t>(total_bitrate.bps())),
        framerate(framerate) {}
  uint32_t total_bitrate_bps = 0;
  double framerate = 0.0;
};

class VideoBitrateAllocator {
 public:
  virtual ~VideoBitrateAllocator() = default;
  virtual VideoBitrateAllocation GetAllocation(uint32_t total_bitrate_bps,
                                               uint32_t framerate) {
    VideoBitrateAllocation allocation;
    allocation.SetBitrate(0, 0, total_bitrate_bps);
    return allocation;
  }
  virtual VideoBitrateAllocation Allocate(
      VideoBitrateAllocationParameters parameters) {
    return GetAllocation(parameters.total_bitrate_bps,
                         static_cast<uint32_t>(parameters.framerate));
  }
};

class VideoBitrateAllocatorFactory {
 public:
  virtual ~VideoBitrateAllocatorFactory() = default;
  virtual std::unique_ptr<VideoBitrateAllocator> CreateVideoBitrateAllocator(
      const VideoCodec& codec) = 0;
};

}  // namespace webrtc
