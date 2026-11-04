#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "google/rpc/status.pb.h"

namespace cuttlefish {
namespace webrtc {

class StartRecordingDisplayRequest {
 public:
  StartRecordingDisplayRequest() = default;
};

class StopRecordingDisplayRequest {
 public:
  StopRecordingDisplayRequest() = default;
};

class ScreenshotDisplayRequest {
 public:
  ScreenshotDisplayRequest() = default;

  int32_t display_number() const { return display_number_; }
  void set_display_number(int32_t value) { display_number_ = value; }

  const std::string& screenshot_path() const { return screenshot_path_; }
  void set_screenshot_path(std::string value) {
    screenshot_path_ = std::move(value);
  }

 private:
  int32_t display_number_ = 0;
  std::string screenshot_path_;
};

class WebrtcCommandRequest {
 public:
  enum ActionsCase {
    ACTIONS_NOT_SET = 0,
    kStartRecordingRequest = 1,
    kStopRecordingRequest = 2,
    kScreenshotDisplayRequest = 3,
  };

  WebrtcCommandRequest() = default;

  ActionsCase actions_case() const { return actions_case_; }

  bool has_start_recording_request() const {
    return actions_case_ == kStartRecordingRequest;
  }
  const StartRecordingDisplayRequest& start_recording_request() const {
    return start_recording_;
  }
  StartRecordingDisplayRequest* mutable_start_recording_request() {
    actions_case_ = kStartRecordingRequest;
    return &start_recording_;
  }

  bool has_stop_recording_request() const {
    return actions_case_ == kStopRecordingRequest;
  }
  const StopRecordingDisplayRequest& stop_recording_request() const {
    return stop_recording_;
  }
  StopRecordingDisplayRequest* mutable_stop_recording_request() {
    actions_case_ = kStopRecordingRequest;
    return &stop_recording_;
  }

  bool has_screenshot_display_request() const {
    return actions_case_ == kScreenshotDisplayRequest;
  }
  const ScreenshotDisplayRequest& screenshot_display_request() const {
    return screenshot_display_;
  }
  ScreenshotDisplayRequest* mutable_screenshot_display_request() {
    actions_case_ = kScreenshotDisplayRequest;
    return &screenshot_display_;
  }

  std::string DebugString() const {
    return "WebrtcCommandRequest{case=" +
           std::to_string(static_cast<int>(actions_case_)) + "}";
  }

  bool SerializeToString(std::string* out) const {
    if (!out) return false;
    out->clear();
    uint8_t tag = static_cast<uint8_t>(actions_case_);
    out->push_back(static_cast<char>(tag));
    if (actions_case_ == kScreenshotDisplayRequest) {
      int32_t disp = screenshot_display_.display_number();
      out->append(reinterpret_cast<const char*>(&disp), sizeof(disp));
      uint32_t len =
          static_cast<uint32_t>(screenshot_display_.screenshot_path().size());
      out->append(reinterpret_cast<const char*>(&len), sizeof(len));
      out->append(screenshot_display_.screenshot_path());
    }
    return true;
  }

  bool ParseFromArray(const void* data, int size) {
    if (!data || size < 1) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    actions_case_ = static_cast<ActionsCase>(bytes[0]);
    if (actions_case_ == kScreenshotDisplayRequest) {
      if (size < static_cast<int>(1 + sizeof(int32_t) + sizeof(uint32_t))) {
        return false;
      }
      int32_t disp = 0;
      uint32_t len = 0;
      std::memcpy(&disp, bytes + 1, sizeof(disp));
      std::memcpy(&len, bytes + 1 + sizeof(disp), sizeof(len));
      const size_t header_len = 1 + sizeof(disp) + sizeof(len);
      if (static_cast<size_t>(size) < header_len + len) {
        return false;
      }
      screenshot_display_.set_display_number(disp);
      screenshot_display_.set_screenshot_path(
          std::string(reinterpret_cast<const char*>(bytes + header_len), len));
    }
    return true;
  }

 private:
  ActionsCase actions_case_ = ACTIONS_NOT_SET;
  StartRecordingDisplayRequest start_recording_;
  StopRecordingDisplayRequest stop_recording_;
  ScreenshotDisplayRequest screenshot_display_;
};

class WebrtcCommandResponse {
 public:
  WebrtcCommandResponse() = default;

  bool has_status() const { return has_status_; }
  const ::google::rpc::Status& status() const { return status_; }
  ::google::rpc::Status* mutable_status() {
    has_status_ = true;
    return &status_;
  }

  bool SerializeToString(std::string* out) const {
    if (!out) return false;
    out->clear();
    int32_t code = status_.code();
    uint32_t len = static_cast<uint32_t>(status_.message().size());
    out->append(reinterpret_cast<const char*>(&code), sizeof(code));
    out->append(reinterpret_cast<const char*>(&len), sizeof(len));
    out->append(status_.message());
    return true;
  }

  bool ParseFromArray(const void* data, int size) {
    if (!data || size < static_cast<int>(sizeof(int32_t) + sizeof(uint32_t))) {
      return false;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    int32_t code = 0;
    uint32_t len = 0;
    std::memcpy(&code, bytes, sizeof(code));
    std::memcpy(&len, bytes + sizeof(code), sizeof(len));
    const size_t header_len = sizeof(code) + sizeof(len);
    if (static_cast<size_t>(size) < header_len + len) {
      return false;
    }
    has_status_ = true;
    status_.set_code(code);
    status_.set_message(
        std::string(reinterpret_cast<const char*>(bytes + header_len), len));
    return true;
  }

 private:
  bool has_status_ = false;
  ::google::rpc::Status status_;
};

}  // namespace webrtc
}  // namespace cuttlefish
