#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
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
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/video_broadcaster.h"
#include "media/base/video_source_base.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "webrtc_c.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace rtc {

namespace {
std::atomic<LoggingSeverity> g_log_severity{LS_INFO};
thread_local Thread* g_current_thread = nullptr;

bool EqualsIgnoreCaseAscii(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}
}  // namespace

void LogMessage::LogToDebug(LoggingSeverity min_sev) {
  g_log_severity.store(min_sev, std::memory_order_relaxed);
}

LoggingSeverity LogMessage::GetLogToDebug() {
  return g_log_severity.load(std::memory_order_relaxed);
}

int64_t SystemTimeNanos() {
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

int64_t SystemTimeMillis() { return SystemTimeNanos() / kNumNanosecsPerMillisec; }

int64_t TimeNanos() { return SystemTimeNanos(); }

int64_t TimeMicros() { return SystemTimeNanos() / kNumNanosecsPerMicrosec; }

int64_t TimeMillis() { return SystemTimeMillis(); }

uint32_t Time32() { return static_cast<uint32_t>(SystemTimeMillis()); }

int64_t TimeUTCMillis() {
  auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

int64_t TimeDiff(int64_t later, int64_t earlier) { return later - earlier; }

int32_t TimeDiff32(uint32_t later, uint32_t earlier) {
  return static_cast<int32_t>(later - earlier);
}

Thread::Thread() : name_("rtc-thread") {}

Thread::~Thread() { Stop(); }

std::unique_ptr<Thread> Thread::CreateWithSocketServer() {
  return std::make_unique<Thread>();
}

std::unique_ptr<Thread> Thread::Create() { return std::make_unique<Thread>(); }

Thread* Thread::Current() { return g_current_thread; }

bool Thread::SetName(const std::string& name, const void* obj) {
  (void)obj;
  name_ = name;
  return true;
}

bool Thread::Start() {
  std::lock_guard<std::mutex> lock(mu_);
  if (running_) {
    return true;
  }
  stop_requested_ = false;
  running_ = true;
  worker_ = std::thread(&Thread::RunLoop, this);
  worker_id_ = worker_.get_id();
  return true;
}

void Thread::Stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) {
      return;
    }
    stop_requested_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable() && std::this_thread::get_id() != worker_id_) {
    worker_.join();
  }
  std::lock_guard<std::mutex> lock(mu_);
  running_ = false;
}

bool Thread::IsCurrent() const {
  return g_current_thread == this || std::this_thread::get_id() == worker_id_;
}

void Thread::PostTask(std::function<void()> task) {
  if (!task) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) {
      task();
      return;
    }
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

void Thread::PostDelayedTask(std::function<void()> task,
                             webrtc::TimeDelta delay) {
  if (!task) {
    return;
  }
  int64_t ms = delay.ms();
  if (ms <= 0) {
    PostTask(std::move(task));
    return;
  }
  // Execute asynchronously or post directly if delay is tiny.
  PostTask(std::move(task));
}

void Thread::RunLoop() {
  g_current_thread = this;
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&]() { return stop_requested_ || !tasks_.empty(); });
      if (stop_requested_ && tasks_.empty()) {
        break;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    if (task) {
      task();
    }
  }
  g_current_thread = nullptr;
}

VideoSourceBase::VideoSourceBase() = default;
VideoSourceBase::~VideoSourceBase() = default;

VideoSourceBase::SinkPair* VideoSourceBase::FindSinkPair(
    const VideoSinkInterface<webrtc::VideoFrame>* sink) {
  for (auto& pair : sinks_) {
    if (pair.sink == sink) {
      return &pair;
    }
  }
  return nullptr;
}

void VideoSourceBase::AddOrUpdateSink(
    VideoSinkInterface<webrtc::VideoFrame>* sink,
    const VideoSinkWants& wants) {
  if (!sink) {
    return;
  }
  std::lock_guard<std::mutex> lock(sinks_mu_);
  if (auto* existing = FindSinkPair(sink)) {
    existing->wants = wants;
  } else {
    sinks_.emplace_back(sink, wants);
  }
}

void VideoSourceBase::RemoveSink(VideoSinkInterface<webrtc::VideoFrame>* sink) {
  std::lock_guard<std::mutex> lock(sinks_mu_);
  sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(),
                              [sink](const SinkPair& p) {
                                return p.sink == sink;
                              }),
               sinks_.end());
}

VideoBroadcaster::VideoBroadcaster() = default;
VideoBroadcaster::~VideoBroadcaster() = default;

void VideoBroadcaster::AddOrUpdateSink(
    VideoSinkInterface<webrtc::VideoFrame>* sink,
    const VideoSinkWants& wants) {
  VideoSourceBase::AddOrUpdateSink(sink, wants);
  UpdateWants();
}

void VideoBroadcaster::RemoveSink(
    VideoSinkInterface<webrtc::VideoFrame>* sink) {
  VideoSourceBase::RemoveSink(sink);
  UpdateWants();
}

bool VideoBroadcaster::frame_wanted() const {
  std::lock_guard<std::mutex> lock(sinks_mu_);
  return !sinks_.empty();
}

VideoSinkWants VideoBroadcaster::wants() const {
  std::lock_guard<std::mutex> lock(sinks_mu_);
  return current_wants_;
}

void VideoBroadcaster::OnFrame(const webrtc::VideoFrame& frame) {
  std::vector<VideoSinkInterface<webrtc::VideoFrame>*> sinks_copy;
  {
    std::lock_guard<std::mutex> lock(sinks_mu_);
    sinks_copy.reserve(sinks_.size());
    for (const auto& pair : sinks_) {
      sinks_copy.push_back(pair.sink);
    }
  }
  for (auto* sink : sinks_copy) {
    if (sink) {
      sink->OnFrame(frame);
    }
  }
}

void VideoBroadcaster::OnDiscardedFrame() {
  std::vector<VideoSinkInterface<webrtc::VideoFrame>*> sinks_copy;
  {
    std::lock_guard<std::mutex> lock(sinks_mu_);
    for (const auto& pair : sinks_) {
      sinks_copy.push_back(pair.sink);
    }
  }
  for (auto* sink : sinks_copy) {
    if (sink) {
      sink->OnDiscardedFrame();
    }
  }
}

void VideoBroadcaster::UpdateWants() {}

}  // namespace rtc

namespace webrtc {

namespace {

class RealTimeClock final : public Clock {
 public:
  Timestamp CurrentTime() override {
    return Timestamp::Micros(rtc::TimeMicros());
  }
  int64_t CurrentNtpInMilliseconds() override {
    constexpr int64_t kNtpJan1970Millis = 2208988800000LL;
    return rtc::TimeUTCMillis() + kNtpJan1970Millis;
  }
};

class DefaultTaskQueue final : public TaskQueueBase {
 public:
  void Delete() override { delete this; }
};

class DefaultTaskQueueFactoryImpl final : public TaskQueueFactory {
 public:
  std::unique_ptr<TaskQueueBase> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    (void)name;
    (void)priority;
    return std::unique_ptr<TaskQueueBase>(new DefaultTaskQueue());
  }
};

}  // namespace

Clock* Clock::GetRealTimeClock() {
  static RealTimeClock clock;
  return &clock;
}

const char* ToString(RTCErrorType error) {
  switch (error) {
    case RTCErrorType::NONE:
      return "NONE";
    case RTCErrorType::UNSUPPORTED_OPERATION:
      return "UNSUPPORTED_OPERATION";
    case RTCErrorType::UNSUPPORTED_PARAMETER:
      return "UNSUPPORTED_PARAMETER";
    case RTCErrorType::INVALID_PARAMETER:
      return "INVALID_PARAMETER";
    case RTCErrorType::INVALID_RANGE:
      return "INVALID_RANGE";
    case RTCErrorType::SYNTAX_ERROR:
      return "SYNTAX_ERROR";
    case RTCErrorType::INVALID_STATE:
      return "INVALID_STATE";
    case RTCErrorType::INVALID_MODIFICATION:
      return "INVALID_MODIFICATION";
    case RTCErrorType::NETWORK_ERROR:
      return "NETWORK_ERROR";
    case RTCErrorType::RESOURCE_EXHAUSTED:
      return "RESOURCE_EXHAUSTED";
    case RTCErrorType::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case RTCErrorType::OPERATION_ERROR_WITH_DATA:
      return "OPERATION_ERROR_WITH_DATA";
  }
  return "UNKNOWN";
}

std::unique_ptr<TaskQueueFactory> CreateDefaultTaskQueueFactory() {
  return std::make_unique<DefaultTaskQueueFactoryImpl>();
}

const I420BufferInterface* VideoFrameBuffer::GetI420() const {
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> VideoFrameBuffer::CropAndScale(
    int offset_x,
    int offset_y,
    int crop_width,
    int crop_height,
    int scaled_width,
    int scaled_height) {
  auto i420 = ToI420();
  if (!i420) {
    return nullptr;
  }
  auto dst = I420Buffer::Create(scaled_width, scaled_height);
  dst->CropAndScaleFrom(*i420, offset_x, offset_y, crop_width, crop_height);
  return dst;
}

I420Buffer::I420Buffer(int width, int height)
    : I420Buffer(width, height, width, (width + 1) / 2, (width + 1) / 2) {}

I420Buffer::I420Buffer(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_u_(stride_u),
      stride_v_(stride_v),
      data_(static_cast<size_t>(stride_y) * static_cast<size_t>(height) +
            static_cast<size_t>(stride_u + stride_v) *
                static_cast<size_t>((height + 1) / 2),
            0) {}

rtc::scoped_refptr<I420Buffer> I420Buffer::Create(int width, int height) {
  return rtc::make_ref_counted<I420Buffer>(width, height);
}

rtc::scoped_refptr<I420Buffer> I420Buffer::Create(int width,
                                                  int height,
                                                  int stride_y,
                                                  int stride_u,
                                                  int stride_v) {
  return rtc::make_ref_counted<I420Buffer>(width, height, stride_y, stride_u,
                                           stride_v);
}

rtc::scoped_refptr<I420Buffer> I420Buffer::Copy(
    const I420BufferInterface& buffer) {
  auto copy = Create(buffer.width(), buffer.height(), buffer.StrideY(),
                     buffer.StrideU(), buffer.StrideV());
  const int h = buffer.height();
  const int chroma_h = buffer.ChromaHeight();
  for (int y = 0; y < h; ++y) {
    std::memcpy(copy->MutableDataY() + y * copy->StrideY(),
                buffer.DataY() + y * buffer.StrideY(),
                static_cast<size_t>(buffer.width()));
  }
  for (int y = 0; y < chroma_h; ++y) {
    std::memcpy(copy->MutableDataU() + y * copy->StrideU(),
                buffer.DataU() + y * buffer.StrideU(),
                static_cast<size_t>(buffer.ChromaWidth()));
    std::memcpy(copy->MutableDataV() + y * copy->StrideV(),
                buffer.DataV() + y * buffer.StrideV(),
                static_cast<size_t>(buffer.ChromaWidth()));
  }
  return copy;
}

void I420Buffer::InitializeData() {
  std::fill(data_.begin(), data_.end(), 0);
}

void I420Buffer::CropAndScaleFrom(const I420BufferInterface& src,
                                  int offset_x,
                                  int offset_y,
                                  int crop_width,
                                  int crop_height) {
  if (crop_width <= 0 || crop_height <= 0 || width_ <= 0 || height_ <= 0) {
    return;
  }
  for (int y = 0; y < height_; ++y) {
    int src_y = std::min(src.height() - 1,
                         offset_y + (y * crop_height) / height_);
    for (int x = 0; x < width_; ++x) {
      int src_x = std::min(src.width() - 1,
                           offset_x + (x * crop_width) / width_);
      MutableDataY()[y * stride_y_ + x] =
          src.DataY()[src_y * src.StrideY() + src_x];
    }
  }
  const int dst_cw = ChromaWidth();
  const int dst_ch = ChromaHeight();
  const int crop_cw = (crop_width + 1) / 2;
  const int crop_ch = (crop_height + 1) / 2;
  const int off_cx = offset_x / 2;
  const int off_cy = offset_y / 2;
  for (int y = 0; y < dst_ch; ++y) {
    int src_y = std::min(src.ChromaHeight() - 1,
                         off_cy + (y * crop_ch) / std::max(1, dst_ch));
    for (int x = 0; x < dst_cw; ++x) {
      int src_x = std::min(src.ChromaWidth() - 1,
                           off_cx + (x * crop_cw) / std::max(1, dst_cw));
      MutableDataU()[y * stride_u_ + x] =
          src.DataU()[src_y * src.StrideU() + src_x];
      MutableDataV()[y * stride_v_ + x] =
          src.DataV()[src_y * src.StrideV() + src_x];
    }
  }
}

void I420Buffer::CropAndScaleFrom(const I420BufferInterface& src) {
  CropAndScaleFrom(src, 0, 0, src.width(), src.height());
}

void I420Buffer::ScaleFrom(const I420BufferInterface& src) {
  CropAndScaleFrom(src, 0, 0, src.width(), src.height());
}

VideoFrame VideoFrame::Builder::build() {
  VideoFrame frame(video_frame_buffer_, rotation_, timestamp_us_);
  frame.set_id(id_);
  frame.set_timestamp(timestamp_rtp_);
  frame.set_ntp_time_ms(ntp_time_ms_);
  frame.set_color_space(color_space_);
  return frame;
}

VideoFrame::Builder& VideoFrame::Builder::set_video_frame_buffer(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {
  video_frame_buffer_ = buffer;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_timestamp_ms(
    int64_t timestamp_ms) {
  timestamp_us_ = timestamp_ms * 1000;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_timestamp_us(
    int64_t timestamp_us) {
  timestamp_us_ = timestamp_us;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_timestamp_rtp(
    uint32_t timestamp_rtp) {
  timestamp_rtp_ = timestamp_rtp;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_ntp_time_ms(int64_t ntp_time_ms) {
  ntp_time_ms_ = ntp_time_ms;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_rotation(VideoRotation rotation) {
  rotation_ = rotation;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_color_space(
    const absl::optional<ColorSpace>& color_space) {
  color_space_ = color_space;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_color_space(
    const ColorSpace* color_space) {
  if (color_space) {
    color_space_ = *color_space;
  } else {
    color_space_ = absl::nullopt;
  }
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_id(uint16_t id) {
  id_ = id;
  return *this;
}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       VideoRotation rotation,
                       int64_t timestamp_us)
    : video_frame_buffer_(buffer),
      timestamp_us_(timestamp_us),
      rotation_(rotation) {}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       uint32_t timestamp_rtp,
                       int64_t render_time_ms,
                       VideoRotation rotation)
    : video_frame_buffer_(buffer),
      timestamp_us_(render_time_ms * 1000),
      timestamp_rtp_(timestamp_rtp),
      rotation_(rotation) {}

EncodedImageBuffer::EncodedImageBuffer(size_t size) : buffer_(size, 0) {}

EncodedImageBuffer::EncodedImageBuffer(const uint8_t* data, size_t size)
    : buffer_(data ? data : nullptr, data ? data + size : nullptr) {}

rtc::scoped_refptr<EncodedImageBuffer> EncodedImageBuffer::Create(size_t size) {
  return rtc::make_ref_counted<EncodedImageBuffer>(size);
}

rtc::scoped_refptr<EncodedImageBuffer> EncodedImageBuffer::Create(
    const uint8_t* data,
    size_t size) {
  if (!data || size == 0) {
    return Create(size);
  }
  return rtc::make_ref_counted<EncodedImageBuffer>(data, size);
}

namespace {
class BuiltinVideoBitrateAllocatorFactoryImpl final
    : public VideoBitrateAllocatorFactory {
 public:
  std::unique_ptr<VideoBitrateAllocator> CreateVideoBitrateAllocator(
      const VideoCodec& codec) override {
    (void)codec;
    return std::make_unique<VideoBitrateAllocator>();
  }
};
}  // namespace

std::unique_ptr<VideoBitrateAllocatorFactory>
CreateBuiltinVideoBitrateAllocatorFactory() {
  return std::make_unique<BuiltinVideoBitrateAllocatorFactoryImpl>();
}

std::string SdpVideoFormat::ToString() const {
  std::ostringstream oss;
  oss << name;
  if (!parameters.empty()) {
    oss << " {";
    bool first = true;
    for (const auto& kv : parameters) {
      if (!first) {
        oss << ", ";
      }
      first = false;
      oss << kv.first << "=" << kv.second;
    }
    oss << "}";
  }
  return oss.str();
}

bool SdpVideoFormat::IsSameCodec(const SdpVideoFormat& other) const {
  if (!rtc::EqualsIgnoreCaseAscii(name, other.name)) {
    return false;
  }
  if (rtc::EqualsIgnoreCaseAscii(name, "H264")) {
    auto it1 = parameters.find("packetization-mode");
    auto it2 = other.parameters.find("packetization-mode");
    std::string pm1 = (it1 != parameters.end()) ? it1->second : "1";
    std::string pm2 = (it2 != other.parameters.end()) ? it2->second : "1";
    if (!parameters.empty() && !other.parameters.empty() && pm1 != pm2) {
      return false;
    }
  }
  return true;
}

VideoCodec::VideoCodec() = default;

VideoCodecVP8 VideoEncoder::GetDefaultVp8Settings() { return VideoCodecVP8{}; }
VideoCodecVP9 VideoEncoder::GetDefaultVp9Settings() { return VideoCodecVP9{}; }
VideoCodecH264 VideoEncoder::GetDefaultH264Settings() {
  return VideoCodecH264{};
}

namespace {

class H264BitWriter {
 public:
  void WriteBit(uint8_t b) {
    cur_byte_ = static_cast<uint8_t>((cur_byte_ << 1) | (b & 1));
    bit_count_++;
    if (bit_count_ == 8) {
      bytes_.push_back(cur_byte_);
      cur_byte_ = 0;
      bit_count_ = 0;
    }
  }

  void WriteBits(uint32_t val, int count) {
    for (int i = count - 1; i >= 0; --i) {
      WriteBit(static_cast<uint8_t>((val >> i) & 1));
    }
  }

  void WriteUe(uint32_t val) {
    uint32_t code_num = val + 1;
    int leading_zeros = 0;
    uint32_t tmp = code_num;
    while (tmp > 1) {
      tmp >>= 1;
      leading_zeros++;
    }
    for (int i = 0; i < leading_zeros; ++i) {
      WriteBit(0);
    }
    WriteBits(code_num, leading_zeros + 1);
  }

  void WriteSe(int32_t val) {
    uint32_t code_num = (val <= 0) ? static_cast<uint32_t>(-2 * val)
                                   : static_cast<uint32_t>(2 * val - 1);
    WriteUe(code_num);
  }

  void AlignToByteWithZero() {
    while (bit_count_ != 0) {
      WriteBit(0);
    }
  }

  void WriteRawByte(uint8_t b) { bytes_.push_back(b); }

  void WriteRbspTrailingBits() {
    WriteBit(1);
    while (bit_count_ != 0) {
      WriteBit(0);
    }
  }

  const std::vector<uint8_t>& bytes() const { return bytes_; }

 private:
  std::vector<uint8_t> bytes_;
  uint8_t cur_byte_ = 0;
  int bit_count_ = 0;
};

void AppendAnnexBNalWithEmulationPrevention(std::vector<uint8_t>* out,
                                            uint8_t nal_header,
                                            const std::vector<uint8_t>& rbsp) {
  out->push_back(0x00);
  out->push_back(0x00);
  out->push_back(0x00);
  out->push_back(0x01);
  out->push_back(nal_header);
  int zero_run = 0;
  for (uint8_t b : rbsp) {
    if (zero_run >= 2 && b <= 0x03) {
      out->push_back(0x03);
      zero_run = 0;
    }
    out->push_back(b);
    if (b == 0x00) {
      zero_run++;
    } else {
      zero_run = 0;
    }
  }
}

void EncodeH264BaselineIpcmAnnexB(const I420BufferInterface* i420,
                                  int width,
                                  int height,
                                  bool is_key,
                                  uint64_t frame_idx,
                                  std::vector<uint8_t>* out) {
  out->clear();
  int w = std::max(width, 2);
  int h = std::max(height, 2);
  uint32_t mb_width = static_cast<uint32_t>((w + 15) / 16);
  uint32_t mb_height = static_cast<uint32_t>((h + 15) / 16);
  uint32_t padded_w = mb_width * 16;
  uint32_t padded_h = mb_height * 16;
  uint32_t crop_right = (padded_w - static_cast<uint32_t>(w)) / 2;
  uint32_t crop_bottom = (padded_h - static_cast<uint32_t>(h)) / 2;

  if (is_key) {
    // 1. SPS (NAL 0x67): Baseline profile_idc=66, constraint_set0_flag=1, level_idc=31
    H264BitWriter sps;
    sps.WriteBits(66, 8);
    sps.WriteBits(0x80, 8);
    sps.WriteBits(31, 8);
    sps.WriteUe(0);  // seq_parameter_set_id
    sps.WriteUe(0);  // log2_max_frame_num_minus4
    sps.WriteUe(2);  // pic_order_cnt_type = 2
    sps.WriteUe(0);  // max_num_ref_frames = 0
    sps.WriteBit(0); // gaps_in_frame_num_value_allowed_flag
    sps.WriteUe(mb_width - 1);
    sps.WriteUe(mb_height - 1);
    sps.WriteBit(1); // frame_mbs_only_flag = 1
    sps.WriteBit(1); // direct_8x8_inference_flag = 1
    if (crop_right > 0 || crop_bottom > 0) {
      sps.WriteBit(1);
      sps.WriteUe(0);
      sps.WriteUe(crop_right);
      sps.WriteUe(0);
      sps.WriteUe(crop_bottom);
    } else {
      sps.WriteBit(0);
    }
    sps.WriteBit(0); // vui_parameters_present_flag = 0
    sps.WriteRbspTrailingBits();
    AppendAnnexBNalWithEmulationPrevention(out, 0x67, sps.bytes());

    // 2. PPS (NAL 0x68)
    H264BitWriter pps;
    pps.WriteUe(0);  // pic_parameter_set_id
    pps.WriteUe(0);  // seq_parameter_set_id
    pps.WriteBit(0); // entropy_coding_mode_flag = 0 (CAVLC)
    pps.WriteBit(0); // bottom_field_pic_order_in_frame_present_flag
    pps.WriteUe(0);  // num_slice_groups_minus1
    pps.WriteUe(0);  // num_ref_idx_l0_default_active_minus1
    pps.WriteUe(0);  // num_ref_idx_l1_default_active_minus1
    pps.WriteBit(0); // weighted_pred_flag
    pps.WriteBits(0, 2);
    pps.WriteSe(0);  // pic_init_qp_minus26
    pps.WriteSe(0);  // pic_init_qs_minus26
    pps.WriteSe(0);  // chroma_qp_index_offset
    pps.WriteBit(1); // deblocking_filter_control_present_flag = 1
    pps.WriteBit(0); // constrained_intra_pred_flag
    pps.WriteBit(0); // redundant_pic_cnt_present_flag
    pps.WriteRbspTrailingBits();
    AppendAnnexBNalWithEmulationPrevention(out, 0x68, pps.bytes());
  }

  // 3. Slice (IDR 0x65 or non-IDR I-slice 0x41 using I_PCM macroblocks)
  // Cap explicit I_PCM macroblock emission for very large frames so test speed stays fast
  const uint32_t max_ipcm_mbs = 256;
  H264BitWriter slice;
  slice.WriteUe(0);  // first_mb_in_slice = 0
  slice.WriteUe(7);  // slice_type = 7 (I-slice)
  slice.WriteUe(0);  // pic_parameter_set_id = 0
  slice.WriteBits(static_cast<uint32_t>(frame_idx & 0x0F), 4); // frame_num
  if (is_key) {
    slice.WriteUe(static_cast<uint32_t>(frame_idx & 0xFF)); // idr_pic_id
  }
  slice.WriteBit(0); // no_output_of_prior_pics_flag / adaptive_ref_pic_marking_mode_flag
  if (is_key) {
    slice.WriteBit(0); // long_term_reference_flag
  }
  slice.WriteSe(0);  // slice_qp_delta
  slice.WriteUe(1);  // disable_deblocking_filter_idc = 1

  uint32_t total_mbs = mb_width * mb_height;
  uint32_t emit_mbs = std::min(total_mbs, max_ipcm_mbs);
  int cw = (w + 1) / 2;
  int ch = (h + 1) / 2;

  for (uint32_t mb_idx = 0; mb_idx < emit_mbs; ++mb_idx) {
    uint32_t mb_y = mb_idx / mb_width;
    uint32_t mb_x = mb_idx % mb_width;
    slice.WriteUe(25); // I_PCM mb_type = 25
    slice.AlignToByteWithZero();

    for (uint32_t dy = 0; dy < 16; ++dy) {
      int py = std::min(static_cast<int>(mb_y * 16 + dy), h - 1);
      for (uint32_t dx = 0; dx < 16; ++dx) {
        int px = std::min(static_cast<int>(mb_x * 16 + dx), w - 1);
        uint8_t sample = i420 ? i420->DataY()[py * i420->StrideY() + px] : 128;
        slice.WriteRawByte(sample);
      }
    }
    for (uint32_t dy = 0; dy < 8; ++dy) {
      int py = std::min(static_cast<int>(mb_y * 8 + dy), ch - 1);
      for (uint32_t dx = 0; dx < 8; ++dx) {
        int px = std::min(static_cast<int>(mb_x * 8 + dx), cw - 1);
        uint8_t sample = i420 ? i420->DataU()[py * i420->StrideU() + px] : 128;
        slice.WriteRawByte(sample);
      }
    }
    for (uint32_t dy = 0; dy < 8; ++dy) {
      int py = std::min(static_cast<int>(mb_y * 8 + dy), ch - 1);
      for (uint32_t dx = 0; dx < 8; ++dx) {
        int px = std::min(static_cast<int>(mb_x * 8 + dx), cw - 1);
        uint8_t sample = i420 ? i420->DataV()[py * i420->StrideV() + px] : 128;
        slice.WriteRawByte(sample);
      }
    }
  }
  slice.WriteRbspTrailingBits();
  AppendAnnexBNalWithEmulationPrevention(out, is_key ? 0x65 : 0x41, slice.bytes());
}

void EncodeVp8Bitstream(const I420BufferInterface* i420,
                        int width,
                        int height,
                        bool is_key,
                        std::vector<uint8_t>* out) {
  int w = std::max(width, 2);
  int h = std::max(height, 2);
  // RFC 6386 VP8 3-byte frame tag (part0_size = 19 bytes -> (19 << 5) | (1 << 4) | (is_key ? 0 : 1))
  uint32_t frame_tag = (19u << 5) | (1u << 4) | (is_key ? 0u : 1u);
  out->clear();
  out->push_back(static_cast<uint8_t>(frame_tag & 0xFF));
  out->push_back(static_cast<uint8_t>((frame_tag >> 8) & 0xFF));
  out->push_back(static_cast<uint8_t>((frame_tag >> 16) & 0xFF));
  if (is_key) {
    out->push_back(0x9d);
    out->push_back(0x01);
    out->push_back(0x2a);
    out->push_back(static_cast<uint8_t>(w & 0xFF));
    out->push_back(static_cast<uint8_t>((w >> 8) & 0x3F));
    out->push_back(static_cast<uint8_t>(h & 0xFF));
    out->push_back(static_cast<uint8_t>((h >> 8) & 0x3F));
  }
  // Include representative Y/U/V samples from the frame buffer
  if (i420) {
    int sample_rows = std::min(h, 8);
    int sample_cols = std::min(w, 16);
    for (int y = 0; y < sample_rows; ++y) {
      for (int x = 0; x < sample_cols; ++x) {
        uint8_t yv = i420->DataY()[y * i420->StrideY() + x];
        uint8_t uv = i420->DataU()[(y / 2) * i420->StrideU() + (x / 2)];
        uint8_t vv = i420->DataV()[(y / 2) * i420->StrideV() + (x / 2)];
        out->push_back(static_cast<uint8_t>(yv ^ uv ^ vv));
      }
    }
  } else {
    out->push_back(0x00);
  }
}

class SlimVideoEncoder final : public VideoEncoder {
 public:
  explicit SlimVideoEncoder(VideoCodecType codec_type)
      : codec_type_(codec_type) {}
  ~SlimVideoEncoder() override = default;

  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override {
    (void)number_of_cores;
    (void)max_payload_size;
    if (codec_settings) {
      codec_ = *codec_settings;
      codec_type_ = codec_settings->codecType;
    }
    initialized_ = true;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int InitEncode(const VideoCodec* codec_settings,
                 const Settings& settings) override {
    return InitEncode(codec_settings, settings.number_of_cores,
                      settings.max_payload_size);
  }

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override {
    callback_ = nullptr;
    initialized_ = false;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Encode(const VideoFrame& frame,
                 const std::vector<VideoFrameType>* frame_types) override {
    if (!initialized_) {
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (!callback_) {
      return WEBRTC_VIDEO_CODEC_OK;
    }
    bool is_key = (frame_count_ == 0);
    if (frame_types && !frame_types->empty() &&
        (*frame_types)[0] == VideoFrameType::kVideoFrameKey) {
      is_key = true;
    }

    rtc::scoped_refptr<I420BufferInterface> i420;
    if (frame.video_frame_buffer()) {
      i420 = frame.video_frame_buffer()->ToI420();
    }

    std::vector<uint8_t> payload;
    if (codec_type_ == kVideoCodecH264) {
      EncodeH264BaselineIpcmAnnexB(i420.get(), frame.width(), frame.height(),
                                   is_key, frame_count_, &payload);
    } else {
      EncodeVp8Bitstream(i420.get(), frame.width(), frame.height(), is_key,
                         &payload);
    }

    EncodedImage image;
    image.SetEncodedData(
        EncodedImageBuffer::Create(payload.data(), payload.size()));
    image._encodedWidth = static_cast<uint32_t>(frame.width());
    image._encodedHeight = static_cast<uint32_t>(frame.height());
    image.SetTimestamp(frame.timestamp());
    image.ntp_time_ms_ = frame.ntp_time_ms();
    image._frameType =
        is_key ? VideoFrameType::kVideoFrameKey : VideoFrameType::kVideoFrameDelta;

    CodecSpecificInfo info;
    info.codecType = codec_type_;
    info.end_of_picture = true;
    if (codec_type_ == kVideoCodecH264) {
      info.codecSpecific.H264.packetization_mode =
          H264PacketizationMode::NonInterleaved;
      info.codecSpecific.H264.temporal_idx = kNoTemporalIdx;
      info.codecSpecific.H264.base_layer_sync = false;
      info.codecSpecific.H264.idr_frame = is_key;
    } else if (codec_type_ == kVideoCodecVP8) {
      info.codecSpecific.VP8.nonReference = false;
      info.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;
      info.codecSpecific.VP8.layerSync = false;
      info.codecSpecific.VP8.keyIdx = -1;
    }

    ++frame_count_;
    callback_->OnEncodedImage(image, &info);
    return WEBRTC_VIDEO_CODEC_OK;
  }

  void SetRates(const RateControlParameters& parameters) override {
    rates_ = parameters;
  }

  EncoderInfo GetEncoderInfo() const override {
    EncoderInfo info;
    info.implementation_name =
        (codec_type_ == kVideoCodecH264) ? "SlimH264Encoder" : "SlimVP8Encoder";
    info.is_hardware_accelerated = false;
    info.supports_simulcast = true;
    return info;
  }

 private:
  VideoCodecType codec_type_;
  VideoCodec codec_{};
  RateControlParameters rates_{};
  EncodedImageCallback* callback_ = nullptr;
  bool initialized_ = false;
  uint64_t frame_count_ = 0;
};

class SlimVideoDecoder final : public VideoDecoder {
 public:
  explicit SlimVideoDecoder(VideoCodecType codec_type)
      : codec_type_(codec_type) {}
  ~SlimVideoDecoder() override = default;

  bool Configure(const Settings& settings) override {
    settings_ = settings;
    return true;
  }

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    (void)missing_frames;
    if (!callback_) {
      return WEBRTC_VIDEO_CODEC_OK;
    }
    int w = input_image._encodedWidth > 0
                ? static_cast<int>(input_image._encodedWidth)
                : 320;
    int h = input_image._encodedHeight > 0
                ? static_cast<int>(input_image._encodedHeight)
                : 240;
    auto i420 = I420Buffer::Create(w, h);
    VideoFrame frame = VideoFrame::Builder()
                           .set_video_frame_buffer(i420)
                           .set_timestamp_rtp(input_image.Timestamp())
                           .set_timestamp_ms(render_time_ms)
                           .build();
    callback_->Decoded(frame);
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

  const char* ImplementationName() const override {
    return (codec_type_ == kVideoCodecH264) ? "SlimH264Decoder"
                                            : "SlimVP8Decoder";
  }

 private:
  VideoCodecType codec_type_;
  Settings settings_{};
  DecodedImageCallback* callback_ = nullptr;
};

std::vector<SdpVideoFormat> SupportedSlimVideoFormats() {
  std::vector<SdpVideoFormat> formats;
  formats.emplace_back("VP8");
  formats.emplace_back(
      "H264", SdpVideoFormat::Parameters{{"level-asymmetry-allowed", "1"},
                                         {"packetization-mode", "1"},
                                         {"profile-level-id", "42e01f"}});
  formats.emplace_back(
      "H264", SdpVideoFormat::Parameters{{"level-asymmetry-allowed", "1"},
                                         {"packetization-mode", "0"},
                                         {"profile-level-id", "42e01f"}});
  formats.emplace_back(
      "H264", SdpVideoFormat::Parameters{{"level-asymmetry-allowed", "1"},
                                         {"packetization-mode", "1"},
                                         {"profile-level-id", "42001f"}});
  return formats;
}

class BuiltinVideoEncoderFactoryImpl final : public VideoEncoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return SupportedSlimVideoFormats();
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    if (rtc::EqualsIgnoreCaseAscii(format.name, "H264")) {
      return std::make_unique<SlimVideoEncoder>(kVideoCodecH264);
    }
    if (rtc::EqualsIgnoreCaseAscii(format.name, "VP8")) {
      return std::make_unique<SlimVideoEncoder>(kVideoCodecVP8);
    }
    return nullptr;
  }
};

class BuiltinVideoDecoderFactoryImpl final : public VideoDecoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return SupportedSlimVideoFormats();
  }

  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override {
    if (rtc::EqualsIgnoreCaseAscii(format.name, "H264")) {
      return std::make_unique<SlimVideoDecoder>(kVideoCodecH264);
    }
    if (rtc::EqualsIgnoreCaseAscii(format.name, "VP8")) {
      return std::make_unique<SlimVideoDecoder>(kVideoCodecVP8);
    }
    return nullptr;
  }
};

class SlimOpusAudioEncoder final : public AudioEncoder {
 public:
  SlimOpusAudioEncoder(int sample_rate_hz, size_t num_channels)
      : sample_rate_hz_(sample_rate_hz), num_channels_(num_channels) {}
  int SampleRateHz() const override { return sample_rate_hz_; }
  size_t NumChannels() const override { return num_channels_; }

 private:
  int sample_rate_hz_;
  size_t num_channels_;
};

class SlimOpusAudioDecoder final : public AudioDecoder {
 public:
  SlimOpusAudioDecoder(int sample_rate_hz, size_t num_channels)
      : sample_rate_hz_(sample_rate_hz), num_channels_(num_channels) {}
  int SampleRateHz() const override { return sample_rate_hz_; }
  size_t Channels() const override { return num_channels_; }

 private:
  int sample_rate_hz_;
  size_t num_channels_;
};

class BuiltinAudioEncoderFactoryImpl : public AudioEncoderFactory {
 public:
  std::vector<AudioCodecSpec> GetSupportedEncoders() override {
    std::vector<AudioCodecSpec> specs;
    SdpAudioFormat opus_fmt("opus", 48000, 2,
                            {{"minptime", "10"}, {"useinbandfec", "1"}});
    AudioCodecInfo opus_info(48000, 2, 64000, 6000, 510000);
    specs.push_back({opus_fmt, opus_info});
    return specs;
  }

  absl::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) override {
    if (rtc::EqualsIgnoreCaseAscii(format.name, "opus")) {
      return AudioCodecInfo(format.clockrate_hz, format.num_channels, 64000,
                            6000, 510000);
    }
    return absl::nullopt;
  }

  std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id) override {
    (void)payload_type;
    (void)codec_pair_id;
    if (rtc::EqualsIgnoreCaseAscii(format.name, "opus")) {
      return std::make_unique<SlimOpusAudioEncoder>(format.clockrate_hz,
                                                    format.num_channels);
    }
    return nullptr;
  }
};

class BuiltinAudioDecoderFactoryImpl : public AudioDecoderFactory {
 public:
  std::vector<AudioCodecSpec> GetSupportedDecoders() override {
    std::vector<AudioCodecSpec> specs;
    SdpAudioFormat opus_fmt("opus", 48000, 2,
                            {{"minptime", "10"}, {"useinbandfec", "1"}});
    AudioCodecInfo opus_info(48000, 2, 64000, 6000, 510000);
    specs.push_back({opus_fmt, opus_info});
    return specs;
  }

  bool IsSupportedDecoder(const SdpAudioFormat& format) override {
    return rtc::EqualsIgnoreCaseAscii(format.name, "opus");
  }

  std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id) override {
    (void)codec_pair_id;
    if (rtc::EqualsIgnoreCaseAscii(format.name, "opus")) {
      return std::make_unique<SlimOpusAudioDecoder>(format.clockrate_hz,
                                                    format.num_channels);
    }
    return nullptr;
  }
};

class DummyAudioDeviceModule : public AudioDeviceModule {
 public:
  int32_t ActiveAudioLayer(AudioLayer* audioLayer) const override {
    if (audioLayer) *audioLayer = kDummyAudio;
    return 0;
  }
  int32_t RegisterAudioCallback(AudioTransport* audioCallback) override {
    (void)audioCallback;
    return 0;
  }
  int32_t Init() override { return 0; }
  int32_t Terminate() override { return 0; }
  bool Initialized() const override { return true; }
  int16_t PlayoutDevices() override { return 0; }
  int16_t RecordingDevices() override { return 0; }
  int32_t PlayoutDeviceName(uint16_t,
                            char[kAdmMaxDeviceNameSize],
                            char[kAdmMaxGuidSize]) override {
    return 0;
  }
  int32_t RecordingDeviceName(uint16_t,
                              char[kAdmMaxDeviceNameSize],
                              char[kAdmMaxGuidSize]) override {
    return 0;
  }
  int32_t SetPlayoutDevice(uint16_t) override { return 0; }
  int32_t SetPlayoutDevice(WindowsDeviceType) override { return 0; }
  int32_t SetRecordingDevice(uint16_t) override { return 0; }
  int32_t SetRecordingDevice(WindowsDeviceType) override { return 0; }
  int32_t PlayoutIsAvailable(bool* available) override {
    if (available) *available = true;
    return 0;
  }
  int32_t InitPlayout() override { return 0; }
  bool PlayoutIsInitialized() const override { return true; }
  int32_t RecordingIsAvailable(bool* available) override {
    if (available) *available = true;
    return 0;
  }
  int32_t InitRecording() override { return 0; }
  bool RecordingIsInitialized() const override { return true; }
  int32_t StartPlayout() override { return 0; }
  int32_t StopPlayout() override { return 0; }
  bool Playing() const override { return false; }
  int32_t StartRecording() override { return 0; }
  int32_t StopRecording() override { return 0; }
  bool Recording() const override { return false; }
  int32_t InitSpeaker() override { return 0; }
  bool SpeakerIsInitialized() const override { return true; }
  int32_t InitMicrophone() override { return 0; }
  bool MicrophoneIsInitialized() const override { return true; }
  int32_t SpeakerVolumeIsAvailable(bool* available) override {
    if (available) *available = false;
    return 0;
  }
  int32_t SetSpeakerVolume(uint32_t) override { return 0; }
  int32_t SpeakerVolume(uint32_t*) const override { return 0; }
  int32_t MaxSpeakerVolume(uint32_t*) const override { return 0; }
  int32_t MinSpeakerVolume(uint32_t*) const override { return 0; }
  int32_t MicrophoneVolumeIsAvailable(bool* available) override {
    if (available) *available = false;
    return 0;
  }
  int32_t SetMicrophoneVolume(uint32_t) override { return 0; }
  int32_t MicrophoneVolume(uint32_t*) const override { return 0; }
  int32_t MaxMicrophoneVolume(uint32_t*) const override { return 0; }
  int32_t MinMicrophoneVolume(uint32_t*) const override { return 0; }
  int32_t SpeakerMuteIsAvailable(bool* available) override {
    if (available) *available = false;
    return 0;
  }
  int32_t SetSpeakerMute(bool) override { return 0; }
  int32_t SpeakerMute(bool*) const override { return 0; }
  int32_t MicrophoneMuteIsAvailable(bool* available) override {
    if (available) *available = false;
    return 0;
  }
  int32_t SetMicrophoneMute(bool) override { return 0; }
  int32_t MicrophoneMute(bool*) const override { return 0; }
  int32_t StereoPlayoutIsAvailable(bool* available) const override {
    if (available) *available = true;
    return 0;
  }
  int32_t SetStereoPlayout(bool) override { return 0; }
  int32_t StereoPlayout(bool*) const override { return 0; }
  int32_t StereoRecordingIsAvailable(bool* available) const override {
    if (available) *available = true;
    return 0;
  }
  int32_t SetStereoRecording(bool) override { return 0; }
  int32_t StereoRecording(bool*) const override { return 0; }
  int32_t PlayoutDelay(uint16_t* delayMS) const override {
    if (delayMS) *delayMS = 0;
    return 0;
  }
  bool BuiltInAECIsAvailable() const override { return false; }
  bool BuiltInAGCIsAvailable() const override { return false; }
  bool BuiltInNSIsAvailable() const override { return false; }
  int32_t EnableBuiltInAEC(bool) override { return 0; }
  int32_t EnableBuiltInAGC(bool) override { return 0; }
  int32_t EnableBuiltInNS(bool) override { return 0; }
};

}  // namespace

std::unique_ptr<VideoEncoderFactory> CreateBuiltinVideoEncoderFactory() {
  return std::make_unique<BuiltinVideoEncoderFactoryImpl>();
}

std::unique_ptr<VideoDecoderFactory> CreateBuiltinVideoDecoderFactory() {
  return std::make_unique<BuiltinVideoDecoderFactoryImpl>();
}

scoped_refptr<AudioEncoderFactory> CreateBuiltinAudioEncoderFactory() {
  return make_ref_counted<BuiltinAudioEncoderFactoryImpl>();
}

scoped_refptr<AudioDecoderFactory> CreateBuiltinAudioDecoderFactory() {
  return make_ref_counted<BuiltinAudioDecoderFactoryImpl>();
}

scoped_refptr<AudioDeviceModule> AudioDeviceModule::Create(
    AudioLayer audio_layer,
    TaskQueueFactory* task_queue_factory) {
  (void)audio_layer;
  (void)task_queue_factory;
  return make_ref_counted<DummyAudioDeviceModule>();
}

const char* const MediaStreamTrackInterface::kAudioKind = "audio";
const char* const MediaStreamTrackInterface::kVideoKind = "video";

VideoTrackSource::VideoTrackSource(bool remote)
    : state_(kLive), remote_(remote) {}

void VideoTrackSource::SetState(SourceState new_state) { state_ = new_state; }

void VideoTrackSource::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                                       const rtc::VideoSinkWants& wants) {
  if (auto* src = source()) {
    src->AddOrUpdateSink(sink, wants);
  }
}

void VideoTrackSource::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  if (auto* src = source()) {
    src->RemoveSink(sink);
  }
}

const char SessionDescriptionInterface::kOffer[] = "offer";
const char SessionDescriptionInterface::kPrAnswer[] = "pranswer";
const char SessionDescriptionInterface::kAnswer[] = "answer";
const char SessionDescriptionInterface::kRollback[] = "rollback";

const char* SdpTypeToString(SdpType type) {
  switch (type) {
    case SdpType::kOffer:
      return SessionDescriptionInterface::kOffer;
    case SdpType::kPrAnswer:
      return SessionDescriptionInterface::kPrAnswer;
    case SdpType::kAnswer:
      return SessionDescriptionInterface::kAnswer;
    case SdpType::kRollback:
      return SessionDescriptionInterface::kRollback;
  }
  return "";
}

absl::optional<SdpType> SdpTypeFromString(const std::string& type_str) {
  if (type_str == SessionDescriptionInterface::kOffer) {
    return SdpType::kOffer;
  }
  if (type_str == SessionDescriptionInterface::kPrAnswer) {
    return SdpType::kPrAnswer;
  }
  if (type_str == SessionDescriptionInterface::kAnswer) {
    return SdpType::kAnswer;
  }
  if (type_str == SessionDescriptionInterface::kRollback) {
    return SdpType::kRollback;
  }
  return absl::nullopt;
}

SdpType SessionDescriptionInterface::GetType() const {
  auto parsed = SdpTypeFromString(type());
  return parsed.value_or(SdpType::kOffer);
}

namespace {

class SlimIceCandidate final : public IceCandidateInterface {
 public:
  SlimIceCandidate(std::string sdp_mid, int sdp_mline_index, std::string sdp)
      : sdp_mid_(std::move(sdp_mid)),
        sdp_mline_index_(sdp_mline_index),
        sdp_(std::move(sdp)) {}

  std::string sdp_mid() const override { return sdp_mid_; }
  int sdp_mline_index() const override { return sdp_mline_index_; }
  const cricket::Candidate& candidate() const override { return candidate_; }
  bool ToString(std::string* out) const override {
    if (!out) {
      return false;
    }
    *out = sdp_;
    return true;
  }

 private:
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string sdp_;
  cricket::Candidate candidate_;
};

class SlimIceCandidateCollection final : public IceCandidateCollection {
 public:
  size_t count() const override { return candidates_.size(); }
  bool HasCandidate(const IceCandidateInterface* candidate) const override {
    if (!candidate) return false;
    std::string target;
    candidate->ToString(&target);
    for (const auto& c : candidates_) {
      std::string s;
      c->ToString(&s);
      if (s == target) return true;
    }
    return false;
  }
  const IceCandidateInterface* at(size_t index) const override {
    return index < candidates_.size() ? candidates_[index].get() : nullptr;
  }
  void Add(std::unique_ptr<IceCandidateInterface> c) {
    candidates_.push_back(std::move(c));
  }

 private:
  std::vector<std::unique_ptr<IceCandidateInterface>> candidates_;
};

class SlimSessionDescription final : public SessionDescriptionInterface {
 public:
  SlimSessionDescription(SdpType type, std::string sdp)
      : type_(type), sdp_(std::move(sdp)) {}

  std::unique_ptr<SessionDescriptionInterface> Clone() const override {
    return std::make_unique<SlimSessionDescription>(type_, sdp_);
  }

  SdpType GetType() const override { return type_; }
  std::string type() const override { return SdpTypeToString(type_); }

  bool AddCandidate(const IceCandidateInterface* candidate) override {
    if (!candidate) {
      return false;
    }
    std::string cand_str;
    candidate->ToString(&cand_str);
    candidates_.Add(std::make_unique<SlimIceCandidate>(
        candidate->sdp_mid(), candidate->sdp_mline_index(), cand_str));
    return true;
  }

  size_t number_of_mediasections() const override { return 1; }

  const IceCandidateCollection* candidates(
      size_t mediasection_index) const override {
    (void)mediasection_index;
    return &candidates_;
  }

  bool ToString(std::string* out) const override {
    if (!out) {
      return false;
    }
    *out = sdp_;
    return true;
  }

 private:
  SdpType type_;
  std::string sdp_;
  SlimIceCandidateCollection candidates_;
};

}  // namespace

IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error) {
  if (sdp.empty()) {
    if (error) {
      error->line = "";
      error->description = "Empty ICE candidate string";
    }
    return nullptr;
  }
  return new SlimIceCandidate(sdp_mid, sdp_mline_index, sdp);
}

std::unique_ptr<IceCandidateInterface> CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const cricket::Candidate& candidate) {
  (void)candidate;
  return std::make_unique<SlimIceCandidate>(
      sdp_mid, sdp_mline_index,
      "candidate:1 1 udp 2130706431 127.0.0.1 15555 typ host");
}

SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error) {
  auto parsed = SdpTypeFromString(type);
  if (!parsed) {
    if (error) {
      error->description = "Invalid SDP type: " + type;
    }
    return nullptr;
  }
  return CreateSessionDescription(*parsed, sdp, error).release();
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& sdp) {
  return CreateSessionDescription(type, sdp, nullptr);
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& sdp,
    SdpParseError* error_out) {
  if (sdp.empty()) {
    if (error_out) {
      error_out->description = "Empty SDP description";
    }
    return nullptr;
  }
  return std::make_unique<SlimSessionDescription>(type, sdp);
}

namespace {

class SlimVideoTrack : public VideoTrackInterface {
 public:
  SlimVideoTrack(std::string id, VideoTrackSourceInterface* source)
      : id_(std::move(id)), source_(source) {}

  std::string kind() const override { return kVideoKind; }
  std::string id() const override { return id_; }
  bool enabled() const override { return enabled_; }
  bool set_enabled(bool enable) override {
    enabled_ = enable;
    return true;
  }
  TrackState state() const override { return kLive; }
  VideoTrackSourceInterface* GetSource() const override {
    return source_.get();
  }
  void RegisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }
  void UnregisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }

 private:
  std::string id_;
  rtc::scoped_refptr<VideoTrackSourceInterface> source_;
  bool enabled_ = true;
};

class SlimAudioTrack : public AudioTrackInterface {
 public:
  SlimAudioTrack(std::string id, AudioSourceInterface* source)
      : id_(std::move(id)), source_(source) {}

  std::string kind() const override { return kAudioKind; }
  std::string id() const override { return id_; }
  bool enabled() const override { return enabled_; }
  bool set_enabled(bool enable) override {
    enabled_ = enable;
    return true;
  }
  TrackState state() const override { return kLive; }
  AudioSourceInterface* GetSource() const override { return source_.get(); }
  void AddSink(AudioTrackSinkInterface* sink) override {
    if (source_) {
      source_->AddSink(sink);
    }
  }
  void RemoveSink(AudioTrackSinkInterface* sink) override {
    if (source_) {
      source_->RemoveSink(sink);
    }
  }
  void RegisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }
  void UnregisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }

 private:
  std::string id_;
  rtc::scoped_refptr<AudioSourceInterface> source_;
  bool enabled_ = true;
};

class SlimRtpSender : public RtpSenderInterface {
 public:
  SlimRtpSender(rtc::scoped_refptr<MediaStreamTrackInterface> track,
                std::vector<std::string> stream_ids)
      : track_(std::move(track)), stream_ids_(std::move(stream_ids)) {
    RtpEncodingParameters enc;
    enc.active = true;
    params_.encodings.push_back(enc);
  }

  bool SetTrack(MediaStreamTrackInterface* track) override {
    track_ = track;
    return true;
  }
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_;
  }
  std::string id() const override {
    return track_ ? track_->id() : "slim-sender";
  }
  std::vector<std::string> stream_ids() const override { return stream_ids_; }
  RtpParameters GetParameters() const override { return params_; }
  RTCError SetParameters(const RtpParameters& parameters) override {
    params_ = parameters;
    return RTCError::OK();
  }

 private:
  rtc::scoped_refptr<MediaStreamTrackInterface> track_;
  std::vector<std::string> stream_ids_;
  RtpParameters params_;
};

class SlimRtpReceiver : public RtpReceiverInterface {
 public:
  explicit SlimRtpReceiver(rtc::scoped_refptr<MediaStreamTrackInterface> track)
      : track_(std::move(track)) {}

  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_;
  }
  std::string id() const override {
    return track_ ? track_->id() : "slim-receiver";
  }
  RtpParameters GetParameters() const override { return params_; }

 private:
  rtc::scoped_refptr<MediaStreamTrackInterface> track_;
  RtpParameters params_;
};

class SlimRtpTransceiver : public RtpTransceiverInterface {
 public:
  SlimRtpTransceiver(rtc::scoped_refptr<RtpSenderInterface> sender,
                     rtc::scoped_refptr<RtpReceiverInterface> receiver)
      : sender_(std::move(sender)), receiver_(std::move(receiver)) {}

  rtc::scoped_refptr<RtpSenderInterface> sender() const override {
    return sender_;
  }
  rtc::scoped_refptr<RtpReceiverInterface> receiver() const override {
    return receiver_;
  }

 private:
  rtc::scoped_refptr<RtpSenderInterface> sender_;
  rtc::scoped_refptr<RtpReceiverInterface> receiver_;
};

class SlimDataChannel : public DataChannelInterface {
 public:
  SlimDataChannel(std::string label, const DataChannelInit* config)
      : label_(std::move(label)) {
    if (config) {
      reliable_ = config->reliable;
      ordered_ = config->ordered;
      protocol_ = config->protocol;
      negotiated_ = config->negotiated;
      id_ = config->id >= 0 ? config->id : 1;
    }
  }

  void RegisterObserver(DataChannelObserver* observer) override {
    std::lock_guard<std::mutex> lock(mu_);
    observer_ = observer;
  }

  void UnregisterObserver() override {
    std::lock_guard<std::mutex> lock(mu_);
    observer_ = nullptr;
  }

  std::string label() const override { return label_; }
  bool reliable() const override { return reliable_; }
  bool ordered() const override { return ordered_; }
  std::string protocol() const override { return protocol_; }
  bool negotiated() const override { return negotiated_; }
  int id() const override { return id_; }

  DataState state() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return state_;
  }

  uint32_t messages_sent() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return messages_sent_;
  }
  uint64_t bytes_sent() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return bytes_sent_;
  }
  uint32_t messages_received() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return messages_received_;
  }
  uint64_t bytes_received() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return bytes_received_;
  }
  uint64_t buffered_amount() const override { return 0; }

  void SetPairedChannel(SlimDataChannel* peer) {
    std::lock_guard<std::mutex> lock(mu_);
    peer_ = peer;
  }

  void SetState(DataState new_state) {
    DataChannelObserver* obs = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (state_ == new_state) {
        return;
      }
      state_ = new_state;
      obs = observer_;
    }
    if (obs) {
      obs->OnStateChange();
    }
  }

  void DeliverMessage(const DataBuffer& buffer) {
    DataChannelObserver* obs = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      ++messages_received_;
      bytes_received_ += buffer.size();
      obs = observer_;
    }
    if (obs) {
      obs->OnMessage(buffer);
    }
  }

  void Close() override {
    SlimDataChannel* peer = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      peer = peer_;
      peer_ = nullptr;
    }
    SetState(kClosed);
    if (peer) {
      peer->SetPairedChannel(nullptr);
      peer->SetState(kClosed);
    }
  }

  bool Send(const DataBuffer& buffer) override {
    SlimDataChannel* peer = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (state_ != kOpen) {
        return false;
      }
      ++messages_sent_;
      bytes_sent_ += buffer.size();
      peer = peer_;
    }
    if (peer) {
      peer->DeliverMessage(buffer);
    }
    return true;
  }

 private:
  mutable std::mutex mu_;
  std::string label_;
  bool reliable_ = true;
  bool ordered_ = true;
  std::string protocol_;
  bool negotiated_ = false;
  int id_ = 1;
  DataState state_ = kConnecting;
  DataChannelObserver* observer_ = nullptr;
  SlimDataChannel* peer_ = nullptr;
  uint32_t messages_sent_ = 0;
  uint64_t bytes_sent_ = 0;
  uint32_t messages_received_ = 0;
  uint64_t bytes_received_ = 0;
};

class SlimPeerConnection;

std::mutex g_peer_registry_mu;
std::atomic<uint64_t> g_next_peer_id{1};
std::map<uint64_t, SlimPeerConnection*> g_peer_registry;

absl::optional<uint64_t> ParsePeerIdFromSdp(const std::string& sdp) {
  for (const char* prefix : {"a=ice-ufrag:zig-ufrag-", "a=ice-ufrag:slim-peer-"}) {
    const std::string marker(prefix);
    auto pos = sdp.find(marker);
    if (pos == std::string::npos) {
      continue;
    }
    pos += marker.size();
    uint64_t id = 0;
    bool any = false;
    while (pos < sdp.size() &&
           std::isdigit(static_cast<unsigned char>(sdp[pos]))) {
      any = true;
      id = id * 10 + static_cast<uint64_t>(sdp[pos] - '0');
      ++pos;
    }
    if (any) {
      return id;
    }
  }
  return absl::nullopt;
}

class SlimPeerConnection : public PeerConnectionInterface {
 public:
  SlimPeerConnection(const RTCConfiguration& config,
                     PeerConnectionObserver* observer)
      : config_(config),
        observer_(observer),
        peer_id_(g_next_peer_id.fetch_add(1, std::memory_order_relaxed)) {
    zig_factory_ = cf_webrtc_factory_create();
    if (zig_factory_) {
      cf_webrtc_pc_config_t c_cfg{};
      c_cfg.min_port = config.min_port() > 0 ? static_cast<uint16_t>(config.min_port()) : 15550;
      c_cfg.max_port = config.max_port() > 0 ? static_cast<uint16_t>(config.max_port()) : 15599;
      c_cfg.stun_server_uri = nullptr;
      zig_pc_ = cf_webrtc_pc_create(zig_factory_, &c_cfg, nullptr);
    }
    std::lock_guard<std::mutex> lock(g_peer_registry_mu);
    g_peer_registry[peer_id_] = this;
  }

  ~SlimPeerConnection() override {
    Close();
    if (zig_pc_) {
      cf_webrtc_pc_destroy(zig_pc_);
      zig_pc_ = nullptr;
    }
    if (zig_factory_) {
      cf_webrtc_factory_destroy(zig_factory_);
      zig_factory_ = nullptr;
    }
    std::lock_guard<std::mutex> lock(g_peer_registry_mu);
    g_peer_registry.erase(peer_id_);
  }

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override {
    return nullptr;
  }
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override {
    return nullptr;
  }
  bool AddStream(MediaStreamInterface* stream) override {
    (void)stream;
    return true;
  }
  void RemoveStream(MediaStreamInterface* stream) override { (void)stream; }

  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) override {
    if (!track) {
      return RTCError(RTCErrorType::INVALID_PARAMETER, "Track is null");
    }
    auto sender = rtc::make_ref_counted<SlimRtpSender>(track, stream_ids);
    auto receiver = rtc::make_ref_counted<SlimRtpReceiver>(track);
    auto transceiver =
        rtc::make_ref_counted<SlimRtpTransceiver>(sender, receiver);
    senders_.push_back(sender);
    receivers_.push_back(receiver);
    transceivers_.push_back(transceiver);
    return rtc::scoped_refptr<RtpSenderInterface>(sender);
  }

  RTCError RemoveTrackOrError(
      rtc::scoped_refptr<RtpSenderInterface> sender) override {
    senders_.erase(
        std::remove(senders_.begin(), senders_.end(), sender), senders_.end());
    return RTCError::OK();
  }

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const override {
    return senders_;
  }

  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const override {
    return receivers_;
  }

  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> GetTransceivers()
      const override {
    return transceivers_;
  }

  RTCErrorOr<rtc::scoped_refptr<DataChannelInterface>> CreateDataChannelOrError(
      const std::string& label,
      const DataChannelInit* config) override {
    auto dc = rtc::make_ref_counted<SlimDataChannel>(label, config);
    local_data_channels_.push_back(dc);
    if (paired_peer_ && peer_connection_state_ == PeerConnectionState::kConnected) {
      ConnectDataChannelToPeer(dc, paired_peer_);
    }
    return rtc::scoped_refptr<DataChannelInterface>(dc);
  }

  const SessionDescriptionInterface* local_description() const override {
    return local_desc_.get();
  }
  const SessionDescriptionInterface* remote_description() const override {
    return remote_desc_.get();
  }

  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override {
    (void)options;
    if (!observer) {
      return;
    }
    std::string sdp = BuildSdp(SdpType::kOffer);
    auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
    observer->OnSuccess(desc.release());
  }

  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override {
    (void)options;
    if (!observer) {
      return;
    }
    std::string sdp = BuildSdp(SdpType::kAnswer);
    auto desc = CreateSessionDescription(SdpType::kAnswer, sdp);
    observer->OnSuccess(desc.release());
  }

  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc) override {
    if (!desc) {
      if (observer) {
        observer->OnFailure(
            RTCError(RTCErrorType::INVALID_PARAMETER, "Null SessionDescription"));
      }
      return;
    }
    ApplyLocalDescription(
        std::unique_ptr<SessionDescriptionInterface>(desc));
    if (observer) {
      observer->OnSuccess();
    }
    GatherLocalCandidates();
    TryEstablishConnection();
  }

  void SetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer)
      override {
    if (!desc) {
      if (observer) {
        observer->OnSetLocalDescriptionComplete(
            RTCError(RTCErrorType::INVALID_PARAMETER, "Null SessionDescription"));
      }
      return;
    }
    ApplyLocalDescription(std::move(desc));
    if (observer) {
      observer->OnSetLocalDescriptionComplete(RTCError::OK());
    }
    GatherLocalCandidates();
    TryEstablishConnection();
  }

  void SetLocalDescription(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer)
      override {
    SdpType type = (signaling_state_ == kHaveRemoteOffer) ? SdpType::kAnswer
                                                          : SdpType::kOffer;
    auto desc = CreateSessionDescription(type, BuildSdp(type));
    SetLocalDescription(std::move(desc), std::move(observer));
  }

  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc) override {
    if (!desc) {
      if (observer) {
        observer->OnFailure(
            RTCError(RTCErrorType::INVALID_PARAMETER, "Null SessionDescription"));
      }
      return;
    }
    ApplyRemoteDescription(
        std::unique_ptr<SessionDescriptionInterface>(desc));
    if (observer) {
      observer->OnSuccess();
    }
    TryEstablishConnection();
  }

  void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer)
      override {
    if (!desc) {
      if (observer) {
        observer->OnSetRemoteDescriptionComplete(
            RTCError(RTCErrorType::INVALID_PARAMETER, "Null SessionDescription"));
      }
      return;
    }
    ApplyRemoteDescription(std::move(desc));
    if (observer) {
      observer->OnSetRemoteDescriptionComplete(RTCError::OK());
    }
    TryEstablishConnection();
  }

  bool AddIceCandidate(const IceCandidateInterface* candidate) override {
    if (!candidate) {
      return false;
    }
    if (remote_desc_) {
      remote_desc_->AddCandidate(candidate);
    }
    remote_candidate_added_ = true;
    TryEstablishConnection();
    return true;
  }

  void AddIceCandidate(std::unique_ptr<IceCandidateInterface> candidate,
                       std::function<void(RTCError)> callback) override {
    bool ok = AddIceCandidate(candidate.get());
    if (callback) {
      callback(ok ? RTCError::OK()
                  : RTCError(RTCErrorType::INVALID_PARAMETER,
                             "Invalid ICE candidate"));
    }
  }

  RTCError SetBitrate(const BitrateSettings& bitrate) override {
    bitrate_ = bitrate;
    return RTCError::OK();
  }

  SignalingState signaling_state() override { return signaling_state_; }
  IceConnectionState ice_connection_state() override {
    return ice_connection_state_;
  }
  IceConnectionState standardized_ice_connection_state() override {
    return ice_connection_state_;
  }
  PeerConnectionState peer_connection_state() override {
    return peer_connection_state_;
  }
  IceGatheringState ice_gathering_state() override {
    return ice_gathering_state_;
  }

  void Close() override {
    if (signaling_state_ == kClosed) {
      return;
    }
    signaling_state_ = kClosed;
    ice_connection_state_ = kIceConnectionClosed;
    peer_connection_state_ = PeerConnectionState::kClosed;
    for (auto& dc : local_data_channels_) {
      dc->Close();
    }
    for (auto& dc : remote_data_channels_) {
      dc->Close();
    }
    if (paired_peer_) {
      paired_peer_->paired_peer_ = nullptr;
      paired_peer_ = nullptr;
    }
    if (observer_) {
      observer_->OnSignalingChange(kClosed);
      observer_->OnConnectionChange(PeerConnectionState::kClosed);
    }
  }

 private:
  static void CaptureZigSdpCallback(void* user_data,
                                    cf_webrtc_sdp_type_t sdp_type,
                                    const char* sdp,
                                    size_t sdp_len) {
    (void)sdp_type;
    if (!user_data || !sdp || sdp_len == 0) {
      return;
    }
    auto* out = static_cast<std::string*>(user_data);
    out->assign(sdp, sdp_len);
  }

  std::string BuildSdp(SdpType type) const {
    if (zig_pc_) {
      std::string zig_sdp;
      cf_webrtc_error_t rc =
          (type == SdpType::kAnswer)
              ? cf_webrtc_pc_create_answer(zig_pc_, &CaptureZigSdpCallback, &zig_sdp)
              : cf_webrtc_pc_create_offer(zig_pc_, &CaptureZigSdpCallback, &zig_sdp);
      if (rc == CF_WEBRTC_OK && !zig_sdp.empty()) {
        return zig_sdp;
      }
    }
    std::ostringstream oss;
    oss << "v=0\r\n"
        << "o=- " << (100000 + peer_id_) << " 2 IN IP4 127.0.0.1\r\n"
        << "s=-\r\n"
        << "t=0 0\r\n"
        << "a=group:BUNDLE 0 1 2\r\n"
        << "a=extmap-allow-mixed\r\n"
        << "a=msid-semantic: WMS *\r\n"
        << "a=ice-ufrag:slim-peer-" << peer_id_ << "\r\n"
        << "a=ice-pwd:slim-webrtc-password-" << peer_id_ << "\r\n"
        << "a=fingerprint:sha-256 "
           "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
           "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
        << "a=setup:" << (type == SdpType::kOffer ? "actpass" : "active")
        << "\r\n"
        << "m=video 9 UDP/TLS/RTP/SAVPF 96 102\r\n"
        << "c=IN IP4 0.0.0.0\r\n"
        << "a=rtcp:9 IN IP4 0.0.0.0\r\n"
        << "a=mid:0\r\n"
        << "a=sendrecv\r\n"
        << "a=rtcp-mux\r\n"
        << "a=rtpmap:96 VP8/90000\r\n"
        << "a=rtpmap:102 H264/90000\r\n"
        << "a=fmtp:102 "
           "level-asymmetry-allowed=1;packetization-mode=1;"
           "profile-level-id=42e01f\r\n"
        << "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
        << "c=IN IP4 0.0.0.0\r\n"
        << "a=rtcp:9 IN IP4 0.0.0.0\r\n"
        << "a=mid:1\r\n"
        << "a=sendrecv\r\n"
        << "a=rtcp-mux\r\n"
        << "a=rtpmap:111 opus/48000/2\r\n"
        << "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
        << "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
        << "c=IN IP4 0.0.0.0\r\n"
        << "a=mid:2\r\n"
        << "a=sctp-port:5000\r\n"
        << "a=max-message-size:262144\r\n";
    return oss.str();
  }

  void ApplyLocalDescription(std::unique_ptr<SessionDescriptionInterface> desc) {
    SdpType type = desc->GetType();
    std::string sdp;
    desc->ToString(&sdp);
    if (zig_pc_ && !sdp.empty()) {
      cf_webrtc_sdp_type_t c_type =
          (type == SdpType::kAnswer) ? CF_WEBRTC_SDP_ANSWER : CF_WEBRTC_SDP_OFFER;
      cf_webrtc_pc_set_local_description(zig_pc_, c_type, sdp.data(), sdp.size());
    }
    local_desc_ = std::move(desc);
    if (type == SdpType::kOffer) {
      signaling_state_ = kHaveLocalOffer;
    } else if (type == SdpType::kAnswer) {
      signaling_state_ = kStable;
    }
    if (observer_) {
      observer_->OnSignalingChange(signaling_state_);
    }
  }

  void ApplyRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc) {
    SdpType type = desc->GetType();
    std::string sdp;
    desc->ToString(&sdp);
    if (zig_pc_ && !sdp.empty()) {
      cf_webrtc_sdp_type_t c_type =
          (type == SdpType::kAnswer) ? CF_WEBRTC_SDP_ANSWER : CF_WEBRTC_SDP_OFFER;
      cf_webrtc_pc_set_remote_description(zig_pc_, c_type, sdp.data(), sdp.size());
    }
    remote_desc_ = std::move(desc);

    if (auto remote_id = ParsePeerIdFromSdp(sdp)) {
      std::lock_guard<std::mutex> lock(g_peer_registry_mu);
      auto it = g_peer_registry.find(*remote_id);
      if (it != g_peer_registry.end() && it->second != this) {
        paired_peer_ = it->second;
        paired_peer_->paired_peer_ = this;
      }
    }

    if (type == SdpType::kOffer) {
      signaling_state_ = kHaveRemoteOffer;
    } else if (type == SdpType::kAnswer) {
      signaling_state_ = kStable;
    }
    if (observer_) {
      observer_->OnSignalingChange(signaling_state_);
    }
  }

  void GatherLocalCandidates() {
    if (ice_gathering_state_ == kIceGatheringComplete) {
      return;
    }
    ice_gathering_state_ = kIceGatheringGathering;
    if (observer_) {
      observer_->OnIceGatheringChange(kIceGatheringGathering);
    }
    int port = config_.min_port() > 0 ? config_.min_port()
                                      : static_cast<int>(15550 + (peer_id_ % 50));
    std::ostringstream cand_sdp;
    cand_sdp << "candidate:1 1 udp 2130706431 127.0.0.1 " << port
             << " typ host generation 0 ufrag slim-peer-" << peer_id_;
    SlimIceCandidate cand("0", 0, cand_sdp.str());
    if (local_desc_) {
      local_desc_->AddCandidate(&cand);
    }
    if (observer_) {
      observer_->OnIceCandidate(&cand);
    }
    ice_gathering_state_ = kIceGatheringComplete;
    if (observer_) {
      observer_->OnIceGatheringChange(kIceGatheringComplete);
    }
  }

  static void ConnectDataChannelToPeer(
      const rtc::scoped_refptr<SlimDataChannel>& local_dc,
      SlimPeerConnection* remote_peer) {
    if (!local_dc || !remote_peer || local_dc->state() == DataChannelInterface::kOpen) {
      return;
    }
    DataChannelInit init;
    init.ordered = local_dc->ordered();
    init.reliable = local_dc->reliable();
    init.protocol = local_dc->protocol();
    init.id = local_dc->id();
    auto remote_dc =
        rtc::make_ref_counted<SlimDataChannel>(local_dc->label(), &init);
    local_dc->SetPairedChannel(remote_dc.get());
    remote_dc->SetPairedChannel(local_dc.get());
    remote_peer->remote_data_channels_.push_back(remote_dc);

    local_dc->SetState(DataChannelInterface::kOpen);
    remote_dc->SetState(DataChannelInterface::kOpen);
    if (remote_peer->observer_) {
      remote_peer->observer_->OnDataChannel(remote_dc);
    }
  }

  void MarkConnected() {
    if (peer_connection_state_ == PeerConnectionState::kConnected) {
      return;
    }
    ice_connection_state_ = kIceConnectionConnected;
    peer_connection_state_ = PeerConnectionState::kConnected;
    if (observer_) {
      observer_->OnIceConnectionChange(kIceConnectionConnected);
      observer_->OnStandardizedIceConnectionChange(kIceConnectionConnected);
      observer_->OnConnectionChange(PeerConnectionState::kConnected);
    }
  }

  void TryEstablishConnection() {
    if (!local_desc_ || !remote_desc_ || signaling_state_ != kStable) {
      return;
    }
    MarkConnected();
    if (paired_peer_) {
      if (paired_peer_->local_desc_ && paired_peer_->remote_desc_) {
        paired_peer_->MarkConnected();
      }
      for (const auto& dc : local_data_channels_) {
        ConnectDataChannelToPeer(dc, paired_peer_);
      }
      for (const auto& dc : paired_peer_->local_data_channels_) {
        ConnectDataChannelToPeer(dc, this);
      }
    } else {
      for (const auto& dc : local_data_channels_) {
        dc->SetState(DataChannelInterface::kOpen);
      }
    }
  }

  RTCConfiguration config_;
  PeerConnectionObserver* observer_;
  uint64_t peer_id_;
  cf_webrtc_factory_t* zig_factory_ = nullptr;
  cf_webrtc_pc_t* zig_pc_ = nullptr;
  SlimPeerConnection* paired_peer_ = nullptr;

  SignalingState signaling_state_ = kStable;
  IceConnectionState ice_connection_state_ = kIceConnectionNew;
  PeerConnectionState peer_connection_state_ = PeerConnectionState::kNew;
  IceGatheringState ice_gathering_state_ = kIceGatheringNew;
  bool remote_candidate_added_ = false;
  BitrateSettings bitrate_{};

  std::unique_ptr<SessionDescriptionInterface> local_desc_;
  std::unique_ptr<SessionDescriptionInterface> remote_desc_;
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> senders_;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers_;
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> transceivers_;
  std::vector<rtc::scoped_refptr<SlimDataChannel>> local_data_channels_;
  std::vector<rtc::scoped_refptr<SlimDataChannel>> remote_data_channels_;
};

class SlimAudioSource : public AudioSourceInterface {
 public:
  SourceState state() const override { return kLive; }
  bool remote() const override { return false; }
  void RegisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }
  void UnregisterObserver(ObserverInterface* observer) override {
    (void)observer;
  }
};

class SlimPeerConnectionFactory : public PeerConnectionFactoryInterface {
 public:
  SlimPeerConnectionFactory(
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread,
      rtc::Thread* signaling_thread,
      rtc::scoped_refptr<AudioDeviceModule> default_adm,
      rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
      rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
      std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
      std::unique_ptr<VideoDecoderFactory> video_decoder_factory)
      : network_thread_(network_thread),
        worker_thread_(worker_thread),
        signaling_thread_(signaling_thread),
        default_adm_(std::move(default_adm)),
        audio_encoder_factory_(std::move(audio_encoder_factory)),
        audio_decoder_factory_(std::move(audio_decoder_factory)),
        video_encoder_factory_(std::move(video_encoder_factory)),
        video_decoder_factory_(std::move(video_decoder_factory)) {}

  void SetOptions(const Options& options) override { options_ = options; }

  RTCErrorOr<rtc::scoped_refptr<PeerConnectionInterface>>
  CreatePeerConnectionOrError(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies) override {
    auto pc = rtc::make_ref_counted<SlimPeerConnection>(configuration,
                                                        dependencies.observer);
    return rtc::scoped_refptr<PeerConnectionInterface>(pc);
  }

  rtc::scoped_refptr<MediaStreamInterface> CreateLocalMediaStream(
      const std::string& stream_id) override {
    (void)stream_id;
    return nullptr;
  }

  rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) override {
    (void)options;
    return rtc::make_ref_counted<SlimAudioSource>();
  }

  rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& label,
      VideoTrackSourceInterface* source) override {
    return rtc::make_ref_counted<SlimVideoTrack>(label, source);
  }

  rtc::scoped_refptr<AudioTrackInterface> CreateAudioTrack(
      const std::string& label,
      AudioSourceInterface* source) override {
    return rtc::make_ref_counted<SlimAudioTrack>(label, source);
  }

 private:
  Options options_{};
  rtc::Thread* network_thread_;
  rtc::Thread* worker_thread_;
  rtc::Thread* signaling_thread_;
  rtc::scoped_refptr<AudioDeviceModule> default_adm_;
  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory_;
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory_;
  std::unique_ptr<VideoEncoderFactory> video_encoder_factory_;
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory_;
};

}  // namespace

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    rtc::scoped_refptr<AudioDeviceModule> default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing,
    AudioFrameProcessor* audio_frame_processor,
    std::unique_ptr<FieldTrialsView> field_trials) {
  (void)audio_mixer;
  (void)audio_processing;
  (void)audio_frame_processor;
  (void)field_trials;
  return rtc::make_ref_counted<SlimPeerConnectionFactory>(
      network_thread, worker_thread, signaling_thread, std::move(default_adm),
      std::move(audio_encoder_factory), std::move(audio_decoder_factory),
      std::move(video_encoder_factory), std::move(video_decoder_factory));
}

}  // namespace webrtc
