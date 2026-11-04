#pragma once

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/scoped_refptr.h"

namespace webrtc {

scoped_refptr<AudioDecoderFactory> CreateBuiltinAudioDecoderFactory();

}  // namespace webrtc
