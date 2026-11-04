#pragma once

namespace webrtc {

enum class VideoFrameType {
  kEmptyFrame = 0,
  kVideoFrameKey = 3,
  kVideoFrameDelta = 4,
};

enum class VideoContentType : uint8_t {
  UNSPECIFIED = 0,
  SCREENSHARE = 1,
};

}  // namespace webrtc
