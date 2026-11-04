#pragma once

#include <cstdint>
#include <string>

#include "google/rpc/code.pb.h"

namespace google {
namespace rpc {

class Status {
 public:
  Status() = default;

  int32_t code() const { return code_; }
  void set_code(int32_t value) { code_ = value; }

  const std::string& message() const { return message_; }
  void set_message(std::string value) { message_ = std::move(value); }
  std::string* mutable_message() { return &message_; }

  void Clear() {
    code_ = 0;
    message_.clear();
  }

 private:
  int32_t code_ = 0;
  std::string message_;
};

}  // namespace rpc
}  // namespace google
