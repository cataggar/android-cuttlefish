#pragma once

#include "api/media_stream_interface.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"

namespace webrtc {

class VideoTrackSource : public VideoTrackSourceInterface {
 public:
  explicit VideoTrackSource(bool remote = false);
  ~VideoTrackSource() override = default;

  void SetState(SourceState new_state);

  SourceState state() const override { return state_; }
  bool remote() const override { return remote_; }

  bool is_screencast() const override { return false; }
  absl::optional<bool> needs_denoising() const override {
    return absl::nullopt;
  }

  bool GetStats(Stats* stats) override {
    (void)stats;
    return false;
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

  bool SupportsEncodedOutput() const override { return false; }
  void GenerateKeyFrame() override {}
  void AddEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) override {
    (void)sink;
  }
  void RemoveEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) override {
    (void)sink;
  }

  void RegisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }
  void UnregisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }

 protected:
  virtual rtc::VideoSourceInterface<VideoFrame>* source() = 0;

 private:
  SourceState state_;
  const bool remote_;
};

}  // namespace webrtc
