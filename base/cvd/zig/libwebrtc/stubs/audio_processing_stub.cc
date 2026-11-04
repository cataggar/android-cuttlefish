#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

namespace {

class NoopAudioProcessing : public AudioProcessing {
 public:
  NoopAudioProcessing() = default;
  ~NoopAudioProcessing() override = default;

  void ApplyConfig(const Config& config) override { (void)config; }
  int Initialize() override { return 0; }
};

}  // namespace

scoped_refptr<AudioProcessing> AudioProcessingBuilder::Create() {
  return make_ref_counted<NoopAudioProcessing>();
}

class EchoCanceller3 {
 public:
  EchoCanceller3() = default;
  ~EchoCanceller3() = default;
};

class Limiter {
 public:
  Limiter() = default;
  ~Limiter() = default;
};

class ApmDataDumper {
 public:
  explicit ApmDataDumper(int instance_index) { (void)instance_index; }
  ~ApmDataDumper() = default;
};

}  // namespace webrtc
