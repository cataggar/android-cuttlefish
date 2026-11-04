#pragma once

#include <string>

namespace google {
namespace protobuf {

class Message {
 public:
  virtual ~Message() = default;
  virtual std::string DebugString() const { return "{}"; }
};

class TextFormat {
 public:
  template <typename T>
  static bool PrintToString(const T&, std::string* output) {
    if (output) {
      output->clear();
    }
    return true;
  }

  template <typename T>
  static bool ParseFromString(const std::string&, T*) {
    return true;
  }
};

}  // namespace protobuf
}  // namespace google
