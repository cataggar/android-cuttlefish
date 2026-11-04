#pragma once

#include <sys/mman.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "android-base/macros.h"

namespace android {
namespace base {

class MappedFile {
 public:
  static std::unique_ptr<MappedFile> FromFd(int fd, off64_t offset,
                                            size_t length, int prot) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;
    off64_t page_offset = offset % page_size;
    off64_t aligned_offset = offset - page_offset;
    size_t aligned_length = length + static_cast<size_t>(page_offset);
    void* base =
        mmap(nullptr, aligned_length, prot, MAP_SHARED, fd, aligned_offset);
    if (base == MAP_FAILED) return nullptr;
    return std::unique_ptr<MappedFile>(new MappedFile(
        static_cast<char*>(base), aligned_length,
        static_cast<size_t>(page_offset), length));
  }

  ~MappedFile() {
    if (base_ != nullptr) {
      munmap(base_, allocated_length_);
    }
  }

  char* data() const { return base_ + offset_; }
  size_t size() const { return size_; }

 private:
  MappedFile(char* base, size_t allocated_length, size_t offset, size_t size)
      : base_(base),
        allocated_length_(allocated_length),
        offset_(offset),
        size_(size) {}

  char* base_ = nullptr;
  size_t allocated_length_ = 0;
  size_t offset_ = 0;
  size_t size_ = 0;
  DISALLOW_COPY_AND_ASSIGN(MappedFile);
};

}  // namespace base
}  // namespace android
