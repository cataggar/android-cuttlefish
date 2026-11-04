#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "api/units/data_rate.h"
#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

struct CodecSpecificInfo;
class FecControllerOverride;

class EncodedImageCallback {
 public:
  virtual ~EncodedImageCallback() = default;

  struct Result {
    enum Error {
      OK,
      ERROR_SEND_FAILED,
    };
    explicit Result(Error error) : error(error) {}
    Result(Error error, uint32_t frame_id) : error(error), frame_id(frame_id) {}

    Error error;
    uint32_t frame_id = 0;
    bool drop_next_frame = false;
  };

  enum class DropReason : uint8_t {
    kDroppedByMediaOptimizations,
    kDroppedByBwe,
    kDroppedByEncoder,
  };

  virtual Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info) = 0;

  virtual void OnDroppedFrame(DropReason reason) { (void)reason; }
};

class VideoEncoder {
 public:
  struct QuestionableOptions {};

  struct Capabilities {
    explicit Capabilities(bool loss_notification)
        : loss_notification(loss_notification) {}
    bool loss_notification;
  };

  struct Settings {
    Settings(const Capabilities& capabilities,
             int number_of_cores,
             size_t max_payload_size)
        : capabilities(capabilities),
          number_of_cores(number_of_cores),
          max_payload_size(max_payload_size) {}

    Capabilities capabilities;
    int number_of_cores;
    size_t max_payload_size;
    absl::optional<int> encoder_thread_limit;
  };

  struct ResolutionBitrateLimits {
    ResolutionBitrateLimits() = default;
    ResolutionBitrateLimits(int frame_size_pixels,
                            int min_start_bitrate_bps,
                            int min_bitrate_bps,
                            int max_bitrate_bps)
        : frame_size_pixels(frame_size_pixels),
          min_start_bitrate_bps(min_start_bitrate_bps),
          min_bitrate_bps(min_bitrate_bps),
          max_bitrate_bps(max_bitrate_bps) {}

    int frame_size_pixels = 0;
    int min_start_bitrate_bps = 0;
    int min_bitrate_bps = 0;
    int max_bitrate_bps = 0;
  };

  struct ScalingSettings {
    ScalingSettings() = default;
    ScalingSettings(int low, int high) : thresholds(true), low(low), high(high) {}
    bool thresholds = false;
    int low = 0;
    int high = 0;
  };

  struct EncoderInfo {
    std::string implementation_name = "slim_encoder";
    bool supports_native_handle = false;
    bool has_trusted_rate_controller = false;
    bool is_hardware_accelerated = false;
    bool supports_simulcast = false;
    ScalingSettings scaling_settings;
    uint32_t requested_resolution_alignment = 1;
    bool apply_alignment_to_all_simulcast_layers = false;
    std::vector<VideoFrameBuffer::Type> preferred_pixel_formats{
        VideoFrameBuffer::Type::kI420};
    std::vector<ResolutionBitrateLimits> resolution_bitrate_limits;
  };

  struct RateControlParameters {
    RateControlParameters() = default;
    RateControlParameters(const VideoBitrateAllocation& bitrate,
                          double framerate_fps)
        : bitrate(bitrate),
          framerate_fps(framerate_fps),
          bandwidth_allocation(DataRate::BitsPerSec(bitrate.get_sum_bps())) {}
    RateControlParameters(const VideoBitrateAllocation& bitrate,
                          double framerate_fps,
                          DataRate bandwidth)
        : bitrate(bitrate),
          framerate_fps(framerate_fps),
          bandwidth_allocation(bandwidth) {}

    VideoBitrateAllocation bitrate;
    double framerate_fps = 0.0;
    DataRate bandwidth_allocation = DataRate::Zero();
  };

  struct LossNotification {
    uint16_t timestamp_of_last_decodable = 0;
    uint16_t timestamp_of_last_received = 0;
    absl::optional<bool> dependencies_of_last_received_decodable;
    absl::optional<bool> last_received_decodable;
  };

  static VideoCodecVP8 GetDefaultVp8Settings();
  static VideoCodecVP9 GetDefaultVp9Settings();
  static VideoCodecH264 GetDefaultH264Settings();

  virtual ~VideoEncoder() = default;

  virtual void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) {
    (void)fec_controller_override;
  }

  virtual int InitEncode(const VideoCodec* codec_settings,
                         const Settings& settings) {
    return InitEncode(codec_settings, settings.number_of_cores,
                      settings.max_payload_size);
  }

  virtual int32_t InitEncode(const VideoCodec* codec_settings,
                             int32_t number_of_cores,
                             size_t max_payload_size) {
    (void)codec_settings;
    (void)number_of_cores;
    (void)max_payload_size;
    return 0;
  }

  virtual int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) = 0;

  virtual int32_t Release() = 0;

  virtual int32_t Encode(const VideoFrame& frame,
                         const std::vector<VideoFrameType>* frame_types) = 0;

  virtual void SetRates(const RateControlParameters& parameters) = 0;

  virtual void OnPacketLossRateUpdate(float packet_loss_rate) {
    (void)packet_loss_rate;
  }

  virtual void OnRttUpdate(int64_t rtt_ms) { (void)rtt_ms; }

  virtual void OnLossNotification(const LossNotification& loss_notification) {
    (void)loss_notification;
  }

  virtual EncoderInfo GetEncoderInfo() const { return EncoderInfo{}; }
};

}  // namespace webrtc
