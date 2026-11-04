#pragma once

#include <memory>

#include "api/task_queue/task_queue_factory.h"

namespace webrtc {

std::unique_ptr<TaskQueueFactory> CreateDefaultTaskQueueFactory();

}  // namespace webrtc
