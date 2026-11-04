#pragma once

#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/include/video_error_codes.h"

namespace webrtc {

struct CodecSpecificInfoVP9 {
  static constexpr size_t kMaxVp9NumberOfSpatialLayers = 8;
  bool first_frame_in_picture = true;
  bool inter_pic_predicted = false;
  bool flexible_mode = false;
  bool ss_data_available = false;
  bool non_ref_for_inter_layer_pred = false;
  uint8_t temporal_idx = kNoTemporalIdx;
  bool temporal_up_switch = false;
  bool inter_layer_predicted = false;
  uint8_t gof_idx = 0;
  size_t num_spatial_layers = 1;
  size_t first_active_layer = 0;
  bool spatial_layer_resolution_present = false;
  uint16_t width[kMaxVp9NumberOfSpatialLayers] = {};
  uint16_t height[kMaxVp9NumberOfSpatialLayers] = {};
};

union CodecSpecificInfoUnion {
  CodecSpecificInfoUnion() : VP8() {}
  CodecSpecificInfoVP8 VP8;
  CodecSpecificInfoVP9 VP9;
  CodecSpecificInfoH264 H264;
};

struct CodecSpecificInfo {
  CodecSpecificInfo() : codecType(kVideoCodecGeneric), codecSpecific() {}
  ~CodecSpecificInfo() = default;

  VideoCodecType codecType;
  CodecSpecificInfoUnion codecSpecific;
  bool end_of_picture = true;
};

}  // namespace webrtc
