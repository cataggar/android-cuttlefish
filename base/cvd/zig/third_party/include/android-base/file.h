#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <string_view>

#include "android-base/macros.h"
#include "android-base/unique_fd.h"

namespace android {
namespace base {

class TemporaryFile {
 public:
  TemporaryFile();
  explicit TemporaryFile(const std::string& temp_dir);
  ~TemporaryFile();

  int release();
  void DoNotRemove() { remove_ = false; }

  int fd = -1;
  char path[1024]{};

 private:
  void init(const std::string& tmp_dir);
  bool remove_ = true;
  DISALLOW_COPY_AND_ASSIGN(TemporaryFile);
};

class TemporaryDir {
 public:
  TemporaryDir();
  ~TemporaryDir();

  void DoNotRemove() { remove_ = false; }

  char path[1024]{};

 private:
  bool init(const std::string& tmp_dir);
  bool remove_ = true;
  DISALLOW_COPY_AND_ASSIGN(TemporaryDir);
};

bool ReadFdToString(borrowed_fd fd, std::string* content);
bool ReadFileToString(const std::string& path, std::string* content,
                      bool follow_symlinks = false);

bool WriteStringToFile(const std::string& content, const std::string& path,
                       bool follow_symlinks = false);
bool WriteStringToFile(const std::string& content, const std::string& path,
                       mode_t mode, uid_t owner, gid_t group,
                       bool follow_symlinks = true);
bool WriteStringToFd(std::string_view content, borrowed_fd fd);

bool ReadFully(borrowed_fd fd, void* data, size_t byte_count);
bool ReadFullyAtOffset(borrowed_fd fd, void* data, size_t byte_count,
                       off64_t offset);
bool WriteFully(borrowed_fd fd, const void* data, size_t byte_count);

bool RemoveFileIfExists(const std::string& path, std::string* err = nullptr);

bool Readlink(const std::string& path, std::string* result);
bool Realpath(const std::string& path, std::string* result);
std::string GetExecutablePath();
std::string GetExecutableDirectory();

std::string Basename(std::string_view path);
std::string Dirname(std::string_view path);

}  // namespace base
}  // namespace android

using ::android::base::TemporaryDir;
using ::android::base::TemporaryFile;
