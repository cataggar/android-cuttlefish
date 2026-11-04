#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "api/candidate.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

struct SdpParseError {
  std::string line;
  std::string description;
};

class IceCandidateInterface {
 public:
  virtual ~IceCandidateInterface() = default;
  virtual std::string sdp_mid() const = 0;
  virtual int sdp_mline_index() const = 0;
  virtual const cricket::Candidate& candidate() const = 0;
  virtual std::string server_url() const { return ""; }
  virtual bool ToString(std::string* out) const = 0;
};

IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error);

std::unique_ptr<IceCandidateInterface> CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const cricket::Candidate& candidate);

class IceCandidateCollection {
 public:
  virtual ~IceCandidateCollection() = default;
  virtual size_t count() const = 0;
  virtual bool HasCandidate(const IceCandidateInterface* candidate) const = 0;
  virtual const IceCandidateInterface* at(size_t index) const = 0;
};

enum class SdpType {
  kOffer,
  kPrAnswer,
  kAnswer,
  kRollback,
};

const char* SdpTypeToString(SdpType type);
absl::optional<SdpType> SdpTypeFromString(const std::string& type_str);

class SessionDescriptionInterface {
 public:
  static const char kOffer[];
  static const char kPrAnswer[];
  static const char kAnswer[];
  static const char kRollback[];

  virtual ~SessionDescriptionInterface() = default;

  virtual std::unique_ptr<SessionDescriptionInterface> Clone() const {
    return nullptr;
  }

  virtual SdpType GetType() const;
  virtual std::string type() const = 0;

  virtual bool AddCandidate(const IceCandidateInterface* candidate) = 0;
  virtual size_t RemoveCandidates(
      const std::vector<cricket::Candidate>& candidates) {
    (void)candidates;
    return 0;
  }

  virtual size_t number_of_mediasections() const = 0;
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const = 0;
  virtual bool ToString(std::string* out) const = 0;
};

SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error);

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type, const std::string& sdp);

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type, const std::string& sdp, SdpParseError* error_out);

class CreateSessionDescriptionObserver : public rtc::RefCountInterface {
 public:
  virtual void OnSuccess(SessionDescriptionInterface* desc) = 0;
  virtual void OnFailure(RTCError error) { (void)error; }
  virtual void OnFailure(const std::string& error) { (void)error; }

 protected:
  ~CreateSessionDescriptionObserver() override = default;
};

class SetSessionDescriptionObserver : public rtc::RefCountInterface {
 public:
  virtual void OnSuccess() = 0;
  virtual void OnFailure(RTCError error) { (void)error; }
  virtual void OnFailure(const std::string& error) { (void)error; }

 protected:
  ~SetSessionDescriptionObserver() override = default;
};

class SetLocalDescriptionObserverInterface : public rtc::RefCountInterface {
 public:
  virtual void OnSetLocalDescriptionComplete(RTCError error) = 0;
};

class SetRemoteDescriptionObserverInterface : public rtc::RefCountInterface {
 public:
  virtual void OnSetRemoteDescriptionComplete(RTCError error) = 0;
};

}  // namespace webrtc
