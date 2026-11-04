#pragma once

namespace rtc {

template <typename VideoFrameT>
class VideoSinkInterface {
 public:
  virtual ~VideoSinkInterface() = default;
  virtual void OnFrame(const VideoFrameT& frame) = 0;
  virtual void OnDiscardedFrame() {}
};

}  // namespace rtc

namespace webrtc {
using rtc::VideoSinkInterface;
}  // namespace webrtc
