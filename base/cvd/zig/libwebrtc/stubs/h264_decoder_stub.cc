#include <memory>

#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/include/video_error_codes.h"

namespace webrtc {

class H264Decoder : public VideoDecoder {
 public:
  static std::unique_ptr<H264Decoder> Create();
  static bool IsSupported();

  ~H264Decoder() override = default;
};

namespace {

class NoopH264Decoder final : public H264Decoder {
 public:
  NoopH264Decoder() = default;
  ~NoopH264Decoder() override = default;

  bool Configure(const Settings& settings) override {
    (void)settings;
    return true;
  }

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    (void)input_image;
    (void)missing_frames;
    (void)render_time_ms;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override {
    callback_ = nullptr;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  const char* ImplementationName() const override { return "NoopH264Decoder"; }

 private:
  DecodedImageCallback* callback_ = nullptr;
};

}  // namespace

std::unique_ptr<H264Decoder> H264Decoder::Create() {
  return std::make_unique<NoopH264Decoder>();
}

bool H264Decoder::IsSupported() { return true; }

}  // namespace webrtc
