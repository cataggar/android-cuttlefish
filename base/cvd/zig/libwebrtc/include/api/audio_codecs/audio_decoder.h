#pragma once

#include <cstddef>
#include <cstdint>

namespace webrtc {

class AudioDecoder {
 public:
  virtual ~AudioDecoder() = default;
  virtual int SampleRateHz() const = 0;
  virtual size_t Channels() const = 0;
  virtual void Reset() {}
};

}  // namespace webrtc
