#pragma once

#if __has_include("absl/types/optional.h")
#include "absl/types/optional.h"
#else
#include <optional>
namespace absl {
using std::nullopt;
using std::nullopt_t;
using std::optional;
}  // namespace absl
#endif

#if __has_include("absl/strings/string_view.h")
#include "absl/strings/string_view.h"
#else
#include <string_view>
namespace absl {
using std::string_view;
}  // namespace absl
#endif

namespace rtc {

enum class RefCountReleaseStatus { kDroppedLastRef, kOtherRefsRemained };

class RefCountInterface {
 public:
  virtual void AddRef() const = 0;
  virtual RefCountReleaseStatus Release() const = 0;

 protected:
  virtual ~RefCountInterface() = default;
};

}  // namespace rtc

namespace webrtc {
using rtc::RefCountInterface;
using rtc::RefCountReleaseStatus;
}  // namespace webrtc
