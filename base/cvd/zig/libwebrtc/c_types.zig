const std = @import("std");

pub const Error = error{
    InitializationFailed,
    InvalidArgument,
    InvalidState,
    InternalError,
    Unsupported,
    BufferTooSmall,
};

/// C ABI declarations matching `webrtc_c.h`.
pub const c = struct {
    pub const cf_webrtc_factory_t = opaque {};
    pub const cf_webrtc_video_source_t = opaque {};
    pub const cf_webrtc_audio_source_t = opaque {};
    pub const cf_webrtc_pc_t = opaque {};
    pub const cf_webrtc_dc_t = opaque {};

    pub const cf_webrtc_error_t = c_int;
    pub const CF_WEBRTC_OK: c_int = 0;
    pub const CF_WEBRTC_ERR_INVALID_ARG: c_int = -1;
    pub const CF_WEBRTC_ERR_STATE: c_int = -2;
    pub const CF_WEBRTC_ERR_INTERNAL: c_int = -3;
    pub const CF_WEBRTC_ERR_UNSUPPORTED: c_int = -4;

    pub const cf_webrtc_video_codec_t = c_uint;
    pub const CF_WEBRTC_VIDEO_CODEC_VP8: c_uint = 1;
    pub const CF_WEBRTC_VIDEO_CODEC_H264: c_uint = 2;

    pub const cf_webrtc_audio_codec_t = c_uint;
    pub const CF_WEBRTC_AUDIO_CODEC_OPUS: c_uint = 1;

    pub const cf_webrtc_h264_packetization_mode_t = c_uint;
    pub const CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED: c_uint = 0;
    pub const CF_WEBRTC_H264_PACKETIZATION_SINGLE_NAL_UNIT: c_uint = 1;

    pub const cf_webrtc_sdp_type_t = c_uint;
    pub const CF_WEBRTC_SDP_OFFER: c_uint = 0;
    pub const CF_WEBRTC_SDP_PRANSWER: c_uint = 1;
    pub const CF_WEBRTC_SDP_ANSWER: c_uint = 2;
    pub const CF_WEBRTC_SDP_ROLLBACK: c_uint = 3;

    pub const cf_webrtc_pc_state_t = c_uint;
    pub const CF_WEBRTC_PC_STATE_NEW: c_uint = 0;
    pub const CF_WEBRTC_PC_STATE_CONNECTING: c_uint = 1;
    pub const CF_WEBRTC_PC_STATE_CONNECTED: c_uint = 2;
    pub const CF_WEBRTC_PC_STATE_DISCONNECTED: c_uint = 3;
    pub const CF_WEBRTC_PC_STATE_FAILED: c_uint = 4;
    pub const CF_WEBRTC_PC_STATE_CLOSED: c_uint = 5;

    pub const cf_webrtc_signaling_state_t = c_uint;
    pub const CF_WEBRTC_SIGNALING_STABLE: c_uint = 0;
    pub const CF_WEBRTC_SIGNALING_HAVE_LOCAL_OFFER: c_uint = 1;
    pub const CF_WEBRTC_SIGNALING_HAVE_LOCAL_PRANSWER: c_uint = 2;
    pub const CF_WEBRTC_SIGNALING_HAVE_REMOTE_OFFER: c_uint = 3;
    pub const CF_WEBRTC_SIGNALING_HAVE_REMOTE_PRANSWER: c_uint = 4;
    pub const CF_WEBRTC_SIGNALING_CLOSED: c_uint = 5;

    pub const cf_webrtc_dc_state_t = c_uint;
    pub const CF_WEBRTC_DC_STATE_CONNECTING: c_uint = 0;
    pub const CF_WEBRTC_DC_STATE_OPEN: c_uint = 1;
    pub const CF_WEBRTC_DC_STATE_CLOSING: c_uint = 2;
    pub const CF_WEBRTC_DC_STATE_CLOSED: c_uint = 3;

    pub const cf_webrtc_video_codec_info_t = extern struct {
        codec: cf_webrtc_video_codec_t,
        name: [16]u8,
        profile_level_id: [16]u8,
        packetization_mode: cf_webrtc_h264_packetization_mode_t,
    };

    pub const cf_webrtc_audio_codec_info_t = extern struct {
        codec: cf_webrtc_audio_codec_t,
        name: [16]u8,
        clockrate_hz: i32,
        channels: u32,
    };

    pub const cf_webrtc_ice_candidate_t = extern struct {
        sdp_mid: [*:0]const u8,
        sdp_mline_index: i32,
        candidate_sdp: [*:0]const u8,
    };

    pub const cf_webrtc_pc_config_t = extern struct {
        min_port: u16,
        max_port: u16,
        stun_server_uri: ?[*:0]const u8,
    };

    pub const cf_webrtc_dc_config_t = extern struct {
        ordered: bool,
        reliable: bool,
        id: i32,
        protocol: ?[*:0]const u8,
    };

    pub const cf_webrtc_on_sdp_fn = ?*const fn (
        user_data: ?*anyopaque,
        sdp_type: cf_webrtc_sdp_type_t,
        sdp: [*]const u8,
        sdp_len: usize,
    ) callconv(.c) void;

    pub const cf_webrtc_on_ice_candidate_fn = ?*const fn (
        user_data: ?*anyopaque,
        candidate: *const cf_webrtc_ice_candidate_t,
    ) callconv(.c) void;

    pub const cf_webrtc_on_pc_state_change_fn = ?*const fn (
        user_data: ?*anyopaque,
        new_state: cf_webrtc_pc_state_t,
    ) callconv(.c) void;

    pub const cf_webrtc_on_data_channel_fn = ?*const fn (
        user_data: ?*anyopaque,
        data_channel: *cf_webrtc_dc_t,
    ) callconv(.c) void;

    pub const cf_webrtc_pc_callbacks_t = extern struct {
        user_data: ?*anyopaque = null,
        on_ice_candidate: cf_webrtc_on_ice_candidate_fn = null,
        on_state_change: cf_webrtc_on_pc_state_change_fn = null,
        on_data_channel: cf_webrtc_on_data_channel_fn = null,
    };

    pub const cf_webrtc_on_dc_state_change_fn = ?*const fn (
        user_data: ?*anyopaque,
        new_state: cf_webrtc_dc_state_t,
    ) callconv(.c) void;

    pub const cf_webrtc_on_dc_message_fn = ?*const fn (
        user_data: ?*anyopaque,
        data: [*]const u8,
        len: usize,
        is_binary: bool,
    ) callconv(.c) void;

    pub const cf_webrtc_dc_callbacks_t = extern struct {
        user_data: ?*anyopaque = null,
        on_state_change: cf_webrtc_on_dc_state_change_fn = null,
        on_message: cf_webrtc_on_dc_message_fn = null,
    };

    pub extern "c" fn cf_webrtc_factory_create() ?*cf_webrtc_factory_t;
    pub extern "c" fn cf_webrtc_factory_destroy(factory: ?*cf_webrtc_factory_t) void;
    pub extern "c" fn cf_webrtc_factory_supported_video_codecs(
        factory: ?*const cf_webrtc_factory_t,
        out_codecs: ?[*]cf_webrtc_video_codec_info_t,
        max_codecs: usize,
    ) usize;
    pub extern "c" fn cf_webrtc_factory_supported_audio_codecs(
        factory: ?*const cf_webrtc_factory_t,
        out_codecs: ?[*]cf_webrtc_audio_codec_info_t,
        max_codecs: usize,
    ) usize;

    pub extern "c" fn cf_webrtc_video_source_create(
        factory: ?*cf_webrtc_factory_t,
        width: i32,
        height: i32,
    ) ?*cf_webrtc_video_source_t;
    pub extern "c" fn cf_webrtc_video_source_push_i420_frame(
        source: ?*cf_webrtc_video_source_t,
        y_plane: [*]const u8,
        y_stride: i32,
        u_plane: [*]const u8,
        u_stride: i32,
        v_plane: [*]const u8,
        v_stride: i32,
        width: i32,
        height: i32,
        timestamp_us: i64,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_video_source_push_abgr_frame(
        source: ?*cf_webrtc_video_source_t,
        abgr_data: [*]const u8,
        stride: i32,
        width: i32,
        height: i32,
        timestamp_us: i64,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_video_source_push_encoded_h264(
        source: ?*cf_webrtc_video_source_t,
        nal_data: [*]const u8,
        nal_size: usize,
        is_idr: bool,
        timestamp_us: i64,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_video_source_frames_pushed(
        source: ?*const cf_webrtc_video_source_t,
    ) u64;
    pub extern "c" fn cf_webrtc_video_source_encoded_bytes_pushed(
        source: ?*const cf_webrtc_video_source_t,
    ) u64;
    pub extern "c" fn cf_webrtc_video_source_destroy(source: ?*cf_webrtc_video_source_t) void;

    pub extern "c" fn cf_webrtc_audio_source_create(
        factory: ?*cf_webrtc_factory_t,
        sample_rate_hz: i32,
        channels: u32,
    ) ?*cf_webrtc_audio_source_t;
    pub extern "c" fn cf_webrtc_audio_source_push_pcm_frame(
        source: ?*cf_webrtc_audio_source_t,
        pcm_samples: [*]const i16,
        frames_per_channel: usize,
        sample_rate_hz: i32,
        channels: u32,
        timestamp_ms: i64,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_audio_source_frames_pushed(
        source: ?*const cf_webrtc_audio_source_t,
    ) u64;
    pub extern "c" fn cf_webrtc_audio_source_destroy(source: ?*cf_webrtc_audio_source_t) void;

    pub extern "c" fn cf_webrtc_pc_create(
        factory: ?*cf_webrtc_factory_t,
        config: ?*const cf_webrtc_pc_config_t,
        callbacks: ?*const cf_webrtc_pc_callbacks_t,
    ) ?*cf_webrtc_pc_t;
    pub extern "c" fn cf_webrtc_pc_create_offer(
        pc_handle: ?*cf_webrtc_pc_t,
        on_sdp: cf_webrtc_on_sdp_fn,
        user_data: ?*anyopaque,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_pc_create_answer(
        pc_handle: ?*cf_webrtc_pc_t,
        on_sdp: cf_webrtc_on_sdp_fn,
        user_data: ?*anyopaque,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_pc_set_local_description(
        pc_handle: ?*cf_webrtc_pc_t,
        sdp_type: cf_webrtc_sdp_type_t,
        sdp_ptr: [*]const u8,
        sdp_len: usize,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_pc_set_remote_description(
        pc_handle: ?*cf_webrtc_pc_t,
        sdp_type: cf_webrtc_sdp_type_t,
        sdp_ptr: [*]const u8,
        sdp_len: usize,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_pc_add_ice_candidate(
        pc_handle: ?*cf_webrtc_pc_t,
        candidate: *const cf_webrtc_ice_candidate_t,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_pc_add_video_track(
        pc_handle: ?*cf_webrtc_pc_t,
        track_label: [*:0]const u8,
        source: ?*cf_webrtc_video_source_t,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_pc_add_audio_track(
        pc_handle: ?*cf_webrtc_pc_t,
        track_label: [*:0]const u8,
        source: ?*cf_webrtc_audio_source_t,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_pc_create_data_channel(
        pc_handle: ?*cf_webrtc_pc_t,
        label: [*:0]const u8,
        config: ?*const cf_webrtc_dc_config_t,
        callbacks: ?*const cf_webrtc_dc_callbacks_t,
    ) ?*cf_webrtc_dc_t;
    pub extern "c" fn cf_webrtc_pc_state(pc_handle: ?*const cf_webrtc_pc_t) cf_webrtc_pc_state_t;
    pub extern "c" fn cf_webrtc_pc_signaling_state(
        pc_handle: ?*const cf_webrtc_pc_t,
    ) cf_webrtc_signaling_state_t;
    pub extern "c" fn cf_webrtc_pc_close(pc_handle: ?*cf_webrtc_pc_t) void;
    pub extern "c" fn cf_webrtc_pc_destroy(pc_handle: ?*cf_webrtc_pc_t) void;

    pub extern "c" fn cf_webrtc_dc_set_callbacks(
        dc: ?*cf_webrtc_dc_t,
        callbacks: ?*const cf_webrtc_dc_callbacks_t,
    ) void;
    pub extern "c" fn cf_webrtc_dc_send(
        dc: ?*cf_webrtc_dc_t,
        data: [*]const u8,
        len: usize,
        is_binary: bool,
    ) cf_webrtc_error_t;
    pub extern "c" fn cf_webrtc_dc_state(dc: ?*const cf_webrtc_dc_t) cf_webrtc_dc_state_t;
    pub extern "c" fn cf_webrtc_dc_label(
        dc: ?*const cf_webrtc_dc_t,
        out_buf: ?[*]u8,
        buf_len: usize,
    ) usize;
    pub extern "c" fn cf_webrtc_dc_close(dc: ?*cf_webrtc_dc_t) void;
    pub extern "c" fn cf_webrtc_dc_destroy(dc: ?*cf_webrtc_dc_t) void;
};

pub fn mapCError(rc: c.cf_webrtc_error_t) Error!void {
    return switch (rc) {
        c.CF_WEBRTC_OK => {},
        c.CF_WEBRTC_ERR_INVALID_ARG => Error.InvalidArgument,
        c.CF_WEBRTC_ERR_STATE => Error.InvalidState,
        c.CF_WEBRTC_ERR_UNSUPPORTED => Error.Unsupported,
        else => Error.InternalError,
    };
}
