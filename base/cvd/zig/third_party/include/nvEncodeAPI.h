#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NVENCAPI
#define NVENCAPI
#endif

#ifndef _GUID_DEFINED
#define _GUID_DEFINED
typedef struct _GUID {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
} GUID;
#endif

#define NVENCAPI_MAJOR_VERSION 12
#define NVENCAPI_MINOR_VERSION 2
#define NVENCAPI_VERSION (NVENCAPI_MAJOR_VERSION | (NVENCAPI_MINOR_VERSION << 24))
#define NVENCAPI_STRUCT_VERSION(ver) \
  ((uint32_t)NVENCAPI_VERSION | ((ver) << 16) | (0x7 << 28))

#define NVENC_INFINITE_GOPLENGTH 0xffffffff

typedef void* NV_ENC_INPUT_PTR;
typedef void* NV_ENC_OUTPUT_PTR;
typedef void* NV_ENC_REGISTERED_PTR;

typedef enum _NVENCSTATUS {
  NV_ENC_SUCCESS = 0,
  NV_ENC_ERR_NO_ENCODE_DEVICE = 1,
  NV_ENC_ERR_UNSUPPORTED_DEVICE = 2,
  NV_ENC_ERR_INVALID_ENCODERDEVICE = 3,
  NV_ENC_ERR_INVALID_DEVICE = 4,
  NV_ENC_ERR_DEVICE_NOT_EXIST = 5,
  NV_ENC_ERR_INVALID_PTR = 6,
  NV_ENC_ERR_INVALID_EVENT = 7,
  NV_ENC_ERR_INVALID_PARAM = 8,
  NV_ENC_ERR_INVALID_CALL = 9,
  NV_ENC_ERR_OUT_OF_MEMORY = 10,
  NV_ENC_ERR_ENCODER_NOT_INITIALIZED = 11,
  NV_ENC_ERR_UNSUPPORTED_PARAM = 12,
  NV_ENC_ERR_LOCK_BUSY = 13,
  NV_ENC_ERR_NOT_ENOUGH_BUFFER = 14,
  NV_ENC_ERR_INVALID_VERSION = 15,
  NV_ENC_ERR_MAP_FAILED = 16,
  NV_ENC_ERR_NEED_MORE_INPUT = 17,
  NV_ENC_ERR_ENCODER_BUSY = 18,
  NV_ENC_ERR_EVENT_NOT_REGISTERD = 19,
  NV_ENC_ERR_GENERIC = 20
} NVENCSTATUS;

typedef enum _NV_ENC_DEVICE_TYPE {
  NV_ENC_DEVICE_TYPE_DIRECTX = 0x0,
  NV_ENC_DEVICE_TYPE_CUDA = 0x1,
  NV_ENC_DEVICE_TYPE_OPENGL = 0x2
} NV_ENC_DEVICE_TYPE;

typedef enum _NV_ENC_BUFFER_FORMAT {
  NV_ENC_BUFFER_FORMAT_UNDEFINED = 0x00000000,
  NV_ENC_BUFFER_FORMAT_NV12 = 0x00000001,
  NV_ENC_BUFFER_FORMAT_YV12 = 0x00000010,
  NV_ENC_BUFFER_FORMAT_IYUV = 0x00000100,
  NV_ENC_BUFFER_FORMAT_YUV444 = 0x00001000,
  NV_ENC_BUFFER_FORMAT_ARGB = 0x01000000,
  NV_ENC_BUFFER_FORMAT_ABGR = 0x10000000
} NV_ENC_BUFFER_FORMAT;

typedef enum _NV_ENC_INPUT_RESOURCE_TYPE {
  NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX = 0x0,
  NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR = 0x1,
  NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY = 0x2,
  NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX = 0x3
} NV_ENC_INPUT_RESOURCE_TYPE;

typedef enum _NV_ENC_TUNING_INFO {
  NV_ENC_TUNING_INFO_UNDEFINED = 0,
  NV_ENC_TUNING_INFO_HIGH_QUALITY = 1,
  NV_ENC_TUNING_INFO_LOW_LATENCY = 2,
  NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY = 3,
  NV_ENC_TUNING_INFO_LOSSLESS = 4
} NV_ENC_TUNING_INFO;

typedef enum _NV_ENC_PARAMS_RC_MODE {
  NV_ENC_PARAMS_RC_CONSTQP = 0x0,
  NV_ENC_PARAMS_RC_VBR = 0x1,
  NV_ENC_PARAMS_RC_CBR = 0x2
} NV_ENC_PARAMS_RC_MODE;

typedef enum _NV_ENC_PIC_STRUCT {
  NV_ENC_PIC_STRUCT_FRAME = 0x01,
  NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM = 0x02,
  NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP = 0x03
} NV_ENC_PIC_STRUCT;

typedef enum _NV_ENC_PIC_TYPE {
  NV_ENC_PIC_TYPE_P = 0x0,
  NV_ENC_PIC_TYPE_B = 0x01,
  NV_ENC_PIC_TYPE_I = 0x02,
  NV_ENC_PIC_TYPE_IDR = 0x03,
  NV_ENC_PIC_TYPE_BI = 0x04,
  NV_ENC_PIC_TYPE_SKIPPED = 0x05,
  NV_ENC_PIC_TYPE_INTRA_REFRESH = 0x06,
  NV_ENC_PIC_TYPE_UNKNOWN = 0xFF
} NV_ENC_PIC_TYPE;

typedef enum _NV_ENC_PIC_FLAGS {
  NV_ENC_PIC_FLAG_FORCEINTRA = 0x1,
  NV_ENC_PIC_FLAG_FORCEIDR = 0x2,
  NV_ENC_PIC_FLAG_OUTPUT_SPSPPS = 0x4,
  NV_ENC_PIC_FLAG_EOS = 0x8
} NV_ENC_PIC_FLAGS;

typedef enum _NV_ENC_LEVEL_AV1 {
  NV_ENC_LEVEL_AV1_20 = 0,
  NV_ENC_LEVEL_AV1_AUTOSELECT = 255
} NV_ENC_LEVEL_AV1;

typedef enum _NV_ENC_TIER_AV1 {
  NV_ENC_TIER_AV1_0 = 0,
  NV_ENC_TIER_AV1_1 = 1
} NV_ENC_TIER_AV1;

static const GUID NV_ENC_CODEC_H264_GUID = {
    0x6bc82762, 0x4e63, 0x4ca4, {0xaa, 0x85, 0x1e, 0x50, 0xf3, 0x21, 0xf6, 0xbf}};
static const GUID NV_ENC_CODEC_HEVC_GUID = {
    0x790cdc88, 0x4522, 0x4d7b, {0x94, 0x25, 0xbd, 0xa9, 0x97, 0x5f, 0x76, 0x03}};
static const GUID NV_ENC_CODEC_AV1_GUID = {
    0x0a352289, 0x0aa7, 0x4759, {0x86, 0x2d, 0x5d, 0x15, 0xcd, 0x16, 0xd2, 0x54}};
static const GUID NV_ENC_PRESET_P4_GUID = {
    0x90a7b826, 0xdf06, 0x4862, {0xb9, 0xd2, 0xcd, 0x6d, 0x73, 0xa0, 0x86, 0x81}};
static const GUID NV_ENC_H264_PROFILE_BASELINE_GUID = {
    0x0727bcaa, 0x78c4, 0x4c83, {0x8c, 0x2f, 0xef, 0x3d, 0xff, 0x26, 0x7c, 0x6a}};
static const GUID NV_ENC_H264_PROFILE_MAIN_GUID = {
    0x60b5c1d4, 0x67fe, 0x4790, {0x94, 0xd5, 0xc4, 0x72, 0x6d, 0x7b, 0x6e, 0x6d}};
static const GUID NV_ENC_H264_PROFILE_HIGH_GUID = {
    0xe7cbc309, 0x4f7a, 0x4b89, {0xaf, 0x2a, 0xd5, 0x37, 0xc9, 0x2b, 0xe3, 0x10}};
static const GUID NV_ENC_AV1_PROFILE_MAIN_GUID = {
    0x5f2a39f5, 0xf14e, 0x4f95, {0x9a, 0x9e, 0xb7, 0x6d, 0x56, 0x8f, 0xcf, 0x97}};

typedef struct _NV_ENC_RC_PARAMS {
  uint32_t version;
  NV_ENC_PARAMS_RC_MODE rateControlMode;
  uint32_t averageBitRate;
  uint32_t maxBitRate;
  uint32_t vbvBufferSize;
  uint32_t vbvInitialDelay;
  uint32_t zeroReorderDelay;
} NV_ENC_RC_PARAMS;

typedef struct _NV_ENC_CONFIG_H264 {
  uint32_t idrPeriod;
  uint32_t repeatSPSPPS;
  uint32_t sliceMode;
  uint32_t sliceModeData;
  uint32_t level;
  uint32_t chromaFormatIDC;
} NV_ENC_CONFIG_H264;

typedef struct _NV_ENC_CONFIG_AV1 {
  uint32_t level;
  uint32_t tier;
  uint32_t minPartSize;
  uint32_t maxPartSize;
  uint32_t outputAnnexBFormat;
  uint32_t enableTimingInfo;
  uint32_t enableDecoderModelInfo;
  uint32_t enableFrameIdNumbers;
  uint32_t disableSeqHdr;
  uint32_t repeatSeqHdr;
  uint32_t enableIntraRefresh;
  uint32_t chromaFormatIDC;
  uint32_t enableBitstreamPadding;
  uint32_t enableCustomTileConfig;
  uint32_t enableFilmGrainParams;
  uint32_t inputPixelBitDepthMinus8;
  uint32_t pixelBitDepthMinus8;
  uint32_t idrPeriod;
  uint32_t intraRefreshPeriod;
  uint32_t intraRefreshCnt;
  uint32_t maxNumRefFramesInDPB;
  uint32_t numTileColumns;
  uint32_t numTileRows;
} NV_ENC_CONFIG_AV1;

typedef union _NV_ENC_CODEC_CONFIG {
  NV_ENC_CONFIG_H264 h264Config;
  NV_ENC_CONFIG_AV1 av1Config;
} NV_ENC_CODEC_CONFIG;

typedef struct _NV_ENC_CONFIG {
  uint32_t version;
  GUID profileGUID;
  uint32_t gopLength;
  int32_t frameIntervalP;
  uint32_t monoChromeEncoding;
  NV_ENC_RC_PARAMS rcParams;
  NV_ENC_CODEC_CONFIG encodeCodecConfig;
} NV_ENC_CONFIG;
#define NV_ENC_CONFIG_VER NVENCAPI_STRUCT_VERSION(8)

typedef struct _NV_ENC_INITIALIZE_PARAMS {
  uint32_t version;
  GUID encodeGUID;
  GUID presetGUID;
  uint32_t encodeWidth;
  uint32_t encodeHeight;
  uint32_t darWidth;
  uint32_t darHeight;
  uint32_t frameRateNum;
  uint32_t frameRateDen;
  uint32_t enableEncodeAsync;
  uint32_t enablePTD;
  NV_ENC_CONFIG* encodeConfig;
  uint32_t maxEncodeWidth;
  uint32_t maxEncodeHeight;
  NV_ENC_TUNING_INFO tuningInfo;
} NV_ENC_INITIALIZE_PARAMS;
#define NV_ENC_INITIALIZE_PARAMS_VER NVENCAPI_STRUCT_VERSION(5)

typedef struct _NV_ENC_RECONFIGURE_PARAMS {
  uint32_t version;
  NV_ENC_INITIALIZE_PARAMS reInitEncodeParams;
  uint32_t resetEncoder;
  uint32_t forceIDR;
} NV_ENC_RECONFIGURE_PARAMS;
#define NV_ENC_RECONFIGURE_PARAMS_VER NVENCAPI_STRUCT_VERSION(1)

typedef struct _NV_ENC_PRESET_CONFIG {
  uint32_t version;
  NV_ENC_CONFIG presetCfg;
} NV_ENC_PRESET_CONFIG;
#define NV_ENC_PRESET_CONFIG_VER NVENCAPI_STRUCT_VERSION(4)

typedef struct _NV_ENC_PIC_PARAMS_AV1 {
  uint32_t displayPOCSyntax;
  uint32_t refPicFlag;
  uint32_t numTileColumns;
  uint32_t numTileRows;
} NV_ENC_PIC_PARAMS_AV1;

typedef struct _NV_ENC_PIC_PARAMS_H264 {
  uint32_t displayPOCSyntax;
  uint32_t refPicFlag;
} NV_ENC_PIC_PARAMS_H264;

typedef union _NV_ENC_CODEC_PIC_PARAMS {
  NV_ENC_PIC_PARAMS_H264 h264PicParams;
  NV_ENC_PIC_PARAMS_AV1 av1PicParams;
} NV_ENC_CODEC_PIC_PARAMS;

typedef struct _NV_ENC_PIC_PARAMS {
  uint32_t version;
  uint32_t inputWidth;
  uint32_t inputHeight;
  uint32_t inputPitch;
  uint32_t encodePicFlags;
  uint32_t frameIdx;
  uint64_t inputTimeStamp;
  uint64_t inputDuration;
  NV_ENC_INPUT_PTR inputBuffer;
  NV_ENC_OUTPUT_PTR outputBitstream;
  void* completionEvent;
  NV_ENC_BUFFER_FORMAT bufferFmt;
  NV_ENC_PIC_STRUCT pictureStruct;
  NV_ENC_PIC_TYPE pictureType;
  NV_ENC_CODEC_PIC_PARAMS codecPicParams;
} NV_ENC_PIC_PARAMS;
#define NV_ENC_PIC_PARAMS_VER NVENCAPI_STRUCT_VERSION(6)

typedef struct _NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS {
  uint32_t version;
  NV_ENC_DEVICE_TYPE deviceType;
  void* device;
  void* reserved;
  uint32_t apiVersion;
} NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS;
#define NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER NVENCAPI_STRUCT_VERSION(1)

typedef struct _NV_ENC_CREATE_BITSTREAM_BUFFER {
  uint32_t version;
  NV_ENC_OUTPUT_PTR bitstreamBuffer;
} NV_ENC_CREATE_BITSTREAM_BUFFER;
#define NV_ENC_CREATE_BITSTREAM_BUFFER_VER NVENCAPI_STRUCT_VERSION(1)

typedef struct _NV_ENC_LOCK_BITSTREAM {
  uint32_t version;
  uint32_t doNotWait;
  NV_ENC_OUTPUT_PTR outputBitstream;
  uint32_t* sliceOffsets;
  uint32_t frameIdx;
  uint32_t hwEncodeStatus;
  uint32_t numSlices;
  uint32_t bitstreamSizeInBytes;
  uint64_t outputTimeStamp;
  uint64_t outputDuration;
  void* bitstreamBufferPtr;
  NV_ENC_PIC_TYPE pictureType;
  NV_ENC_PIC_STRUCT pictureStruct;
} NV_ENC_LOCK_BITSTREAM;
#define NV_ENC_LOCK_BITSTREAM_VER NVENCAPI_STRUCT_VERSION(1)

typedef struct _NV_ENC_REGISTER_RESOURCE {
  uint32_t version;
  NV_ENC_INPUT_RESOURCE_TYPE resourceType;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t subResourceIndex;
  void* resourceToRegister;
  NV_ENC_REGISTERED_PTR registeredResource;
  NV_ENC_BUFFER_FORMAT bufferFormat;
} NV_ENC_REGISTER_RESOURCE;
#define NV_ENC_REGISTER_RESOURCE_VER NVENCAPI_STRUCT_VERSION(4)

typedef struct _NV_ENC_MAP_INPUT_RESOURCE {
  uint32_t version;
  uint32_t subResourceIndex;
  void* inputResource;
  NV_ENC_REGISTERED_PTR registeredResource;
  NV_ENC_INPUT_PTR mappedResource;
  NV_ENC_BUFFER_FORMAT mappedBufferFmt;
} NV_ENC_MAP_INPUT_RESOURCE;
#define NV_ENC_MAP_INPUT_RESOURCE_VER NVENCAPI_STRUCT_VERSION(4)

typedef struct _NV_ENCODE_API_FUNCTION_LIST {
  uint32_t version;
  uint32_t reserved;
  NVENCSTATUS(NVENCAPI* nvEncOpenEncodeSession)(void* device, uint32_t deviceType, void** encoder);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodeGUIDCount)(void* encoder, uint32_t* encodeGUIDCount);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodeProfileGUIDCount)(void* encoder, GUID encodeGUID, uint32_t* encodeProfileGUIDCount);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodeProfileGUIDs)(void* encoder, GUID encodeGUID, GUID* profileGUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodeGUIDs)(void* encoder, GUID* GUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);
  NVENCSTATUS(NVENCAPI* nvEncGetInputFormatCount)(void* encoder, GUID encodeGUID, uint32_t* inputFmtCount);
  NVENCSTATUS(NVENCAPI* nvEncGetInputFormats)(void* encoder, GUID encodeGUID, NV_ENC_BUFFER_FORMAT* inputFmts, uint32_t inputFmtArraySize, uint32_t* inputFmtCount);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodeCaps)(void* encoder, GUID encodeGUID, void* capsParam, int* capsVal);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodePresetCount)(void* encoder, GUID encodeGUID, uint32_t* encodePresetGUIDCount);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodePresetGUIDs)(void* encoder, GUID encodeGUID, GUID* presetGUIDs, uint32_t guidArraySize, uint32_t* encodePresetGUIDCount);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodePresetConfig)(void* encoder, GUID encodeGUID, GUID presetGUID, NV_ENC_PRESET_CONFIG* presetConfig);
  NVENCSTATUS(NVENCAPI* nvEncInitializeEncoder)(void* encoder, NV_ENC_INITIALIZE_PARAMS* createEncodeParams);
  NVENCSTATUS(NVENCAPI* nvEncCreateInputBuffer)(void* encoder, void* createInputBufferParams);
  NVENCSTATUS(NVENCAPI* nvEncDestroyInputBuffer)(void* encoder, NV_ENC_INPUT_PTR inputBuffer);
  NVENCSTATUS(NVENCAPI* nvEncCreateBitstreamBuffer)(void* encoder, NV_ENC_CREATE_BITSTREAM_BUFFER* createBitstreamBufferParams);
  NVENCSTATUS(NVENCAPI* nvEncDestroyBitstreamBuffer)(void* encoder, NV_ENC_OUTPUT_PTR bitstreamBuffer);
  NVENCSTATUS(NVENCAPI* nvEncEncodePicture)(void* encoder, NV_ENC_PIC_PARAMS* encodePicParams);
  NVENCSTATUS(NVENCAPI* nvEncLockBitstream)(void* encoder, NV_ENC_LOCK_BITSTREAM* lockBitstreamBufferParams);
  NVENCSTATUS(NVENCAPI* nvEncUnlockBitstream)(void* encoder, NV_ENC_OUTPUT_PTR bitstreamBuffer);
  NVENCSTATUS(NVENCAPI* nvEncLockInputBuffer)(void* encoder, void* lockInputBufferParams);
  NVENCSTATUS(NVENCAPI* nvEncUnlockInputBuffer)(void* encoder, NV_ENC_INPUT_PTR inputBuffer);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodeStats)(void* encoder, void* encodeStats);
  NVENCSTATUS(NVENCAPI* nvEncGetSequenceParams)(void* encoder, void* sequenceParamPayload);
  NVENCSTATUS(NVENCAPI* nvEncRegisterAsyncEvent)(void* encoder, void* eventParams);
  NVENCSTATUS(NVENCAPI* nvEncUnregisterAsyncEvent)(void* encoder, void* eventParams);
  NVENCSTATUS(NVENCAPI* nvEncMapInputResource)(void* encoder, NV_ENC_MAP_INPUT_RESOURCE* mapInputResParams);
  NVENCSTATUS(NVENCAPI* nvEncUnmapInputResource)(void* encoder, NV_ENC_INPUT_PTR mappedInputBuffer);
  NVENCSTATUS(NVENCAPI* nvEncDestroyEncoder)(void* encoder);
  NVENCSTATUS(NVENCAPI* nvEncInvalidateRefFrames)(void* encoder, uint64_t invalidRefFrameTimeStamp);
  NVENCSTATUS(NVENCAPI* nvEncOpenEncodeSessionEx)(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS* openSessionExParams, void** encoder);
  NVENCSTATUS(NVENCAPI* nvEncRegisterResource)(void* encoder, NV_ENC_REGISTER_RESOURCE* registerResParams);
  NVENCSTATUS(NVENCAPI* nvEncUnregisterResource)(void* encoder, NV_ENC_REGISTERED_PTR registeredRes);
  NVENCSTATUS(NVENCAPI* nvEncReconfigureEncoder)(void* encoder, NV_ENC_RECONFIGURE_PARAMS* reInitEncodeParams);
  void* reserved1;
  NVENCSTATUS(NVENCAPI* nvEncCreateMVBuffer)(void* encoder, void* createMVBufferParams);
  NVENCSTATUS(NVENCAPI* nvEncDestroyMVBuffer)(void* encoder, void* mvBuffer);
  NVENCSTATUS(NVENCAPI* nvEncRunMotionEstimationOnly)(void* encoder, void* meOnlyParams);
  const char*(NVENCAPI* nvEncGetLastErrorString)(void* encoder);
  NVENCSTATUS(NVENCAPI* nvEncSetIOCudaStreams)(void* encoder, void* inputStream, void* outputStream);
  NVENCSTATUS(NVENCAPI* nvEncGetEncodePresetConfigEx)(void* encoder, GUID encodeGUID, GUID presetGUID, NV_ENC_TUNING_INFO tuningInfo, NV_ENC_PRESET_CONFIG* presetConfig);
  NVENCSTATUS(NVENCAPI* nvEncGetSequenceParamEx)(void* encoder, NV_ENC_INITIALIZE_PARAMS* encInitParams, void* sequenceParamPayload);
  NVENCSTATUS(NVENCAPI* nvEncRestoreEncoderState)(void* encoder, void* stateParams);
  NVENCSTATUS(NVENCAPI* nvEncLookaheadPicture)(void* encoder, void* lookaheadParams);
  void* reserved2[275];
} NV_ENCODE_API_FUNCTION_LIST;
#define NV_ENCODE_API_FUNCTION_LIST_VER NVENCAPI_STRUCT_VERSION(2)

#ifdef __cplusplus
}
#endif
