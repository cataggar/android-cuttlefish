#pragma once

#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/scoped_refptr.h"

namespace webrtc {

scoped_refptr<AudioEncoderFactory> CreateBuiltinAudioEncoderFactory();

}  // namespace webrtc
