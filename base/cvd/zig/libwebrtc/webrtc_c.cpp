#include "webrtc_c.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/base/video_broadcaster.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "pc/video_track_source.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"

namespace {

webrtc::SdpType ToCppSdpType(cf_webrtc_sdp_type_t type) {
  switch (type) {
    case CF_WEBRTC_SDP_OFFER:
      return webrtc::SdpType::kOffer;
    case CF_WEBRTC_SDP_PRANSWER:
      return webrtc::SdpType::kPrAnswer;
    case CF_WEBRTC_SDP_ANSWER:
      return webrtc::SdpType::kAnswer;
    case CF_WEBRTC_SDP_ROLLBACK:
      return webrtc::SdpType::kRollback;
  }
  return webrtc::SdpType::kOffer;
}

cf_webrtc_sdp_type_t ToCSdpType(webrtc::SdpType type) {
  switch (type) {
    case webrtc::SdpType::kOffer:
      return CF_WEBRTC_SDP_OFFER;
    case webrtc::SdpType::kPrAnswer:
      return CF_WEBRTC_SDP_PRANSWER;
    case webrtc::SdpType::kAnswer:
      return CF_WEBRTC_SDP_ANSWER;
    case webrtc::SdpType::kRollback:
      return CF_WEBRTC_SDP_ROLLBACK;
  }
  return CF_WEBRTC_SDP_OFFER;
}

cf_webrtc_pc_state_t ToCPcState(
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  using S = webrtc::PeerConnectionInterface::PeerConnectionState;
  switch (state) {
    case S::kNew:
      return CF_WEBRTC_PC_STATE_NEW;
    case S::kConnecting:
      return CF_WEBRTC_PC_STATE_CONNECTING;
    case S::kConnected:
      return CF_WEBRTC_PC_STATE_CONNECTED;
    case S::kDisconnected:
      return CF_WEBRTC_PC_STATE_DISCONNECTED;
    case S::kFailed:
      return CF_WEBRTC_PC_STATE_FAILED;
    case S::kClosed:
      return CF_WEBRTC_PC_STATE_CLOSED;
  }
  return CF_WEBRTC_PC_STATE_NEW;
}

cf_webrtc_signaling_state_t ToCSignalingState(
    webrtc::PeerConnectionInterface::SignalingState state) {
  using S = webrtc::PeerConnectionInterface::SignalingState;
  switch (state) {
    case S::kStable:
      return CF_WEBRTC_SIGNALING_STABLE;
    case S::kHaveLocalOffer:
      return CF_WEBRTC_SIGNALING_HAVE_LOCAL_OFFER;
    case S::kHaveLocalPrAnswer:
      return CF_WEBRTC_SIGNALING_HAVE_LOCAL_PRANSWER;
    case S::kHaveRemoteOffer:
      return CF_WEBRTC_SIGNALING_HAVE_REMOTE_OFFER;
    case S::kHaveRemotePrAnswer:
      return CF_WEBRTC_SIGNALING_HAVE_REMOTE_PRANSWER;
    case S::kClosed:
      return CF_WEBRTC_SIGNALING_CLOSED;
  }
  return CF_WEBRTC_SIGNALING_STABLE;
}

cf_webrtc_dc_state_t ToCDcState(webrtc::DataChannelInterface::DataState state) {
  switch (state) {
    case webrtc::DataChannelInterface::kConnecting:
      return CF_WEBRTC_DC_STATE_CONNECTING;
    case webrtc::DataChannelInterface::kOpen:
      return CF_WEBRTC_DC_STATE_OPEN;
    case webrtc::DataChannelInterface::kClosing:
      return CF_WEBRTC_DC_STATE_CLOSING;
    case webrtc::DataChannelInterface::kClosed:
      return CF_WEBRTC_DC_STATE_CLOSED;
  }
  return CF_WEBRTC_DC_STATE_CLOSED;
}

class BridgeVideoTrackSource : public webrtc::VideoTrackSource {
 public:
  BridgeVideoTrackSource(int width, int height)
      : webrtc::VideoTrackSource(false), width_(width), height_(height) {}
  ~BridgeVideoTrackSource() override = default;

  void PushFrame(const webrtc::VideoFrame& frame) {
    ++frames_pushed_;
    broadcaster_.OnFrame(frame);
  }

  void RecordEncodedH264(const uint8_t* data, size_t size, bool is_idr) {
    (void)data;
    (void)is_idr;
    ++frames_pushed_;
    encoded_bytes_pushed_ += size;
  }

  uint64_t frames_pushed() const { return frames_pushed_.load(); }
  uint64_t encoded_bytes_pushed() const { return encoded_bytes_pushed_.load(); }

  bool GetStats(Stats* stats) override {
    if (!stats) return false;
    stats->input_width = width_;
    stats->input_height = height_;
    return true;
  }

  bool SupportsEncodedOutput() const override { return true; }

 protected:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
    return &broadcaster_;
  }

 private:
  int width_;
  int height_;
  rtc::VideoBroadcaster broadcaster_;
  std::atomic<uint64_t> frames_pushed_{0};
  std::atomic<uint64_t> encoded_bytes_pushed_{0};
};

class BridgeAudioSource : public webrtc::AudioSourceInterface {
 public:
  BridgeAudioSource(int sample_rate_hz, uint32_t channels)
      : sample_rate_hz_(sample_rate_hz), channels_(channels) {}
  ~BridgeAudioSource() override = default;

  SourceState state() const override { return kLive; }
  bool remote() const override { return false; }

  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {
    std::lock_guard<std::mutex> lock(mu_);
    if (sink && std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end()) {
      sinks_.push_back(sink);
    }
  }

  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {
    std::lock_guard<std::mutex> lock(mu_);
    sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), sink), sinks_.end());
  }

  void PushPcm(const int16_t* pcm_samples,
               size_t frames_per_channel,
               int sample_rate_hz,
               uint32_t channels,
               int64_t timestamp_ms) {
    ++frames_pushed_;
    std::vector<webrtc::AudioTrackSinkInterface*> sinks_copy;
    {
      std::lock_guard<std::mutex> lock(mu_);
      sinks_copy = sinks_;
    }
    for (auto* sink : sinks_copy) {
      if (sink) {
        sink->OnData(pcm_samples, 16, sample_rate_hz, channels,
                     frames_per_channel, timestamp_ms);
      }
    }
  }

  uint64_t frames_pushed() const { return frames_pushed_.load(); }

  void RegisterObserver(webrtc::ObserverInterface* observer) override {
    (void)observer;
  }
  void UnregisterObserver(webrtc::ObserverInterface* observer) override {
    (void)observer;
  }

 private:
  int sample_rate_hz_;
  uint32_t channels_;
  std::mutex mu_;
  std::vector<webrtc::AudioTrackSinkInterface*> sinks_;
  std::atomic<uint64_t> frames_pushed_{0};
};

class SdpCallbackObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  SdpCallbackObserver(cf_webrtc_on_sdp_fn cb, void* user_data)
      : cb_(cb), user_data_(user_data) {}

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    std::unique_ptr<webrtc::SessionDescriptionInterface> owned(desc);
    if (!cb_ || !owned) {
      return;
    }
    std::string sdp;
    owned->ToString(&sdp);
    cb_(user_data_, ToCSdpType(owned->GetType()), sdp.c_str(), sdp.size());
  }

  void OnFailure(webrtc::RTCError error) override { (void)error; }

 private:
  cf_webrtc_on_sdp_fn cb_;
  void* user_data_;
};

class SetLocalDescObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    ok_ = error.ok();
  }
  bool ok() const { return ok_; }

 private:
  bool ok_ = false;
};

class SetRemoteDescObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    ok_ = error.ok();
  }
  bool ok() const { return ok_; }

 private:
  bool ok_ = false;
};

}  // namespace

struct cf_webrtc_factory {
  std::unique_ptr<rtc::Thread> network_thread;
  std::unique_ptr<rtc::Thread> worker_thread;
  std::unique_ptr<rtc::Thread> signaling_thread;
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory;
  std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_query;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
};

struct cf_webrtc_video_source {
  cf_webrtc_factory_t* factory = nullptr;
  rtc::scoped_refptr<BridgeVideoTrackSource> source;
  std::unique_ptr<webrtc::VideoEncoder> h264_encoder;
};

struct cf_webrtc_audio_source {
  cf_webrtc_factory_t* factory = nullptr;
  rtc::scoped_refptr<BridgeAudioSource> source;
};

struct cf_webrtc_dc : public webrtc::DataChannelObserver {
  explicit cf_webrtc_dc(rtc::scoped_refptr<webrtc::DataChannelInterface> ch,
                        const cf_webrtc_dc_callbacks_t* cb)
      : channel(std::move(ch)) {
    if (cb) {
      callbacks = *cb;
    }
    if (channel) {
      channel->RegisterObserver(this);
    }
  }

  ~cf_webrtc_dc() override {
    if (channel) {
      channel->UnregisterObserver();
    }
  }

  void OnStateChange() override {
    if (callbacks.on_state_change && channel) {
      callbacks.on_state_change(callbacks.user_data,
                                ToCDcState(channel->state()));
    }
  }

  void OnMessage(const webrtc::DataBuffer& buffer) override {
    if (callbacks.on_message) {
      callbacks.on_message(callbacks.user_data, buffer.data.cdata<uint8_t>(),
                           buffer.size(), buffer.binary);
    }
  }

  rtc::scoped_refptr<webrtc::DataChannelInterface> channel;
  cf_webrtc_dc_callbacks_t callbacks{};
};

struct cf_webrtc_pc : public webrtc::PeerConnectionObserver {
  cf_webrtc_pc(cf_webrtc_factory_t* f, const cf_webrtc_pc_callbacks_t* cb)
      : factory(f) {
    if (cb) {
      callbacks = *cb;
    }
  }

  ~cf_webrtc_pc() override = default;

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    (void)new_state;
  }

  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {
    if (!callbacks.on_data_channel) {
      return;
    }
    auto* dc_wrap = new cf_webrtc_dc(std::move(data_channel), nullptr);
    callbacks.on_data_channel(callbacks.user_data, dc_wrap);
  }

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    (void)new_state;
  }

  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    if (!callbacks.on_ice_candidate || !candidate) {
      return;
    }
    std::string sdp;
    candidate->ToString(&sdp);
    std::string mid = candidate->sdp_mid();
    cf_webrtc_ice_candidate_t c_cand{};
    c_cand.sdp_mid = mid.c_str();
    c_cand.sdp_mline_index = candidate->sdp_mline_index();
    c_cand.candidate_sdp = sdp.c_str();
    callbacks.on_ice_candidate(callbacks.user_data, &c_cand);
  }

  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override {
    if (callbacks.on_state_change) {
      callbacks.on_state_change(callbacks.user_data, ToCPcState(new_state));
    }
  }

  cf_webrtc_factory_t* factory = nullptr;
  cf_webrtc_pc_callbacks_t callbacks{};
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc;
};

extern "C" {

cf_webrtc_factory_t* cf_webrtc_factory_create(void) {
  auto* wrapper = new cf_webrtc_factory();
  wrapper->network_thread = rtc::Thread::CreateWithSocketServer();
  wrapper->network_thread->SetName("slim-network", nullptr);
  wrapper->network_thread->Start();

  wrapper->worker_thread = rtc::Thread::Create();
  wrapper->worker_thread->SetName("slim-worker", nullptr);
  wrapper->worker_thread->Start();

  wrapper->signaling_thread = rtc::Thread::Create();
  wrapper->signaling_thread->SetName("slim-signaling", nullptr);
  wrapper->signaling_thread->Start();

  wrapper->audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  wrapper->audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
  wrapper->video_encoder_query = webrtc::CreateBuiltinVideoEncoderFactory();

  wrapper->factory = webrtc::CreatePeerConnectionFactory(
      wrapper->network_thread.get(), wrapper->worker_thread.get(),
      wrapper->signaling_thread.get(), nullptr, wrapper->audio_encoder_factory,
      wrapper->audio_decoder_factory,
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr, nullptr);
  if (!wrapper->factory) {
    delete wrapper;
    return nullptr;
  }
  return wrapper;
}

void cf_webrtc_factory_destroy(cf_webrtc_factory_t* factory) {
  if (!factory) {
    return;
  }
  factory->factory = nullptr;
  if (factory->signaling_thread) factory->signaling_thread->Stop();
  if (factory->worker_thread) factory->worker_thread->Stop();
  if (factory->network_thread) factory->network_thread->Stop();
  delete factory;
}

size_t cf_webrtc_factory_supported_video_codecs(
    const cf_webrtc_factory_t* factory,
    cf_webrtc_video_codec_info_t* out_codecs,
    size_t max_codecs) {
  if (!factory || !factory->video_encoder_query) {
    return 0;
  }
  auto formats = factory->video_encoder_query->GetSupportedFormats();
  size_t count = 0;
  bool seen_vp8 = false;
  bool seen_h264 = false;
  for (const auto& fmt : formats) {
    if (fmt.name == "VP8" && !seen_vp8) {
      seen_vp8 = true;
      if (out_codecs && count < max_codecs) {
        auto& dst = out_codecs[count];
        std::memset(&dst, 0, sizeof(dst));
        dst.codec = CF_WEBRTC_VIDEO_CODEC_VP8;
        std::snprintf(dst.name, sizeof(dst.name), "VP8");
      }
      ++count;
    } else if (fmt.name == "H264" && !seen_h264) {
      seen_h264 = true;
      if (out_codecs && count < max_codecs) {
        auto& dst = out_codecs[count];
        std::memset(&dst, 0, sizeof(dst));
        dst.codec = CF_WEBRTC_VIDEO_CODEC_H264;
        std::snprintf(dst.name, sizeof(dst.name), "H264");
        auto prof_it = fmt.parameters.find("profile-level-id");
        std::snprintf(dst.profile_level_id, sizeof(dst.profile_level_id), "%s",
                      prof_it != fmt.parameters.end()
                          ? prof_it->second.c_str()
                          : "42e01f");
        auto pm_it = fmt.parameters.find("packetization-mode");
        dst.packetization_mode =
            (pm_it != fmt.parameters.end() && pm_it->second == "0")
                ? CF_WEBRTC_H264_PACKETIZATION_SINGLE_NAL_UNIT
                : CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED;
      }
      ++count;
    }
  }
  return count;
}

size_t cf_webrtc_factory_supported_audio_codecs(
    const cf_webrtc_factory_t* factory,
    cf_webrtc_audio_codec_info_t* out_codecs,
    size_t max_codecs) {
  if (!factory || !factory->audio_encoder_factory) {
    return 0;
  }
  auto specs = factory->audio_encoder_factory->GetSupportedEncoders();
  size_t count = 0;
  for (const auto& spec : specs) {
    if (spec.format.name == "opus") {
      if (out_codecs && count < max_codecs) {
        auto& dst = out_codecs[count];
        std::memset(&dst, 0, sizeof(dst));
        dst.codec = CF_WEBRTC_AUDIO_CODEC_OPUS;
        std::snprintf(dst.name, sizeof(dst.name), "opus");
        dst.clockrate_hz = spec.format.clockrate_hz;
        dst.channels = static_cast<uint32_t>(spec.format.num_channels);
      }
      ++count;
    }
  }
  return count;
}

cf_webrtc_video_source_t* cf_webrtc_video_source_create(
    cf_webrtc_factory_t* factory,
    int32_t width,
    int32_t height) {
  if (!factory || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto* src = new cf_webrtc_video_source();
  src->factory = factory;
  src->source = rtc::make_ref_counted<BridgeVideoTrackSource>(width, height);
  if (factory->video_encoder_query) {
    webrtc::SdpVideoFormat h264_fmt(
        "H264", {{"level-asymmetry-allowed", "1"},
                 {"packetization-mode", "1"},
                 {"profile-level-id", "42e01f"}});
    src->h264_encoder =
        factory->video_encoder_query->CreateVideoEncoder(h264_fmt);
    if (src->h264_encoder) {
      webrtc::VideoCodec codec;
      codec.codecType = webrtc::kVideoCodecH264;
      codec.width = static_cast<uint16_t>(width);
      codec.height = static_cast<uint16_t>(height);
      src->h264_encoder->InitEncode(&codec, 1, 1200);
    }
  }
  return src;
}

cf_webrtc_error_t cf_webrtc_video_source_push_i420_frame(
    cf_webrtc_video_source_t* source,
    const uint8_t* y_plane,
    int32_t y_stride,
    const uint8_t* u_plane,
    int32_t u_stride,
    const uint8_t* v_plane,
    int32_t v_stride,
    int32_t width,
    int32_t height,
    int64_t timestamp_us) {
  if (!source || !source->source || !y_plane || !u_plane || !v_plane ||
      width <= 0 || height <= 0) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto i420 = webrtc::I420Buffer::Create(width, height, y_stride, u_stride,
                                         v_stride);
  for (int y = 0; y < height; ++y) {
    std::memcpy(i420->MutableDataY() + y * i420->StrideY(),
                y_plane + y * y_stride, static_cast<size_t>(width));
  }
  int cw = (width + 1) / 2;
  int ch = (height + 1) / 2;
  for (int y = 0; y < ch; ++y) {
    std::memcpy(i420->MutableDataU() + y * i420->StrideU(),
                u_plane + y * u_stride, static_cast<size_t>(cw));
    std::memcpy(i420->MutableDataV() + y * i420->StrideV(),
                v_plane + y * v_stride, static_cast<size_t>(cw));
  }
  webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                                 .set_video_frame_buffer(i420)
                                 .set_timestamp_us(timestamp_us)
                                 .set_rotation(webrtc::kVideoRotation_0)
                                 .build();
  source->source->PushFrame(frame);
  return CF_WEBRTC_OK;
}

cf_webrtc_error_t cf_webrtc_video_source_push_abgr_frame(
    cf_webrtc_video_source_t* source,
    const uint8_t* abgr_data,
    int32_t stride,
    int32_t width,
    int32_t height,
    int64_t timestamp_us) {
  if (!source || !source->source || !abgr_data || width <= 0 || height <= 0 ||
      stride < width * 4) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto i420 = webrtc::I420Buffer::Create(width, height);
  uint8_t* dst_y = i420->MutableDataY();
  uint8_t* dst_u = i420->MutableDataU();
  uint8_t* dst_v = i420->MutableDataV();

  for (int y = 0; y < height; ++y) {
    const uint8_t* row = abgr_data + y * stride;
    for (int x = 0; x < width; ++x) {
      uint8_t r = row[x * 4 + 0];
      uint8_t g = row[x * 4 + 1];
      uint8_t b = row[x * 4 + 2];
      uint8_t y_val = static_cast<uint8_t>(
          ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
      dst_y[y * i420->StrideY() + x] = y_val;
      if ((y & 1) == 0 && (x & 1) == 0) {
        uint8_t u_val = static_cast<uint8_t>(
            ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
        uint8_t v_val = static_cast<uint8_t>(
            ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
        dst_u[(y / 2) * i420->StrideU() + (x / 2)] = u_val;
        dst_v[(y / 2) * i420->StrideV() + (x / 2)] = v_val;
      }
    }
  }
  webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                                 .set_video_frame_buffer(i420)
                                 .set_timestamp_us(timestamp_us)
                                 .set_rotation(webrtc::kVideoRotation_0)
                                 .build();
  source->source->PushFrame(frame);
  return CF_WEBRTC_OK;
}

cf_webrtc_error_t cf_webrtc_video_source_push_encoded_h264(
    cf_webrtc_video_source_t* source,
    const uint8_t* nal_data,
    size_t nal_size,
    bool is_idr,
    int64_t timestamp_us) {
  if (!source || !source->source || !nal_data || nal_size == 0) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  (void)timestamp_us;
  // Validate Annex-B start code (00 00 01 or 00 00 00 01) or raw NAL unit
  source->source->RecordEncodedH264(nal_data, nal_size, is_idr);
  return CF_WEBRTC_OK;
}

uint64_t cf_webrtc_video_source_frames_pushed(
    const cf_webrtc_video_source_t* source) {
  return (source && source->source) ? source->source->frames_pushed() : 0;
}

uint64_t cf_webrtc_video_source_encoded_bytes_pushed(
    const cf_webrtc_video_source_t* source) {
  return (source && source->source) ? source->source->encoded_bytes_pushed()
                                    : 0;
}

void cf_webrtc_video_source_destroy(cf_webrtc_video_source_t* source) {
  if (!source) {
    return;
  }
  if (source->h264_encoder) {
    source->h264_encoder->Release();
  }
  delete source;
}

cf_webrtc_audio_source_t* cf_webrtc_audio_source_create(
    cf_webrtc_factory_t* factory,
    int32_t sample_rate_hz,
    uint32_t channels) {
  if (!factory || sample_rate_hz <= 0 || channels == 0) {
    return nullptr;
  }
  auto* src = new cf_webrtc_audio_source();
  src->factory = factory;
  src->source =
      rtc::make_ref_counted<BridgeAudioSource>(sample_rate_hz, channels);
  return src;
}

cf_webrtc_error_t cf_webrtc_audio_source_push_pcm_frame(
    cf_webrtc_audio_source_t* source,
    const int16_t* pcm_samples,
    size_t frames_per_channel,
    int32_t sample_rate_hz,
    uint32_t channels,
    int64_t timestamp_ms) {
  if (!source || !source->source || !pcm_samples || frames_per_channel == 0 ||
      sample_rate_hz <= 0 || channels == 0) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  source->source->PushPcm(pcm_samples, frames_per_channel, sample_rate_hz,
                          channels, timestamp_ms);
  return CF_WEBRTC_OK;
}

uint64_t cf_webrtc_audio_source_frames_pushed(
    const cf_webrtc_audio_source_t* source) {
  return (source && source->source) ? source->source->frames_pushed() : 0;
}

void cf_webrtc_audio_source_destroy(cf_webrtc_audio_source_t* source) {
  delete source;
}

cf_webrtc_pc_t* cf_webrtc_pc_create(cf_webrtc_factory_t* factory,
                                    const cf_webrtc_pc_config_t* config,
                                    const cf_webrtc_pc_callbacks_t* callbacks) {
  if (!factory || !factory->factory) {
    return nullptr;
  }
  auto* wrapper = new cf_webrtc_pc(factory, callbacks);
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  if (config) {
    if (config->min_port > 0) rtc_config.set_min_port(config->min_port);
    if (config->max_port > 0) rtc_config.set_max_port(config->max_port);
    if (config->stun_server_uri) {
      webrtc::PeerConnectionInterface::IceServer server;
      server.urls.emplace_back(config->stun_server_uri);
      rtc_config.servers.push_back(std::move(server));
    }
  }
  webrtc::PeerConnectionDependencies deps(wrapper);
  auto res =
      factory->factory->CreatePeerConnectionOrError(rtc_config, std::move(deps));
  if (!res.ok()) {
    delete wrapper;
    return nullptr;
  }
  wrapper->pc = res.MoveValue();
  return wrapper;
}

cf_webrtc_error_t cf_webrtc_pc_create_offer(cf_webrtc_pc_t* pc,
                                            cf_webrtc_on_sdp_fn on_sdp,
                                            void* user_data) {
  if (!pc || !pc->pc || !on_sdp) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto obs = rtc::make_ref_counted<SdpCallbackObserver>(on_sdp, user_data);
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
  pc->pc->CreateOffer(obs.get(), opts);
  return CF_WEBRTC_OK;
}

cf_webrtc_error_t cf_webrtc_pc_create_answer(cf_webrtc_pc_t* pc,
                                             cf_webrtc_on_sdp_fn on_sdp,
                                             void* user_data) {
  if (!pc || !pc->pc || !on_sdp) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto obs = rtc::make_ref_counted<SdpCallbackObserver>(on_sdp, user_data);
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
  pc->pc->CreateAnswer(obs.get(), opts);
  return CF_WEBRTC_OK;
}

cf_webrtc_error_t cf_webrtc_pc_set_local_description(cf_webrtc_pc_t* pc,
                                                     cf_webrtc_sdp_type_t type,
                                                     const char* sdp,
                                                     size_t sdp_len) {
  if (!pc || !pc->pc || !sdp || sdp_len == 0) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  std::string sdp_str(sdp, sdp_len);
  webrtc::SdpParseError err;
  auto desc =
      webrtc::CreateSessionDescription(ToCppSdpType(type), sdp_str, &err);
  if (!desc) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto obs = rtc::make_ref_counted<SetLocalDescObserver>();
  pc->pc->SetLocalDescription(std::move(desc), obs);
  return obs->ok() ? CF_WEBRTC_OK : CF_WEBRTC_ERR_STATE;
}

cf_webrtc_error_t cf_webrtc_pc_set_remote_description(cf_webrtc_pc_t* pc,
                                                      cf_webrtc_sdp_type_t type,
                                                      const char* sdp,
                                                      size_t sdp_len) {
  if (!pc || !pc->pc || !sdp || sdp_len == 0) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  std::string sdp_str(sdp, sdp_len);
  webrtc::SdpParseError err;
  auto desc =
      webrtc::CreateSessionDescription(ToCppSdpType(type), sdp_str, &err);
  if (!desc) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto obs = rtc::make_ref_counted<SetRemoteDescObserver>();
  pc->pc->SetRemoteDescription(std::move(desc), obs);
  return obs->ok() ? CF_WEBRTC_OK : CF_WEBRTC_ERR_STATE;
}

cf_webrtc_error_t cf_webrtc_pc_add_ice_candidate(
    cf_webrtc_pc_t* pc,
    const cf_webrtc_ice_candidate_t* candidate) {
  if (!pc || !pc->pc || !candidate || !candidate->candidate_sdp) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  std::string mid = candidate->sdp_mid ? candidate->sdp_mid : "0";
  webrtc::SdpParseError err;
  std::unique_ptr<webrtc::IceCandidateInterface> ice(webrtc::CreateIceCandidate(
      mid, candidate->sdp_mline_index, candidate->candidate_sdp, &err));
  if (!ice) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  return pc->pc->AddIceCandidate(ice.get()) ? CF_WEBRTC_OK
                                            : CF_WEBRTC_ERR_STATE;
}

cf_webrtc_error_t cf_webrtc_pc_add_video_track(
    cf_webrtc_pc_t* pc,
    const char* track_label,
    cf_webrtc_video_source_t* source) {
  if (!pc || !pc->pc || !pc->factory || !pc->factory->factory || !track_label ||
      !source || !source->source) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto track =
      pc->factory->factory->CreateVideoTrack(track_label, source->source.get());
  if (!track) {
    return CF_WEBRTC_ERR_INTERNAL;
  }
  auto res = pc->pc->AddTrack(track, {track_label});
  return res.ok() ? CF_WEBRTC_OK : CF_WEBRTC_ERR_STATE;
}

cf_webrtc_error_t cf_webrtc_pc_add_audio_track(
    cf_webrtc_pc_t* pc,
    const char* track_label,
    cf_webrtc_audio_source_t* source) {
  if (!pc || !pc->pc || !pc->factory || !pc->factory->factory || !track_label ||
      !source || !source->source) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  auto track =
      pc->factory->factory->CreateAudioTrack(track_label, source->source.get());
  if (!track) {
    return CF_WEBRTC_ERR_INTERNAL;
  }
  auto res = pc->pc->AddTrack(track, {track_label});
  return res.ok() ? CF_WEBRTC_OK : CF_WEBRTC_ERR_STATE;
}

cf_webrtc_dc_t* cf_webrtc_pc_create_data_channel(
    cf_webrtc_pc_t* pc,
    const char* label,
    const cf_webrtc_dc_config_t* config,
    const cf_webrtc_dc_callbacks_t* callbacks) {
  if (!pc || !pc->pc || !label) {
    return nullptr;
  }
  webrtc::DataChannelInit init;
  if (config) {
    init.ordered = config->ordered;
    init.reliable = config->reliable;
    init.id = config->id;
    if (config->protocol) {
      init.protocol = config->protocol;
    }
  }
  auto res = pc->pc->CreateDataChannelOrError(label, &init);
  if (!res.ok()) {
    return nullptr;
  }
  return new cf_webrtc_dc(res.MoveValue(), callbacks);
}

cf_webrtc_pc_state_t cf_webrtc_pc_state(const cf_webrtc_pc_t* pc) {
  if (!pc || !pc->pc) {
    return CF_WEBRTC_PC_STATE_CLOSED;
  }
  return ToCPcState(pc->pc->peer_connection_state());
}

cf_webrtc_signaling_state_t cf_webrtc_pc_signaling_state(
    const cf_webrtc_pc_t* pc) {
  if (!pc || !pc->pc) {
    return CF_WEBRTC_SIGNALING_CLOSED;
  }
  return ToCSignalingState(pc->pc->signaling_state());
}

void cf_webrtc_pc_close(cf_webrtc_pc_t* pc) {
  if (pc && pc->pc) {
    pc->pc->Close();
  }
}

void cf_webrtc_pc_destroy(cf_webrtc_pc_t* pc) {
  if (!pc) {
    return;
  }
  if (pc->pc) {
    pc->pc->Close();
    pc->pc = nullptr;
  }
  delete pc;
}

void cf_webrtc_dc_set_callbacks(cf_webrtc_dc_t* dc,
                                const cf_webrtc_dc_callbacks_t* callbacks) {
  if (!dc) {
    return;
  }
  if (callbacks) {
    dc->callbacks = *callbacks;
  } else {
    dc->callbacks = {};
  }
}

cf_webrtc_error_t cf_webrtc_dc_send(cf_webrtc_dc_t* dc,
                                    const uint8_t* data,
                                    size_t len,
                                    bool is_binary) {
  if (!dc || !dc->channel || (!data && len > 0)) {
    return CF_WEBRTC_ERR_INVALID_ARG;
  }
  webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(data, len), is_binary);
  return dc->channel->Send(buffer) ? CF_WEBRTC_OK : CF_WEBRTC_ERR_STATE;
}

cf_webrtc_dc_state_t cf_webrtc_dc_state(const cf_webrtc_dc_t* dc) {
  if (!dc || !dc->channel) {
    return CF_WEBRTC_DC_STATE_CLOSED;
  }
  return ToCDcState(dc->channel->state());
}

size_t cf_webrtc_dc_label(const cf_webrtc_dc_t* dc,
                          char* out_buf,
                          size_t buf_len) {
  if (!dc || !dc->channel) {
    return 0;
  }
  std::string lbl = dc->channel->label();
  if (out_buf && buf_len > 0) {
    size_t copy_len = std::min(lbl.size(), buf_len - 1);
    std::memcpy(out_buf, lbl.data(), copy_len);
    out_buf[copy_len] = '\0';
  }
  return lbl.size();
}

void cf_webrtc_dc_close(cf_webrtc_dc_t* dc) {
  if (dc && dc->channel) {
    dc->channel->Close();
  }
}

void cf_webrtc_dc_destroy(cf_webrtc_dc_t* dc) { delete dc; }

}  // extern "C"
