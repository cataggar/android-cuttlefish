#pragma once

#include <cstddef>
#include <cstdint>

namespace webrtc {

class AudioTransport {
 public:
  virtual int32_t RecordedDataIsAvailable(
      const void* audioSamples,
      const size_t nSamples,
      const size_t nBytesPerSample,
      const size_t nChannels,
      const uint32_t samplesPerSec,
      const uint32_t totalDelayMS,
      const int32_t clockDrift,
      const uint32_t currentMicLevel,
      const bool keyPressed,
      uint32_t& newMicLevel) = 0;  // NOLINT

  virtual int32_t NeedMorePlayData(
      const size_t nSamples,
      const size_t nBytesPerSample,
      const size_t nChannels,
      const uint32_t samplesPerSec,
      void* audioSamples,
      size_t& nSamplesOut,  // NOLINT
      int64_t* elapsed_time_ms,
      int64_t* ntp_time_ms) = 0;

  virtual void PullRenderData(
      int bits_per_sample,
      int sample_rate,
      size_t number_of_channels,
      size_t number_of_frames,
      void* audio_data,
      int64_t* elapsed_time_ms,
      int64_t* ntp_time_ms) {}

 protected:
  virtual ~AudioTransport() = default;
};

}  // namespace webrtc
