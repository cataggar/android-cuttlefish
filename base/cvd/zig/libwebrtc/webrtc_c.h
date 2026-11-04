#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cf_webrtc_factory cf_webrtc_factory_t;
typedef struct cf_webrtc_video_source cf_webrtc_video_source_t;
typedef struct cf_webrtc_audio_source cf_webrtc_audio_source_t;
typedef struct cf_webrtc_pc cf_webrtc_pc_t;
typedef struct cf_webrtc_dc cf_webrtc_dc_t;

typedef enum cf_webrtc_error {
  CF_WEBRTC_OK = 0,
  CF_WEBRTC_ERR_INVALID_ARG = -1,
  CF_WEBRTC_ERR_STATE = -2,
  CF_WEBRTC_ERR_INTERNAL = -3,
  CF_WEBRTC_ERR_UNSUPPORTED = -4,
} cf_webrtc_error_t;

typedef enum cf_webrtc_video_codec {
  CF_WEBRTC_VIDEO_CODEC_VP8 = 1,
  CF_WEBRTC_VIDEO_CODEC_H264 = 2,
} cf_webrtc_video_codec_t;

typedef enum cf_webrtc_audio_codec {
  CF_WEBRTC_AUDIO_CODEC_OPUS = 1,
} cf_webrtc_audio_codec_t;

typedef enum cf_webrtc_h264_packetization_mode {
  CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED = 0,
  CF_WEBRTC_H264_PACKETIZATION_SINGLE_NAL_UNIT = 1,
} cf_webrtc_h264_packetization_mode_t;

typedef enum cf_webrtc_sdp_type {
  CF_WEBRTC_SDP_OFFER = 0,
  CF_WEBRTC_SDP_PRANSWER = 1,
  CF_WEBRTC_SDP_ANSWER = 2,
  CF_WEBRTC_SDP_ROLLBACK = 3,
} cf_webrtc_sdp_type_t;

typedef enum cf_webrtc_pc_state {
  CF_WEBRTC_PC_STATE_NEW = 0,
  CF_WEBRTC_PC_STATE_CONNECTING = 1,
  CF_WEBRTC_PC_STATE_CONNECTED = 2,
  CF_WEBRTC_PC_STATE_DISCONNECTED = 3,
  CF_WEBRTC_PC_STATE_FAILED = 4,
  CF_WEBRTC_PC_STATE_CLOSED = 5,
} cf_webrtc_pc_state_t;

typedef enum cf_webrtc_signaling_state {
  CF_WEBRTC_SIGNALING_STABLE = 0,
  CF_WEBRTC_SIGNALING_HAVE_LOCAL_OFFER = 1,
  CF_WEBRTC_SIGNALING_HAVE_LOCAL_PRANSWER = 2,
  CF_WEBRTC_SIGNALING_HAVE_REMOTE_OFFER = 3,
  CF_WEBRTC_SIGNALING_HAVE_REMOTE_PRANSWER = 4,
  CF_WEBRTC_SIGNALING_CLOSED = 5,
} cf_webrtc_signaling_state_t;

typedef enum cf_webrtc_dc_state {
  CF_WEBRTC_DC_STATE_CONNECTING = 0,
  CF_WEBRTC_DC_STATE_OPEN = 1,
  CF_WEBRTC_DC_STATE_CLOSING = 2,
  CF_WEBRTC_DC_STATE_CLOSED = 3,
} cf_webrtc_dc_state_t;

typedef struct cf_webrtc_video_codec_info {
  cf_webrtc_video_codec_t codec;
  char name[16];
  char profile_level_id[16];
  cf_webrtc_h264_packetization_mode_t packetization_mode;
} cf_webrtc_video_codec_info_t;

typedef struct cf_webrtc_audio_codec_info {
  cf_webrtc_audio_codec_t codec;
  char name[16];
  int32_t clockrate_hz;
  uint32_t channels;
} cf_webrtc_audio_codec_info_t;

typedef struct cf_webrtc_ice_candidate {
  const char* sdp_mid;
  int32_t sdp_mline_index;
  const char* candidate_sdp;
} cf_webrtc_ice_candidate_t;

typedef struct cf_webrtc_pc_config {
  uint16_t min_port;
  uint16_t max_port;
  const char* stun_server_uri;
} cf_webrtc_pc_config_t;

typedef struct cf_webrtc_dc_config {
  bool ordered;
  bool reliable;
  int32_t id;
  const char* protocol;
} cf_webrtc_dc_config_t;

typedef void (*cf_webrtc_on_sdp_fn)(void* user_data,
                                    cf_webrtc_sdp_type_t type,
                                    const char* sdp,
                                    size_t sdp_len);
typedef void (*cf_webrtc_on_ice_candidate_fn)(
    void* user_data,
    const cf_webrtc_ice_candidate_t* candidate);
typedef void (*cf_webrtc_on_pc_state_change_fn)(void* user_data,
                                                cf_webrtc_pc_state_t new_state);
typedef void (*cf_webrtc_on_data_channel_fn)(void* user_data,
                                             cf_webrtc_dc_t* data_channel);

typedef struct cf_webrtc_pc_callbacks {
  void* user_data;
  cf_webrtc_on_ice_candidate_fn on_ice_candidate;
  cf_webrtc_on_pc_state_change_fn on_state_change;
  cf_webrtc_on_data_channel_fn on_data_channel;
} cf_webrtc_pc_callbacks_t;

typedef void (*cf_webrtc_on_dc_state_change_fn)(void* user_data,
                                                cf_webrtc_dc_state_t new_state);
typedef void (*cf_webrtc_on_dc_message_fn)(void* user_data,
                                           const uint8_t* data,
                                           size_t len,
                                           bool is_binary);

typedef struct cf_webrtc_dc_callbacks {
  void* user_data;
  cf_webrtc_on_dc_state_change_fn on_state_change;
  cf_webrtc_on_dc_message_fn on_message;
} cf_webrtc_dc_callbacks_t;

/* Factory API */
cf_webrtc_factory_t* cf_webrtc_factory_create(void);
void cf_webrtc_factory_destroy(cf_webrtc_factory_t* factory);

size_t cf_webrtc_factory_supported_video_codecs(
    const cf_webrtc_factory_t* factory,
    cf_webrtc_video_codec_info_t* out_codecs,
    size_t max_codecs);

size_t cf_webrtc_factory_supported_audio_codecs(
    const cf_webrtc_factory_t* factory,
    cf_webrtc_audio_codec_info_t* out_codecs,
    size_t max_codecs);

/* VideoSource API */
cf_webrtc_video_source_t* cf_webrtc_video_source_create(
    cf_webrtc_factory_t* factory,
    int32_t width,
    int32_t height);

cf_webrtc_error_t cf_webrtc_video_source_push_i420_frame(
    cf_webrtc_video_source_t* source,
    const uint8_t* y_plane,
    int32_t y_stride,
    const uint8_t* u_plane,
    int32_t u_stride,
    const uint8_t* v_plane,
    int32_t v_stride,
    int32_t width,
    int32_t height,
    int64_t timestamp_us);

cf_webrtc_error_t cf_webrtc_video_source_push_abgr_frame(
    cf_webrtc_video_source_t* source,
    const uint8_t* abgr_data,
    int32_t stride,
    int32_t width,
    int32_t height,
    int64_t timestamp_us);

cf_webrtc_error_t cf_webrtc_video_source_push_encoded_h264(
    cf_webrtc_video_source_t* source,
    const uint8_t* nal_data,
    size_t nal_size,
    bool is_idr,
    int64_t timestamp_us);

uint64_t cf_webrtc_video_source_frames_pushed(
    const cf_webrtc_video_source_t* source);

uint64_t cf_webrtc_video_source_encoded_bytes_pushed(
    const cf_webrtc_video_source_t* source);

void cf_webrtc_video_source_destroy(cf_webrtc_video_source_t* source);

/* AudioSource API */
cf_webrtc_audio_source_t* cf_webrtc_audio_source_create(
    cf_webrtc_factory_t* factory,
    int32_t sample_rate_hz,
    uint32_t channels);

cf_webrtc_error_t cf_webrtc_audio_source_push_pcm_frame(
    cf_webrtc_audio_source_t* source,
    const int16_t* pcm_samples,
    size_t frames_per_channel,
    int32_t sample_rate_hz,
    uint32_t channels,
    int64_t timestamp_ms);

uint64_t cf_webrtc_audio_source_frames_pushed(
    const cf_webrtc_audio_source_t* source);

void cf_webrtc_audio_source_destroy(cf_webrtc_audio_source_t* source);

/* PeerConnection API */
cf_webrtc_pc_t* cf_webrtc_pc_create(cf_webrtc_factory_t* factory,
                                    const cf_webrtc_pc_config_t* config,
                                    const cf_webrtc_pc_callbacks_t* callbacks);

cf_webrtc_error_t cf_webrtc_pc_create_offer(cf_webrtc_pc_t* pc,
                                            cf_webrtc_on_sdp_fn on_sdp,
                                            void* user_data);

cf_webrtc_error_t cf_webrtc_pc_create_answer(cf_webrtc_pc_t* pc,
                                             cf_webrtc_on_sdp_fn on_sdp,
                                             void* user_data);

cf_webrtc_error_t cf_webrtc_pc_set_local_description(cf_webrtc_pc_t* pc,
                                                     cf_webrtc_sdp_type_t type,
                                                     const char* sdp,
                                                     size_t sdp_len);

cf_webrtc_error_t cf_webrtc_pc_set_remote_description(cf_webrtc_pc_t* pc,
                                                      cf_webrtc_sdp_type_t type,
                                                      const char* sdp,
                                                      size_t sdp_len);

cf_webrtc_error_t cf_webrtc_pc_add_ice_candidate(
    cf_webrtc_pc_t* pc,
    const cf_webrtc_ice_candidate_t* candidate);

cf_webrtc_error_t cf_webrtc_pc_add_video_track(
    cf_webrtc_pc_t* pc,
    const char* track_label,
    cf_webrtc_video_source_t* source);

cf_webrtc_error_t cf_webrtc_pc_add_audio_track(
    cf_webrtc_pc_t* pc,
    const char* track_label,
    cf_webrtc_audio_source_t* source);

cf_webrtc_dc_t* cf_webrtc_pc_create_data_channel(
    cf_webrtc_pc_t* pc,
    const char* label,
    const cf_webrtc_dc_config_t* config,
    const cf_webrtc_dc_callbacks_t* callbacks);

cf_webrtc_pc_state_t cf_webrtc_pc_state(const cf_webrtc_pc_t* pc);
cf_webrtc_signaling_state_t cf_webrtc_pc_signaling_state(
    const cf_webrtc_pc_t* pc);

void cf_webrtc_pc_close(cf_webrtc_pc_t* pc);
void cf_webrtc_pc_destroy(cf_webrtc_pc_t* pc);

/* DataChannel API */
void cf_webrtc_dc_set_callbacks(cf_webrtc_dc_t* dc,
                                const cf_webrtc_dc_callbacks_t* callbacks);

cf_webrtc_error_t cf_webrtc_dc_send(cf_webrtc_dc_t* dc,
                                    const uint8_t* data,
                                    size_t len,
                                    bool is_binary);

cf_webrtc_dc_state_t cf_webrtc_dc_state(const cf_webrtc_dc_t* dc);

size_t cf_webrtc_dc_label(const cf_webrtc_dc_t* dc,
                          char* out_buf,
                          size_t buf_len);

void cf_webrtc_dc_close(cf_webrtc_dc_t* dc);
void cf_webrtc_dc_destroy(cf_webrtc_dc_t* dc);

#ifdef __cplusplus
}
#endif
