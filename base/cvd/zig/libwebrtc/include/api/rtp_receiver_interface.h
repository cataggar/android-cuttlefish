#pragma once

#include <string>
#include <vector>

#include "api/media_stream_interface.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class RtpReceiverInterface : public rtc::RefCountInterface {
 public:
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> track() const = 0;
  virtual std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams() const {
    return {};
  }
  virtual std::vector<std::string> stream_ids() const { return {}; }
  virtual std::string id() const = 0;
  virtual RtpParameters GetParameters() const = 0;
  virtual bool SetParameters(const RtpParameters& parameters) {
    (void)parameters;
    return true;
  }

 protected:
  ~RtpReceiverInterface() override = default;
};

}  // namespace webrtc
