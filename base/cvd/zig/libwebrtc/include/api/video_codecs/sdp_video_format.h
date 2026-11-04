#pragma once

#include <map>
#include <string>
#include <vector>

#include "api/video_codecs/scalability_mode.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

struct SdpVideoFormat {
  using Parameters = std::map<std::string, std::string>;

  SdpVideoFormat(const std::string& name) : name(name) {}  // NOLINT
  SdpVideoFormat(const std::string& name, const Parameters& parameters)
      : name(name), parameters(parameters) {}
  SdpVideoFormat(const std::string& name,
                 const Parameters& parameters,
                 const std::vector<ScalabilityMode>& scalability_modes)
      : name(name),
        parameters(parameters),
        scalability_modes(scalability_modes) {}
  SdpVideoFormat(const SdpVideoFormat&) = default;
  SdpVideoFormat(SdpVideoFormat&&) = default;
  SdpVideoFormat& operator=(const SdpVideoFormat&) = default;
  SdpVideoFormat& operator=(SdpVideoFormat&&) = default;
  ~SdpVideoFormat() = default;

  std::string ToString() const;
  bool IsSameCodec(const SdpVideoFormat& other) const;

  friend bool operator==(const SdpVideoFormat& a, const SdpVideoFormat& b) {
    return a.name == b.name && a.parameters == b.parameters;
  }
  friend bool operator!=(const SdpVideoFormat& a, const SdpVideoFormat& b) {
    return !(a == b);
  }

  std::string name;
  Parameters parameters;
  std::vector<ScalabilityMode> scalability_modes;
};

}  // namespace webrtc
