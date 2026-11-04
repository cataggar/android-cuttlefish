#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "api/audio_options.h"
#include "api/candidate.h"
#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/thread.h"

namespace webrtc {

enum class SdpSemantics {
  kPlanB_DEPRECATED,
  kPlanB = kPlanB_DEPRECATED,
  kUnifiedPlan
};

struct BitrateSettings {
  absl::optional<int> min_bitrate_bps;
  absl::optional<int> start_bitrate_bps;
  absl::optional<int> max_bitrate_bps;
};

class PeerConnectionObserver;

struct PeerConnectionDependencies final {
  explicit PeerConnectionDependencies(PeerConnectionObserver* observer_in)
      : observer(observer_in) {}
  PeerConnectionDependencies(const PeerConnectionDependencies&) = delete;
  PeerConnectionDependencies& operator=(const PeerConnectionDependencies&) =
      delete;
  PeerConnectionDependencies(PeerConnectionDependencies&&) = default;
  PeerConnectionDependencies& operator=(PeerConnectionDependencies&&) = default;
  ~PeerConnectionDependencies() = default;

  PeerConnectionObserver* observer = nullptr;
};

class PeerConnectionInterface : public rtc::RefCountInterface {
 public:
  enum SignalingState {
    kStable,
    kHaveLocalOffer,
    kHaveLocalPrAnswer,
    kHaveRemoteOffer,
    kHaveRemotePrAnswer,
    kClosed,
  };

  enum IceGatheringState {
    kIceGatheringNew,
    kIceGatheringGathering,
    kIceGatheringComplete
  };

  enum class PeerConnectionState {
    kNew,
    kConnecting,
    kConnected,
    kDisconnected,
    kFailed,
    kClosed,
  };

  enum IceConnectionState {
    kIceConnectionNew,
    kIceConnectionChecking,
    kIceConnectionConnected,
    kIceConnectionCompleted,
    kIceConnectionFailed,
    kIceConnectionDisconnected,
    kIceConnectionClosed,
    kIceConnectionMax,
  };

  enum TlsCertPolicy {
    kTlsCertPolicySecure,
    kTlsCertPolicyInsecureNoCheck,
  };

  struct IceServer {
    IceServer() = default;
    ~IceServer() = default;

    std::string uri;
    std::vector<std::string> urls;
    std::string username;
    std::string password;
    TlsCertPolicy tls_cert_policy = kTlsCertPolicySecure;
    std::string hostname;
    std::vector<std::string> tls_alpn_protocols;
    std::vector<std::string> tls_elliptic_curves;

    bool operator==(const IceServer& o) const {
      return uri == o.uri && urls == o.urls && username == o.username &&
             password == o.password && tls_cert_policy == o.tls_cert_policy &&
             hostname == o.hostname &&
             tls_alpn_protocols == o.tls_alpn_protocols &&
             tls_elliptic_curves == o.tls_elliptic_curves;
    }
    bool operator!=(const IceServer& o) const { return !(*this == o); }
  };
  typedef std::vector<IceServer> IceServers;

  enum IceTransportsType { kNone, kRelay, kNoHost, kAll };

  struct RTCConfiguration {
    RTCConfiguration() = default;
    ~RTCConfiguration() = default;

    IceServers servers;
    IceTransportsType type = kAll;
    SdpSemantics sdp_semantics = SdpSemantics::kUnifiedPlan;

    void set_min_port(int port) { min_port_ = port; }
    int min_port() const { return min_port_; }
    void set_max_port(int port) { max_port_ = port; }
    int max_port() const { return max_port_; }

   private:
    int min_port_ = 0;
    int max_port_ = 0;
  };

  struct RTCOfferAnswerOptions {
    static const int kUndefined = -1;
    static const int kMaxOfferToReceiveMedia = 1;
    static const int kOfferToReceiveMediaTrue = 1;

    int offer_to_receive_video = kUndefined;
    int offer_to_receive_audio = kUndefined;
    bool voice_activity_detection = true;
    bool ice_restart = false;
    bool use_rtp_mux = true;
    bool raw_packetization_for_video = false;
    int num_simulcast_layers = 1;
    bool use_obsolete_sctp_sdp = false;

    RTCOfferAnswerOptions() = default;
    RTCOfferAnswerOptions(int offer_to_receive_video,
                          int offer_to_receive_audio,
                          bool voice_activity_detection,
                          bool ice_restart,
                          bool use_rtp_mux)
        : offer_to_receive_video(offer_to_receive_video),
          offer_to_receive_audio(offer_to_receive_audio),
          voice_activity_detection(voice_activity_detection),
          ice_restart(ice_restart),
          use_rtp_mux(use_rtp_mux) {}
  };

  virtual rtc::scoped_refptr<StreamCollectionInterface> local_streams() = 0;
  virtual rtc::scoped_refptr<StreamCollectionInterface> remote_streams() = 0;
  virtual bool AddStream(MediaStreamInterface* stream) = 0;
  virtual void RemoveStream(MediaStreamInterface* stream) = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) = 0;

  virtual RTCError RemoveTrackOrError(
      rtc::scoped_refptr<RtpSenderInterface> sender) = 0;

  virtual std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const = 0;
  virtual std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const = 0;
  virtual std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
  GetTransceivers() const = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<DataChannelInterface>>
  CreateDataChannelOrError(const std::string& label,
                           const DataChannelInit* config) = 0;

  virtual const SessionDescriptionInterface* local_description() const = 0;
  virtual const SessionDescriptionInterface* remote_description() const = 0;
  virtual const SessionDescriptionInterface* current_local_description() const {
    return local_description();
  }
  virtual const SessionDescriptionInterface* current_remote_description()
      const {
    return remote_description();
  }
  virtual const SessionDescriptionInterface* pending_local_description() const {
    return local_description();
  }
  virtual const SessionDescriptionInterface* pending_remote_description()
      const {
    return remote_description();
  }

  virtual void RestartIce() {}

  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const RTCOfferAnswerOptions& options) = 0;
  virtual void CreateAnswer(CreateSessionDescriptionObserver* observer,
                            const RTCOfferAnswerOptions& options) = 0;

  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer,
                                   SessionDescriptionInterface* desc) = 0;
  virtual void SetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) = 0;
  virtual void SetLocalDescription(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) = 0;

  virtual void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                                    SessionDescriptionInterface* desc) = 0;
  virtual void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer) = 0;

  virtual bool AddIceCandidate(const IceCandidateInterface* candidate) = 0;
  virtual void AddIceCandidate(std::unique_ptr<IceCandidateInterface> candidate,
                               std::function<void(RTCError)> callback) = 0;
  virtual bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>& candidates) {
    (void)candidates;
    return true;
  }

  virtual RTCError SetBitrate(const BitrateSettings& bitrate) = 0;

  virtual SignalingState signaling_state() = 0;
  virtual IceConnectionState ice_connection_state() = 0;
  virtual IceConnectionState standardized_ice_connection_state() = 0;
  virtual PeerConnectionState peer_connection_state() = 0;
  virtual IceGatheringState ice_gathering_state() = 0;

  virtual void Close() = 0;

 protected:
  ~PeerConnectionInterface() override = default;
};

class PeerConnectionObserver {
 public:
  virtual ~PeerConnectionObserver() = default;

  virtual void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) = 0;
  virtual void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) {
    (void)stream;
  }
  virtual void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) {
    (void)stream;
  }
  virtual void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) = 0;
  virtual void OnRenegotiationNeeded() {}
  virtual void OnNegotiationNeededEvent(uint32_t event_id) { (void)event_id; }
  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {
    (void)new_state;
  }
  virtual void OnStandardizedIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {
    (void)new_state;
  }
  virtual void OnConnectionChange(
      PeerConnectionInterface::PeerConnectionState new_state) {
    (void)new_state;
  }
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) = 0;
  virtual void OnIceCandidate(const IceCandidateInterface* candidate) = 0;
  virtual void OnIceCandidateError(const std::string& address,
                                   int port,
                                   const std::string& url,
                                   int error_code,
                                   const std::string& error_text) {
    (void)address;
    (void)port;
    (void)url;
    (void)error_code;
    (void)error_text;
  }
  virtual void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) {
    (void)candidates;
  }
  virtual void OnIceConnectionReceivingChange(bool receiving) {
    (void)receiving;
  }
  virtual void OnAddTrack(
      rtc::scoped_refptr<RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
    (void)receiver;
    (void)streams;
  }
  virtual void OnTrack(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver) {
    (void)transceiver;
  }
  virtual void OnRemoveTrack(
      rtc::scoped_refptr<RtpReceiverInterface> receiver) {
    (void)receiver;
  }
  virtual void OnInterestingUsage(int usage_pattern) { (void)usage_pattern; }
};

class PeerConnectionFactoryInterface : public rtc::RefCountInterface {
 public:
  class Options {
   public:
    Options() = default;
    bool disable_encryption = false;
    bool disable_network_monitor = false;
    int network_ignore_mask = 0;
  };

  virtual void SetOptions(const Options& options) = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<PeerConnectionInterface>>
  CreatePeerConnectionOrError(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies) = 0;

  virtual rtc::scoped_refptr<MediaStreamInterface> CreateLocalMediaStream(
      const std::string& stream_id) = 0;

  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) = 0;

  virtual rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& label, VideoTrackSourceInterface* source) = 0;

  virtual rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      rtc::scoped_refptr<VideoTrackSourceInterface> source,
      absl::string_view label) {
    return CreateVideoTrack(std::string(label), source.get());
  }

  virtual rtc::scoped_refptr<AudioTrackInterface> CreateAudioTrack(
      const std::string& label, AudioSourceInterface* source) = 0;

  virtual void StopAecDump() {}

 protected:
  ~PeerConnectionFactoryInterface() override = default;
};

}  // namespace webrtc
