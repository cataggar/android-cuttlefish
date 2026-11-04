#pragma once

#include <string>

#include "api/rtc_error.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

enum class RtpTransceiverDirection {
  kSendRecv,
  kSendOnly,
  kRecvOnly,
  kInactive,
  kStopped,
};

class RtpTransceiverInterface : public rtc::RefCountInterface {
 public:
  virtual absl::optional<std::string> mid() const { return absl::nullopt; }
  virtual rtc::scoped_refptr<RtpSenderInterface> sender() const = 0;
  virtual rtc::scoped_refptr<RtpReceiverInterface> receiver() const = 0;
  virtual bool stopped() const { return false; }
  virtual bool stopping() const { return false; }
  virtual RtpTransceiverDirection direction() const {
    return RtpTransceiverDirection::kSendRecv;
  }
  virtual RTCError SetDirectionWithError(
      RtpTransceiverDirection new_direction) {
    (void)new_direction;
    return RTCError::OK();
  }

 protected:
  ~RtpTransceiverInterface() override = default;
};

}  // namespace webrtc
