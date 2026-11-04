#pragma once

#include <memory>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/thread.h"

namespace webrtc {

class AudioMixer : public rtc::RefCountInterface {};
class AudioFrameProcessor {};
class FieldTrialsView {
 public:
  virtual ~FieldTrialsView() = default;
};

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    rtc::scoped_refptr<AudioDeviceModule> default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing,
    AudioFrameProcessor* audio_frame_processor = nullptr,
    std::unique_ptr<FieldTrialsView> field_trials = nullptr);

}  // namespace webrtc
