#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int CUdevice;
typedef struct CUctx_st* CUcontext;
typedef struct CUstream_st* CUstream;
typedef struct CUarray_st* CUarray;
typedef uint64_t CUdeviceptr;

typedef enum cudaError_enum {
  CUDA_SUCCESS = 0,
  CUDA_ERROR_INVALID_VALUE = 1,
  CUDA_ERROR_OUT_OF_MEMORY = 2,
  CUDA_ERROR_NOT_INITIALIZED = 3,
  CUDA_ERROR_DEINITIALIZED = 4,
  CUDA_ERROR_NO_DEVICE = 100,
  CUDA_ERROR_INVALID_DEVICE = 101,
  CUDA_ERROR_INVALID_CONTEXT = 201,
  CUDA_ERROR_UNKNOWN = 999
} CUresult;

typedef enum CUmemorytype_enum {
  CU_MEMORYTYPE_HOST = 0x01,
  CU_MEMORYTYPE_DEVICE = 0x02,
  CU_MEMORYTYPE_ARRAY = 0x03,
  CU_MEMORYTYPE_UNIFIED = 0x04
} CUmemorytype;

typedef struct CUDA_MEMCPY2D_st {
  size_t srcXInBytes;
  size_t srcY;
  CUmemorytype srcMemoryType;
  const void* srcHost;
  CUdeviceptr srcDevice;
  CUarray srcArray;
  size_t srcPitch;

  size_t dstXInBytes;
  size_t dstY;
  CUmemorytype dstMemoryType;
  void* dstHost;
  CUdeviceptr dstDevice;
  CUarray dstArray;
  size_t dstPitch;

  size_t WidthInBytes;
  size_t Height;
} CUDA_MEMCPY2D;

#ifdef __cplusplus
}
#endif
