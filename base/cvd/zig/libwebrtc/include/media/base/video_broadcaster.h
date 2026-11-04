#pragma once

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_source_base.h"

namespace rtc {

class VideoBroadcaster : public VideoSourceBase,
                         public VideoSinkInterface<webrtc::VideoFrame> {
 public:
  VideoBroadcaster();
  ~VideoBroadcaster() override;

  void AddOrUpdateSink(VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const VideoSinkWants& wants) override;
  void RemoveSink(VideoSinkInterface<webrtc::VideoFrame>* sink) override;

  bool frame_wanted() const;
  VideoSinkWants wants() const;

  void OnFrame(const webrtc::VideoFrame& frame) override;
  void OnDiscardedFrame() override;

 private:
  void UpdateWants();
  VideoSinkWants current_wants_;
};

}  // namespace rtc
