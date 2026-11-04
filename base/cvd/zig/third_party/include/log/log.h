#pragma once

#include <cstdio>

#ifndef LOG_TAG
#define LOG_TAG nullptr
#endif

typedef enum android_LogPriority {
  ANDROID_LOG_UNKNOWN = 0,
  ANDROID_LOG_DEFAULT,
  ANDROID_LOG_VERBOSE,
  ANDROID_LOG_DEBUG,
  ANDROID_LOG_INFO,
  ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR,
  ANDROID_LOG_FATAL,
  ANDROID_LOG_SILENT,
} android_LogPriority;

#define ALOG(priority, tag, fmt, ...) \
  std::fprintf(stderr, "[%s] " fmt "\n", (tag) ? (tag) : "LOG", ##__VA_ARGS__)

#define ALOGV(fmt, ...) ALOG(LOG_VERBOSE, LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGD(fmt, ...) ALOG(LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGI(fmt, ...) ALOG(LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGW(fmt, ...) ALOG(LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGE(fmt, ...) ALOG(LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_ALWAYS_FATAL(fmt, ...)                \
  do {                                            \
    ALOG(LOG_FATAL, LOG_TAG, fmt, ##__VA_ARGS__); \
    __builtin_trap();                             \
  } while (0)
#define LOG_ALWAYS_FATAL_IF(cond, fmt, ...)       \
  do {                                            \
    if (cond) LOG_ALWAYS_FATAL(fmt, ##__VA_ARGS__); \
  } while (0)
