#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cuttlefish {
namespace config {

enum Audio_ChannelLayout : int {
  Audio_ChannelLayout_UNKNOWN = 0,
  Audio_ChannelLayout_MONO = 1,
  Audio_ChannelLayout_STEREO = 2,
  Audio_ChannelLayout_SURROUND51 = 3,
};

enum Audio_SampleRate : int {
  Audio_SampleRate_RATE_UNKNOWN = 0,
  Audio_SampleRate_RATE_32000 = 1,
  Audio_SampleRate_RATE_44100 = 2,
  Audio_SampleRate_RATE_48000 = 3,
  Audio_SampleRate_RATE_64000 = 4,
};

class Audio_AudioMixer {
 public:
  bool has_channel_layout() const { return has_channel_layout_; }
  Audio_ChannelLayout channel_layout() const { return channel_layout_; }
  void set_channel_layout(Audio_ChannelLayout v) {
    has_channel_layout_ = true;
    channel_layout_ = v;
  }

  bool has_sample_rate() const { return has_sample_rate_; }
  Audio_SampleRate sample_rate() const { return sample_rate_; }
  void set_sample_rate(Audio_SampleRate v) {
    has_sample_rate_ = true;
    sample_rate_ = v;
  }

 private:
  bool has_channel_layout_ = false;
  Audio_ChannelLayout channel_layout_ = Audio_ChannelLayout_STEREO;
  bool has_sample_rate_ = false;
  Audio_SampleRate sample_rate_ = Audio_SampleRate_RATE_48000;
};

class Audio_PCMDevice_Controls_Volume {
 public:
  uint32_t min() const { return min_; }
  uint32_t max() const { return max_; }
  uint32_t step() const { return step_; }

 private:
  uint32_t min_ = 0;
  uint32_t max_ = 100;
  uint32_t step_ = 1;
};

class Audio_PCMDevice_Controls {
 public:
  bool mute_control_enabled() const { return mute_control_enabled_; }
  bool duck_control_enabled() const { return duck_control_enabled_; }
  bool fade_balance_control_enabled() const {
    return fade_balance_control_enabled_;
  }
  bool has_volume_control() const { return has_volume_control_; }
  const Audio_PCMDevice_Controls_Volume& volume_control() const {
    return volume_control_;
  }

 private:
  bool mute_control_enabled_ = false;
  bool duck_control_enabled_ = false;
  bool fade_balance_control_enabled_ = false;
  bool has_volume_control_ = false;
  Audio_PCMDevice_Controls_Volume volume_control_;
};

class Audio_PCMDevice_Stream {
 public:
  uint32_t id() const { return id_; }
  void set_id(uint32_t v) { id_ = v; }

  Audio_ChannelLayout channel_layout() const { return channel_layout_; }
  void set_channel_layout(Audio_ChannelLayout v) { channel_layout_ = v; }

  bool has_controls() const { return has_controls_; }
  const Audio_PCMDevice_Controls& controls() const { return controls_; }

 private:
  uint32_t id_ = 0;
  Audio_ChannelLayout channel_layout_ = Audio_ChannelLayout_STEREO;
  bool has_controls_ = false;
  Audio_PCMDevice_Controls controls_;
};

class Audio_PCMDevice {
 public:
  const std::vector<Audio_PCMDevice_Stream>& capture_streams() const {
    return capture_streams_;
  }
  const std::vector<Audio_PCMDevice_Stream>& playback_streams() const {
    return playback_streams_;
  }
  bool has_mixer() const { return has_mixer_; }
  const Audio_AudioMixer& mixer() const { return mixer_; }
  bool has_virtual_tuner() const { return has_virtual_tuner_; }
  const Audio_PCMDevice_Stream& virtual_tuner() const { return virtual_tuner_; }

 private:
  std::vector<Audio_PCMDevice_Stream> capture_streams_;
  std::vector<Audio_PCMDevice_Stream> playback_streams_;
  bool has_mixer_ = false;
  Audio_AudioMixer mixer_;
  bool has_virtual_tuner_ = false;
  Audio_PCMDevice_Stream virtual_tuner_;
};

class Audio {
 public:
  using ChannelLayout = Audio_ChannelLayout;
  using SampleRate = Audio_SampleRate;
  using AudioMixer = Audio_AudioMixer;
  using PCMDevice = Audio_PCMDevice;

  static constexpr Audio_ChannelLayout UNKNOWN = Audio_ChannelLayout_UNKNOWN;
  static constexpr Audio_ChannelLayout MONO = Audio_ChannelLayout_MONO;
  static constexpr Audio_ChannelLayout STEREO = Audio_ChannelLayout_STEREO;
  static constexpr Audio_ChannelLayout SURROUND51 =
      Audio_ChannelLayout_SURROUND51;

  static constexpr Audio_SampleRate RATE_UNKNOWN =
      Audio_SampleRate_RATE_UNKNOWN;
  static constexpr Audio_SampleRate RATE_32000 = Audio_SampleRate_RATE_32000;
  static constexpr Audio_SampleRate RATE_44100 = Audio_SampleRate_RATE_44100;
  static constexpr Audio_SampleRate RATE_48000 = Audio_SampleRate_RATE_48000;
  static constexpr Audio_SampleRate RATE_64000 = Audio_SampleRate_RATE_64000;

  const std::vector<Audio_PCMDevice>& pcm_devices() const {
    return pcm_devices_;
  }

 private:
  std::vector<Audio_PCMDevice> pcm_devices_;
};

class GuestConfigFile {
 public:
  const Audio& audio() const { return audio_; }

 private:
  Audio audio_;
};

}  // namespace config
}  // namespace cuttlefish
