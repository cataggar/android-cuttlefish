#pragma once

#include <stdint.h>

#include "fruit/fruit.h"

#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/host/libs/confui/host_mode_ctrl.h"
#include "cuttlefish/host/libs/screen_connector/screen_connector_common.h"

namespace cuttlefish {
namespace confui {

struct PipeConnectionPair {
  SharedFD from_guest_;
  SharedFD to_guest_;
};

class ConfUiRenderer {
 public:
  INJECT(ConfUiRenderer(ScreenConnectorFrameRenderer& screen_connector))
      : screen_connector_(screen_connector) {}
  ~ConfUiRenderer() = default;

 private:
  ScreenConnectorFrameRenderer& screen_connector_;
};

class HostServer {
 public:
  INJECT(HostServer(HostModeCtrl& host_mode_ctrl, ConfUiRenderer& host_renderer,
                    const PipeConnectionPair& fd_pair))
      : host_mode_ctrl_(host_mode_ctrl),
        host_renderer_(host_renderer),
        fd_pair_(fd_pair) {}

  virtual ~HostServer() = default;

  void Start() {}
  void TouchEvent(int, int, bool) {}
  void UserAbortEvent() {}

 private:
  HostModeCtrl& host_mode_ctrl_;
  ConfUiRenderer& host_renderer_;
  PipeConnectionPair fd_pair_;
};

}  // namespace confui
}  // namespace cuttlefish
