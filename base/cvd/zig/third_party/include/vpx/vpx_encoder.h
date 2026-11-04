#pragma once

#include "vpx/vpx_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VPX_ENCODER_ABI_VERSION (15 + VPX_IMAGE_ABI_VERSION)

#define VPX_CODEC_CAP_PSNR 0x10000
#define VPX_CODEC_CAP_OUTPUT_PARTITION 0x20000
#define VPX_CODEC_CAP_HIGHBITDEPTH 0x40000

#define VPX_CODEC_USE_PSNR 0x10000
#define VPX_CODEC_USE_OUTPUT_PARTITION 0x20000
#define VPX_CODEC_USE_HIGHBITDEPTH 0x40000

#define VPX_FRAME_IS_KEY 0x1
#define VPX_FRAME_IS_DROPPABLE 0x2
#define VPX_FRAME_IS_INVISIBLE 0x4
#define VPX_FRAME_IS_FRAGMENT 0x8

#define VPX_DL_REALTIME (1)
#define VPX_DL_GOOD_QUALITY (1000000)
#define VPX_DL_BEST_QUALITY (0)

#define VPX_EFLAG_FORCE_KF (1 << 0)

typedef uint32_t vpx_codec_frame_flags_t;
typedef long vpx_enc_frame_flags_t;
typedef int64_t vpx_codec_pts_t;
typedef unsigned long vpx_codec_er_flags_t;

typedef struct vpx_rational {
  int num;
  int den;
} vpx_rational_t;

enum vpx_enc_pass {
  VPX_RC_ONE_PASS,
  VPX_RC_FIRST_PASS,
  VPX_RC_LAST_PASS
};

enum vpx_rc_mode {
  VPX_VBR,
  VPX_CBR,
  VPX_CQ,
  VPX_Q
};

enum vpx_kf_mode {
  VPX_KF_FIXED,
  VPX_KF_AUTO,
  VPX_KF_DISABLED = 0
};

typedef struct vpx_fixed_buf {
  void* buf;
  size_t sz;
} vpx_fixed_buf_t;

enum vpx_codec_cx_pkt_kind {
  VPX_CODEC_CX_FRAME_PKT,
  VPX_CODEC_STATS_PKT,
  VPX_CODEC_FPMB_STATS_PKT,
  VPX_CODEC_PSNR_PKT,
  VPX_CODEC_CUSTOM_PKT = 256
};

typedef struct vpx_codec_cx_pkt {
  enum vpx_codec_cx_pkt_kind kind;
  union {
    struct {
      void* buf;
      size_t sz;
      vpx_codec_pts_t pts;
      unsigned long duration;
      vpx_codec_frame_flags_t flags;
      int partition_id;
      unsigned int width[5];
      unsigned int height[5];
      uint8_t spatial_layer_encoded[5];
    } frame;
    vpx_fixed_buf_t twopass_stats;
    vpx_fixed_buf_t firstpass_mb_stats;
    struct vpx_psnr_pkt {
      unsigned int samples[4];
      uint64_t sse[4];
      double psnr[4];
    } psnr;
    vpx_fixed_buf_t raw;
    char pad[128 - sizeof(enum vpx_codec_cx_pkt_kind)];
  } data;
} vpx_codec_cx_pkt_t;

#define VPX_TS_MAX_PERIODICITY 16
#define VPX_TS_MAX_LAYERS 5
#define VPX_MAX_LAYERS 12
#define VPX_SS_MAX_LAYERS 5
#define VPX_SS_DEFAULT_LAYERS 1

typedef struct vpx_codec_enc_cfg {
  unsigned int g_usage;
  unsigned int g_threads;
  unsigned int g_profile;
  unsigned int g_w;
  unsigned int g_h;
  vpx_bit_depth_t g_bit_depth;
  unsigned int g_input_bit_depth;
  struct vpx_rational g_timebase;
  vpx_codec_er_flags_t g_error_resilient;
  enum vpx_enc_pass g_pass;
  unsigned int g_lag_in_frames;
  unsigned int rc_dropframe_thresh;
  unsigned int rc_resize_allowed;
  unsigned int rc_scaled_width;
  unsigned int rc_scaled_height;
  unsigned int rc_resize_up_thresh;
  unsigned int rc_resize_down_thresh;
  enum vpx_rc_mode rc_end_usage;
  vpx_fixed_buf_t rc_twopass_stats_in;
  vpx_fixed_buf_t rc_firstpass_mb_stats_in;
  unsigned int rc_target_bitrate;
  unsigned int rc_min_quantizer;
  unsigned int rc_max_quantizer;
  unsigned int rc_undershoot_pct;
  unsigned int rc_overshoot_pct;
  unsigned int rc_buf_sz;
  unsigned int rc_buf_initial_sz;
  unsigned int rc_buf_optimal_sz;
  unsigned int rc_2pass_vbr_bias_pct;
  unsigned int rc_2pass_vbr_minsection_pct;
  unsigned int rc_2pass_vbr_maxsection_pct;
  unsigned int rc_2pass_vbr_corpus_complexity;
  enum vpx_kf_mode kf_mode;
  unsigned int kf_min_dist;
  unsigned int kf_max_dist;
  unsigned int ss_number_layers;
  int ss_enable_auto_alt_ref[VPX_SS_MAX_LAYERS];
  unsigned int ss_target_bitrate[VPX_SS_MAX_LAYERS];
  unsigned int ts_number_layers;
  unsigned int ts_target_bitrate[VPX_TS_MAX_LAYERS];
  unsigned int ts_rate_decimator[VPX_TS_MAX_LAYERS];
  unsigned int ts_periodicity;
  unsigned int ts_layer_id[VPX_TS_MAX_PERIODICITY];
  unsigned int layer_target_bitrate[VPX_MAX_LAYERS];
  int temporal_layering_mode;
} vpx_codec_enc_cfg_t;

typedef struct vpx_svc_extra_cfg {
  int max_quantizers[VPX_MAX_LAYERS];
  int min_quantizers[VPX_MAX_LAYERS];
  int scaling_factor_num[VPX_SS_MAX_LAYERS];
  int scaling_factor_den[VPX_SS_MAX_LAYERS];
  int speed_per_layer[VPX_MAX_LAYERS];
  int loopfilter_ctrl[VPX_MAX_LAYERS];
} vpx_svc_extra_cfg_t;

vpx_codec_err_t vpx_codec_enc_init_ver(vpx_codec_ctx_t* ctx,
                                       vpx_codec_iface_t* iface,
                                       const vpx_codec_enc_cfg_t* cfg,
                                       vpx_codec_flags_t flags, int ver);

#define vpx_codec_enc_init(ctx, iface, cfg, flags) \
  vpx_codec_enc_init_ver(ctx, iface, cfg, flags, VPX_ENCODER_ABI_VERSION)

vpx_codec_err_t vpx_codec_enc_config_default(vpx_codec_iface_t* iface,
                                             vpx_codec_enc_cfg_t* cfg,
                                             unsigned int usage);

vpx_codec_err_t vpx_codec_enc_config_set(vpx_codec_ctx_t* ctx,
                                         const vpx_codec_enc_cfg_t* cfg);

vpx_codec_err_t vpx_codec_encode(vpx_codec_ctx_t* ctx, const vpx_image_t* img,
                                 vpx_codec_pts_t pts, unsigned long duration,
                                 vpx_enc_frame_flags_t flags,
                                 unsigned long deadline);

const vpx_codec_cx_pkt_t* vpx_codec_get_cx_data(vpx_codec_ctx_t* ctx,
                                                vpx_codec_iter_t* iter);

const vpx_image_t* vpx_codec_get_preview_frame(vpx_codec_ctx_t* ctx);

#ifdef __cplusplus
}
#endif
