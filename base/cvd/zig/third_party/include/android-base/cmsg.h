#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <cerrno>
#include <cstring>
#include <vector>

#include "android-base/macros.h"
#include "android-base/unique_fd.h"

namespace android {
namespace base {

ssize_t SendFileDescriptorVector(borrowed_fd sockfd, const void* data,
                                 size_t len, const std::vector<int>& fds);

ssize_t ReceiveFileDescriptorVector(borrowed_fd sockfd, void* data, size_t len,
                                    size_t max_fds,
                                    std::vector<unique_fd>* fds);

template <typename... Args>
ssize_t SendFileDescriptors(int sockfd, const void* data, size_t len,
                            Args&&... sent_fds) {
  std::vector<int> fds{static_cast<int>(sent_fds)...};
  return SendFileDescriptorVector(sockfd, data, len, fds);
}

namespace cmsg_internal {
inline bool CollectFds(std::vector<unique_fd>&, size_t) { return true; }

template <typename... Args>
bool CollectFds(std::vector<unique_fd>& fds, size_t idx, unique_fd* out,
                Args&&... rest) {
  if (idx >= fds.size()) return false;
  *out = std::move(fds[idx]);
  return CollectFds(fds, idx + 1, std::forward<Args>(rest)...);
}
}  // namespace cmsg_internal

template <typename... Args>
ssize_t ReceiveFileDescriptors(int sockfd, void* data, size_t len,
                               Args&&... received_fds) {
  std::vector<unique_fd> fds;
  ssize_t result =
      ReceiveFileDescriptorVector(sockfd, data, len, sizeof...(Args), &fds);
  if (result < 0) return result;
  if (fds.size() != sizeof...(Args)) {
    errno = EMSGSIZE;
    return -1;
  }
  cmsg_internal::CollectFds(fds, 0, std::forward<Args>(received_fds)...);
  return result;
}

}  // namespace base
}  // namespace android
