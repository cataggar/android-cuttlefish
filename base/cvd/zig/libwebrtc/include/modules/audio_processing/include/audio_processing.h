#pragma once

#include <cstddef>
#include <cstdint>

#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class AudioProcessing : public RefCountInterface {
 public:
  struct Config {};
  virtual void ApplyConfig(const Config& config) {}
  virtual int Initialize() { return 0; }
};

class AudioProcessingBuilder {
 public:
  AudioProcessingBuilder() = default;
  ~AudioProcessingBuilder() = default;
  AudioProcessingBuilder& SetConfig(const AudioProcessing::Config& config) {
    (void)config;
    return *this;
  }
  scoped_refptr<AudioProcessing> Create();
};

}  // namespace webrtc
