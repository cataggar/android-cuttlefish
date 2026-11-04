#pragma once

#include <cstddef>
#include <map>
#include <string>

#include "rtc_base/ref_count.h"

namespace webrtc {

struct SdpAudioFormat {
  using Parameters = std::map<std::string, std::string>;

  SdpAudioFormat(absl::string_view name, int clockrate_hz, size_t num_channels)
      : name(name), clockrate_hz(clockrate_hz), num_channels(num_channels) {}
  SdpAudioFormat(absl::string_view name,
                 int clockrate_hz,
                 size_t num_channels,
                 const Parameters& param)
      : name(name),
        clockrate_hz(clockrate_hz),
        num_channels(num_channels),
        parameters(param) {}

  bool Matches(const SdpAudioFormat& o) const {
    return name == o.name && clockrate_hz == o.clockrate_hz &&
           num_channels == o.num_channels;
  }

  std::string name;
  int clockrate_hz = 48000;
  size_t num_channels = 2;
  Parameters parameters;
};

struct AudioCodecInfo {
  AudioCodecInfo(int sample_rate_hz,
                 size_t num_channels,
                 int default_bitrate_bps)
      : sample_rate_hz(sample_rate_hz),
        num_channels(num_channels),
        default_bitrate_bps(default_bitrate_bps),
        min_bitrate_bps(default_bitrate_bps),
        max_bitrate_bps(default_bitrate_bps) {}
  AudioCodecInfo(int sample_rate_hz,
                 size_t num_channels,
                 int default_bitrate_bps,
                 int min_bitrate_bps,
                 int max_bitrate_bps)
      : sample_rate_hz(sample_rate_hz),
        num_channels(num_channels),
        default_bitrate_bps(default_bitrate_bps),
        min_bitrate_bps(min_bitrate_bps),
        max_bitrate_bps(max_bitrate_bps) {}

  int sample_rate_hz = 48000;
  size_t num_channels = 2;
  int default_bitrate_bps = 64000;
  int min_bitrate_bps = 6000;
  int max_bitrate_bps = 510000;
  bool allow_comfort_noise = false;
  bool supports_network_adaption = false;
};

struct AudioCodecSpec {
  SdpAudioFormat format;
  AudioCodecInfo info;
};

}  // namespace webrtc
