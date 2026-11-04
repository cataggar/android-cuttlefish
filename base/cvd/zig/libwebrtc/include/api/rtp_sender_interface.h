#pragma once

#include <string>
#include <vector>

#include "api/media_stream_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class RtpSenderInterface : public rtc::RefCountInterface {
 public:
  virtual bool SetTrack(MediaStreamTrackInterface* track) = 0;
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> track() const = 0;
  virtual uint32_t ssrc() const { return 0; }
  virtual std::string id() const = 0;
  virtual std::vector<std::string> stream_ids() const = 0;
  virtual void SetStreams(const std::vector<std::string>& stream_ids) {
    (void)stream_ids;
  }
  virtual RtpParameters GetParameters() const = 0;
  virtual RTCError SetParameters(const RtpParameters& parameters) = 0;

 protected:
  ~RtpSenderInterface() override = default;
};

}  // namespace webrtc
