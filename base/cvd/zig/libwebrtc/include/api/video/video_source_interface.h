#pragma once

#include <limits>
#include <vector>

#include "api/video/video_sink_interface.h"
#include "rtc_base/ref_count.h"

namespace rtc {

struct VideoSinkWants {
  struct FrameSize {
    FrameSize(int width, int height) : width(width), height(height) {}
    int width;
    int height;
  };

  bool rotation_applied = false;
  bool black_frames = false;
  int max_pixel_count = std::numeric_limits<int>::max();
  absl::optional<int> target_pixel_count;
  int max_framerate_fps = std::numeric_limits<int>::max();
  int resolution_alignment = 1;
  std::vector<FrameSize> resolutions;
};

template <typename VideoFrameT>
class VideoSourceInterface {
 public:
  virtual ~VideoSourceInterface() = default;
  virtual void AddOrUpdateSink(VideoSinkInterface<VideoFrameT>* sink,
                               const VideoSinkWants& wants) = 0;
  virtual void RemoveSink(VideoSinkInterface<VideoFrameT>* sink) = 0;
  virtual void RequestRefreshFrame() {}
};

}  // namespace rtc

namespace webrtc {
using rtc::VideoSinkWants;
using rtc::VideoSourceInterface;
}  // namespace webrtc
