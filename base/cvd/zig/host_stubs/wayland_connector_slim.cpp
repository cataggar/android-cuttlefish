#include "cuttlefish/host/libs/screen_connector/wayland_screen_connector.h"

#include "cuttlefish/host/libs/wayland/wayland_server.h"
#include "cuttlefish/host/libs/wayland/wayland_surfaces.h"

namespace wayland {

namespace internal {
struct WaylandServerState {};
}  // namespace internal

void Surfaces::SetFrameCallback(FrameCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  callback_ = std::move(callback);
}

void Surfaces::SetDisplayEventCallback(DisplayEventCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  event_callback_ = std::move(callback);
}

void Surfaces::SetFramesAreRGBA(bool frames_are_rgba) {
  frames_are_rgba_ = frames_are_rgba;
}

WaylandServer::WaylandServer(int wayland_socket_fd,
                             bool wayland_frames_are_rgba)
    : server_state_(std::make_unique<internal::WaylandServerState>()) {
  (void)wayland_socket_fd;
  (void)wayland_frames_are_rgba;
  server_ready_ = true;
}

WaylandServer::~WaylandServer() = default;

void WaylandServer::SetFrameCallback(Surfaces::FrameCallback callback) {
  (void)callback;
}

void WaylandServer::SetDisplayEventCallback(DisplayEventCallback callback) {
  (void)callback;
}

}  // namespace wayland

namespace cuttlefish {

WaylandScreenConnector::WaylandScreenConnector(int frames_fd,
                                               bool frames_are_rgba)
    : server_(std::make_unique<wayland::WaylandServer>(frames_fd,
                                                       frames_are_rgba)) {}

void WaylandScreenConnector::SetFrameCallback(
    GenerateProcessedFrameCallbackImpl frame_callback) {
  if (server_) {
    server_->SetFrameCallback(std::move(frame_callback));
  }
}

void WaylandScreenConnector::SetDisplayEventCallback(
    DisplayEventCallback event_callback) {
  if (server_) {
    server_->SetDisplayEventCallback(std::move(event_callback));
  }
}

}  // namespace cuttlefish
