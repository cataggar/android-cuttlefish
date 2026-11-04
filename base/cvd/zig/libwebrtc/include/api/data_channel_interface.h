#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

enum class Priority {
  kVeryLow,
  kLow,
  kMedium,
  kHigh,
};

struct DataBuffer {
  DataBuffer() : binary(false) {}
  DataBuffer(const rtc::CopyOnWriteBuffer& data_in, bool binary_in)
      : data(data_in), binary(binary_in) {}
  explicit DataBuffer(const std::string& text)
      : data(text.data(), text.length()), binary(false) {}

  size_t size() const { return data.size(); }

  rtc::CopyOnWriteBuffer data;
  bool binary;
};

struct DataChannelInit {
  bool reliable = false;
  bool ordered = true;
  absl::optional<int> maxRetransmitTime;
  absl::optional<int> maxRetransmits;
  std::string protocol;
  bool negotiated = false;
  int id = -1;
  absl::optional<Priority> priority;
};

class DataChannelObserver {
 public:
  virtual void OnStateChange() = 0;
  virtual void OnMessage(const DataBuffer& buffer) = 0;
  virtual void OnBufferedAmountChange(uint64_t sent_data_size) {
    (void)sent_data_size;
  }
  virtual bool IsOkToCallOnTheNetworkThread() { return false; }

 protected:
  virtual ~DataChannelObserver() = default;
};

class DataChannelInterface : public rtc::RefCountInterface {
 public:
  enum DataState { kConnecting, kOpen, kClosing, kClosed };

  static const char* DataStateString(DataState state) {
    switch (state) {
      case kConnecting:
        return "connecting";
      case kOpen:
        return "open";
      case kClosing:
        return "closing";
      case kClosed:
        return "closed";
    }
    return "";
  }

  virtual void RegisterObserver(DataChannelObserver* observer) = 0;
  virtual void UnregisterObserver() = 0;

  virtual std::string label() const = 0;
  virtual bool reliable() const = 0;
  virtual bool ordered() const { return true; }
  virtual uint16_t maxRetransmitTime() const { return 0; }
  virtual uint16_t maxRetransmits() const { return 0; }
  virtual absl::optional<int> maxRetransmitsOpt() const {
    return absl::nullopt;
  }
  virtual absl::optional<int> maxPacketLifeTime() const {
    return absl::nullopt;
  }
  virtual std::string protocol() const { return ""; }
  virtual bool negotiated() const { return false; }
  virtual int id() const = 0;
  virtual Priority priority() const { return Priority::kLow; }
  virtual DataState state() const = 0;
  virtual RTCError error() const { return RTCError::OK(); }
  virtual uint32_t messages_sent() const = 0;
  virtual uint64_t bytes_sent() const = 0;
  virtual uint32_t messages_received() const = 0;
  virtual uint64_t bytes_received() const = 0;
  virtual uint64_t buffered_amount() const = 0;
  virtual void Close() = 0;
  virtual bool Send(const DataBuffer& buffer) = 0;
  virtual void SendAsync(
      DataBuffer buffer,
      std::function<void(RTCError)> on_complete = nullptr) {
    bool ok = Send(buffer);
    if (on_complete) {
      on_complete(ok ? RTCError::OK() : RTCError(RTCErrorType::NETWORK_ERROR));
    }
  }

 protected:
  ~DataChannelInterface() override = default;
};

}  // namespace webrtc
