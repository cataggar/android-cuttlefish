#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "rtc_base/ref_count.h"

namespace webrtc {

enum class DegradationPreference {
  DISABLED,
  MAINTAIN_FRAMERATE,
  MAINTAIN_RESOLUTION,
  BALANCED,
};

struct RtpEncodingParameters {
  absl::optional<uint32_t> ssrc;
  double bitrate_priority = 1.0;
  absl::optional<int> max_bitrate_bps;
  absl::optional<int> min_bitrate_bps;
  absl::optional<double> max_framerate;
  absl::optional<int> num_temporal_layers;
  absl::optional<double> scale_resolution_down_by;
  bool active = true;
  std::string rid;
  bool adaptive_ptime = false;
  absl::optional<std::string> scalability_mode;
};

struct RtpCodecParameters {
  std::string name;
  int payload_type = 0;
  absl::optional<int> clock_rate;
  absl::optional<int> num_channels;
  std::map<std::string, std::string> parameters;
};

struct RtpExtension {
  std::string uri;
  int id = 0;
  bool encrypt = false;
};

struct RtcpParameters {
  absl::optional<uint32_t> ssrc;
  std::string cname;
  bool reduced_size = false;
  bool mux = true;
};

struct RtpParameters {
  std::string transaction_id;
  std::string mid;
  std::vector<RtpCodecParameters> codecs;
  std::vector<RtpExtension> header_extensions;
  std::vector<RtpEncodingParameters> encodings;
  RtcpParameters rtcp;
  absl::optional<DegradationPreference> degradation_preference;
};

}  // namespace webrtc
