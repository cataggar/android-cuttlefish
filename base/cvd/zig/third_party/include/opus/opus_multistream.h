#pragma once

#include "opus/opus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpusMSEncoder OpusMSEncoder;
typedef struct OpusMSDecoder OpusMSDecoder;

OpusMSEncoder* opus_multistream_encoder_create(
    opus_int32 Fs, int channels, int streams, int coupled_streams,
    const unsigned char* mapping, int application, int* error);

OpusMSEncoder* opus_multistream_surround_encoder_create(
    opus_int32 Fs, int channels, int mapping_family, int* streams,
    int* coupled_streams, unsigned char* mapping, int application, int* error);

int opus_multistream_encode(OpusMSEncoder* st, const opus_int16* pcm,
                            int frame_size, unsigned char* data,
                            opus_int32 max_data_bytes);

int opus_multistream_encode_float(OpusMSEncoder* st, const float* pcm,
                                  int frame_size, unsigned char* data,
                                  opus_int32 max_data_bytes);

void opus_multistream_encoder_destroy(OpusMSEncoder* st);
int opus_multistream_encoder_ctl(OpusMSEncoder* st, int request, ...);

OpusMSDecoder* opus_multistream_decoder_create(
    opus_int32 Fs, int channels, int streams, int coupled_streams,
    const unsigned char* mapping, int* error);

int opus_multistream_decode(OpusMSDecoder* st, const unsigned char* data,
                            opus_int32 len, opus_int16* pcm, int frame_size,
                            int decode_fec);

int opus_multistream_decode_float(OpusMSDecoder* st, const unsigned char* data,
                                  opus_int32 len, float* pcm, int frame_size,
                                  int decode_fec);

int opus_multistream_decoder_ctl(OpusMSDecoder* st, int request, ...);
void opus_multistream_decoder_destroy(OpusMSDecoder* st);

#ifdef __cplusplus
}
#endif
