#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "api/audio_options.h"
#include "api/scoped_refptr.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class ObserverInterface {
 public:
  virtual void OnChanged() = 0;

 protected:
  virtual ~ObserverInterface() = default;
};

class NotifierInterface {
 public:
  virtual void RegisterObserver(ObserverInterface* observer) = 0;
  virtual void UnregisterObserver(ObserverInterface* observer) = 0;

  virtual ~NotifierInterface() = default;
};

class MediaSourceInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  enum SourceState { kInitializing, kLive, kEnded, kMuted };

  virtual SourceState state() const = 0;
  virtual bool remote() const = 0;

 protected:
  ~MediaSourceInterface() override = default;
};

class MediaStreamTrackInterface : public rtc::RefCountInterface,
                                  public NotifierInterface {
 public:
  enum TrackState { kLive, kEnded };

  static const char* const kAudioKind;
  static const char* const kVideoKind;

  virtual std::string kind() const = 0;
  virtual std::string id() const = 0;
  virtual bool enabled() const = 0;
  virtual bool set_enabled(bool enable) = 0;
  virtual TrackState state() const = 0;

 protected:
  ~MediaStreamTrackInterface() override = default;
};

struct VideoTrackSourceConstraints {
  absl::optional<double> min_fps;
  absl::optional<double> max_fps;
};

class VideoTrackSourceInterface
    : public MediaSourceInterface,
      public rtc::VideoSourceInterface<VideoFrame> {
 public:
  struct Stats {
    int input_width = 0;
    int input_height = 0;
  };

  virtual bool is_screencast() const = 0;
  virtual absl::optional<bool> needs_denoising() const = 0;
  virtual bool GetStats(Stats* stats) = 0;
  virtual bool SupportsEncodedOutput() const = 0;
  virtual void GenerateKeyFrame() = 0;
  virtual void AddEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) = 0;
  virtual void RemoveEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) = 0;
  virtual void ProcessConstraints(
      const VideoTrackSourceConstraints& constraints) {
    (void)constraints;
  }

 protected:
  ~VideoTrackSourceInterface() override = default;
};

class VideoTrackInterface : public MediaStreamTrackInterface,
                            public rtc::VideoSourceInterface<VideoFrame> {
 public:
  enum class ContentHint { kNone, kFluid, kDetailed, kText };

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {
    if (GetSource()) {
      GetSource()->AddOrUpdateSink(sink, wants);
    }
  }
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {
    if (GetSource()) {
      GetSource()->RemoveSink(sink);
    }
  }

  virtual VideoTrackSourceInterface* GetSource() const = 0;
  virtual ContentHint content_hint() const { return ContentHint::kNone; }
  virtual void set_content_hint(ContentHint hint) { (void)hint; }

 protected:
  ~VideoTrackInterface() override = default;
};

class AudioTrackSinkInterface {
 public:
  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames) {
    (void)audio_data;
    (void)bits_per_sample;
    (void)sample_rate;
    (void)number_of_channels;
    (void)number_of_frames;
  }

  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames,
                      absl::optional<int64_t> absolute_capture_timestamp_ms) {
    (void)absolute_capture_timestamp_ms;
    OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
           number_of_frames);
  }

  virtual int NumPreferredChannels() const { return -1; }

 protected:
  virtual ~AudioTrackSinkInterface() = default;
};

class AudioSourceInterface : public MediaSourceInterface {
 public:
  class AudioObserver {
   public:
    virtual void OnSetVolume(double volume) = 0;

   protected:
    virtual ~AudioObserver() = default;
  };

  virtual void SetVolume(double volume) { (void)volume; }
  virtual void RegisterAudioObserver(AudioObserver* observer) {
    (void)observer;
  }
  virtual void UnregisterAudioObserver(AudioObserver* observer) {
    (void)observer;
  }
  virtual void AddSink(AudioTrackSinkInterface* sink) { (void)sink; }
  virtual void RemoveSink(AudioTrackSinkInterface* sink) { (void)sink; }
  virtual const cricket::AudioOptions options() const { return {}; }

 protected:
  ~AudioSourceInterface() override = default;
};

class AudioTrackInterface : public MediaStreamTrackInterface {
 public:
  virtual AudioSourceInterface* GetSource() const = 0;
  virtual void AddSink(AudioTrackSinkInterface* sink) = 0;
  virtual void RemoveSink(AudioTrackSinkInterface* sink) = 0;
  virtual bool GetSignalLevel(int* level) {
    (void)level;
    return false;
  }

 protected:
  ~AudioTrackInterface() override = default;
};

typedef std::vector<rtc::scoped_refptr<AudioTrackInterface>> AudioTrackVector;
typedef std::vector<rtc::scoped_refptr<VideoTrackInterface>> VideoTrackVector;

class MediaStreamInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  virtual std::string id() const = 0;
  virtual AudioTrackVector GetAudioTracks() = 0;
  virtual VideoTrackVector GetVideoTracks() = 0;
  virtual rtc::scoped_refptr<AudioTrackInterface> FindAudioTrack(
      const std::string& track_id) = 0;
  virtual rtc::scoped_refptr<VideoTrackInterface> FindVideoTrack(
      const std::string& track_id) = 0;
  virtual bool AddTrack(rtc::scoped_refptr<AudioTrackInterface> track) = 0;
  virtual bool AddTrack(rtc::scoped_refptr<VideoTrackInterface> track) = 0;
  virtual bool RemoveTrack(rtc::scoped_refptr<AudioTrackInterface> track) = 0;
  virtual bool RemoveTrack(rtc::scoped_refptr<VideoTrackInterface> track) = 0;

 protected:
  ~MediaStreamInterface() override = default;
};

class StreamCollectionInterface : public rtc::RefCountInterface {
 public:
  virtual size_t count() = 0;
  virtual MediaStreamInterface* at(size_t index) = 0;
  virtual MediaStreamInterface* find(const std::string& label) = 0;

 protected:
  ~StreamCollectionInterface() override = default;
};

}  // namespace webrtc
