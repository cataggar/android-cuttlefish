#pragma once

#include "wels/codec_app_def.h"
#include "wels/codec_def.h"
#include "wels/codec_ver.h"

#ifndef EXTAPI
#define EXTAPI
#endif

class ISVCEncoder {
 public:
  virtual int Initialize(const SEncParamBase* pParam) = 0;
  virtual int InitializeExt(const SEncParamExt* pParam) = 0;
  virtual int GetDefaultParams(SEncParamExt* pParam) = 0;
  virtual int Uninitialize() = 0;
  virtual int EncodeFrame(const SSourcePicture* kpSrcPic,
                          SFrameBSInfo* pBsInfo) = 0;
  virtual int EncodeParameterSets(SFrameBSInfo* pBsInfo) = 0;
  virtual int ForceIntraFrame(bool bIDR, int iLayerId = -1) = 0;
  virtual int SetOption(ENCODER_OPTION eOptionId, void* pOption) = 0;
  virtual int GetOption(ENCODER_OPTION eOptionId, void* pOption) = 0;
  virtual ~ISVCEncoder() {}
};

class ISVCDecoder {
 public:
  virtual long Initialize(const SDecodingParam* pParam) = 0;
  virtual long Uninitialize() = 0;
  virtual DECODING_STATE DecodeFrame(const unsigned char* pSrc,
                                     const int iSrcLen, unsigned char** ppDst,
                                     int* pStride, int& iWidth,
                                     int& iHeight) = 0;
  virtual DECODING_STATE DecodeFrameNoDelay(const unsigned char* pSrc,
                                            const int iSrcLen,
                                            unsigned char** ppDst,
                                            SBufferInfo* pDstInfo) = 0;
  virtual DECODING_STATE DecodeFrame2(const unsigned char* pSrc,
                                      const int iSrcLen, unsigned char** ppDst,
                                      SBufferInfo* pDstInfo) = 0;
  virtual DECODING_STATE FlushFrame(unsigned char** ppDst,
                                    SBufferInfo* pDstInfo) = 0;
  virtual DECODING_STATE DecodeParser(const unsigned char* pSrc,
                                      const int iSrcLen,
                                      SParserBsInfo* pDstInfo) = 0;
  virtual DECODING_STATE DecodeFrameEx(const unsigned char* pSrc,
                                       const int iSrcLen, unsigned char* pDst,
                                       int iDstStride, int& iDstLen,
                                       int& iWidth, int& iHeight,
                                       int& iColorFormat) = 0;
  virtual long SetOption(DECODER_OPTION eOptionId, void* pOption) = 0;
  virtual long GetOption(DECODER_OPTION eOptionId, void* pOption) = 0;
  virtual ~ISVCDecoder() {}
};

extern "C" {
int WelsCreateSVCEncoder(ISVCEncoder** ppEncoder);
void WelsDestroySVCEncoder(ISVCEncoder* pEncoder);
long WelsCreateDecoder(ISVCDecoder** ppDecoder);
void WelsDestroyDecoder(ISVCDecoder* pDecoder);
OpenH264Version WelsGetCodecVersion(void);
void WelsGetCodecVersionEx(OpenH264Version* pVersion);
}
