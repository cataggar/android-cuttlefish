#pragma once

#include "wels/codec_def.h"

#define MAX_TEMPORAL_LAYER_NUM 4
#define MAX_SPATIAL_LAYER_NUM 4
#define MAX_QUALITY_LAYER_NUM 4
#define MAX_LAYER_NUM_OF_FRAME 128
#define MAX_NAL_UNITS_IN_LAYER 128

typedef enum {
  DECODER_OPTION_END_OF_STREAM = 1,
  DECODER_OPTION_VCL_NAL,
  DECODER_OPTION_TEMPORAL_ID,
  DECODER_OPTION_FRAME_NUM,
  DECODER_OPTION_IDR_PIC_ID,
  DECODER_OPTION_LTR_MARKING_FLAG,
  DECODER_OPTION_LTR_MARKED_FRAME_NUM,
  DECODER_OPTION_ERROR_CON_IDC,
  DECODER_OPTION_TRACE_LEVEL,
  DECODER_OPTION_TRACE_CALLBACK,
  DECODER_OPTION_TRACE_CALLBACK_CONTEXT,
  DECODER_OPTION_GET_STATISTICS,
  DECODER_OPTION_GET_SAR_INFO,
  DECODER_OPTION_PROFILE,
  DECODER_OPTION_LEVEL,
  DECODER_OPTION_STATISTICS_LOG_INTERVAL,
  DECODER_OPTION_IS_REF_PIC,
  DECODER_OPTION_NUM_OF_FRAMES_REMAINING_IN_BUFFER,
  DECODER_OPTION_NUM_OF_THREADS
} DECODER_OPTION;

typedef enum {
  ENCODER_OPTION_DATAFORMAT = 0,
  ENCODER_OPTION_IDR_INTERVAL,
  ENCODER_OPTION_SVC_ENCODE_PARAM_BASE,
  ENCODER_OPTION_SVC_ENCODE_PARAM_EXT,
  ENCODER_OPTION_FRAME_RATE,
  ENCODER_OPTION_BITRATE,
  ENCODER_OPTION_MAX_BITRATE,
  ENCODER_OPTION_INTER_SPATIAL_PRED,
  ENCODER_OPTION_RC_MODE,
  ENCODER_OPTION_RC_FRAME_SKIP,
  ENCODER_PADDING_PADDING,
  ENCODER_OPTION_PROFILE,
  ENCODER_OPTION_LEVEL,
  ENCODER_OPTION_NUMBER_REF,
  ENCODER_OPTION_DELIVERY_STATUS,
  ENCODER_LTR_RECOVERY_REQUEST,
  ENCODER_LTR_MARKING_FEEDBACK,
  ENCODER_LTR_MARKING_PERIOD,
  ENCODER_OPTION_LTR,
  ENCODER_OPTION_COMPLEXITY,
  ENCODER_OPTION_ENABLE_SSEI,
  ENCODER_OPTION_ENABLE_PREFIX_NAL_ADDING,
  ENCODER_OPTION_SPS_PPS_ID_STRATEGY,
  ENCODER_OPTION_CURRENT_PATH,
  ENCODER_OPTION_DUMP_FILE,
  ENCODER_OPTION_TRACE_LEVEL,
  ENCODER_OPTION_TRACE_CALLBACK,
  ENCODER_OPTION_TRACE_CALLBACK_CONTEXT,
  ENCODER_OPTION_GET_STATISTICS,
  ENCODER_OPTION_STATISTICS_LOG_INTERVAL,
  ENCODER_OPTION_IS_LOSSLESS_LINK,
  ENCODER_OPTION_BITS_VARY_PERCENTAGE
} ENCODER_OPTION;

typedef enum {
  ERROR_CON_DISABLE = 0,
  ERROR_CON_FRAME_COPY,
  ERROR_CON_SLICE_COPY,
  ERROR_CON_FRAME_COPY_CROSS_IDR,
  ERROR_CON_SLICE_COPY_CROSS_IDR,
  ERROR_CON_SLICE_COPY_CROSS_IDR_FREEZE_RES_CHANGE,
  ERROR_CON_SLICE_MV_COPY_CROSS_IDR,
  ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE
} ERROR_CON_IDC;

typedef enum {
  CAMERA_VIDEO_REAL_TIME,
  SCREEN_CONTENT_REAL_TIME,
  CAMERA_VIDEO_NON_REAL_TIME,
  SCREEN_CONTENT_NON_REAL_TIME,
  INPUT_CONTENT_TYPE_ALL
} EUsageType;

typedef enum {
  SM_SINGLE_SLICE = 0,
  SM_FIXEDSLCNUM_SLICE = 1,
  SM_RASTER_SLICE = 2,
  SM_SIZELIMITED_SLICE = 3,
  SM_RESERVED = 4
} SliceModeEnum;

typedef enum {
  RC_QUALITY_MODE = 0,
  RC_BITRATE_MODE = 1,
  RC_BUFFERBASED_MODE = 2,
  RC_TIMESTAMP_MODE = 3,
  RC_BITRATE_MODE_POST_SKIP = 4,
  RC_OFF_MODE = -1
} RC_MODES;

typedef enum {
  PRO_UNKNOWN = 0,
  PRO_BASELINE = 66,
  PRO_MAIN = 77,
  PRO_EXTENDED = 88,
  PRO_HIGH = 100,
  PRO_HIGH10 = 110,
  PRO_HIGH422 = 122,
  PRO_HIGH444 = 244,
  PRO_CAVLC444 = 44,
  PRO_SCALABLE_BASELINE = 83,
  PRO_SCALABLE_HIGH = 86
} EProfileIdc;

typedef enum {
  LEVEL_UNKNOWN = 0,
  LEVEL_1_0 = 10,
  LEVEL_1_B = 9,
  LEVEL_1_1 = 11,
  LEVEL_1_2 = 12,
  LEVEL_1_3 = 13,
  LEVEL_2_0 = 20,
  LEVEL_2_1 = 21,
  LEVEL_2_2 = 22,
  LEVEL_3_0 = 30,
  LEVEL_3_1 = 31,
  LEVEL_3_2 = 32,
  LEVEL_4_0 = 40,
  LEVEL_4_1 = 41,
  LEVEL_4_2 = 42,
  LEVEL_5_0 = 50,
  LEVEL_5_1 = 51,
  LEVEL_5_2 = 52
} ELevelIdc;

typedef enum {
  WELS_LOG_QUIET = 0x00,
  WELS_LOG_ERROR = 1 << 0,
  WELS_LOG_WARNING = 1 << 1,
  WELS_LOG_INFO = 1 << 2,
  WELS_LOG_DEBUG = 1 << 3,
  WELS_LOG_DETAIL = 1 << 4,
  WELS_LOG_RESV = 1 << 5,
  WELS_LOG_LEVEL_COUNT = 6,
  WELS_LOG_DEFAULT = WELS_LOG_WARNING
} WELS_LOG;

typedef enum {
  CONSTANT_ID = 0,
  INCREASING_ID = 0x01,
  SPS_LISTING = 0x02,
  SPS_LISTING_AND_PPS_INCREASING = 0x03,
  SPS_PPS_LISTING = 0x06
} EParameterSetStrategy;

typedef enum {
  LOW_COMPLEXITY = 0,
  MEDIUM_COMPLEXITY,
  HIGH_COMPLEXITY
} EComplexityMode;

typedef struct {
  SliceModeEnum uiSliceMode;
  unsigned int uiSliceNum;
  unsigned int uiSliceMbNum[MAX_NAL_UNITS_IN_LAYER];
  unsigned int uiSliceSizeConstraint;
} SSliceArgument;

typedef struct {
  int iVideoWidth;
  int iVideoHeight;
  float fFrameRate;
  int iSpatialBitrate;
  int iMaxSpatialBitrate;
  EProfileIdc uiProfileIdc;
  ELevelIdc uiLevelIdc;
  int iDLayerQp;
  SSliceArgument sSliceArgument;
  bool bVideoSignalTypePresent;
  unsigned char uiVideoFormat;
  bool bFullRange;
  bool bColorDescriptionPresent;
  unsigned char uiColorPrimaries;
  unsigned char uiTransferCharacteristics;
  unsigned char uiColorMatrix;
  bool bAspectRatioPresent;
  unsigned char uiAspectRatio;
  unsigned short sAspectRatioExtWidth;
  unsigned short sAspectRatioExtHeight;
} SSpatialLayerConfig;

typedef struct TagEncParamBase {
  EUsageType iUsageType;
  int iPicWidth;
  int iPicHeight;
  int iTargetBitrate;
  RC_MODES iRCMode;
  float fMaxFrameRate;
} SEncParamBase, *PEncParamBase;

typedef struct TagEncParamExt {
  EUsageType iUsageType;
  int iPicWidth;
  int iPicHeight;
  int iTargetBitrate;
  RC_MODES iRCMode;
  float fMaxFrameRate;
  int iTemporalLayerNum;
  int iSpatialLayerNum;
  SSpatialLayerConfig sSpatialLayers[MAX_SPATIAL_LAYER_NUM];
  EComplexityMode iComplexityMode;
  unsigned int uiIntraPeriod;
  int iNumRefFrame;
  EParameterSetStrategy eSpsPpsIdStrategy;
  bool bPrefixNalAddingCtrl;
  bool bEnableSSEI;
  bool bSimulcastAVC;
  int iPaddingFlag;
  int iEntropyCodingModeFlag;
  bool bEnableFrameSkip;
  int iMaxBitrate;
  int iMaxQp;
  int iMinQp;
  unsigned int uiMaxNalSize;
  bool bEnableLongTermReference;
  int iLTRRefNum;
  unsigned int iLtrMarkPeriod;
  unsigned short iMultipleThreadIdc;
  bool bUseLoadBalancing;
  int iLoopFilterDisableIdc;
  int iLoopFilterAlphaC0Offset;
  int iLoopFilterBetaOffset;
  bool bEnableDenoise;
  bool bEnableBackgroundDetection;
  bool bEnableAdaptiveQuant;
  bool bEnableFrameCroppingFlag;
  bool bEnableSceneChangeDetect;
  bool bIsLosslessLink;
  bool bFixRCOverShoot;
  int iIdrBitrateRatio;
} SEncParamExt;

typedef struct {
  int iLayer;
  int iBitrate;
} SBitrateInfo;

typedef struct {
  unsigned char uiTemporalId;
  unsigned char uiSpatialId;
  unsigned char uiQualityId;
  EVideoFrameType eFrameType;
  unsigned char uiLayerType;
  int iSubSeqId;
  int iNalCount;
  int* pNalLengthInByte;
  unsigned char* pBsBuf;
} SLayerBSInfo, *PLayerBSInfo;

typedef struct {
  int iLayerNum;
  SLayerBSInfo sLayerInfo[MAX_LAYER_NUM_OF_FRAME];
  EVideoFrameType eFrameType;
  int iFrameSizeInBytes;
  long long uiTimeStamp;
} SFrameBSInfo, *PFrameBSInfo;

typedef struct TagVideoDecoderArgs {
  EVideoFormatType eOutputColorFormat;
  unsigned int uiCpuLoad;
  unsigned char uiTargetDqLayer;
  ERROR_CON_IDC eEcActiveIdc;
  bool bParseOnly;
  SSysMemBuffer sVideoProperty;
} SDecodingParam, *PDecodingParam;

typedef struct TagParserBsInfo {
  int iNalNum;
  int* pNalLenInByte;
  unsigned char* pDstBuff;
  int iSpsWidthInPixel;
  int iSpsHeightInPixel;
  unsigned long long uiInBsTimeStamp;
  unsigned long long uiOutYuvTimeStamp;
} SParserBsInfo, *PParserBsInfo;

typedef void (*WelsTraceCallback)(void* ctx, int level, const char* string);
