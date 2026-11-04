#pragma once

#include <cstdint>
#include <string>

#include "api/units/data_rate.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/scalability_mode.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

enum VideoCodecComplexity {
  kComplexityNormal = 0,
  kComplexityHigh = 1,
  kComplexityHigher = 2,
  kComplexityMax = 3
};

struct VideoCodecVP8 {
  bool operator==(const VideoCodecVP8& other) const {
    return complexity == other.complexity &&
           numberOfTemporalLayers == other.numberOfTemporalLayers &&
           denoisingOn == other.denoisingOn &&
           automaticResizeOn == other.automaticResizeOn &&
           frameDroppingOn == other.frameDroppingOn &&
           keyFrameInterval == other.keyFrameInterval;
  }
  bool operator!=(const VideoCodecVP8& other) const {
    return !(*this == other);
  }
  VideoCodecComplexity complexity = kComplexityNormal;
  unsigned char numberOfTemporalLayers = 1;
  bool denoisingOn = true;
  bool automaticResizeOn = false;
  bool frameDroppingOn = true;
  int keyFrameInterval = 3000;
};

enum class InterLayerPredMode : int {
  kOff = 0,
  kOn = 1,
  kOnKeyPic = 2,
};

struct VideoCodecVP9 {
  VideoCodecComplexity complexity = kComplexityNormal;
  unsigned char numberOfTemporalLayers = 1;
  bool denoisingOn = false;
  bool frameDroppingOn = true;
  int keyFrameInterval = 3000;
  bool adaptiveQpMode = true;
  bool automaticResizeOn = false;
  unsigned char numberOfSpatialLayers = 1;
  bool flexibleMode = false;
  InterLayerPredMode interLayerPred = InterLayerPredMode::kOff;
};

struct VideoCodecH264 {
  bool operator==(const VideoCodecH264& other) const {
    return frameDroppingOn == other.frameDroppingOn &&
           keyFrameInterval == other.keyFrameInterval &&
           numberOfTemporalLayers == other.numberOfTemporalLayers;
  }
  bool operator!=(const VideoCodecH264& other) const {
    return !(*this == other);
  }
  bool frameDroppingOn = true;
  int keyFrameInterval = 3000;
  uint8_t numberOfTemporalLayers = 1;
};

struct VideoCodecAV1 {
  bool automatic_resize_on = true;
};

enum class VideoCodecMode { kRealtimeVideo, kScreensharing };

struct SpatialLayer {
  uint16_t width = 0;
  uint16_t height = 0;
  float maxFramerate = 0.0f;
  unsigned char numberOfTemporalLayers = 0;
  unsigned int maxBitrate = 0;
  unsigned int targetBitrate = 0;
  unsigned int minBitrate = 0;
  unsigned int qpMax = 0;
  bool active = false;
};

constexpr size_t kMaxSimulcastStreams = 4;
constexpr size_t kMaxSpatialLayers = 5;

class VideoCodec {
 public:
  VideoCodec();

  VideoCodecVP8* VP8() { return &codec_specific_.VP8; }
  const VideoCodecVP8& VP8() const { return codec_specific_.VP8; }
  VideoCodecVP9* VP9() { return &codec_specific_.VP9; }
  const VideoCodecVP9& VP9() const { return codec_specific_.VP9; }
  VideoCodecH264* H264() { return &codec_specific_.H264; }
  const VideoCodecH264& H264() const { return codec_specific_.H264; }
  VideoCodecAV1* AV1() { return &codec_specific_.AV1; }
  const VideoCodecAV1& AV1() const { return codec_specific_.AV1; }

  VideoCodecType codecType = kVideoCodecGeneric;
  uint16_t width = 0;
  uint16_t height = 0;
  unsigned int startBitrate = 0;
  unsigned int maxBitrate = 0;
  unsigned int minBitrate = 0;
  uint32_t maxFramerate = 0;
  bool active = true;
  unsigned int qpMax = 0;
  unsigned char numberOfSimulcastStreams = 0;
  SpatialLayer simulcastStream[kMaxSimulcastStreams] = {};
  SpatialLayer spatialLayers[kMaxSpatialLayers] = {};
  VideoCodecMode mode = VideoCodecMode::kRealtimeVideo;
  bool expect_encode_from_texture = false;

 private:
  union VideoCodecUnion {
    VideoCodecUnion() : VP8() {}
    VideoCodecVP8 VP8;
    VideoCodecVP9 VP9;
    VideoCodecH264 H264;
    VideoCodecAV1 AV1;
  } codec_specific_;
};

}  // namespace webrtc
