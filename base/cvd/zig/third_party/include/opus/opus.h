#pragma once

#include "opus/opus_defines.h"
#include "opus/opus_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;
typedef struct OpusRepacketizer OpusRepacketizer;

int opus_encoder_get_size(int channels);
OpusEncoder* opus_encoder_create(opus_int32 Fs, int channels, int application,
                                 int* error);
int opus_encoder_init(OpusEncoder* st, opus_int32 Fs, int channels,
                      int application);
opus_int32 opus_encode(OpusEncoder* st, const opus_int16* pcm, int frame_size,
                       unsigned char* data, opus_int32 max_data_bytes);
opus_int32 opus_encode_float(OpusEncoder* st, const float* pcm, int frame_size,
                             unsigned char* data, opus_int32 max_data_bytes);
void opus_encoder_destroy(OpusEncoder* st);
int opus_encoder_ctl(OpusEncoder* st, int request, ...);

int opus_decoder_get_size(int channels);
OpusDecoder* opus_decoder_create(opus_int32 Fs, int channels, int* error);
int opus_decoder_init(OpusDecoder* st, opus_int32 Fs, int channels);
int opus_decode(OpusDecoder* st, const unsigned char* data, opus_int32 len,
                opus_int16* pcm, int frame_size, int decode_fec);
int opus_decode_float(OpusDecoder* st, const unsigned char* data,
                      opus_int32 len, float* pcm, int frame_size,
                      int decode_fec);
int opus_decoder_ctl(OpusDecoder* st, int request, ...);
void opus_decoder_destroy(OpusDecoder* st);

int opus_packet_get_bandwidth(const unsigned char* data);
int opus_packet_get_samples_per_frame(const unsigned char* data, opus_int32 Fs);
int opus_packet_get_nb_channels(const unsigned char* data);
int opus_packet_get_nb_frames(const unsigned char data[], opus_int32 len);
int opus_packet_get_nb_samples(const unsigned char packet[], opus_int32 len,
                               opus_int32 Fs);
int opus_decoder_get_nb_samples(const OpusDecoder* dec,
                                const unsigned char packet[], opus_int32 len);

OpusRepacketizer* opus_repacketizer_create(void);
void opus_repacketizer_destroy(OpusRepacketizer* rp);
OpusRepacketizer* opus_repacketizer_init(OpusRepacketizer* rp);
int opus_repacketizer_cat(OpusRepacketizer* rp, const unsigned char* data,
                          opus_int32 len);
opus_int32 opus_repacketizer_out(OpusRepacketizer* rp, unsigned char* data,
                                 opus_int32 maxlen);
int opus_repacketizer_get_nb_frames(OpusRepacketizer* rp);

#ifdef __cplusplus
}
#endif
