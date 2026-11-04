#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
  videoFormatRGB = 1,
  videoFormatRGBA = 2,
  videoFormatRGB555 = 3,
  videoFormatRGB565 = 4,
  videoFormatBGR = 5,
  videoFormatBGRA = 6,
  videoFormatABGR = 7,
  videoFormatARGB = 8,
  videoFormatYUY2 = 20,
  videoFormatYVYU = 21,
  videoFormatUYVY = 22,
  videoFormatI420 = 23,
  videoFormatYV12 = 24,
  videoFormatInternal = 25,
  videoFormatNV12 = 26,
  videoFormatVFlip = 0x80000000
} EVideoFormatType;

typedef enum {
  videoFrameTypeInvalid,
  videoFrameTypeIDR,
  videoFrameTypeI,
  videoFrameTypeP,
  videoFrameTypeSkip,
  videoFrameTypeIPMixed
} EVideoFrameType;

typedef enum {
  cmResultSuccess,
  cmInitParaError,
  cmUnknownReason,
  cmMallocMemeError,
  cmInitExpected,
  cmUnsupportedData
} CM_RETURN;

typedef enum {
  NON_VIDEO_CODING_LAYER = 0,
  VIDEO_CODING_LAYER = 1
} ELayerType;

typedef enum {
  NAL_UNKNOWN = 0,
  NAL_SLICE = 1,
  NAL_SLICE_DPA = 2,
  NAL_SLICE_DPB = 3,
  NAL_SLICE_DPC = 4,
  NAL_SLICE_IDR = 5,
  NAL_SEI = 6,
  NAL_SPS = 7,
  NAL_PPS = 8
} ENalUnitType;

typedef enum {
  NAL_PRIO_DISPOSABLE = 0,
  NAL_PRIO_LOW = 1,
  NAL_PRIO_HIGH = 2,
  NAL_PRIO_HIGHEST = 3
} ENalPriority;

typedef enum {
  dsErrorFree = 0x00,
  dsFramePending = 0x01,
  dsRefLost = 0x02,
  dsBitstreamError = 0x04,
  dsDepLayerLost = 0x08,
  dsNoParamSets = 0x10,
  dsDataErrorConcealed = 0x20,
  dsRefListNullPtrs = 0x40,
  dsInvalidArgument = 0x1000,
  dsInitialOptExpected = 0x2000,
  dsOutOfMemory = 0x4000,
  dsDstBufNeedExpan = 0x8000
} DECODING_STATE;

typedef struct TagSysMemBuffer {
  int iWidth;
  int iHeight;
  int iFormat;
  int iStride[2];
} SSysMemBuffer;

typedef struct TagBufferInfo {
  int iBufferStatus;
  unsigned long long uiInBsTimeStamp;
  unsigned long long uiOutYuvTimeStamp;
  union {
    SSysMemBuffer sSystemBuffer;
  } UsrData;
  unsigned char* pDst[3];
} SBufferInfo;

typedef struct Source_Picture_s {
  int iColorFormat;
  int iStride[4];
  unsigned char* pData[4];
  int iPicWidth;
  int iPicHeight;
  long long uiTimeStamp;
} SSourcePicture;
