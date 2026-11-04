#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gflags {

using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;

enum FlagSettingMode {
  SET_FLAGS_VALUE,
  SET_FLAG_IF_DEFAULT,
  SET_FLAGS_DEFAULT
};

struct CommandLineFlagInfo {
  std::string name;
  std::string type;
  std::string description;
  std::string current_value;
  std::string default_value;
  std::string filename;
  bool has_validator_fn = false;
  bool is_default = true;
  const void* flag_ptr = nullptr;
};

uint32 ParseCommandLineFlags(int* argc, char*** argv, bool remove_flags);
uint32 ParseCommandLineNonHelpFlags(int* argc, char*** argv, bool remove_flags);

bool GetCommandLineFlagInfo(const char* name, CommandLineFlagInfo* OUTPUT);
CommandLineFlagInfo GetCommandLineFlagInfoOrDie(const char* name);
void GetAllFlags(std::vector<CommandLineFlagInfo>* OUTPUT);

std::string SetCommandLineOption(const char* name, const char* value);
std::string SetCommandLineOptionWithMode(const char* name, const char* value,
                                         FlagSettingMode set_mode);

void SetUsageMessage(const std::string& usage);
const char* ProgramUsage();

void SetVersionString(const std::string& version);
const char* VersionString();

void ShutDownCommandLineFlags();

bool RegisterFlagValidator(const bool* flag,
                           bool (*validate_fn)(const char*, bool));
bool RegisterFlagValidator(const int32* flag,
                           bool (*validate_fn)(const char*, int32));
bool RegisterFlagValidator(const uint32* flag,
                           bool (*validate_fn)(const char*, uint32));
bool RegisterFlagValidator(const int64* flag,
                           bool (*validate_fn)(const char*, int64));
bool RegisterFlagValidator(const uint64* flag,
                           bool (*validate_fn)(const char*, uint64));
bool RegisterFlagValidator(const double* flag,
                           bool (*validate_fn)(const char*, double));
bool RegisterFlagValidator(const std::string* flag,
                           bool (*validate_fn)(const char*,
                                               const std::string&));

void RegisterFlag(const char* name, const char* help, const char* filename,
                  bool* current_val, bool* default_val);
void RegisterFlag(const char* name, const char* help, const char* filename,
                  int32* current_val, int32* default_val);
void RegisterFlag(const char* name, const char* help, const char* filename,
                  uint32* current_val, uint32* default_val);
void RegisterFlag(const char* name, const char* help, const char* filename,
                  int64* current_val, int64* default_val);
void RegisterFlag(const char* name, const char* help, const char* filename,
                  uint64* current_val, uint64* default_val);
void RegisterFlag(const char* name, const char* help, const char* filename,
                  double* current_val, double* default_val);
void RegisterFlag(const char* name, const char* help, const char* filename,
                  std::string* current_val, std::string* default_val);

class FlagRegisterer {
 public:
  template <typename FlagType>
  FlagRegisterer(const char* name, const char* help, const char* filename,
                 FlagType* current_storage, FlagType* defvalue_storage) {
    RegisterFlag(name, help, filename, current_storage, defvalue_storage);
  }
};

}  // namespace gflags

namespace google {
using gflags::CommandLineFlagInfo;
using gflags::FlagRegisterer;
using gflags::FlagSettingMode;
using gflags::GetAllFlags;
using gflags::GetCommandLineFlagInfo;
using gflags::GetCommandLineFlagInfoOrDie;
using gflags::int32;
using gflags::int64;
using gflags::ParseCommandLineFlags;
using gflags::ParseCommandLineNonHelpFlags;
using gflags::ProgramUsage;
using gflags::RegisterFlagValidator;
using gflags::SET_FLAG_IF_DEFAULT;
using gflags::SET_FLAGS_DEFAULT;
using gflags::SET_FLAGS_VALUE;
using gflags::SetCommandLineOption;
using gflags::SetCommandLineOptionWithMode;
using gflags::SetUsageMessage;
using gflags::SetVersionString;
using gflags::ShutDownCommandLineFlags;
using gflags::uint32;
using gflags::uint64;
using gflags::VersionString;
}  // namespace google

#define GFLAGS_DEFINE_VARIABLE(type, shorttype, name, val, help) \
  namespace fL##shorttype {                                      \
    static const type FLAGS_nono##name = (val);                  \
    type FLAGS_##name = FLAGS_nono##name;                        \
    type FLAGS_no##name = FLAGS_nono##name;                      \
    static ::gflags::FlagRegisterer o_##name(                    \
        #name, (help), __FILE__, &FLAGS_##name, &FLAGS_no##name);\
  }                                                              \
  using fL##shorttype::FLAGS_##name

#define GFLAGS_DECLARE_VARIABLE(type, shorttype, name) \
  namespace fL##shorttype {                            \
    extern type FLAGS_##name;                          \
  }                                                    \
  using fL##shorttype::FLAGS_##name

#define DEFINE_bool(name, val, txt) \
  GFLAGS_DEFINE_VARIABLE(bool, B, name, val, txt)
#define DEFINE_int32(name, val, txt) \
  GFLAGS_DEFINE_VARIABLE(::gflags::int32, I, name, val, txt)
#define DEFINE_uint32(name, val, txt) \
  GFLAGS_DEFINE_VARIABLE(::gflags::uint32, U, name, val, txt)
#define DEFINE_int64(name, val, txt) \
  GFLAGS_DEFINE_VARIABLE(::gflags::int64, I64, name, val, txt)
#define DEFINE_uint64(name, val, txt) \
  GFLAGS_DEFINE_VARIABLE(::gflags::uint64, U64, name, val, txt)
#define DEFINE_double(name, val, txt) \
  GFLAGS_DEFINE_VARIABLE(double, D, name, val, txt)
#define DEFINE_string(name, val, txt) \
  GFLAGS_DEFINE_VARIABLE(std::string, S, name, val, txt)

#define DECLARE_bool(name) GFLAGS_DECLARE_VARIABLE(bool, B, name)
#define DECLARE_int32(name) GFLAGS_DECLARE_VARIABLE(::gflags::int32, I, name)
#define DECLARE_uint32(name) GFLAGS_DECLARE_VARIABLE(::gflags::uint32, U, name)
#define DECLARE_int64(name) GFLAGS_DECLARE_VARIABLE(::gflags::int64, I64, name)
#define DECLARE_uint64(name) GFLAGS_DECLARE_VARIABLE(::gflags::uint64, U64, name)
#define DECLARE_double(name) GFLAGS_DECLARE_VARIABLE(double, D, name)
#define DECLARE_string(name) GFLAGS_DECLARE_VARIABLE(std::string, S, name)

#define DEFINE_validator(name, validator)                     \
  static const bool FLAGS_##name##_validator_registered =     \
      ::gflags::RegisterFlagValidator(&FLAGS_##name, validator)
