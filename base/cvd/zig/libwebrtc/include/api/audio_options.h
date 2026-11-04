#pragma once

#include "rtc_base/ref_count.h"

namespace cricket {

struct AudioOptions {
  absl::optional<bool> echo_cancellation;
  absl::optional<bool> auto_gain_control;
  absl::optional<bool> noise_suppression;
  absl::optional<bool> highpass_filter;
  absl::optional<bool> stereo_swapping;
  absl::optional<int> audio_jitter_buffer_max_packets;
  absl::optional<bool> audio_jitter_buffer_fast_accelerate;
  absl::optional<int> audio_jitter_buffer_min_delay_ms;
};

}  // namespace cricket
