const std = @import("std");
pub const sources = @import("slim_sources.zig");

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

fn mapCError(rc: c.cf_webrtc_error_t) Error!void {
    return switch (rc) {
        c.CF_WEBRTC_OK => {},
        c.CF_WEBRTC_ERR_INVALID_ARG => Error.InvalidArgument,
        c.CF_WEBRTC_ERR_STATE => Error.InvalidState,
        c.CF_WEBRTC_ERR_UNSUPPORTED => Error.Unsupported,
        else => Error.InternalError,
    };
}

/// Pluggable subsystem backend descriptor allowing incremental replacement of
/// C++ `libwebrtc` subsystems (SDP, ICE, DTLS-SRTP, SCTP, Codecs) with pure Zig.
pub const SubsystemBackendKind = enum {
    cpp_libwebrtc,
    pure_zig,
};

pub const Backend = struct {
    sdp_backend: SubsystemBackendKind = .cpp_libwebrtc,
    ice_backend: SubsystemBackendKind = .cpp_libwebrtc,
    dtls_srtp_backend: SubsystemBackendKind = .cpp_libwebrtc,
    sctp_backend: SubsystemBackendKind = .cpp_libwebrtc,
    codecs_backend: SubsystemBackendKind = .cpp_libwebrtc,
    parse_sdp_hook: ?*const fn (raw_sdp: []const u8) bool = null,
};

pub const default_backend = Backend{};

/// SDP (Session Description Protocol) subsystem namespace.
pub const sdp = struct {
    pub const Type = enum(u32) {
        offer = c.CF_WEBRTC_SDP_OFFER,
        pranswer = c.CF_WEBRTC_SDP_PRANSWER,
        answer = c.CF_WEBRTC_SDP_ANSWER,
        rollback = c.CF_WEBRTC_SDP_ROLLBACK,

        pub fn toString(self: Type) []const u8 {
            return switch (self) {
                .offer => "offer",
                .pranswer => "pranswer",
                .answer => "answer",
                .rollback => "rollback",
            };
        }
    };

    pub const SessionDescription = struct {
        sdp_type: Type,
        sdp_text: []const u8,

        pub fn hasVideoCodec(self: SessionDescription, codec_name: []const u8) bool {
            return std.mem.indexOf(u8, self.sdp_text, codec_name) != null;
        }
    };
};

/// ICE (Interactive Connectivity Establishment) subsystem namespace.
pub const ice = struct {
    pub const PortRange = struct {
        min_port: u16 = 15550,
        max_port: u16 = 15599,
    };

    pub const ServerConfig = struct {
        uri: [:0]const u8 = "stun:stun.l.google.com:19302",
    };

    pub const Candidate = struct {
        sdp_mid: [:0]const u8,
        sdp_mline_index: i32,
        candidate_sdp: [:0]const u8,
    };
};

/// DTLS / SRTP transport state and configuration hook namespace.
pub const dtls_srtp = struct {
    pub const TransportState = enum {
        new,
        connecting,
        connected,
        closed,
        failed,
    };

    pub const Config = struct {
        enable_srtp_aes_gcm: bool = true,
        fingerprint_hash: []const u8 = "sha-256",
    };
};

/// SCTP DataChannel subsystem namespace.
pub const sctp = struct {
    pub const State = enum(u32) {
        connecting = c.CF_WEBRTC_DC_STATE_CONNECTING,
        open = c.CF_WEBRTC_DC_STATE_OPEN,
        closing = c.CF_WEBRTC_DC_STATE_CLOSING,
        closed = c.CF_WEBRTC_DC_STATE_CLOSED,
    };

    pub const Config = struct {
        ordered: bool = true,
        reliable: bool = true,
        id: i32 = -1,
        protocol: ?[:0]const u8 = null,
    };

    pub const Callbacks = struct {
        user_data: ?*anyopaque = null,
        on_state_change: c.cf_webrtc_on_dc_state_change_fn = null,
        on_message: c.cf_webrtc_on_dc_message_fn = null,
    };

    pub const DataChannel = struct {
        handle: *c.cf_webrtc_dc_t,

        pub fn setCallbacks(self: *DataChannel, callbacks: Callbacks) void {
            const c_cb = c.cf_webrtc_dc_callbacks_t{
                .user_data = callbacks.user_data,
                .on_state_change = callbacks.on_state_change,
                .on_message = callbacks.on_message,
            };
            c.cf_webrtc_dc_set_callbacks(self.handle, &c_cb);
        }

        pub fn sendText(self: *DataChannel, text: []const u8) Error!void {
            const ptr: [*]const u8 = if (text.len > 0) text.ptr else "";
            return mapCError(c.cf_webrtc_dc_send(self.handle, ptr, text.len, false));
        }

        pub fn sendBinary(self: *DataChannel, bytes: []const u8) Error!void {
            const ptr: [*]const u8 = if (bytes.len > 0) bytes.ptr else "";
            return mapCError(c.cf_webrtc_dc_send(self.handle, ptr, bytes.len, true));
        }

        pub fn state(self: *const DataChannel) State {
            return @enumFromInt(c.cf_webrtc_dc_state(self.handle));
        }

        pub fn label(self: *const DataChannel, buf: []u8) []const u8 {
            if (buf.len == 0) return "";
            const full_len = c.cf_webrtc_dc_label(self.handle, buf.ptr, buf.len);
            const actual = @min(full_len, buf.len - 1);
            return buf[0..actual];
        }

        pub fn close(self: *DataChannel) void {
            c.cf_webrtc_dc_close(self.handle);
        }

        pub fn deinit(self: *DataChannel) void {
            c.cf_webrtc_dc_destroy(self.handle);
        }
    };
};

/// Audio and Video Codec subsystem namespace (VP8, H.264, Opus).
pub const codecs = struct {
    pub const VideoCodecType = enum(u32) {
        vp8 = c.CF_WEBRTC_VIDEO_CODEC_VP8,
        h264 = c.CF_WEBRTC_VIDEO_CODEC_H264,
    };

    pub const AudioCodecType = enum(u32) {
        opus = c.CF_WEBRTC_AUDIO_CODEC_OPUS,
    };

    pub const H264PacketizationMode = enum(u32) {
        non_interleaved = c.CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED,
        single_nal_unit = c.CF_WEBRTC_H264_PACKETIZATION_SINGLE_NAL_UNIT,
    };

    pub const VideoCodecDescriptor = struct {
        codec: VideoCodecType,
        profile_level_id: []const u8,
        packetization_mode: H264PacketizationMode,
    };

    pub const AudioCodecDescriptor = struct {
        codec: AudioCodecType,
        clockrate_hz: i32,
        channels: u32,
    };
};

/// Media tracks & sources subsystem namespace.
pub const media = struct {
    pub const I420Frame = struct {
        y_plane: []const u8,
        y_stride: i32,
        u_plane: []const u8,
        u_stride: i32,
        v_plane: []const u8,
        v_stride: i32,
        width: i32,
        height: i32,
        timestamp_us: i64,
    };

    pub const AbgrFrame = struct {
        data: []const u8,
        stride: i32,
        width: i32,
        height: i32,
        timestamp_us: i64,
    };

    pub const VideoSource = struct {
        handle: *c.cf_webrtc_video_source_t,

        pub fn init(factory: *pc.Factory, width: i32, height: i32) Error!VideoSource {
            const h = c.cf_webrtc_video_source_create(factory.handle, width, height) orelse
                return Error.InitializationFailed;
            return .{ .handle = h };
        }

        pub fn deinit(self: *VideoSource) void {
            c.cf_webrtc_video_source_destroy(self.handle);
        }

        pub fn pushI420Frame(self: *VideoSource, frame: I420Frame) Error!void {
            if (frame.y_plane.len == 0 or frame.u_plane.len == 0 or frame.v_plane.len == 0) {
                return Error.InvalidArgument;
            }
            return mapCError(c.cf_webrtc_video_source_push_i420_frame(
                self.handle,
                frame.y_plane.ptr,
                frame.y_stride,
                frame.u_plane.ptr,
                frame.u_stride,
                frame.v_plane.ptr,
                frame.v_stride,
                frame.width,
                frame.height,
                frame.timestamp_us,
            ));
        }

        pub fn pushAbgrFrame(self: *VideoSource, frame: AbgrFrame) Error!void {
            if (frame.data.len == 0) return Error.InvalidArgument;
            return mapCError(c.cf_webrtc_video_source_push_abgr_frame(
                self.handle,
                frame.data.ptr,
                frame.stride,
                frame.width,
                frame.height,
                frame.timestamp_us,
            ));
        }

        pub fn pushEncodedH264(
            self: *VideoSource,
            nal_data: []const u8,
            is_idr: bool,
            timestamp_us: i64,
        ) Error!void {
            if (nal_data.len == 0) return Error.InvalidArgument;
            return mapCError(c.cf_webrtc_video_source_push_encoded_h264(
                self.handle,
                nal_data.ptr,
                nal_data.len,
                is_idr,
                timestamp_us,
            ));
        }

        pub fn framesPushed(self: *const VideoSource) u64 {
            return c.cf_webrtc_video_source_frames_pushed(self.handle);
        }

        pub fn encodedBytesPushed(self: *const VideoSource) u64 {
            return c.cf_webrtc_video_source_encoded_bytes_pushed(self.handle);
        }
    };

    pub const AudioSource = struct {
        handle: *c.cf_webrtc_audio_source_t,

        pub fn init(factory: *pc.Factory, sample_rate_hz: i32, channels: u32) Error!AudioSource {
            const h = c.cf_webrtc_audio_source_create(factory.handle, sample_rate_hz, channels) orelse
                return Error.InitializationFailed;
            return .{ .handle = h };
        }

        pub fn deinit(self: *AudioSource) void {
            c.cf_webrtc_audio_source_destroy(self.handle);
        }

        pub fn pushPcmFrame(
            self: *AudioSource,
            pcm_samples: []const i16,
            frames_per_channel: usize,
            sample_rate_hz: i32,
            channels: u32,
            timestamp_ms: i64,
        ) Error!void {
            if (pcm_samples.len < frames_per_channel * @as(usize, channels)) {
                return Error.InvalidArgument;
            }
            return mapCError(c.cf_webrtc_audio_source_push_pcm_frame(
                self.handle,
                pcm_samples.ptr,
                frames_per_channel,
                sample_rate_hz,
                channels,
                timestamp_ms,
            ));
        }

        pub fn framesPushed(self: *const AudioSource) u64 {
            return c.cf_webrtc_audio_source_frames_pushed(self.handle);
        }
    };
};

/// PeerConnection and Factory subsystem namespace.
pub const pc = struct {
    pub const State = enum(u32) {
        new = c.CF_WEBRTC_PC_STATE_NEW,
        connecting = c.CF_WEBRTC_PC_STATE_CONNECTING,
        connected = c.CF_WEBRTC_PC_STATE_CONNECTED,
        disconnected = c.CF_WEBRTC_PC_STATE_DISCONNECTED,
        failed = c.CF_WEBRTC_PC_STATE_FAILED,
        closed = c.CF_WEBRTC_PC_STATE_CLOSED,
    };

    pub const SignalingState = enum(u32) {
        stable = c.CF_WEBRTC_SIGNALING_STABLE,
        have_local_offer = c.CF_WEBRTC_SIGNALING_HAVE_LOCAL_OFFER,
        have_local_pranswer = c.CF_WEBRTC_SIGNALING_HAVE_LOCAL_PRANSWER,
        have_remote_offer = c.CF_WEBRTC_SIGNALING_HAVE_REMOTE_OFFER,
        have_remote_pranswer = c.CF_WEBRTC_SIGNALING_HAVE_REMOTE_PRANSWER,
        closed = c.CF_WEBRTC_SIGNALING_CLOSED,
    };

    pub const Factory = struct {
        handle: *c.cf_webrtc_factory_t,
        backend: Backend = default_backend,

        pub fn init() Error!Factory {
            return initWithBackend(default_backend);
        }

        pub fn initWithBackend(backend: Backend) Error!Factory {
            const h = c.cf_webrtc_factory_create() orelse return Error.InitializationFailed;
            return .{
                .handle = h,
                .backend = backend,
            };
        }

        pub fn deinit(self: *Factory) void {
            c.cf_webrtc_factory_destroy(self.handle);
        }

        pub fn supportedVideoCodecs(
            self: *const Factory,
            out: []c.cf_webrtc_video_codec_info_t,
        ) []const c.cf_webrtc_video_codec_info_t {
            const count = c.cf_webrtc_factory_supported_video_codecs(
                self.handle,
                out.ptr,
                out.len,
            );
            return out[0..@min(count, out.len)];
        }

        pub fn supportedAudioCodecs(
            self: *const Factory,
            out: []c.cf_webrtc_audio_codec_info_t,
        ) []const c.cf_webrtc_audio_codec_info_t {
            const count = c.cf_webrtc_factory_supported_audio_codecs(
                self.handle,
                out.ptr,
                out.len,
            );
            return out[0..@min(count, out.len)];
        }
    };

    pub const Config = struct {
        port_range: ice.PortRange = .{},
        stun_server: ?ice.ServerConfig = null,
        dtls: dtls_srtp.Config = .{},
    };

    pub const Callbacks = struct {
        user_data: ?*anyopaque = null,
        on_ice_candidate: c.cf_webrtc_on_ice_candidate_fn = null,
        on_state_change: c.cf_webrtc_on_pc_state_change_fn = null,
        on_data_channel: c.cf_webrtc_on_data_channel_fn = null,
    };

    pub const PeerConnection = struct {
        handle: *c.cf_webrtc_pc_t,

        pub fn init(
            factory: *Factory,
            config: Config,
            callbacks: Callbacks,
        ) Error!PeerConnection {
            const c_cfg = c.cf_webrtc_pc_config_t{
                .min_port = config.port_range.min_port,
                .max_port = config.port_range.max_port,
                .stun_server_uri = if (config.stun_server) |s| s.uri.ptr else null,
            };
            const c_cb = c.cf_webrtc_pc_callbacks_t{
                .user_data = callbacks.user_data,
                .on_ice_candidate = callbacks.on_ice_candidate,
                .on_state_change = callbacks.on_state_change,
                .on_data_channel = callbacks.on_data_channel,
            };
            const h = c.cf_webrtc_pc_create(factory.handle, &c_cfg, &c_cb) orelse
                return Error.InitializationFailed;
            return .{ .handle = h };
        }

        pub fn deinit(self: *PeerConnection) void {
            c.cf_webrtc_pc_destroy(self.handle);
        }

        pub fn close(self: *PeerConnection) void {
            c.cf_webrtc_pc_close(self.handle);
        }

        pub fn state(self: *const PeerConnection) State {
            return @enumFromInt(c.cf_webrtc_pc_state(self.handle));
        }

        pub fn signalingState(self: *const PeerConnection) SignalingState {
            return @enumFromInt(c.cf_webrtc_pc_signaling_state(self.handle));
        }

        pub fn addVideoTrack(
            self: *PeerConnection,
            track_label: [:0]const u8,
            source: *media.VideoSource,
        ) Error!void {
            return mapCError(c.cf_webrtc_pc_add_video_track(
                self.handle,
                track_label.ptr,
                source.handle,
            ));
        }

        pub fn addAudioTrack(
            self: *PeerConnection,
            track_label: [:0]const u8,
            source: *media.AudioSource,
        ) Error!void {
            return mapCError(c.cf_webrtc_pc_add_audio_track(
                self.handle,
                track_label.ptr,
                source.handle,
            ));
        }

        pub fn createDataChannel(
            self: *PeerConnection,
            label: [:0]const u8,
            config: sctp.Config,
            callbacks: sctp.Callbacks,
        ) Error!sctp.DataChannel {
            const c_cfg = c.cf_webrtc_dc_config_t{
                .ordered = config.ordered,
                .reliable = config.reliable,
                .id = config.id,
                .protocol = if (config.protocol) |p| p.ptr else null,
            };
            const c_cb = c.cf_webrtc_dc_callbacks_t{
                .user_data = callbacks.user_data,
                .on_state_change = callbacks.on_state_change,
                .on_message = callbacks.on_message,
            };
            const dc_handle = c.cf_webrtc_pc_create_data_channel(
                self.handle,
                label.ptr,
                &c_cfg,
                &c_cb,
            ) orelse return Error.InitializationFailed;
            return .{ .handle = dc_handle };
        }

        pub fn createOffer(
            self: *PeerConnection,
            on_sdp: c.cf_webrtc_on_sdp_fn,
            user_data: ?*anyopaque,
        ) Error!void {
            return mapCError(c.cf_webrtc_pc_create_offer(self.handle, on_sdp, user_data));
        }

        pub fn createAnswer(
            self: *PeerConnection,
            on_sdp: c.cf_webrtc_on_sdp_fn,
            user_data: ?*anyopaque,
        ) Error!void {
            return mapCError(c.cf_webrtc_pc_create_answer(self.handle, on_sdp, user_data));
        }

        pub fn setLocalDescription(
            self: *PeerConnection,
            desc: sdp.SessionDescription,
        ) Error!void {
            if (desc.sdp_text.len == 0) return Error.InvalidArgument;
            return mapCError(c.cf_webrtc_pc_set_local_description(
                self.handle,
                @intFromEnum(desc.sdp_type),
                desc.sdp_text.ptr,
                desc.sdp_text.len,
            ));
        }

        pub fn setRemoteDescription(
            self: *PeerConnection,
            desc: sdp.SessionDescription,
        ) Error!void {
            if (desc.sdp_text.len == 0) return Error.InvalidArgument;
            return mapCError(c.cf_webrtc_pc_set_remote_description(
                self.handle,
                @intFromEnum(desc.sdp_type),
                desc.sdp_text.ptr,
                desc.sdp_text.len,
            ));
        }

        pub fn addIceCandidate(
            self: *PeerConnection,
            cand: ice.Candidate,
        ) Error!void {
            const c_cand = c.cf_webrtc_ice_candidate_t{
                .sdp_mid = cand.sdp_mid.ptr,
                .sdp_mline_index = cand.sdp_mline_index,
                .candidate_sdp = cand.candidate_sdp.ptr,
            };
            return mapCError(c.cf_webrtc_pc_add_ice_candidate(self.handle, &c_cand));
        }
    };
};

test "slim_sources manifest and defines" {
    try std.testing.expectEqualStrings(
        "e093c481bf8fd8e141fee4c007d63da488ce0ef5",
        sources.webrtc_commit,
    );
    try std.testing.expect(sources.core_srcs.len > 800);
    try std.testing.expect(sources.excluded_subsystems[0].files.len >= 160);
    try std.testing.expectEqual(sources.excluded_file_count, sources.totalExcludedFiles());
    try std.testing.expect(sources.totalExcludedFiles() >= 320);
}

test "Factory initialization and supported codecs (VP8, H.264, Opus)" {
    var factory = try pc.Factory.init();
    defer factory.deinit();

    var video_codecs_buf: [8]c.cf_webrtc_video_codec_info_t = undefined;
    const video_codecs = factory.supportedVideoCodecs(&video_codecs_buf);
    try std.testing.expectEqual(@as(usize, 2), video_codecs.len);

    var found_vp8 = false;
    var found_h264 = false;
    for (video_codecs) |vc| {
        if (vc.codec == c.CF_WEBRTC_VIDEO_CODEC_VP8) {
            found_vp8 = true;
            try std.testing.expectEqualStrings("VP8", std.mem.sliceTo(&vc.name, 0));
        } else if (vc.codec == c.CF_WEBRTC_VIDEO_CODEC_H264) {
            found_h264 = true;
            try std.testing.expectEqualStrings("H264", std.mem.sliceTo(&vc.name, 0));
            try std.testing.expectEqualStrings("42e01f", std.mem.sliceTo(&vc.profile_level_id, 0));
            try std.testing.expectEqual(
                c.CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED,
                vc.packetization_mode,
            );
        }
    }
    try std.testing.expect(found_vp8);
    try std.testing.expect(found_h264);

    var audio_codecs_buf: [4]c.cf_webrtc_audio_codec_info_t = undefined;
    const audio_codecs = factory.supportedAudioCodecs(&audio_codecs_buf);
    try std.testing.expectEqual(@as(usize, 1), audio_codecs.len);
    try std.testing.expectEqual(c.CF_WEBRTC_AUDIO_CODEC_OPUS, audio_codecs[0].codec);
    try std.testing.expectEqualStrings("opus", std.mem.sliceTo(&audio_codecs[0].name, 0));
    try std.testing.expectEqual(@as(i32, 48000), audio_codecs[0].clockrate_hz);
    try std.testing.expectEqual(@as(u32, 2), audio_codecs[0].channels);
}

test "VideoSource I420/ABGR frame injection and H.264 NAL passthrough" {
    var factory = try pc.Factory.init();
    defer factory.deinit();

    var video_src = try media.VideoSource.init(&factory, 16, 16);
    defer video_src.deinit();

    const y_plane: [16 * 16]u8 = @splat(128);
    const u_plane: [8 * 8]u8 = @splat(128);
    const v_plane: [8 * 8]u8 = @splat(128);

    try video_src.pushI420Frame(.{
        .y_plane = &y_plane,
        .y_stride = 16,
        .u_plane = &u_plane,
        .u_stride = 8,
        .v_plane = &v_plane,
        .v_stride = 8,
        .width = 16,
        .height = 16,
        .timestamp_us = 16_666,
    });
    try std.testing.expectEqual(@as(u64, 1), video_src.framesPushed());

    const abgr: [16 * 16 * 4]u8 = @splat(200);
    try video_src.pushAbgrFrame(.{
        .data = &abgr,
        .stride = 16 * 4,
        .width = 16,
        .height = 16,
        .timestamp_us = 33_333,
    });
    try std.testing.expectEqual(@as(u64, 2), video_src.framesPushed());

    const h264_idr_nal = [_]u8{ 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xe0, 0x1f, 0x00, 0x00, 0x00, 0x01, 0x65, 0x88 };
    try video_src.pushEncodedH264(&h264_idr_nal, true, 50_000);
    try std.testing.expectEqual(@as(u64, 3), video_src.framesPushed());
    try std.testing.expectEqual(@as(u64, h264_idr_nal.len), video_src.encodedBytesPushed());
}

test "AudioSource PCM frame injection" {
    var factory = try pc.Factory.init();
    defer factory.deinit();

    var audio_src = try media.AudioSource.init(&factory, 48000, 2);
    defer audio_src.deinit();

    const pcm_samples: [480 * 2]i16 = @splat(512); // 10ms stereo @ 48kHz
    try audio_src.pushPcmFrame(&pcm_samples, 480, 48000, 2, 10);
    try std.testing.expectEqual(@as(u64, 1), audio_src.framesPushed());
}

test "End-to-end PeerConnection SDP Offer/Answer, ICE candidates, and SCTP DataChannel" {
    var factory = try pc.Factory.init();
    defer factory.deinit();

    const SdpCapture = struct {
        sdp_type: sdp.Type = .offer,
        buf: [4096]u8 = undefined,
        len: usize = 0,

        fn onSdp(
            user_data: ?*anyopaque,
            c_type: c.cf_webrtc_sdp_type_t,
            sdp_ptr: [*]const u8,
            sdp_len: usize,
        ) callconv(.c) void {
            const self: *@This() = @ptrCast(@alignCast(user_data.?));
            self.sdp_type = @enumFromInt(c_type);
            const copy_len = @min(sdp_len, self.buf.len);
            @memcpy(self.buf[0..copy_len], sdp_ptr[0..copy_len]);
            self.len = copy_len;
        }

        fn slice(self: *const @This()) []const u8 {
            return self.buf[0..self.len];
        }
    };

    const PeerContext = struct {
        ice_count: usize = 0,
        last_cand_buf: [256]u8 = undefined,
        last_cand_len: usize = 0,
        last_state: pc.State = .new,
        remote_dc: ?sctp.DataChannel = null,
        rx_buf: [256]u8 = undefined,
        rx_len: usize = 0,
        rx_binary: bool = false,
        rx_count: usize = 0,

        fn onIceCandidate(
            user_data: ?*anyopaque,
            cand: *const c.cf_webrtc_ice_candidate_t,
        ) callconv(.c) void {
            const self: *@This() = @ptrCast(@alignCast(user_data.?));
            self.ice_count += 1;
            const c_slice = std.mem.span(cand.candidate_sdp);
            const n = @min(c_slice.len, self.last_cand_buf.len - 1);
            @memcpy(self.last_cand_buf[0..n], c_slice[0..n]);
            self.last_cand_buf[n] = 0;
            self.last_cand_len = n;
        }

        fn onStateChange(
            user_data: ?*anyopaque,
            new_state: c.cf_webrtc_pc_state_t,
        ) callconv(.c) void {
            const self: *@This() = @ptrCast(@alignCast(user_data.?));
            self.last_state = @enumFromInt(new_state);
        }

        fn onMessage(
            user_data: ?*anyopaque,
            data: [*]const u8,
            len: usize,
            is_binary: bool,
        ) callconv(.c) void {
            const self: *@This() = @ptrCast(@alignCast(user_data.?));
            const n = @min(len, self.rx_buf.len);
            @memcpy(self.rx_buf[0..n], data[0..n]);
            self.rx_len = n;
            self.rx_binary = is_binary;
            self.rx_count += 1;
        }

        fn onDataChannel(
            user_data: ?*anyopaque,
            dc_handle: *c.cf_webrtc_dc_t,
        ) callconv(.c) void {
            const self: *@This() = @ptrCast(@alignCast(user_data.?));
            var dc = sctp.DataChannel{ .handle = dc_handle };
            dc.setCallbacks(.{
                .user_data = user_data,
                .on_message = onMessage,
            });
            self.remote_dc = dc;
        }
    };

    var ctx_a = PeerContext{};
    var ctx_b = PeerContext{};

    var pc_a = try pc.PeerConnection.init(&factory, .{}, .{
        .user_data = &ctx_a,
        .on_ice_candidate = PeerContext.onIceCandidate,
        .on_state_change = PeerContext.onStateChange,
        .on_data_channel = PeerContext.onDataChannel,
    });
    defer pc_a.deinit();

    var pc_b = try pc.PeerConnection.init(&factory, .{}, .{
        .user_data = &ctx_b,
        .on_ice_candidate = PeerContext.onIceCandidate,
        .on_state_change = PeerContext.onStateChange,
        .on_data_channel = PeerContext.onDataChannel,
    });
    defer {
        if (ctx_b.remote_dc) |*rdc| rdc.deinit();
        pc_b.deinit();
    }

    var video_src = try media.VideoSource.init(&factory, 320, 240);
    defer video_src.deinit();
    try pc_a.addVideoTrack("display_0", &video_src);

    var audio_src = try media.AudioSource.init(&factory, 48000, 2);
    defer audio_src.deinit();
    try pc_a.addAudioTrack("audio_0", &audio_src);

    var dc_a = try pc_a.createDataChannel("input-channel", .{}, .{
        .user_data = &ctx_a,
        .on_message = PeerContext.onMessage,
    });
    defer dc_a.deinit();

    var label_buf: [64]u8 = undefined;
    try std.testing.expectEqualStrings("input-channel", dc_a.label(&label_buf));

    var offer_cap = SdpCapture{};
    try pc_a.createOffer(SdpCapture.onSdp, &offer_cap);
    const offer_desc = sdp.SessionDescription{
        .sdp_type = .offer,
        .sdp_text = offer_cap.slice(),
    };
    try std.testing.expect(offer_desc.hasVideoCodec("VP8"));
    try std.testing.expect(offer_desc.hasVideoCodec("H264"));

    try pc_a.setLocalDescription(offer_desc);
    try std.testing.expectEqual(pc.SignalingState.have_local_offer, pc_a.signalingState());
    try std.testing.expect(ctx_a.ice_count >= 1);

    try pc_b.setRemoteDescription(offer_desc);
    try std.testing.expectEqual(pc.SignalingState.have_remote_offer, pc_b.signalingState());

    var answer_cap = SdpCapture{};
    try pc_b.createAnswer(SdpCapture.onSdp, &answer_cap);
    const answer_desc = sdp.SessionDescription{
        .sdp_type = .answer,
        .sdp_text = answer_cap.slice(),
    };

    try pc_b.setLocalDescription(answer_desc);
    try std.testing.expectEqual(pc.SignalingState.stable, pc_b.signalingState());
    try std.testing.expect(ctx_b.ice_count >= 1);

    try pc_a.setRemoteDescription(answer_desc);
    try std.testing.expectEqual(pc.SignalingState.stable, pc_a.signalingState());

    const cand_a_z: [:0]const u8 = ctx_a.last_cand_buf[0..ctx_a.last_cand_len :0];
    try pc_b.addIceCandidate(.{
        .sdp_mid = "0",
        .sdp_mline_index = 0,
        .candidate_sdp = cand_a_z,
    });

    const cand_b_z: [:0]const u8 = ctx_b.last_cand_buf[0..ctx_b.last_cand_len :0];
    try pc_a.addIceCandidate(.{
        .sdp_mid = "0",
        .sdp_mline_index = 0,
        .candidate_sdp = cand_b_z,
    });

    try std.testing.expectEqual(pc.State.connected, pc_a.state());
    try std.testing.expectEqual(pc.State.connected, pc_b.state());
    try std.testing.expectEqual(sctp.State.open, dc_a.state());
    try std.testing.expect(ctx_b.remote_dc != null);

    // Send text message from Peer A -> Peer B
    try dc_a.sendText("touch:down:120,240");
    try std.testing.expectEqual(@as(usize, 1), ctx_b.rx_count);
    try std.testing.expect(!ctx_b.rx_binary);
    try std.testing.expectEqualStrings("touch:down:120,240", ctx_b.rx_buf[0..ctx_b.rx_len]);

    // Send binary message from Peer B -> Peer A
    var remote_dc = ctx_b.remote_dc.?;
    const bin_payload = [_]u8{ 0xCA, 0xFE, 0xBA, 0xBE, 0x01, 0x02 };
    try remote_dc.sendBinary(&bin_payload);
    try std.testing.expectEqual(@as(usize, 1), ctx_a.rx_count);
    try std.testing.expect(ctx_a.rx_binary);
    try std.testing.expectEqualSlices(u8, &bin_payload, ctx_a.rx_buf[0..ctx_a.rx_len]);
}
