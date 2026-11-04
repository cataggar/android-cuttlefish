#pragma once

#include <cstddef>
#include <cstdint>

namespace webrtc {

class AudioEncoder {
 public:
  virtual ~AudioEncoder() = default;
  virtual int SampleRateHz() const = 0;
  virtual size_t NumChannels() const = 0;
  virtual int RtpTimestampRateHz() const { return SampleRateHz(); }
  virtual size_t Num10MsFramesInNextPacket() const { return 2; }
  virtual size_t Max10MsFramesInAPacket() const { return 6; }
  virtual int GetTargetBitrate() const { return 64000; }
  virtual void Reset() {}
};

}  // namespace webrtc
