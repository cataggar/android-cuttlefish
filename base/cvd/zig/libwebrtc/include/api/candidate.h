#pragma once

#include <cstdint>
#include <string>

namespace cricket {

class Candidate {
 public:
  Candidate() = default;
  ~Candidate() = default;

  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  int component() const { return component_; }
  void set_component(int component) { component_ = component; }

  const std::string& protocol() const { return protocol_; }
  void set_protocol(const std::string& protocol) { protocol_ = protocol; }

  uint32_t priority() const { return priority_; }
  void set_priority(uint32_t priority) { priority_ = priority; }

  const std::string& username() const { return username_; }
  void set_username(const std::string& username) { username_ = username; }

  const std::string& password() const { return password_; }
  void set_password(const std::string& password) { password_ = password; }

  const std::string& type() const { return type_; }
  void set_type(const std::string& type) { type_ = type; }

  const std::string& foundation() const { return foundation_; }
  void set_foundation(const std::string& foundation) {
    foundation_ = foundation;
  }

 private:
  std::string id_;
  int component_ = 1;
  std::string protocol_ = "udp";
  uint32_t priority_ = 2130706431;
  std::string username_;
  std::string password_;
  std::string type_ = "host";
  std::string foundation_ = "1";
};

}  // namespace cricket
