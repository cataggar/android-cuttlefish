#pragma once

#include <cstdint>
#include <string>

#include "mkvmuxer/mkvmuxertypes.h"
#include "mkvmuxer/mkvwriter.h"

namespace mkvmuxer {

class SegmentInfo {
 public:
  void set_writing_app(const char* app) { writing_app_ = app ? app : ""; }
  const char* writing_app() const { return writing_app_.c_str(); }
  void set_muxing_app(const char* app) { muxing_app_ = app ? app : ""; }
  const char* muxing_app() const { return muxing_app_.c_str(); }

 private:
  std::string writing_app_;
  std::string muxing_app_;
};

class Segment {
 public:
  static constexpr const char* kVp8CodecId = "V_VP8";
  static constexpr const char* kVp9CodecId = "V_VP9";
  static constexpr const char* kAv1CodecId = "V_AV1";
  static constexpr const char* kOpusCodecId = "A_OPUS";
  static constexpr const char* kVorbisCodecId = "A_VORBIS";

  Segment() = default;
  ~Segment() = default;

  bool Init(IMkvWriter* writer);
  uint64 AddVideoTrack(int32 width, int32 height, int32 track_number);
  uint64 AddAudioTrack(int32 sample_rate, int32 channels, int32 track_number);
  bool AddFrame(const uint8* frame, uint64 length, uint64 track_number,
                uint64 timestamp_ns, bool is_key);
  bool Finalize();

  SegmentInfo* GetSegmentInfo() { return &segment_info_; }
  void OutputCues(bool output_cues) { output_cues_ = output_cues; }

 private:
  IMkvWriter* writer_ = nullptr;
  SegmentInfo segment_info_;
  bool output_cues_ = false;
  uint64 next_track_num_ = 1;
};

}  // namespace mkvmuxer
