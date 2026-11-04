#pragma once

#include <memory>

#include "api/video/video_bitrate_allocator_factory.h"

namespace webrtc {

std::unique_ptr<VideoBitrateAllocatorFactory>
CreateBuiltinVideoBitrateAllocatorFactory();

}  // namespace webrtc
