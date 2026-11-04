#pragma once

#include <memory>

#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

std::unique_ptr<VideoEncoderFactory> CreateBuiltinVideoEncoderFactory();

}  // namespace webrtc
