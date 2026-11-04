#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cuttlefish/host/frontend/webrtc/gpx_locations_handler.h"
#include "cuttlefish/host/frontend/webrtc/kml_locations_handler.h"
#include "cuttlefish/host/frontend/webrtc/location_handler.h"
#include "cuttlefish/host/frontend/webrtc/screenshot_handler.h"
#include "cuttlefish/host/frontend/webrtc/libdevice/local_recorder.h"
#include "cuttlefish/host/libs/confui/host_mode_ctrl.h"
#include "cuttlefish/host/libs/confui/host_server.h"
#include "cuttlefish/host/libs/confui/host_virtual_input.h"
#include "cuttlefish/host/libs/vm_manager/crosvm_display_controller.h"

namespace cuttlefish {

namespace webrtc_streaming {

LocationHandler::LocationHandler(
    std::function<void(const uint8_t*, size_t)> /*send_to_client*/) {}
LocationHandler::~LocationHandler() = default;
void LocationHandler::HandleMessage(float /*longitude*/, float /*latitude*/,
                                    float /*elevation*/) {}

KmlLocationsHandler::KmlLocationsHandler(
    std::function<void(const uint8_t*, size_t)> /*send_to_client*/) {}
KmlLocationsHandler::~KmlLocationsHandler() = default;
void KmlLocationsHandler::HandleMessage(const uint8_t* /*msg*/, size_t /*len*/) {}

GpxLocationsHandler::GpxLocationsHandler(
    std::function<void(const uint8_t*, size_t)> /*send_to_client*/) {}
GpxLocationsHandler::~GpxLocationsHandler() = default;
void GpxLocationsHandler::HandleMessage(const uint8_t* /*msg*/, size_t /*len*/) {}

class LocalRecorder::Impl {};

LocalRecorder::LocalRecorder(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
LocalRecorder::~LocalRecorder() = default;

std::unique_ptr<LocalRecorder> LocalRecorder::Create(
    const std::string& /*filename*/) {
  return std::unique_ptr<LocalRecorder>(
      new LocalRecorder(std::make_unique<Impl>()));
}

void LocalRecorder::AddDisplay(
    const std::string& /*label*/, size_t /*width*/, size_t /*height*/,
    std::shared_ptr<::webrtc::VideoTrackSourceInterface> /*source*/) {}

void LocalRecorder::Stop() {}

}  // namespace webrtc_streaming

namespace vm_manager {

Result<int> CrosvmDisplayController::Add(
    int /*instance_num*/,
    const std::vector<CuttlefishConfig::DisplayConfig>& /*display_configs*/)
    const {
  return CF_ERR("CrosvmDisplayController disabled in slim WebRTC build");
}

Result<int> CrosvmDisplayController::Remove(
    int /*instance_num*/, std::vector<std::string> /*display_ids*/) const {
  return CF_ERR("CrosvmDisplayController disabled in slim WebRTC build");
}

Result<std::string> CrosvmDisplayController::List(int /*instance_num*/) {
  return CF_ERR("CrosvmDisplayController disabled in slim WebRTC build");
}

Result<int> CrosvmDisplayController::RunCrosvmDisplayCommand(
    int /*instance_num*/, const std::vector<std::string>& /*args*/,
    std::string* /*stdout_str*/) const {
  return CF_ERR("CrosvmDisplayController disabled in slim WebRTC build");
}

Result<CrosvmDisplayController> GetCrosvmDisplayController() {
  return CF_ERR("CrosvmDisplayController disabled in slim WebRTC build");
}

}  // namespace vm_manager

namespace confui {

HostVirtualInput::HostVirtualInput(HostServer& host_server,
                                   HostModeCtrl& host_mode_ctrl,
                                   InputConnector& android_mode_input)
    : host_server_(host_server),
      host_mode_ctrl_(host_mode_ctrl),
      android_mode_input_(android_mode_input) {}

void HostVirtualInput::UserAbortEvent() { host_server_.UserAbortEvent(); }

bool HostVirtualInput::IsConfUiActive() {
  return host_mode_ctrl_.IsConfirmatioUiMode();
}

std::unique_ptr<InputConnector::EventSink> HostVirtualInput::CreateSink() {
  return android_mode_input_.CreateSink();
}

namespace thread {

std::string GetName(const std::thread::id /*tid*/) { return "confui_slim"; }

}  // namespace thread

}  // namespace confui

Result<void> ScreenshotHandler::Screenshot(
    uint32_t /*display_number*/, const std::string& /*screenshot_path*/) {
  return {};
}

void ScreenshotHandler::OnFrame(uint32_t /*display_number*/,
                                SharedFrame& /*frame*/) {}

}  // namespace cuttlefish
