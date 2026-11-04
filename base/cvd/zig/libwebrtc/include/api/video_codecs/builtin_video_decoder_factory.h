#pragma once

#include <memory>

#include "api/video_codecs/video_decoder_factory.h"

namespace webrtc {

std::unique_ptr<VideoDecoderFactory> CreateBuiltinVideoDecoderFactory();

}  // namespace webrtc
