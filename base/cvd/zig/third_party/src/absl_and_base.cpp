#include "absl/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "android-base/cmsg.h"
#include "android-base/file.h"
#include "android-base/logging.h"
#include "android-base/mapped_file.h"
#include "android-base/parsebool.h"
#include "android-base/properties.h"
#include "android-base/strings.h"
#include "cutils/sockets.h"
#include "gflags/gflags.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0
#endif

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// absl::log implementation
// ============================================================================
namespace absl {
namespace {

std::atomic<LogSeverityAtLeast> g_min_log_level{LogSeverityAtLeast::kInfo};
std::atomic<LogSeverityAtLeast> g_stderr_threshold{LogSeverityAtLeast::kInfo};
std::atomic<int> g_vlog_level{0};
std::atomic<bool> g_enable_prefix{true};

std::mutex& SinkMutex() {
  static std::mutex m;
  return m;
}

std::vector<LogSink*>& RegisteredSinks() {
  static std::vector<LogSink*> sinks;
  return sinks;
}

}  // namespace

void InitializeLog() {}

LogSeverityAtLeast MinLogLevel() {
  return g_min_log_level.load(std::memory_order_relaxed);
}

void SetMinLogLevel(LogSeverityAtLeast severity) {
  g_min_log_level.store(severity, std::memory_order_relaxed);
}

LogSeverityAtLeast StderrThreshold() {
  return g_stderr_threshold.load(std::memory_order_relaxed);
}

void SetStderrThreshold(LogSeverityAtLeast severity) {
  g_stderr_threshold.store(severity, std::memory_order_relaxed);
}

void SetStderrThreshold(LogSeverity severity) {
  g_stderr_threshold.store(static_cast<LogSeverityAtLeast>(severity),
                           std::memory_order_relaxed);
}

int SetVLogLevel(std::string_view module_pattern, int log_level) {
  (void)module_pattern;
  return g_vlog_level.exchange(log_level, std::memory_order_relaxed);
}

int SetGlobalVLogLevel(int log_level) {
  return g_vlog_level.exchange(log_level, std::memory_order_relaxed);
}

int GlobalVLogLevel() { return g_vlog_level.load(std::memory_order_relaxed); }

int MaxVLogLevel() { return g_vlog_level.load(std::memory_order_relaxed); }

void EnableLogPrefix(bool on_off) {
  g_enable_prefix.store(on_off, std::memory_order_relaxed);
}

bool ShouldPrependLogPrefix() {
  return g_enable_prefix.load(std::memory_order_relaxed);
}

void SetProgramInvocationName(std::string_view name) { (void)name; }

void AddLogSink(LogSink* sink) {
  if (!sink) return;
  std::lock_guard<std::mutex> lock(SinkMutex());
  auto& sinks = RegisteredSinks();
  if (std::find(sinks.begin(), sinks.end(), sink) == sinks.end()) {
    sinks.push_back(sink);
  }
}

void RemoveLogSink(LogSink* sink) {
  std::lock_guard<std::mutex> lock(SinkMutex());
  auto& sinks = RegisteredSinks();
  sinks.erase(std::remove(sinks.begin(), sinks.end(), sink), sinks.end());
}

void FlushLogSinks() {
  std::lock_guard<std::mutex> lock(SinkMutex());
  for (LogSink* sink : RegisteredSinks()) {
    if (sink) sink->Flush();
  }
  std::fflush(stderr);
}

namespace log_internal {

void DispatchLogMessage(LogSeverity severity, int verbosity, const char* file,
                        int line, bool prefix, const std::string& msg) {
  std::string_view fname = file ? file : "";
  auto pos = fname.find_last_of("/\\");
  std::string_view bname =
      pos == std::string_view::npos ? fname : fname.substr(pos + 1);
  char sev_char = 'I';
  switch (severity) {
    case LogSeverity::kInfo:
      sev_char = 'I';
      break;
    case LogSeverity::kWarning:
      sev_char = 'W';
      break;
    case LogSeverity::kError:
      sev_char = 'E';
      break;
    case LogSeverity::kFatal:
      sev_char = 'F';
      break;
  }
  std::string with_prefix;
  if (prefix && ShouldPrependLogPrefix()) {
    with_prefix = std::string(1, sev_char) + " " + std::string(bname) + ":" +
                  std::to_string(line) + "] " + msg;
  } else {
    with_prefix = msg;
  }
  std::string with_newline = with_prefix + "\n";

  LogEntry entry(severity, verbosity, fname, line, prefix, 0, msg, with_prefix,
                 with_newline);
  bool has_sinks = false;
  {
    std::lock_guard<std::mutex> lock(SinkMutex());
    for (LogSink* sink : RegisteredSinks()) {
      if (sink) {
        sink->Send(entry);
        has_sinks = true;
      }
    }
  }

  const auto min_lvl = static_cast<int>(MinLogLevel());
  const auto thr_lvl = static_cast<int>(StderrThreshold());
  const auto sev = static_cast<int>(severity);
  if (sev >= min_lvl && (sev >= thr_lvl || !has_sinks)) {
    std::string_view formatted = entry.text_message_with_prefix_and_newline();
    std::fwrite(formatted.data(), 1, formatted.size(), stderr);
    if (sev >= static_cast<int>(LogSeverity::kError)) {
      std::fflush(stderr);
    }
  }
}

}  // namespace log_internal
}  // namespace absl

// ============================================================================
// gflags implementation
// ============================================================================
namespace gflags {
namespace {

enum class FlagValueKind {
  kBool,
  kInt32,
  kUInt32,
  kInt64,
  kUInt64,
  kDouble,
  kString,
};

struct FlagEntry {
  std::string name;
  std::string type;
  std::string description;
  std::string filename;
  FlagValueKind kind;
  void* current_ptr = nullptr;
  void* default_ptr = nullptr;
  bool is_default = true;

  std::string ValueToString(const void* ptr) const {
    switch (kind) {
      case FlagValueKind::kBool:
        return *static_cast<const bool*>(ptr) ? "true" : "false";
      case FlagValueKind::kInt32:
        return std::to_string(*static_cast<const int32_t*>(ptr));
      case FlagValueKind::kUInt32:
        return std::to_string(*static_cast<const uint32_t*>(ptr));
      case FlagValueKind::kInt64:
        return std::to_string(*static_cast<const int64_t*>(ptr));
      case FlagValueKind::kUInt64:
        return std::to_string(*static_cast<const uint64_t*>(ptr));
      case FlagValueKind::kDouble: {
        std::ostringstream oss;
        oss << *static_cast<const double*>(ptr);
        return oss.str();
      }
      case FlagValueKind::kString:
        return *static_cast<const std::string*>(ptr);
    }
    return "";
  }

  bool SetFromString(const std::string& val) {
    switch (kind) {
      case FlagValueKind::kBool: {
        if (val.empty() || val == "true" || val == "1" || val == "t" ||
            val == "yes" || val == "y") {
          *static_cast<bool*>(current_ptr) = true;
          is_default = false;
          return true;
        }
        if (val == "false" || val == "0" || val == "f" || val == "no" ||
            val == "n") {
          *static_cast<bool*>(current_ptr) = false;
          is_default = false;
          return true;
        }
        return false;
      }
      case FlagValueKind::kInt32: {
        char* end = nullptr;
        long v = std::strtol(val.c_str(), &end, 0);
        if (!end || *end != '\0') return false;
        *static_cast<int32_t*>(current_ptr) = static_cast<int32_t>(v);
        is_default = false;
        return true;
      }
      case FlagValueKind::kUInt32: {
        char* end = nullptr;
        unsigned long v = std::strtoul(val.c_str(), &end, 0);
        if (!end || *end != '\0') return false;
        *static_cast<uint32_t*>(current_ptr) = static_cast<uint32_t>(v);
        is_default = false;
        return true;
      }
      case FlagValueKind::kInt64: {
        char* end = nullptr;
        long long v = std::strtoll(val.c_str(), &end, 0);
        if (!end || *end != '\0') return false;
        *static_cast<int64_t*>(current_ptr) = static_cast<int64_t>(v);
        is_default = false;
        return true;
      }
      case FlagValueKind::kUInt64: {
        char* end = nullptr;
        unsigned long long v = std::strtoull(val.c_str(), &end, 0);
        if (!end || *end != '\0') return false;
        *static_cast<uint64_t*>(current_ptr) = static_cast<uint64_t>(v);
        is_default = false;
        return true;
      }
      case FlagValueKind::kDouble: {
        char* end = nullptr;
        double v = std::strtod(val.c_str(), &end);
        if (!end || *end != '\0') return false;
        *static_cast<double*>(current_ptr) = v;
        is_default = false;
        return true;
      }
      case FlagValueKind::kString: {
        *static_cast<std::string*>(current_ptr) = val;
        is_default = false;
        return true;
      }
    }
    return false;
  }

  CommandLineFlagInfo ToInfo() const {
    CommandLineFlagInfo info;
    info.name = name;
    info.type = type;
    info.description = description;
    info.current_value = ValueToString(current_ptr);
    info.default_value = ValueToString(default_ptr);
    info.filename = filename;
    info.has_validator_fn = false;
    info.is_default = is_default;
    info.flag_ptr = current_ptr;
    return info;
  }
};

std::mutex& FlagsMutex() {
  static std::mutex m;
  return m;
}

std::map<std::string, FlagEntry>& FlagRegistry() {
  static std::map<std::string, FlagEntry> reg;
  return reg;
}

std::string& UsageMsg() {
  static std::string msg;
  return msg;
}

std::string& VersionStr() {
  static std::string ver;
  return ver;
}

void RegisterFlagInternal(const char* name, const char* type, const char* help,
                          const char* filename, FlagValueKind kind,
                          void* current_val, void* default_val) {
  std::lock_guard<std::mutex> lock(FlagsMutex());
  FlagEntry entry;
  entry.name = name ? name : "";
  entry.type = type ? type : "";
  entry.description = help ? help : "";
  entry.filename = filename ? filename : "";
  entry.kind = kind;
  entry.current_ptr = current_val;
  entry.default_ptr = default_val;
  entry.is_default = true;
  FlagRegistry()[entry.name] = std::move(entry);
}

}  // namespace

void RegisterFlag(const char* name, const char* help, const char* filename,
                  bool* current_val, bool* default_val) {
  RegisterFlagInternal(name, "bool", help, filename, FlagValueKind::kBool,
                       current_val, default_val);
}

void RegisterFlag(const char* name, const char* help, const char* filename,
                  int32_t* current_val, int32_t* default_val) {
  RegisterFlagInternal(name, "int32", help, filename, FlagValueKind::kInt32,
                       current_val, default_val);
}

void RegisterFlag(const char* name, const char* help, const char* filename,
                  uint32_t* current_val, uint32_t* default_val) {
  RegisterFlagInternal(name, "uint32", help, filename, FlagValueKind::kUInt32,
                       current_val, default_val);
}

void RegisterFlag(const char* name, const char* help, const char* filename,
                  int64_t* current_val, int64_t* default_val) {
  RegisterFlagInternal(name, "int64", help, filename, FlagValueKind::kInt64,
                       current_val, default_val);
}

void RegisterFlag(const char* name, const char* help, const char* filename,
                  uint64_t* current_val, uint64_t* default_val) {
  RegisterFlagInternal(name, "uint64", help, filename, FlagValueKind::kUInt64,
                       current_val, default_val);
}

void RegisterFlag(const char* name, const char* help, const char* filename,
                  double* current_val, double* default_val) {
  RegisterFlagInternal(name, "double", help, filename, FlagValueKind::kDouble,
                       current_val, default_val);
}

void RegisterFlag(const char* name, const char* help, const char* filename,
                  std::string* current_val, std::string* default_val) {
  RegisterFlagInternal(name, "string", help, filename, FlagValueKind::kString,
                       current_val, default_val);
}

uint32_t ParseCommandLineFlags(int* argc, char*** argv, bool remove_flags) {
  if (!argc || !argv || !*argv) return 0;
  std::vector<char*> remaining;
  remaining.push_back((*argv)[0]);

  int i = 1;
  while (i < *argc) {
    const char* arg = (*argv)[i];
    if (!arg || arg[0] != '-' || std::strcmp(arg, "-") == 0) {
      remaining.push_back((*argv)[i]);
      ++i;
      continue;
    }
    if (std::strcmp(arg, "--") == 0) {
      if (!remove_flags) remaining.push_back((*argv)[i]);
      ++i;
      while (i < *argc) {
        remaining.push_back((*argv)[i]);
        ++i;
      }
      break;
    }

    const char* name_start = (arg[1] == '-') ? arg + 2 : arg + 1;
    const char* eq = std::strchr(name_start, '=');
    std::string name;
    std::string value;
    bool has_eq = (eq != nullptr);
    if (has_eq) {
      name.assign(name_start, eq);
      value.assign(eq + 1);
    } else {
      name.assign(name_start);
    }

    if (name == "help" || name == "h" || name == "helpshort") {
      std::fprintf(stdout, "Usage: %s [flags]\n", (*argv)[0]);
      std::lock_guard<std::mutex> lock(FlagsMutex());
      for (const auto& kv : FlagRegistry()) {
        std::fprintf(stdout, "  -%s (%s) type: %s default: %s\n",
                     kv.second.name.c_str(), kv.second.description.c_str(),
                     kv.second.type.c_str(),
                     kv.second.ValueToString(kv.second.default_ptr).c_str());
      }
      std::fflush(stdout);
      std::exit(0);
    }

    bool handled = false;
    {
      std::lock_guard<std::mutex> lock(FlagsMutex());
      auto& reg = FlagRegistry();
      auto it = reg.find(name);
      if (it != reg.end()) {
        if (has_eq) {
          handled = it->second.SetFromString(value);
        } else if (it->second.kind == FlagValueKind::kBool) {
          handled = it->second.SetFromString("true");
        } else if (i + 1 < *argc && (*argv)[i + 1] != nullptr) {
          handled = it->second.SetFromString((*argv)[i + 1]);
          if (handled) ++i;
        }
      } else if (!has_eq && name.rfind("no", 0) == 0 && name.size() > 2) {
        auto bool_it = reg.find(name.substr(2));
        if (bool_it != reg.end() &&
            bool_it->second.kind == FlagValueKind::kBool) {
          handled = bool_it->second.SetFromString("false");
        }
      }
    }

    if (!handled || !remove_flags) {
      remaining.push_back((*argv)[i]);
    }
    ++i;
  }

  if (remove_flags) {
    for (size_t j = 0; j < remaining.size(); ++j) {
      (*argv)[j] = remaining[j];
    }
    (*argv)[remaining.size()] = nullptr;
    *argc = static_cast<int>(remaining.size());
  }
  return 1;
}

uint32_t ParseCommandLineNonHelpFlags(int* argc, char*** argv,
                                      bool remove_flags) {
  return ParseCommandLineFlags(argc, argv, remove_flags);
}

void ReparseCommandLineNonHelpFlags() {}
void AllowCommandLineReparsing() {}
void ShutDownCommandLineFlags() {}

void SetUsageMessage(const std::string& usage) {
  std::lock_guard<std::mutex> lock(FlagsMutex());
  UsageMsg() = usage;
}

const char* ProgramUsage() {
  std::lock_guard<std::mutex> lock(FlagsMutex());
  return UsageMsg().c_str();
}

void SetVersionString(const std::string& version) {
  std::lock_guard<std::mutex> lock(FlagsMutex());
  VersionStr() = version;
}

const char* VersionString() {
  std::lock_guard<std::mutex> lock(FlagsMutex());
  return VersionStr().c_str();
}

bool GetCommandLineOption(const char* name, std::string* value) {
  if (!name || !value) return false;
  std::lock_guard<std::mutex> lock(FlagsMutex());
  auto& reg = FlagRegistry();
  auto it = reg.find(name);
  if (it == reg.end()) return false;
  *value = it->second.ValueToString(it->second.current_ptr);
  return true;
}

bool GetCommandLineFlagInfo(const char* name, CommandLineFlagInfo* OUTPUT) {
  if (!name || !OUTPUT) return false;
  std::lock_guard<std::mutex> lock(FlagsMutex());
  auto& reg = FlagRegistry();
  auto it = reg.find(name);
  if (it == reg.end()) return false;
  *OUTPUT = it->second.ToInfo();
  return true;
}

CommandLineFlagInfo GetCommandLineFlagInfoOrDie(const char* name) {
  CommandLineFlagInfo info;
  if (!GetCommandLineFlagInfo(name, &info)) {
    std::fprintf(stderr, "Flag '%s' does not exist\n", name ? name : "(null)");
    std::abort();
  }
  return info;
}

std::string SetCommandLineOption(const char* name, const char* value) {
  return SetCommandLineOptionWithMode(name, value, SET_FLAGS_VALUE);
}

std::string SetCommandLineOptionWithMode(const char* name, const char* value,
                                         FlagSettingMode set_mode) {
  if (!name || !value) return "";
  std::lock_guard<std::mutex> lock(FlagsMutex());
  auto& reg = FlagRegistry();
  auto it = reg.find(name);
  if (it == reg.end()) return "";
  if (set_mode == SET_FLAG_IF_DEFAULT && !it->second.is_default) {
    return it->second.ValueToString(it->second.current_ptr);
  }
  if (!it->second.SetFromString(value)) {
    return "";
  }
  if (set_mode == SET_FLAGS_DEFAULT) {
    it->second.is_default = true;
  }
  return std::string(name) + " set to " + value + "\n";
}

void GetAllFlags(std::vector<CommandLineFlagInfo>* OUTPUT) {
  if (!OUTPUT) return;
  OUTPUT->clear();
  std::lock_guard<std::mutex> lock(FlagsMutex());
  for (const auto& kv : FlagRegistry()) {
    OUTPUT->push_back(kv.second.ToInfo());
  }
}

void ShowUsageWithFlagsRestrict(const char* argv0, const char* restrict_arg) {
  (void)argv0;
  (void)restrict_arg;
}

void HandleCommandLineHelpFlags() {}

bool RegisterFlagValidator(const bool* flag,
                           bool (*validate_fn)(const char*, bool)) {
  (void)flag;
  (void)validate_fn;
  return true;
}
bool RegisterFlagValidator(const int32* flag,
                           bool (*validate_fn)(const char*, int32)) {
  (void)flag;
  (void)validate_fn;
  return true;
}
bool RegisterFlagValidator(const uint32* flag,
                           bool (*validate_fn)(const char*, uint32)) {
  (void)flag;
  (void)validate_fn;
  return true;
}
bool RegisterFlagValidator(const int64* flag,
                           bool (*validate_fn)(const char*, int64)) {
  (void)flag;
  (void)validate_fn;
  return true;
}
bool RegisterFlagValidator(const uint64* flag,
                           bool (*validate_fn)(const char*, uint64)) {
  (void)flag;
  (void)validate_fn;
  return true;
}
bool RegisterFlagValidator(const double* flag,
                           bool (*validate_fn)(const char*, double)) {
  (void)flag;
  (void)validate_fn;
  return true;
}
bool RegisterFlagValidator(const std::string* flag,
                           bool (*validate_fn)(const char*,
                                               const std::string&)) {
  (void)flag;
  (void)validate_fn;
  return true;
}

}  // namespace gflags

// ============================================================================
// android::base implementation
// ============================================================================
namespace android {
namespace base {

// File utilities
bool ReadFdToString(borrowed_fd fd, std::string* content) {
  content->clear();
  char buf[4096];
  ssize_t n;
  while ((n = TEMP_FAILURE_RETRY(::read(fd.get(), buf, sizeof(buf)))) > 0) {
    content->append(buf, static_cast<size_t>(n));
  }
  return (n == 0);
}

bool ReadFileToString(const std::string& path, std::string* content,
                      bool follow_symlinks) {
  content->clear();
  int flags = O_RDONLY | O_CLOEXEC | (follow_symlinks ? 0 : O_NOFOLLOW);
  int fd = TEMP_FAILURE_RETRY(::open(path.c_str(), flags));
  if (fd == -1) return false;
  bool ok = ReadFdToString(fd, content);
  ::close(fd);
  return ok;
}

bool WriteStringToFd(std::string_view content, borrowed_fd fd) {
  return WriteFully(fd, content.data(), content.size());
}

bool WriteStringToFile(const std::string& content, const std::string& path,
                       bool follow_symlinks) {
  int flags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC |
              (follow_symlinks ? 0 : O_NOFOLLOW);
  int fd = TEMP_FAILURE_RETRY(::open(path.c_str(), flags, 0666));
  if (fd == -1) return false;
  bool ok = WriteStringToFd(content, fd);
  ::close(fd);
  return ok;
}

bool WriteStringToFile(const std::string& content, const std::string& path,
                       mode_t mode, uid_t owner, gid_t group,
                       bool follow_symlinks) {
  int flags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC |
              (follow_symlinks ? 0 : O_NOFOLLOW);
  int fd = TEMP_FAILURE_RETRY(::open(path.c_str(), flags, mode));
  if (fd == -1) return false;
  if (::fchmod(fd, mode) == -1) {
    ::close(fd);
    return false;
  }
  if (::fchown(fd, owner, group) == -1) {
    ::close(fd);
    return false;
  }
  bool ok = WriteStringToFd(content, fd);
  ::close(fd);
  return ok;
}

bool ReadFully(borrowed_fd fd, void* data, size_t byte_count) {
  uint8_t* p = reinterpret_cast<uint8_t*>(data);
  size_t remaining = byte_count;
  while (remaining > 0) {
    ssize_t n = TEMP_FAILURE_RETRY(::read(fd.get(), p, remaining));
    if (n <= 0) return false;
    p += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

bool ReadFullyAtOffset(borrowed_fd fd, void* data, size_t byte_count,
                       off64_t offset) {
  uint8_t* p = reinterpret_cast<uint8_t*>(data);
  size_t remaining = byte_count;
  while (remaining > 0) {
    ssize_t n = TEMP_FAILURE_RETRY(::pread(fd.get(), p, remaining, offset));
    if (n <= 0) return false;
    p += n;
    offset += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

bool WriteFully(borrowed_fd fd, const void* data, size_t byte_count) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  size_t remaining = byte_count;
  while (remaining > 0) {
    ssize_t n = TEMP_FAILURE_RETRY(::write(fd.get(), p, remaining));
    if (n == -1) return false;
    p += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

bool RemoveFileIfExists(const std::string& path, std::string* err) {
  if (::unlink(path.c_str()) == -1) {
    if (errno == ENOENT || errno == ENOTDIR) return true;
    if (err) *err = std::strerror(errno);
    return false;
  }
  return true;
}

bool Readlink(const std::string& path, std::string* result) {
  result->clear();
  char buf[PATH_MAX];
  ssize_t len = ::readlink(path.c_str(), buf, sizeof(buf));
  if (len < 0) return false;
  result->assign(buf, static_cast<size_t>(len));
  return true;
}

bool Realpath(const std::string& path, std::string* result) {
  result->clear();
  char* resolved = ::realpath(path.c_str(), nullptr);
  if (!resolved) return false;
  *result = resolved;
  std::free(resolved);
  return true;
}

TemporaryFile::TemporaryFile() { init("."); }
TemporaryFile::TemporaryFile(const std::string& tmp_dir) { init(tmp_dir); }
TemporaryFile::~TemporaryFile() {
  if (fd != -1) ::close(fd);
  if (remove_ && path[0] != '\0') ::unlink(path);
}

int TemporaryFile::release() {
  int r = fd;
  fd = -1;
  return r;
}

void TemporaryFile::init(const std::string& tmp_dir) {
  std::string dir = tmp_dir.empty() ? "." : tmp_dir;
  std::snprintf(path, sizeof(path), "%s/.cf_tmp_file_%d_%p", dir.c_str(),
                static_cast<int>(::getpid()), static_cast<void*>(this));
  fd = ::open(path, O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0600);
}

TemporaryDir::TemporaryDir() { init("."); }

bool TemporaryDir::init(const std::string& tmp_dir) {
  std::string dir = tmp_dir.empty() ? "." : tmp_dir;
  std::snprintf(path, sizeof(path), "%s/.cf_tmp_dir_%d_%p", dir.c_str(),
                static_cast<int>(::getpid()), static_cast<void*>(this));
  return ::mkdir(path, 0700) == 0;
}

TemporaryDir::~TemporaryDir() {
  if (remove_ && path[0] != '\0') {
    ::rmdir(path);
  }
}

std::string GetExecutablePath() {
  std::string result;
  Readlink("/proc/self/exe", &result);
  return result;
}

std::string GetExecutableDirectory() { return Dirname(GetExecutablePath()); }

std::string Basename(std::string_view path) {
  std::string copy(path);
  return ::basename(&copy[0]);
}

std::string Dirname(std::string_view path) {
  std::string copy(path);
  return ::dirname(&copy[0]);
}

// cmsg utilities
ssize_t SendFileDescriptorVector(borrowed_fd sockfd, const void* data,
                                 size_t len, const std::vector<int>& fds) {
  struct iovec iov;
  iov.iov_base = const_cast<void*>(data);
  iov.iov_len = len;

  struct msghdr msg;
  std::memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  std::vector<char> cmsg_buf;
  if (!fds.empty()) {
    size_t fds_bytes = fds.size() * sizeof(int);
    cmsg_buf.resize(CMSG_SPACE(fds_bytes), 0);
    msg.msg_control = cmsg_buf.data();
    msg.msg_controllen = cmsg_buf.size();

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(fds_bytes);
    std::memcpy(CMSG_DATA(cmsg), fds.data(), fds_bytes);
  }

  return TEMP_FAILURE_RETRY(::sendmsg(sockfd.get(), &msg, MSG_NOSIGNAL));
}

ssize_t ReceiveFileDescriptorVector(borrowed_fd sockfd, void* data, size_t len,
                                    size_t max_fds,
                                    std::vector<unique_fd>* fds) {
  if (fds) fds->clear();

  struct iovec iov;
  iov.iov_base = data;
  iov.iov_len = len;

  struct msghdr msg;
  std::memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  std::vector<char> cmsg_buf;
  if (max_fds > 0) {
    cmsg_buf.resize(CMSG_SPACE(max_fds * sizeof(int)), 0);
    msg.msg_control = cmsg_buf.data();
    msg.msg_controllen = cmsg_buf.size();
  }

  ssize_t rc =
      TEMP_FAILURE_RETRY(::recvmsg(sockfd.get(), &msg, MSG_CMSG_CLOEXEC));
  if (rc < 0) return rc;

  for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      size_t count = payload_len / sizeof(int);
      const int* raw_fds = reinterpret_cast<const int*>(CMSG_DATA(cmsg));
      for (size_t i = 0; i < count; ++i) {
        if (fds && fds->size() < max_fds) {
          fds->emplace_back(raw_fds[i]);
        } else {
          ::close(raw_fds[i]);
        }
      }
    }
  }
  return rc;
}

// Logging
namespace {

std::atomic<LogSeverity> g_android_min_severity{INFO};
std::mutex g_log_lock;
LogFunction g_logger = StderrLogger;
AbortFunction g_aborter = DefaultAborter;
std::string g_default_tag = "cuttlefish";

char SeverityChar(LogSeverity s) {
  switch (s) {
    case VERBOSE:
      return 'V';
    case DEBUG:
      return 'D';
    case INFO:
      return 'I';
    case WARNING:
      return 'W';
    case ERROR:
      return 'E';
    case FATAL_WITHOUT_ABORT:
    case FATAL:
      return 'F';
  }
  return 'I';
}

}  // namespace

void DefaultAborter(const char* abort_message) {
  if (abort_message) {
    std::fprintf(stderr, "ABORT: %s\n", abort_message);
  }
  std::abort();
}

void StdioLogger(LogId id, LogSeverity severity, const char* tag,
                 const char* file, unsigned int line, const char* message) {
  (void)id;
  FILE* out = (severity >= WARNING) ? stderr : stdout;
  std::fprintf(out, "%c %s: %s:%u %s\n", SeverityChar(severity),
               tag ? tag : "cuttlefish", file ? file : "", line,
               message ? message : "");
  std::fflush(out);
}

void StderrLogger(LogId id, LogSeverity severity, const char* tag,
                  const char* file, unsigned int line, const char* message) {
  (void)id;
  std::fprintf(stderr, "%c %s: %s:%u %s\n", SeverityChar(severity),
               tag ? tag : "cuttlefish", file ? file : "", line,
               message ? message : "");
  std::fflush(stderr);
}

void InitLogging(char* argv[], LogFunction&& logger, AbortFunction&& aborter) {
  if (argv && argv[0]) {
    g_default_tag = Basename(argv[0]);
  }
  if (logger) SetLogger(std::move(logger));
  if (aborter) SetAborter(std::move(aborter));
}

void SetLogger(LogFunction&& logger) {
  std::lock_guard<std::mutex> lock(g_log_lock);
  g_logger = std::move(logger);
}

void SetAborter(AbortFunction&& aborter) {
  std::lock_guard<std::mutex> lock(g_log_lock);
  g_aborter = std::move(aborter);
}

std::string GetDefaultTag() {
  std::lock_guard<std::mutex> lock(g_log_lock);
  return g_default_tag;
}

void SetDefaultTag(const std::string& tag) {
  std::lock_guard<std::mutex> lock(g_log_lock);
  g_default_tag = tag;
}

LogSeverity GetMinimumLogSeverity() {
  return g_android_min_severity.load(std::memory_order_relaxed);
}

LogSeverity SetMinimumLogSeverity(LogSeverity new_severity) {
  return g_android_min_severity.exchange(new_severity,
                                         std::memory_order_relaxed);
}

bool ShouldLog(LogSeverity severity, const char* tag) {
  (void)tag;
  return severity >= GetMinimumLogSeverity();
}

}  // namespace base
}  // namespace android

// ============================================================================
// cutils sockets implementation
// ============================================================================
extern "C" {

static int MakeUnixAddr(const char* name, int namespace_id,
                        struct sockaddr_un* addr, socklen_t* alen) {
  if (!name || !addr || !alen) return -1;
  std::memset(addr, 0, sizeof(*addr));
  addr->sun_family = AF_UNIX;
  size_t namelen = std::strlen(name);
  if (namespace_id == ANDROID_SOCKET_NAMESPACE_ABSTRACT) {
    if (namelen + 1 > sizeof(addr->sun_path)) return -1;
    addr->sun_path[0] = '\0';
    std::memcpy(addr->sun_path + 1, name, namelen);
    *alen = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + 1 +
                                   namelen);
  } else {
    if (namelen >= sizeof(addr->sun_path)) return -1;
    std::memcpy(addr->sun_path, name, namelen + 1);
    *alen = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) +
                                   namelen + 1);
  }
  return 0;
}

int socket_local_client_connect(int fd, const char* name, int namespace_id,
                                int type) {
  (void)type;
  struct sockaddr_un addr;
  socklen_t alen = 0;
  if (MakeUnixAddr(name, namespace_id, &addr, &alen) < 0) return -1;
  return ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), alen);
}

int socket_local_client(const char* name, int namespace_id, int type) {
  int fd = ::socket(AF_UNIX, type | SOCK_CLOEXEC, 0);
  if (fd < 0) return -1;
  if (socket_local_client_connect(fd, name, namespace_id, type) < 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

int socket_local_server_bind(int s, const char* name, int namespace_id) {
  struct sockaddr_un addr;
  socklen_t alen = 0;
  if (MakeUnixAddr(name, namespace_id, &addr, &alen) < 0) return -1;
  if (namespace_id != ANDROID_SOCKET_NAMESPACE_ABSTRACT) {
    ::unlink(addr.sun_path);
  }
  int n = 1;
  ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));
  return ::bind(s, reinterpret_cast<struct sockaddr*>(&addr), alen);
}

int socket_local_server(const char* name, int namespace_id, int type) {
  int s = ::socket(AF_UNIX, type | SOCK_CLOEXEC, 0);
  if (s < 0) return -1;
  if (socket_local_server_bind(s, name, namespace_id) < 0) {
    ::close(s);
    return -1;
  }
  if (type == SOCK_STREAM || type == SOCK_SEQPACKET) {
    if (::listen(s, SOMAXCONN) < 0) {
      ::close(s);
      return -1;
    }
  }
  return s;
}

int socket_inaddr_any_server(int port, int type) {
  int s = ::socket(AF_INET6, type | SOCK_CLOEXEC, 0);
  if (s < 0) return -1;
  int n = 1;
  ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));
  int off = 0;
  ::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

  struct sockaddr_in6 addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(static_cast<uint16_t>(port));
  addr.sin6_addr = in6addr_any;

  if (::bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(s);
    return -1;
  }
  if (type == SOCK_STREAM) {
    if (::listen(s, SOMAXCONN) < 0) {
      ::close(s);
      return -1;
    }
  }
  return s;
}

int socket_network_client(const char* host, int port, int type) {
  int s = ::socket(AF_INET, type | SOCK_CLOEXEC, 0);
  if (s < 0) return -1;
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET, host ? host : "127.0.0.1", &addr.sin_addr) <= 0) {
    ::close(s);
    return -1;
  }
  if (::connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <
      0) {
    ::close(s);
    return -1;
  }
  return s;
}

int socket_get_local_port(int s) {
  struct sockaddr_storage addr;
  socklen_t alen = sizeof(addr);
  if (::getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &alen) < 0) {
    return -1;
  }
  if (addr.ss_family == AF_INET) {
    return ntohs(reinterpret_cast<struct sockaddr_in*>(&addr)->sin_port);
  }
  if (addr.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<struct sockaddr_in6*>(&addr)->sin6_port);
  }
  return -1;
}

int android_get_control_socket(const char* name) {
  if (!name) return -1;
  std::string key = std::string(ANDROID_SOCKET_ENV_PREFIX) + name;
  const char* val = ::getenv(key.c_str());
  if (!val) return -1;
  return std::atoi(val);
}

}  // extern "C"
