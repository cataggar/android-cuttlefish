#pragma once

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace android {
namespace base {

struct DefaultCloser {
  static void Close(int fd) {
    if (fd >= 0) {
      int saved_errno = errno;
      ::close(fd);
      errno = saved_errno;
    }
  }
};

template <typename Closer = DefaultCloser>
class unique_fd_impl final {
 public:
  unique_fd_impl() noexcept : fd_(-1) {}
  explicit unique_fd_impl(int fd) noexcept : fd_(fd) {}
  ~unique_fd_impl() { reset(); }

  unique_fd_impl(const unique_fd_impl&) = delete;
  void operator=(const unique_fd_impl&) = delete;

  unique_fd_impl(unique_fd_impl&& other) noexcept : fd_(other.release()) {}
  unique_fd_impl& operator=(unique_fd_impl&& s) noexcept {
    int fd = s.fd_;
    s.fd_ = -1;
    reset(fd);
    return *this;
  }

  void reset(int new_value = -1) noexcept {
    int previous = fd_;
    fd_ = new_value;
    if (previous != -1) {
      Closer::Close(previous);
    }
  }

  void clear() noexcept { reset(); }

  int get() const noexcept { return fd_; }
  operator int() const noexcept { return get(); }
  explicit operator bool() const noexcept { return fd_ != -1; }
  bool ok() const noexcept { return fd_ >= 0; }

  int release() noexcept {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }

 private:
  int fd_;
};

using unique_fd = unique_fd_impl<DefaultCloser>;

struct borrowed_fd {
  /* implicit */ borrowed_fd(int fd) : fd_(fd) {}
  template <typename Closer>
  /* implicit */ borrowed_fd(const unique_fd_impl<Closer>& ufd)
      : fd_(ufd.get()) {}

  int get() const { return fd_; }
  bool operator==(const borrowed_fd& rhs) const { return fd_ == rhs.fd_; }
  bool operator!=(const borrowed_fd& rhs) const { return fd_ != rhs.fd_; }

 private:
  int fd_ = -1;
};

template <typename Closer>
inline bool Pipe(unique_fd_impl<Closer>* read, unique_fd_impl<Closer>* write,
                 int flags = O_CLOEXEC) {
  int pipefd[2];
  if (pipe2(pipefd, flags) != 0) return false;
  read->reset(pipefd[0]);
  write->reset(pipefd[1]);
  return true;
}

template <typename Closer>
inline bool Socketpair(int domain, int type, int protocol,
                       unique_fd_impl<Closer>* left,
                       unique_fd_impl<Closer>* right) {
  int sockfd[2];
  if (socketpair(domain, type, protocol, sockfd) != 0) return false;
  left->reset(sockfd[0]);
  right->reset(sockfd[1]);
  return true;
}

template <typename Closer>
inline bool Socketpair(int type, unique_fd_impl<Closer>* left,
                       unique_fd_impl<Closer>* right) {
  return Socketpair(AF_UNIX, type, 0, left, right);
}

}  // namespace base
}  // namespace android
