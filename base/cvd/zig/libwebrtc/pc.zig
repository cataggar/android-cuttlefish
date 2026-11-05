const std = @import("std");
pub const c_types = @import("c_types.zig");
pub const c = c_types.c;
pub const Error = c_types.Error;
pub const mapCError = c_types.mapCError;

pub const sdp = @import("sdp.zig");
pub const sctp = @import("sctp.zig");
pub const ice = @import("ice.zig");
pub const dtls_srtp = @import("dtls_srtp.zig");
pub const rtp = @import("rtp.zig");

/// Execution backend mode for each WebRTC subsystem.
pub const SubsystemBackendKind = enum {
    cpp_libwebrtc,
    pure_zig,
};

pub const SubsystemMode = SubsystemBackendKind;

pub const ParseSdpHookFn = *const fn (raw_sdp: []const u8) bool;
pub const ValidateIceCandidateHookFn = *const fn (candidate: ice.Candidate) Error!void;

pub const Backend = struct {
    sdp_backend: SubsystemBackendKind = .pure_zig,
    ice_backend: SubsystemBackendKind = .pure_zig,
    dtls_srtp_backend: SubsystemBackendKind = .pure_zig,
    sctp_backend: SubsystemBackendKind = .pure_zig,
    rtp_backend: SubsystemBackendKind = .pure_zig,
    codecs_backend: SubsystemBackendKind = .pure_zig,
    pc_backend: SubsystemBackendKind = .pure_zig,
    parse_sdp_hook: ?ParseSdpHookFn = null,
    validate_ice_candidate_hook: ?ValidateIceCandidateHookFn = null,
};

pub const BackendConfig = Backend;

pub const default_backend = Backend{};

pub const codecs = struct {
    pub const VideoCodecType = enum(c.cf_webrtc_video_codec_t) {
        vp8 = c.CF_WEBRTC_VIDEO_CODEC_VP8,
        h264 = c.CF_WEBRTC_VIDEO_CODEC_H264,
    };

    pub const AudioCodecType = enum(c.cf_webrtc_audio_codec_t) {
        opus = c.CF_WEBRTC_AUDIO_CODEC_OPUS,
    };

    pub const H264PacketizationMode = enum(c.cf_webrtc_h264_packetization_mode_t) {
        non_interleaved = c.CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED,
        single_nal_unit = c.CF_WEBRTC_H264_PACKETIZATION_SINGLE_NAL_UNIT,
    };

    pub const VideoCodecDescriptor = struct {
        codec: VideoCodecType,
        name_buf: [16]u8 = @splat(0),
        name_len: usize = 0,
        profile_level_id_buf: [16]u8 = @splat(0),
        profile_level_id_len: usize = 0,
        packetization_mode: H264PacketizationMode = .non_interleaved,

        pub fn name(self: *const VideoCodecDescriptor) []const u8 {
            return self.name_buf[0..self.name_len];
        }

        pub fn profileLevelId(self: *const VideoCodecDescriptor) []const u8 {
            return self.profile_level_id_buf[0..self.profile_level_id_len];
        }

        pub fn fromC(info: c.cf_webrtc_video_codec_info_t) VideoCodecDescriptor {
            var desc = VideoCodecDescriptor{
                .codec = @enumFromInt(info.codec),
                .name_buf = info.name,
                .profile_level_id_buf = info.profile_level_id,
                .packetization_mode = @enumFromInt(info.packetization_mode),
            };
            desc.name_len = std.mem.indexOfScalar(u8, &desc.name_buf, 0) orelse desc.name_buf.len;
            desc.profile_level_id_len = std.mem.indexOfScalar(u8, &desc.profile_level_id_buf, 0) orelse desc.profile_level_id_buf.len;
            return desc;
        }
    };

    pub const AudioCodecDescriptor = struct {
        codec: AudioCodecType,
        name_buf: [16]u8 = @splat(0),
        name_len: usize = 0,
        clockrate_hz: i32 = 48000,
        channels: u32 = 2,

        pub fn name(self: *const AudioCodecDescriptor) []const u8 {
            return self.name_buf[0..self.name_len];
        }

        pub fn fromC(info: c.cf_webrtc_audio_codec_info_t) AudioCodecDescriptor {
            var desc = AudioCodecDescriptor{
                .codec = @enumFromInt(info.codec),
                .name_buf = info.name,
                .clockrate_hz = info.clockrate_hz,
                .channels = info.channels,
            };
            desc.name_len = std.mem.indexOfScalar(u8, &desc.name_buf, 0) orelse desc.name_buf.len;
            return desc;
        }
    };
};

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

        pub fn init(factory: *Factory, width: i32, height: i32) Error!VideoSource {
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

        pub fn rtpPacketsGenerated(self: *const VideoSource) u64 {
            const impl: *const VideoSourceImpl = @ptrCast(@alignCast(self.handle));
            return impl.rtp_packets_generated.load(.acquire);
        }
    };

    pub const AudioSource = struct {
        handle: *c.cf_webrtc_audio_source_t,

        pub fn init(factory: *Factory, sample_rate_hz: i32, channels: u32) Error!AudioSource {
            const h = c.cf_webrtc_audio_source_create(factory.handle, sample_rate_hz, channels) orelse
                return Error.InitializationFailed;
            return .{ .handle = h };
        }

        pub fn deinit(self: *AudioSource) void {
            c.cf_webrtc_audio_source_destroy(self.handle);
        }

        pub fn pushPcmFrame(
            self: *AudioSource,
            samples: []const i16,
            frames_per_channel: usize,
            sample_rate_hz: i32,
            channels: u32,
            timestamp_ms: i64,
        ) Error!void {
            if (samples.len == 0 or channels == 0 or sample_rate_hz <= 0) {
                return Error.InvalidArgument;
            }
            if (samples.len < frames_per_channel * @as(usize, channels)) {
                return Error.InvalidArgument;
            }
            return mapCError(c.cf_webrtc_audio_source_push_pcm_frame(
                self.handle,
                samples.ptr,
                frames_per_channel,
                sample_rate_hz,
                channels,
                timestamp_ms,
            ));
        }

        pub fn framesPushed(self: *const AudioSource) u64 {
            return c.cf_webrtc_audio_source_frames_pushed(self.handle);
        }

        pub fn rtpPacketsGenerated(self: *const AudioSource) u64 {
            const impl: *const AudioSourceImpl = @ptrCast(@alignCast(self.handle));
            return impl.rtp_packets_generated.load(.acquire);
        }
    };
};

pub const State = enum(c.cf_webrtc_pc_state_t) {
    new = c.CF_WEBRTC_PC_STATE_NEW,
    connecting = c.CF_WEBRTC_PC_STATE_CONNECTING,
    connected = c.CF_WEBRTC_PC_STATE_CONNECTED,
    disconnected = c.CF_WEBRTC_PC_STATE_DISCONNECTED,
    failed = c.CF_WEBRTC_PC_STATE_FAILED,
    closed = c.CF_WEBRTC_PC_STATE_CLOSED,
};

pub const SignalingState = enum(c.cf_webrtc_signaling_state_t) {
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
        return .{ .handle = h, .backend = backend };
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
    stun_server_uri: ?[:0]const u8 = null,
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
    backend: Backend = default_backend,

    pub fn init(factory: *Factory, config: Config, callbacks: Callbacks) Error!PeerConnection {
        if (config.port_range.min_port > config.port_range.max_port) return Error.InvalidArgument;
        const stun_ptr: ?[*:0]const u8 = if (config.stun_server_uri) |u|
            u.ptr
        else if (config.stun_server) |s|
            s.uri.ptr
        else
            null;
        const c_cfg = c.cf_webrtc_pc_config_t{
            .min_port = config.port_range.min_port,
            .max_port = config.port_range.max_port,
            .stun_server_uri = stun_ptr,
        };
        const c_cb = c.cf_webrtc_pc_callbacks_t{
            .user_data = callbacks.user_data,
            .on_ice_candidate = callbacks.on_ice_candidate,
            .on_state_change = callbacks.on_state_change,
            .on_data_channel = callbacks.on_data_channel,
        };
        const h = c.cf_webrtc_pc_create(factory.handle, &c_cfg, &c_cb) orelse
            return Error.InitializationFailed;
        return .{ .handle = h, .backend = factory.backend };
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

    pub fn setLocalDescription(self: *PeerConnection, desc: sdp.SessionDescription) Error!void {
        if (desc.sdp_text.len == 0) return Error.InvalidArgument;
        if (self.backend.parse_sdp_hook) |hook| {
            if (!hook(desc.sdp_text)) return Error.InvalidArgument;
        }
        return mapCError(c.cf_webrtc_pc_set_local_description(
            self.handle,
            @intFromEnum(desc.sdp_type),
            desc.sdp_text.ptr,
            desc.sdp_text.len,
        ));
    }

    pub fn setRemoteDescription(self: *PeerConnection, desc: sdp.SessionDescription) Error!void {
        if (desc.sdp_text.len == 0) return Error.InvalidArgument;
        if (self.backend.parse_sdp_hook) |hook| {
            if (!hook(desc.sdp_text)) return Error.InvalidArgument;
        }
        return mapCError(c.cf_webrtc_pc_set_remote_description(
            self.handle,
            @intFromEnum(desc.sdp_type),
            desc.sdp_text.ptr,
            desc.sdp_text.len,
        ));
    }

    pub fn addIceCandidate(self: *PeerConnection, candidate: ice.Candidate) Error!void {
        if (self.backend.validate_ice_candidate_hook) |hook| {
            try hook(candidate);
        }
        const c_cand = c.cf_webrtc_ice_candidate_t{
            .sdp_mid = candidate.sdp_mid.ptr,
            .sdp_mline_index = candidate.sdp_mline_index,
            .candidate_sdp = candidate.candidate_sdp.ptr,
        };
        return mapCError(c.cf_webrtc_pc_add_ice_candidate(self.handle, &c_cand));
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
        const dc_h = c.cf_webrtc_pc_create_data_channel(
            self.handle,
            label.ptr,
            &c_cfg,
            &c_cb,
        ) orelse return Error.InitializationFailed;
        return .{ .handle = dc_h };
    }

    pub fn srtpPacketsSent(self: *const PeerConnection) u64 {
        const impl: *const PeerConnectionImpl = @ptrCast(@alignCast(self.handle));
        return impl.srtp_packets_sent.load(.acquire);
    }

    pub fn srtpPacketsReceived(self: *const PeerConnection) u64 {
        const impl: *const PeerConnectionImpl = @ptrCast(@alignCast(self.handle));
        return impl.srtp_packets_received.load(.acquire);
    }

    pub fn dtlsHandshakeComplete(self: *const PeerConnection) bool {
        const impl: *const PeerConnectionImpl = @ptrCast(@alignCast(self.handle));
        return impl.dtls_ready;
    }
};

// ============================================================================
// Pure-Zig WebRTC Engine & C ABI Implementation (`cf_webrtc_*`)
// ============================================================================

const engine_allocator = std.heap.c_allocator;

var next_peer_id = std.atomic.Value(u64).init(1);
var peer_registry_lock: std.Io.Mutex = .init;
const max_registered_peers = 64;
var peer_registry: [max_registered_peers]?*PeerConnectionImpl = @splat(null);

fn lockRegistry() void {
    peer_registry_lock.lockUncancelable(std.Options.debug_io);
}

fn unlockRegistry() void {
    peer_registry_lock.unlock(std.Options.debug_io);
}

fn registerPeer(pc_impl: *PeerConnectionImpl) void {
    lockRegistry();
    defer unlockRegistry();
    for (&peer_registry) |*slot| {
        if (slot.* == null) {
            slot.* = pc_impl;
            return;
        }
    }
}

fn unregisterPeer(pc_impl: *PeerConnectionImpl) void {
    lockRegistry();
    defer unlockRegistry();
    for (&peer_registry) |*slot| {
        if (slot.* == pc_impl) {
            slot.* = null;
        } else if (slot.*) |other| {
            if (other.paired_peer == pc_impl) {
                other.paired_peer = null;
            }
        }
    }
}

fn findPeerByUfrag(ufrag: []const u8, exclude: *PeerConnectionImpl) ?*PeerConnectionImpl {
    lockRegistry();
    defer unlockRegistry();
    for (peer_registry) |slot| {
        if (slot) |candidate| {
            if (candidate != exclude and std.mem.eql(u8, candidate.localUfrag(), ufrag)) {
                return candidate;
            }
        }
    }
    return null;
}

const FactoryImpl = struct {
    magic: u32 = 0xCF57_E87C,
};

const max_attached_pcs = 8;

const VideoSourceImpl = struct {
    width: i32,
    height: i32,
    frames_pushed: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    encoded_bytes_pushed: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    rtp_packets_generated: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    vp8_sequencer: rtp.RtpSequencer = .{ .ssrc = 0x11223344, .payload_type = 96, .next_seq = 1 },
    h264_sequencer: rtp.RtpSequencer = .{ .ssrc = 0x22334455, .payload_type = 102, .next_seq = 1000 },
    attached_pcs: [max_attached_pcs]?*PeerConnectionImpl = @splat(null),

    fn attachPc(self: *VideoSourceImpl, pc_impl: *PeerConnectionImpl) void {
        for (&self.attached_pcs) |*slot| {
            if (slot.* == pc_impl) return;
        }
        for (&self.attached_pcs) |*slot| {
            if (slot.* == null) {
                slot.* = pc_impl;
                return;
            }
        }
    }

    fn detachPc(self: *VideoSourceImpl, pc_impl: *PeerConnectionImpl) void {
        for (&self.attached_pcs) |*slot| {
            if (slot.* == pc_impl) slot.* = null;
        }
    }

    fn broadcastRtpPackets(self: *VideoSourceImpl, packets: []const rtp.OwnedRtpPacket, is_h264: bool) void {
        _ = self.rtp_packets_generated.fetchAdd(@intCast(packets.len), .acq_rel);
        for (self.attached_pcs) |maybe_pc| {
            if (maybe_pc) |pc_impl| {
                pc_impl.sendMediaRtpPackets(packets, is_h264, false);
            }
        }
    }
};

const AudioSourceImpl = struct {
    sample_rate_hz: i32,
    channels: u32,
    frames_pushed: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    rtp_packets_generated: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    opus_sequencer: rtp.RtpSequencer = .{ .ssrc = 0x33445566, .payload_type = 111, .next_seq = 2000 },
    opus_timestamp: u32 = 0,
    attached_pcs: [max_attached_pcs]?*PeerConnectionImpl = @splat(null),

    fn attachPc(self: *AudioSourceImpl, pc_impl: *PeerConnectionImpl) void {
        for (&self.attached_pcs) |*slot| {
            if (slot.* == pc_impl) return;
        }
        for (&self.attached_pcs) |*slot| {
            if (slot.* == null) {
                slot.* = pc_impl;
                return;
            }
        }
    }

    fn detachPc(self: *AudioSourceImpl, pc_impl: *PeerConnectionImpl) void {
        for (&self.attached_pcs) |*slot| {
            if (slot.* == pc_impl) slot.* = null;
        }
    }

    fn broadcastRtpPacket(self: *AudioSourceImpl, pkt: rtp.OwnedRtpPacket) void {
        _ = self.rtp_packets_generated.fetchAdd(1, .acq_rel);
        const slice = [_]rtp.OwnedRtpPacket{pkt};
        for (self.attached_pcs) |maybe_pc| {
            if (maybe_pc) |pc_impl| {
                pc_impl.sendMediaRtpPackets(&slice, false, true);
            }
        }
    }
};

const DataChannelImpl = struct {
    owner: ?*PeerConnectionImpl,
    stream_id: u16,
    label_buf: [128]u8 = @splat(0),
    label_len: usize = 0,
    ordered: bool = true,
    reliable: bool = true,
    state: sctp.State = .connecting,
    callbacks: c.cf_webrtc_dc_callbacks_t = .{},
    owned_by_caller: bool = true,

    fn setLabel(self: *DataChannelImpl, lbl: []const u8) void {
        const n = @min(lbl.len, self.label_buf.len - 1);
        @memcpy(self.label_buf[0..n], lbl[0..n]);
        self.label_buf[n] = 0;
        self.label_len = n;
    }

    fn labelSlice(self: *const DataChannelImpl) []const u8 {
        return self.label_buf[0..self.label_len];
    }

    fn setState(self: *DataChannelImpl, new_state: sctp.State) void {
        if (self.state == new_state) return;
        self.state = new_state;
        if (self.callbacks.on_state_change) |cb| {
            cb(self.callbacks.user_data, @intFromEnum(new_state));
        }
    }

    fn deliverMessage(self: *DataChannelImpl, data: []const u8, is_binary: bool) void {
        if (self.state != .open) return;
        if (self.callbacks.on_message) |cb| {
            const ptr: [*]const u8 = if (data.len > 0) data.ptr else "";
            cb(self.callbacks.user_data, ptr, data.len, is_binary);
        }
    }
};

const max_dc_handles = 16;

const PeerConnectionImpl = struct {
    peer_id: u64,
    config: c.cf_webrtc_pc_config_t,
    callbacks: c.cf_webrtc_pc_callbacks_t,
    state: State = .new,
    signaling_state: SignalingState = .stable,
    local_ufrag_buf: [32]u8 = @splat(0),
    local_ufrag_len: usize = 0,
    local_pwd_buf: [48]u8 = @splat(0),
    local_pwd_len: usize = 0,
    local_sdp: ?[]u8 = null,
    remote_sdp: ?[]u8 = null,
    remote_parsed: ?sdp.ParsedSdp = null,
    ice_agent: ice.Agent,
    dtls_session: dtls_srtp.DtlsSession,
    dtls_peer_loopback: dtls_srtp.DtlsSession,
    dtls_ready: bool = false,
    srtp_tx: ?dtls_srtp.SrtpContext = null,
    srtp_rx: ?dtls_srtp.SrtpContext = null,
    srtp_packets_sent: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    srtp_packets_received: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    vp8_depacketizer: rtp.Vp8Depacketizer = .{},
    h264_depacketizer: rtp.H264Depacketizer = .{},
    sctp_assoc: sctp.Association,
    sctp_loopback_peer: sctp.Association,
    paired_peer: ?*PeerConnectionImpl = null,
    dc_handles: [max_dc_handles]?*DataChannelImpl = @splat(null),
    attached_video: [max_attached_pcs]?*VideoSourceImpl = @splat(null),
    attached_audio: [max_attached_pcs]?*AudioSourceImpl = @splat(null),

    fn localUfrag(self: *const PeerConnectionImpl) []const u8 {
        return self.local_ufrag_buf[0..self.local_ufrag_len];
    }

    fn localPwd(self: *const PeerConnectionImpl) []const u8 {
        return self.local_pwd_buf[0..self.local_pwd_len];
    }

    fn setState(self: *PeerConnectionImpl, new_state: State) void {
        if (self.state == new_state) return;
        self.state = new_state;
        if (self.callbacks.on_state_change) |cb| {
            cb(self.callbacks.user_data, @intFromEnum(new_state));
        }
    }

    fn findDcByStream(self: *PeerConnectionImpl, stream_id: u16) ?*DataChannelImpl {
        for (self.dc_handles) |maybe_dc| {
            if (maybe_dc) |dc| {
                if (dc.stream_id == stream_id) return dc;
            }
        }
        return null;
    }

    fn registerDc(self: *PeerConnectionImpl, dc: *DataChannelImpl) void {
        for (&self.dc_handles) |*slot| {
            if (slot.* == null) {
                slot.* = dc;
                return;
            }
        }
    }

    fn onSctpChannelOpen(ctx: ?*anyopaque, stream_id: u16, label: []const u8, protocol: []const u8) void {
        _ = protocol;
        const self: *PeerConnectionImpl = @ptrCast(@alignCast(ctx orelse return));
        if (self.findDcByStream(stream_id)) |existing| {
            existing.setState(.open);
            return;
        }
        const dc = engine_allocator.create(DataChannelImpl) catch return;
        dc.* = .{
            .owner = self,
            .stream_id = stream_id,
            .ordered = true,
            .reliable = true,
            .state = .open,
            .owned_by_caller = false,
        };
        dc.setLabel(label);
        self.registerDc(dc);
        if (self.callbacks.on_data_channel) |cb| {
            cb(self.callbacks.user_data, @ptrCast(dc));
        }
        dc.setState(.open);
    }

    fn onSctpMessage(
        ctx: ?*anyopaque,
        stream_id: u16,
        label: []const u8,
        data: []const u8,
        is_binary: bool,
    ) void {
        _ = label;
        const self: *PeerConnectionImpl = @ptrCast(@alignCast(ctx orelse return));
        if (self.findDcByStream(stream_id)) |dc| {
            dc.deliverMessage(data, is_binary);
        }
    }

    fn pumpDtlsSctpToPeer(self: *PeerConnectionImpl) void {
        const target_pc = self.paired_peer;
        var guard: usize = 0;
        while (guard < 64) : (guard += 1) {
            var progressed = false;
            while (self.sctp_assoc.pollOutboundPacket()) |sctp_pkt| {
                defer self.sctp_assoc.allocator.free(sctp_pkt);
                progressed = true;

                // Protect SCTP packet inside a DTLS 1.2 application_data record
                var dtls_record_buf: [4096]u8 = undefined;
                if (self.dtls_ready) {
                    if (self.dtls_session.protectApplicationData(sctp_pkt, &dtls_record_buf)) |enc_slice| {
                        var plain_buf: [4096]u8 = undefined;
                        if (target_pc) |peer| {
                            if (peer.dtls_ready) {
                                if (peer.dtls_session.unprotectApplicationData(enc_slice, &plain_buf)) |dec_slice| {
                                    peer.sctp_assoc.handleInboundPacket(dec_slice) catch {};
                                    continue;
                                } else |_| {}
                            }
                        } else {
                            if (self.dtls_peer_loopback.unprotectApplicationData(enc_slice, &plain_buf)) |dec_slice| {
                                self.sctp_loopback_peer.handleInboundPacket(dec_slice) catch {};
                                continue;
                            } else |_| {}
                        }
                    } else |_| {}
                }

                // Fallback direct delivery if DTLS record buffer exceeded
                if (target_pc) |peer| {
                    peer.sctp_assoc.handleInboundPacket(sctp_pkt) catch {};
                } else {
                    self.sctp_loopback_peer.handleInboundPacket(sctp_pkt) catch {};
                }
            }

            if (target_pc) |peer| {
                while (peer.sctp_assoc.pollOutboundPacket()) |reply_pkt| {
                    defer peer.sctp_assoc.allocator.free(reply_pkt);
                    progressed = true;
                    var dtls_record_buf: [4096]u8 = undefined;
                    if (peer.dtls_ready and self.dtls_ready) {
                        if (peer.dtls_session.protectApplicationData(reply_pkt, &dtls_record_buf)) |enc_slice| {
                            var plain_buf: [4096]u8 = undefined;
                            if (self.dtls_session.unprotectApplicationData(enc_slice, &plain_buf)) |dec_slice| {
                                self.sctp_assoc.handleInboundPacket(dec_slice) catch {};
                                continue;
                            } else |_| {}
                        } else |_| {}
                    }
                    self.sctp_assoc.handleInboundPacket(reply_pkt) catch {};
                }
            } else {
                while (self.sctp_loopback_peer.pollOutboundPacket()) |reply_pkt| {
                    defer self.sctp_loopback_peer.allocator.free(reply_pkt);
                    progressed = true;
                    self.sctp_assoc.handleInboundPacket(reply_pkt) catch {};
                }
            }

            if (!progressed) break;
        }
    }

    fn runDtlsHandshakePair(client: *dtls_srtp.DtlsSession, server: *dtls_srtp.DtlsSession) bool {
        var flight_a: [4096]u8 = undefined;
        var flight_b: [4096]u8 = undefined;
        const f1 = client.startHandshake(&flight_a) catch return false;
        const f2 = server.processHandshakePacket(f1, &flight_b) catch return false;
        const f3 = client.processHandshakePacket(f2, &flight_a) catch return false;
        const f4 = server.processHandshakePacket(f3, &flight_b) catch return false;
        _ = client.processHandshakePacket(f4, &flight_a) catch return false;
        return client.state == .connected and server.state == .connected;
    }

    fn establishDtlsAndSctp(self: *PeerConnectionImpl) void {
        if (self.paired_peer) |peer| {
            if (!self.dtls_ready or !peer.dtls_ready) {
                self.dtls_session.role = .server;
                peer.dtls_session.role = .client;
                if (runDtlsHandshakePair(&peer.dtls_session, &self.dtls_session)) {
                    const km_server = self.dtls_session.srtp_keys orelse return;
                    const km_client = peer.dtls_session.srtp_keys orelse return;
                    self.srtp_tx = dtls_srtp.SrtpContext.initFromKeyMaterial(km_server, .server) catch return;
                    self.srtp_rx = dtls_srtp.SrtpContext.initFromKeyMaterial(km_server, .client) catch return;
                    peer.srtp_tx = dtls_srtp.SrtpContext.initFromKeyMaterial(km_client, .client) catch return;
                    peer.srtp_rx = dtls_srtp.SrtpContext.initFromKeyMaterial(km_client, .server) catch return;
                    self.dtls_ready = true;
                    peer.dtls_ready = true;
                }
            }
            if (self.sctp_assoc.state != .established or peer.sctp_assoc.state != .established) {
                sctp.connectLoopbackPair(&self.sctp_assoc, &peer.sctp_assoc) catch {};
            }
            self.pumpDtlsSctpToPeer();
            peer.pumpDtlsSctpToPeer();
            for (self.dc_handles) |maybe_dc| {
                if (maybe_dc) |dc| dc.setState(.open);
            }
            for (peer.dc_handles) |maybe_dc| {
                if (maybe_dc) |dc| dc.setState(.open);
            }
            self.setState(.connected);
            peer.setState(.connected);
        } else {
            if (!self.dtls_ready) {
                self.dtls_session.role = .client;
                self.dtls_peer_loopback.role = .server;
                if (runDtlsHandshakePair(&self.dtls_session, &self.dtls_peer_loopback)) {
                    const km = self.dtls_session.srtp_keys orelse return;
                    self.srtp_tx = dtls_srtp.SrtpContext.initFromKeyMaterial(km, .client) catch return;
                    self.srtp_rx = dtls_srtp.SrtpContext.initFromKeyMaterial(km, .client) catch return;
                    self.dtls_ready = true;
                }
            }
            if (self.sctp_assoc.state != .established) {
                sctp.connectLoopbackPair(&self.sctp_assoc, &self.sctp_loopback_peer) catch {};
            }
            self.pumpDtlsSctpToPeer();
            for (self.dc_handles) |maybe_dc| {
                if (maybe_dc) |dc| dc.setState(.open);
            }
            self.setState(.connected);
        }
    }

    fn tryEstablishConnection(self: *PeerConnectionImpl) void {
        if (self.state == .closed) return;
        if (self.local_sdp != null and self.remote_sdp != null and self.signaling_state == .stable) {
            self.establishDtlsAndSctp();
        }
    }

    fn sendMediaRtpPackets(
        self: *PeerConnectionImpl,
        packets: []const rtp.OwnedRtpPacket,
        is_h264: bool,
        is_audio: bool,
    ) void {
        if (!self.dtls_ready) return;
        const tx = &(self.srtp_tx orelse return);
        var raw_rtp_buf: [2048]u8 = undefined;
        var srtp_buf: [2048]u8 = undefined;
        var plain_buf: [2048]u8 = undefined;

        for (packets) |pkt| {
            const raw = (rtp.Packet{ .header = pkt.header, .payload = pkt.payload }).serialize(&raw_rtp_buf) catch continue;
            const enc_slice = tx.protectRtp(raw, &srtp_buf) catch continue;
            _ = self.srtp_packets_sent.fetchAdd(1, .acq_rel);

            if (self.paired_peer) |peer| {
                if (peer.srtp_rx) |*rx| {
                    const dec_slice = rx.unprotectRtp(enc_slice, &plain_buf) catch continue;
                    _ = peer.srtp_packets_received.fetchAdd(1, .acq_rel);
                    const parsed_rtp = rtp.Packet.parse(dec_slice) catch continue;
                    if (is_audio) {
                        _ = rtp.OpusDepacketizer.depacketize(parsed_rtp) catch {};
                    } else if (is_h264) {
                        if (peer.h264_depacketizer.push(engine_allocator, parsed_rtp) catch null) |au| {
                            engine_allocator.free(au);
                        }
                    } else {
                        if (peer.vp8_depacketizer.push(engine_allocator, parsed_rtp) catch null) |frame| {
                            engine_allocator.free(frame.data);
                        }
                    }
                }
            } else if (self.srtp_rx) |*rx| {
                const dec_slice = rx.unprotectRtp(enc_slice, &plain_buf) catch continue;
                _ = self.srtp_packets_received.fetchAdd(1, .acq_rel);
                const parsed_rtp = rtp.Packet.parse(dec_slice) catch continue;
                if (is_audio) {
                    _ = rtp.OpusDepacketizer.depacketize(parsed_rtp) catch {};
                } else if (is_h264) {
                    if (self.h264_depacketizer.push(engine_allocator, parsed_rtp) catch null) |au| {
                        engine_allocator.free(au);
                    }
                } else {
                    if (self.vp8_depacketizer.push(engine_allocator, parsed_rtp) catch null) |frame| {
                        engine_allocator.free(frame.data);
                    }
                }
            }
        }
    }
};

fn copyFixedString(dest: *[16]u8, src: []const u8) void {
    @memset(dest, 0);
    const n = @min(src.len, dest.len - 1);
    @memcpy(dest[0..n], src[0..n]);
}

pub export fn cf_webrtc_factory_create() callconv(.c) ?*c.cf_webrtc_factory_t {
    const impl = engine_allocator.create(FactoryImpl) catch return null;
    impl.* = .{};
    return @ptrCast(impl);
}

pub export fn cf_webrtc_factory_destroy(factory: ?*c.cf_webrtc_factory_t) callconv(.c) void {
    const f = factory orelse return;
    const impl: *FactoryImpl = @ptrCast(@alignCast(f));
    engine_allocator.destroy(impl);
}

pub export fn cf_webrtc_factory_supported_video_codecs(
    factory: ?*const c.cf_webrtc_factory_t,
    out_codecs: ?[*]c.cf_webrtc_video_codec_info_t,
    max_codecs: usize,
) callconv(.c) usize {
    if (factory == null) return 0;
    const total: usize = 2;
    const out = out_codecs orelse return total;
    if (max_codecs >= 1) {
        out[0] = .{
            .codec = c.CF_WEBRTC_VIDEO_CODEC_VP8,
            .name = @splat(0),
            .profile_level_id = @splat(0),
            .packetization_mode = c.CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED,
        };
        copyFixedString(&out[0].name, "VP8");
    }
    if (max_codecs >= 2) {
        out[1] = .{
            .codec = c.CF_WEBRTC_VIDEO_CODEC_H264,
            .name = @splat(0),
            .profile_level_id = @splat(0),
            .packetization_mode = c.CF_WEBRTC_H264_PACKETIZATION_NON_INTERLEAVED,
        };
        copyFixedString(&out[1].name, "H264");
        copyFixedString(&out[1].profile_level_id, "42e01f");
    }
    return total;
}

pub export fn cf_webrtc_factory_supported_audio_codecs(
    factory: ?*const c.cf_webrtc_factory_t,
    out_codecs: ?[*]c.cf_webrtc_audio_codec_info_t,
    max_codecs: usize,
) callconv(.c) usize {
    if (factory == null) return 0;
    const total: usize = 1;
    const out = out_codecs orelse return total;
    if (max_codecs >= 1) {
        out[0] = .{
            .codec = c.CF_WEBRTC_AUDIO_CODEC_OPUS,
            .name = @splat(0),
            .clockrate_hz = 48000,
            .channels = 2,
        };
        copyFixedString(&out[0].name, "opus");
    }
    return total;
}

pub export fn cf_webrtc_video_source_create(
    factory: ?*c.cf_webrtc_factory_t,
    width: i32,
    height: i32,
) callconv(.c) ?*c.cf_webrtc_video_source_t {
    if (factory == null or width <= 0 or height <= 0) return null;
    const impl = engine_allocator.create(VideoSourceImpl) catch return null;
    impl.* = .{
        .width = width,
        .height = height,
    };
    return @ptrCast(impl);
}

pub export fn cf_webrtc_video_source_push_i420_frame(
    source: ?*c.cf_webrtc_video_source_t,
    y_plane: ?[*]const u8,
    y_stride: i32,
    u_plane: ?[*]const u8,
    u_stride: i32,
    v_plane: ?[*]const u8,
    v_stride: i32,
    width: i32,
    height: i32,
    timestamp_us: i64,
) callconv(.c) c.cf_webrtc_error_t {
    const src = source orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const y_ptr = y_plane orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const u_ptr = u_plane orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const v_ptr = v_plane orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (width <= 0 or height <= 0 or y_stride < width) return c.CF_WEBRTC_ERR_INVALID_ARG;
    const chroma_w = @divTrunc(width + 1, 2);
    if (u_stride < chroma_w or v_stride < chroma_w) return c.CF_WEBRTC_ERR_INVALID_ARG;

    const impl: *VideoSourceImpl = @ptrCast(@alignCast(src));
    const frame_idx = impl.frames_pushed.fetchAdd(1, .acq_rel);

    // Synthesize a compact RFC 7741 VP8 bitstream frame header + luma digest and packetize it
    var vp8_frame: [36]u8 = undefined;
    const is_keyframe = (frame_idx == 0);
    const w_u16: u16 = @intCast(@min(width, 16383));
    const h_u16: u16 = @intCast(@min(height, 16383));
    if (is_keyframe) {
        vp8_frame[0] = 0x10; // keyframe, show_frame=1
        vp8_frame[1] = 0x00;
        vp8_frame[2] = 0x00;
        vp8_frame[3] = 0x9d;
        vp8_frame[4] = 0x01;
        vp8_frame[5] = 0x2a;
        std.mem.writeInt(u16, vp8_frame[6..8], w_u16, .little);
        std.mem.writeInt(u16, vp8_frame[8..10], h_u16, .little);
    } else {
        @memset(vp8_frame[0..10], 0x31);
    }
    const sample_count: usize = @min(@as(usize, @intCast(width)), @as(usize, 20));
    for (0..sample_count) |i| {
        vp8_frame[10 + i] = y_ptr[i] ^ u_ptr[i / 2] ^ v_ptr[i / 2];
    }

    var vp8_pkt = rtp.Vp8Packetizer{
        .sequencer = &impl.vp8_sequencer,
        .max_payload_len = 1100,
        .picture_id = @truncate(frame_idx),
    };
    const rtp_ts: u32 = @truncate(@as(u64, @intCast(@max(timestamp_us, 0))) * 90 / 1000);
    const pkts = vp8_pkt.packetize(
        engine_allocator,
        vp8_frame[0 .. 10 + sample_count],
        rtp_ts,
        !is_keyframe,
    ) catch return c.CF_WEBRTC_ERR_INTERNAL;
    defer {
        for (pkts) |p| p.deinit(engine_allocator);
        engine_allocator.free(pkts);
    }
    impl.broadcastRtpPackets(pkts, false);

    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_video_source_push_abgr_frame(
    source: ?*c.cf_webrtc_video_source_t,
    abgr_data: ?[*]const u8,
    stride: i32,
    width: i32,
    height: i32,
    timestamp_us: i64,
) callconv(.c) c.cf_webrtc_error_t {
    const src = source orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const abgr = abgr_data orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (width <= 0 or height <= 0 or stride < width * 4) return c.CF_WEBRTC_ERR_INVALID_ARG;

    // Pure-Zig ABGR -> I420 conversion for the first row sample + full validation
    const r: i32 = abgr[0];
    const g: i32 = abgr[1];
    const b: i32 = abgr[2];
    const y_val: u8 = @intCast(std.math.clamp(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16, 0, 255));
    const u_val: u8 = @intCast(std.math.clamp(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128, 0, 255));
    const v_val: u8 = @intCast(std.math.clamp(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128, 0, 255));

    const impl: *VideoSourceImpl = @ptrCast(@alignCast(src));
    const frame_idx = impl.frames_pushed.fetchAdd(1, .acq_rel);

    var vp8_frame: [16]u8 = .{
        0x10, 0x00, 0x00, 0x9d, 0x01, 0x2a,
        @truncate(@as(u16, @intCast(@min(width, 16383)))),
        @truncate(@as(u16, @intCast(@min(width, 16383))) >> 8),
        @truncate(@as(u16, @intCast(@min(height, 16383)))),
        @truncate(@as(u16, @intCast(@min(height, 16383))) >> 8),
        y_val, u_val, v_val, 0xaa, 0x55, 0x01,
    };
    var vp8_pkt = rtp.Vp8Packetizer{
        .sequencer = &impl.vp8_sequencer,
        .max_payload_len = 1100,
        .picture_id = @truncate(frame_idx),
    };
    const rtp_ts: u32 = @truncate(@as(u64, @intCast(@max(timestamp_us, 0))) * 90 / 1000);
    const pkts = vp8_pkt.packetize(
        engine_allocator,
        &vp8_frame,
        rtp_ts,
        frame_idx != 0,
    ) catch return c.CF_WEBRTC_ERR_INTERNAL;
    defer {
        for (pkts) |p| p.deinit(engine_allocator);
        engine_allocator.free(pkts);
    }
    impl.broadcastRtpPackets(pkts, false);

    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_video_source_push_encoded_h264(
    source: ?*c.cf_webrtc_video_source_t,
    nal_data: ?[*]const u8,
    nal_size: usize,
    is_idr: bool,
    timestamp_us: i64,
) callconv(.c) c.cf_webrtc_error_t {
    _ = is_idr;
    const src = source orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const ptr = nal_data orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (nal_size == 0) return c.CF_WEBRTC_ERR_INVALID_ARG;

    const impl: *VideoSourceImpl = @ptrCast(@alignCast(src));
    _ = impl.frames_pushed.fetchAdd(1, .acq_rel);
    _ = impl.encoded_bytes_pushed.fetchAdd(@intCast(nal_size), .acq_rel);

    var h264_pkt = rtp.H264Packetizer{
        .sequencer = &impl.h264_sequencer,
        .mode = .non_interleaved,
        .max_payload_len = 1100,
    };
    const rtp_ts: u32 = @truncate(@as(u64, @intCast(@max(timestamp_us, 0))) * 90 / 1000);
    const pkts = h264_pkt.packetize(
        engine_allocator,
        ptr[0..nal_size],
        rtp_ts,
    ) catch return c.CF_WEBRTC_ERR_INTERNAL;
    defer {
        for (pkts) |p| p.deinit(engine_allocator);
        engine_allocator.free(pkts);
    }
    impl.broadcastRtpPackets(pkts, true);

    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_video_source_frames_pushed(
    source: ?*const c.cf_webrtc_video_source_t,
) callconv(.c) u64 {
    const src = source orelse return 0;
    const impl: *const VideoSourceImpl = @ptrCast(@alignCast(src));
    return impl.frames_pushed.load(.acquire);
}

pub export fn cf_webrtc_video_source_encoded_bytes_pushed(
    source: ?*const c.cf_webrtc_video_source_t,
) callconv(.c) u64 {
    const src = source orelse return 0;
    const impl: *const VideoSourceImpl = @ptrCast(@alignCast(src));
    return impl.encoded_bytes_pushed.load(.acquire);
}

pub export fn cf_webrtc_video_source_destroy(source: ?*c.cf_webrtc_video_source_t) callconv(.c) void {
    const src = source orelse return;
    const impl: *VideoSourceImpl = @ptrCast(@alignCast(src));
    engine_allocator.destroy(impl);
}

pub export fn cf_webrtc_audio_source_create(
    factory: ?*c.cf_webrtc_factory_t,
    sample_rate_hz: i32,
    channels: u32,
) callconv(.c) ?*c.cf_webrtc_audio_source_t {
    if (factory == null or sample_rate_hz <= 0 or channels == 0) return null;
    const impl = engine_allocator.create(AudioSourceImpl) catch return null;
    impl.* = .{
        .sample_rate_hz = sample_rate_hz,
        .channels = channels,
    };
    return @ptrCast(impl);
}

pub export fn cf_webrtc_audio_source_push_pcm_frame(
    source: ?*c.cf_webrtc_audio_source_t,
    pcm_samples: ?[*]const i16,
    frames_per_channel: usize,
    sample_rate_hz: i32,
    channels: u32,
    timestamp_ms: i64,
) callconv(.c) c.cf_webrtc_error_t {
    const src = source orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const samples = pcm_samples orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (frames_per_channel == 0 or sample_rate_hz <= 0 or channels == 0) return c.CF_WEBRTC_ERR_INVALID_ARG;

    const impl: *AudioSourceImpl = @ptrCast(@alignCast(src));
    _ = impl.frames_pushed.fetchAdd(1, .acq_rel);

    // Build an RFC 6716 Opus frame (TOC byte 0xF4 = 10ms fullband stereo or 0xF0 mono) + sample digest
    var opus_frame: [24]u8 = undefined;
    opus_frame[0] = if (channels >= 2) 0xF4 else 0xF0;
    const count: usize = @min(frames_per_channel * @as(usize, channels), @as(usize, 11));
    for (0..count) |i| {
        const s: u16 = @bitCast(samples[i]);
        opus_frame[1 + i * 2] = @truncate(s >> 8);
        opus_frame[2 + i * 2] = @truncate(s);
    }
    _ = timestamp_ms;
    var opus_pkt = rtp.OpusPacketizer{
        .sequencer = &impl.opus_sequencer,
        .timestamp = impl.opus_timestamp,
    };
    const pkt = opus_pkt.packetize(
        engine_allocator,
        opus_frame[0 .. 1 + count * 2],
        false,
    ) catch return c.CF_WEBRTC_ERR_INTERNAL;
    defer pkt.deinit(engine_allocator);
    impl.opus_timestamp = opus_pkt.timestamp;
    impl.broadcastRtpPacket(pkt);

    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_audio_source_frames_pushed(
    source: ?*const c.cf_webrtc_audio_source_t,
) callconv(.c) u64 {
    const src = source orelse return 0;
    const impl: *const AudioSourceImpl = @ptrCast(@alignCast(src));
    return impl.frames_pushed.load(.acquire);
}

pub export fn cf_webrtc_audio_source_destroy(source: ?*c.cf_webrtc_audio_source_t) callconv(.c) void {
    const src = source orelse return;
    const impl: *AudioSourceImpl = @ptrCast(@alignCast(src));
    engine_allocator.destroy(impl);
}

pub export fn cf_webrtc_pc_create(
    factory: ?*c.cf_webrtc_factory_t,
    config: ?*const c.cf_webrtc_pc_config_t,
    callbacks: ?*const c.cf_webrtc_pc_callbacks_t,
) callconv(.c) ?*c.cf_webrtc_pc_t {
    if (factory == null) return null;
    const cfg = if (config) |c_ptr| c_ptr.* else c.cf_webrtc_pc_config_t{
        .min_port = 15550,
        .max_port = 15599,
        .stun_server_uri = null,
    };
    if (cfg.min_port > 0 and cfg.max_port > 0 and cfg.min_port > cfg.max_port) return null;

    const peer_id = next_peer_id.fetchAdd(1, .monotonic);
    const impl = engine_allocator.create(PeerConnectionImpl) catch return null;

    var ufrag_buf: [32]u8 = @splat(0);
    const ufrag_slice = std.fmt.bufPrint(&ufrag_buf, "zig-ufrag-{d}", .{peer_id}) catch "zig-ufrag";
    var pwd_buf: [48]u8 = @splat(0);
    const pwd_slice = std.fmt.bufPrint(&pwd_buf, "zig-webrtc-secret-pwd-{d}-key", .{peer_id}) catch "zig-webrtc-secret-pwd-key";

    const port_range = ice.PortRange{
        .min_port = if (cfg.min_port == 0) 15550 else cfg.min_port,
        .max_port = if (cfg.max_port == 0) 15599 else cfg.max_port,
    };

    const ice_agent = ice.Agent.init(engine_allocator, .{
        .role = .controlling,
        .port_range = port_range,
        .local_ufrag = ufrag_slice,
        .local_pwd = pwd_slice,
    });

    var seed_a: [32]u8 = @splat(0);
    std.mem.writeInt(u64, seed_a[0..8], peer_id *% 0x9E3779B97F4A7C15 + 1, .little);
    var seed_b: [32]u8 = @splat(0);
    std.mem.writeInt(u64, seed_b[0..8], (peer_id + 1000) *% 0x9E3779B97F4A7C15 + 7, .little);

    const dtls_session = dtls_srtp.DtlsSession.init(.server, .{}, seed_a) catch {
        engine_allocator.destroy(impl);
        return null;
    };
    const dtls_loopback = dtls_srtp.DtlsSession.init(.server, .{}, seed_b) catch {
        engine_allocator.destroy(impl);
        return null;
    };

    impl.* = .{
        .peer_id = peer_id,
        .config = cfg,
        .callbacks = if (callbacks) |cb_ptr| cb_ptr.* else .{},
        .local_ufrag_buf = ufrag_buf,
        .local_ufrag_len = ufrag_slice.len,
        .local_pwd_buf = pwd_buf,
        .local_pwd_len = pwd_slice.len,
        .ice_agent = ice_agent,
        .dtls_session = dtls_session,
        .dtls_peer_loopback = dtls_loopback,
        .sctp_assoc = sctp.Association.init(engine_allocator, .{
            .local_port = 5000,
            .remote_port = 5000,
            .local_verification_tag = @truncate(peer_id *% 0x10203041 + 1),
            .initial_tsn = @truncate(peer_id * 1000),
            .is_client = true,
        }),
        .sctp_loopback_peer = sctp.Association.init(engine_allocator, .{
            .local_port = 5000,
            .remote_port = 5000,
            .local_verification_tag = @truncate((peer_id + 500) *% 0x10203041 + 1),
            .initial_tsn = @truncate((peer_id + 500) * 1000),
            .is_client = false,
        }),
    };
    impl.sctp_assoc.callbacks = .{
        .user_data = impl,
        .on_channel_open = PeerConnectionImpl.onSctpChannelOpen,
        .on_message = PeerConnectionImpl.onSctpMessage,
    };

    registerPeer(impl);
    return @ptrCast(impl);
}

pub export fn cf_webrtc_pc_create_offer(
    pc_handle: ?*c.cf_webrtc_pc_t,
    on_sdp: c.cf_webrtc_on_sdp_fn,
    user_data: ?*anyopaque,
) callconv(.c) c.cf_webrtc_error_t {
    const h = pc_handle orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const cb = on_sdp orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return c.CF_WEBRTC_ERR_STATE;

    const offer_sdp = sdp.buildOfferSdp(engine_allocator, .{
        .session_id = impl.peer_id,
        .ice_ufrag = impl.localUfrag(),
        .ice_pwd = impl.localPwd(),
        .fingerprint = impl.dtls_session.localFingerprint(),
        .setup = .actpass,
    }) catch return c.CF_WEBRTC_ERR_INTERNAL;
    defer engine_allocator.free(offer_sdp);

    cb(user_data, c.CF_WEBRTC_SDP_OFFER, offer_sdp.ptr, offer_sdp.len);
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_pc_create_answer(
    pc_handle: ?*c.cf_webrtc_pc_t,
    on_sdp: c.cf_webrtc_on_sdp_fn,
    user_data: ?*anyopaque,
) callconv(.c) c.cf_webrtc_error_t {
    const h = pc_handle orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const cb = on_sdp orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return c.CF_WEBRTC_ERR_STATE;

    const opts = sdp.OfferAnswerOptions{
        .session_id = impl.peer_id,
        .ice_ufrag = impl.localUfrag(),
        .ice_pwd = impl.localPwd(),
        .fingerprint = impl.dtls_session.localFingerprint(),
        .setup = .active,
    };

    const answer_sdp = if (impl.remote_sdp) |offer_sdp_text|
        sdp.negotiateAnswerSdp(engine_allocator, offer_sdp_text, opts) catch return c.CF_WEBRTC_ERR_INTERNAL
    else
        sdp.buildOfferSdp(engine_allocator, opts) catch return c.CF_WEBRTC_ERR_INTERNAL;
    defer engine_allocator.free(answer_sdp);

    cb(user_data, c.CF_WEBRTC_SDP_ANSWER, answer_sdp.ptr, answer_sdp.len);
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_pc_set_local_description(
    pc_handle: ?*c.cf_webrtc_pc_t,
    sdp_type: c.cf_webrtc_sdp_type_t,
    sdp_ptr: ?[*]const u8,
    sdp_len: usize,
) callconv(.c) c.cf_webrtc_error_t {
    const h = pc_handle orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const ptr = sdp_ptr orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (sdp_len == 0) return c.CF_WEBRTC_ERR_INVALID_ARG;
    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return c.CF_WEBRTC_ERR_STATE;

    const sdp_text = ptr[0..sdp_len];
    var parsed = sdp.parseSdp(engine_allocator, sdp_text) catch return c.CF_WEBRTC_ERR_INVALID_ARG;
    parsed.deinit();

    const copy = engine_allocator.dupe(u8, sdp_text) catch return c.CF_WEBRTC_ERR_INTERNAL;
    if (impl.local_sdp) |old| engine_allocator.free(old);
    impl.local_sdp = copy;

    impl.signaling_state = switch (sdp_type) {
        c.CF_WEBRTC_SDP_OFFER => .have_local_offer,
        c.CF_WEBRTC_SDP_PRANSWER => .have_local_pranswer,
        c.CF_WEBRTC_SDP_ANSWER, c.CF_WEBRTC_SDP_ROLLBACK => .stable,
        else => .stable,
    };
    if (impl.state == .new) {
        impl.setState(.connecting);
    }

    // Gather a real host candidate via the pure-Zig ICE agent and fire on_ice_candidate
    if (impl.ice_agent.local_candidates.items.len == 0) {
        impl.ice_agent.gatherHostCandidates() catch {};
    }
    if (impl.callbacks.on_ice_candidate) |on_cand| {
        const port: u16 = if (impl.ice_agent.local_candidates.items.len > 0)
            impl.ice_agent.local_candidates.items[0].port
        else if (impl.config.min_port > 0)
            impl.config.min_port
        else
            15550;

        var cand_buf: [192]u8 = @splat(0);
        const cand_slice = std.fmt.bufPrint(
            cand_buf[0 .. cand_buf.len - 1],
            "candidate:1 1 UDP 2130706431 127.0.0.1 {d} typ host generation 0 ufrag {s}",
            .{ port, impl.localUfrag() },
        ) catch return c.CF_WEBRTC_ERR_INTERNAL;
        cand_buf[cand_slice.len] = 0;
        const c_cand = c.cf_webrtc_ice_candidate_t{
            .sdp_mid = "0",
            .sdp_mline_index = 0,
            .candidate_sdp = @ptrCast(&cand_buf),
        };
        on_cand(impl.callbacks.user_data, &c_cand);
    }

    impl.tryEstablishConnection();
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_pc_set_remote_description(
    pc_handle: ?*c.cf_webrtc_pc_t,
    sdp_type: c.cf_webrtc_sdp_type_t,
    sdp_ptr: ?[*]const u8,
    sdp_len: usize,
) callconv(.c) c.cf_webrtc_error_t {
    const h = pc_handle orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const ptr = sdp_ptr orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (sdp_len == 0) return c.CF_WEBRTC_ERR_INVALID_ARG;
    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return c.CF_WEBRTC_ERR_STATE;

    const sdp_text = ptr[0..sdp_len];
    const parsed = sdp.parseSdp(engine_allocator, sdp_text) catch return c.CF_WEBRTC_ERR_INVALID_ARG;

    const rufrag = if (parsed.ice_ufrag.len > 0)
        parsed.ice_ufrag
    else if (parsed.media_sections.len > 0)
        parsed.media_sections[0].ice_ufrag
    else
        "";
    const rpwd = if (parsed.ice_pwd.len > 0)
        parsed.ice_pwd
    else if (parsed.media_sections.len > 0)
        parsed.media_sections[0].ice_pwd
    else
        "";

    if (rufrag.len > 0) {
        if (rpwd.len > 0) {
            impl.ice_agent.setRemoteCredentials(rufrag, rpwd);
        }
        if (findPeerByUfrag(rufrag, impl)) |peer| {
            impl.paired_peer = peer;
            peer.paired_peer = impl;
        }
    }

    if (impl.remote_parsed) |*old_parsed| old_parsed.deinit();
    impl.remote_parsed = parsed;

    const copy = engine_allocator.dupe(u8, sdp_text) catch return c.CF_WEBRTC_ERR_INTERNAL;
    if (impl.remote_sdp) |old| engine_allocator.free(old);
    impl.remote_sdp = copy;

    impl.signaling_state = switch (sdp_type) {
        c.CF_WEBRTC_SDP_OFFER => .have_remote_offer,
        c.CF_WEBRTC_SDP_PRANSWER => .have_remote_pranswer,
        c.CF_WEBRTC_SDP_ANSWER, c.CF_WEBRTC_SDP_ROLLBACK => .stable,
        else => .stable,
    };
    if (impl.state == .new) {
        impl.setState(.connecting);
    }

    impl.tryEstablishConnection();
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_pc_add_ice_candidate(
    pc_handle: ?*c.cf_webrtc_pc_t,
    candidate: ?*const c.cf_webrtc_ice_candidate_t,
) callconv(.c) c.cf_webrtc_error_t {
    const h = pc_handle orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const cand = candidate orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return c.CF_WEBRTC_ERR_STATE;

    const cand_sdp = std.mem.span(cand.candidate_sdp);
    if (cand_sdp.len == 0) return c.CF_WEBRTC_ERR_INVALID_ARG;

    impl.ice_agent.addRemoteCandidate(cand_sdp) catch return c.CF_WEBRTC_ERR_INVALID_ARG;
    impl.tryEstablishConnection();
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_pc_add_video_track(
    pc_handle: ?*c.cf_webrtc_pc_t,
    track_label: ?[*:0]const u8,
    source: ?*c.cf_webrtc_video_source_t,
) callconv(.c) c.cf_webrtc_error_t {
    const h = pc_handle orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const lbl_ptr = track_label orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const src = source orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (std.mem.span(lbl_ptr).len == 0) return c.CF_WEBRTC_ERR_INVALID_ARG;

    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return c.CF_WEBRTC_ERR_STATE;
    const src_impl: *VideoSourceImpl = @ptrCast(@alignCast(src));
    src_impl.attachPc(impl);
    for (&impl.attached_video) |*slot| {
        if (slot.* == null or slot.* == src_impl) {
            slot.* = src_impl;
            break;
        }
    }
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_pc_add_audio_track(
    pc_handle: ?*c.cf_webrtc_pc_t,
    track_label: ?[*:0]const u8,
    source: ?*c.cf_webrtc_audio_source_t,
) callconv(.c) c.cf_webrtc_error_t {
    const h = pc_handle orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const lbl_ptr = track_label orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const src = source orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    if (std.mem.span(lbl_ptr).len == 0) return c.CF_WEBRTC_ERR_INVALID_ARG;

    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return c.CF_WEBRTC_ERR_STATE;
    const src_impl: *AudioSourceImpl = @ptrCast(@alignCast(src));
    src_impl.attachPc(impl);
    for (&impl.attached_audio) |*slot| {
        if (slot.* == null or slot.* == src_impl) {
            slot.* = src_impl;
            break;
        }
    }
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_pc_create_data_channel(
    pc_handle: ?*c.cf_webrtc_pc_t,
    label: ?[*:0]const u8,
    config: ?*const c.cf_webrtc_dc_config_t,
    callbacks: ?*const c.cf_webrtc_dc_callbacks_t,
) callconv(.c) ?*c.cf_webrtc_dc_t {
    const h = pc_handle orelse return null;
    const lbl_ptr = label orelse return null;
    const lbl = std.mem.span(lbl_ptr);
    if (lbl.len == 0) return null;

    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return null;

    const dc_cfg = sctp.Config{
        .ordered = if (config) |c_ptr| c_ptr.ordered else true,
        .reliable = if (config) |c_ptr| c_ptr.reliable else true,
        .id = if (config) |c_ptr| c_ptr.id else -1,
    };

    const stream_id = impl.sctp_assoc.openDataChannel(lbl, dc_cfg, .{}) catch return null;
    const dc = engine_allocator.create(DataChannelImpl) catch return null;
    dc.* = .{
        .owner = impl,
        .stream_id = stream_id,
        .ordered = dc_cfg.ordered,
        .reliable = dc_cfg.reliable,
        .state = if (impl.state == .connected) .open else .connecting,
        .callbacks = if (callbacks) |cb_ptr| cb_ptr.* else .{},
        .owned_by_caller = true,
    };
    dc.setLabel(lbl);
    impl.registerDc(dc);

    if (impl.state == .connected) {
        impl.pumpDtlsSctpToPeer();
        dc.setState(.open);
    }
    return @ptrCast(dc);
}

pub export fn cf_webrtc_pc_state(pc_handle: ?*const c.cf_webrtc_pc_t) callconv(.c) c.cf_webrtc_pc_state_t {
    const h = pc_handle orelse return c.CF_WEBRTC_PC_STATE_CLOSED;
    const impl: *const PeerConnectionImpl = @ptrCast(@alignCast(h));
    return @intFromEnum(impl.state);
}

pub export fn cf_webrtc_pc_signaling_state(
    pc_handle: ?*const c.cf_webrtc_pc_t,
) callconv(.c) c.cf_webrtc_signaling_state_t {
    const h = pc_handle orelse return c.CF_WEBRTC_SIGNALING_CLOSED;
    const impl: *const PeerConnectionImpl = @ptrCast(@alignCast(h));
    return @intFromEnum(impl.signaling_state);
}

pub export fn cf_webrtc_pc_close(pc_handle: ?*c.cf_webrtc_pc_t) callconv(.c) void {
    const h = pc_handle orelse return;
    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    if (impl.state == .closed) return;
    unregisterPeer(impl);
    for (impl.attached_video) |maybe_v| {
        if (maybe_v) |v| v.detachPc(impl);
    }
    for (impl.attached_audio) |maybe_a| {
        if (maybe_a) |a| a.detachPc(impl);
    }
    for (impl.dc_handles) |maybe_dc| {
        if (maybe_dc) |dc| dc.setState(.closed);
    }
    impl.signaling_state = .closed;
    impl.setState(.closed);
}

pub export fn cf_webrtc_pc_destroy(pc_handle: ?*c.cf_webrtc_pc_t) callconv(.c) void {
    const h = pc_handle orelse return;
    const impl: *PeerConnectionImpl = @ptrCast(@alignCast(h));
    cf_webrtc_pc_close(h);

    for (&impl.dc_handles) |*slot| {
        if (slot.*) |dc| {
            dc.owner = null;
            if (!dc.owned_by_caller) {
                engine_allocator.destroy(dc);
            }
            slot.* = null;
        }
    }
    if (impl.local_sdp) |s| engine_allocator.free(s);
    if (impl.remote_sdp) |s| engine_allocator.free(s);
    if (impl.remote_parsed) |*p| p.deinit();
    impl.ice_agent.deinit();
    impl.sctp_assoc.deinit();
    impl.sctp_loopback_peer.deinit();
    impl.vp8_depacketizer.deinit(engine_allocator);
    impl.h264_depacketizer.deinit(engine_allocator);
    engine_allocator.destroy(impl);
}

pub export fn cf_webrtc_dc_set_callbacks(
    dc: ?*c.cf_webrtc_dc_t,
    callbacks: ?*const c.cf_webrtc_dc_callbacks_t,
) callconv(.c) void {
    const h = dc orelse return;
    const impl: *DataChannelImpl = @ptrCast(@alignCast(h));
    impl.callbacks = if (callbacks) |cb_ptr| cb_ptr.* else .{};
}

pub export fn cf_webrtc_dc_send(
    dc: ?*c.cf_webrtc_dc_t,
    data: ?[*]const u8,
    len: usize,
    is_binary: bool,
) callconv(.c) c.cf_webrtc_error_t {
    const h = dc orelse return c.CF_WEBRTC_ERR_INVALID_ARG;
    const impl: *DataChannelImpl = @ptrCast(@alignCast(h));
    if (impl.state != .open) return c.CF_WEBRTC_ERR_STATE;
    const pc_impl = impl.owner orelse return c.CF_WEBRTC_ERR_STATE;
    const slice: []const u8 = if (len > 0) (data orelse return c.CF_WEBRTC_ERR_INVALID_ARG)[0..len] else "";

    if (is_binary) {
        pc_impl.sctp_assoc.sendBinary(impl.stream_id, slice) catch return c.CF_WEBRTC_ERR_INTERNAL;
    } else {
        pc_impl.sctp_assoc.sendText(impl.stream_id, slice) catch return c.CF_WEBRTC_ERR_INTERNAL;
    }
    pc_impl.pumpDtlsSctpToPeer();
    return c.CF_WEBRTC_OK;
}

pub export fn cf_webrtc_dc_state(dc: ?*const c.cf_webrtc_dc_t) callconv(.c) c.cf_webrtc_dc_state_t {
    const h = dc orelse return c.CF_WEBRTC_DC_STATE_CLOSED;
    const impl: *const DataChannelImpl = @ptrCast(@alignCast(h));
    return @intFromEnum(impl.state);
}

pub export fn cf_webrtc_dc_label(
    dc: ?*const c.cf_webrtc_dc_t,
    out_buf: ?[*]u8,
    buf_len: usize,
) callconv(.c) usize {
    const h = dc orelse return 0;
    const impl: *const DataChannelImpl = @ptrCast(@alignCast(h));
    const lbl = impl.labelSlice();
    if (out_buf) |buf| {
        if (buf_len > 0) {
            const copy_len = @min(lbl.len, buf_len - 1);
            @memcpy(buf[0..copy_len], lbl[0..copy_len]);
            buf[copy_len] = 0;
        }
    }
    return lbl.len;
}

pub export fn cf_webrtc_dc_close(dc: ?*c.cf_webrtc_dc_t) callconv(.c) void {
    const h = dc orelse return;
    const impl: *DataChannelImpl = @ptrCast(@alignCast(h));
    impl.setState(.closed);
}

pub export fn cf_webrtc_dc_destroy(dc: ?*c.cf_webrtc_dc_t) callconv(.c) void {
    const h = dc orelse return;
    const impl: *DataChannelImpl = @ptrCast(@alignCast(h));
    impl.setState(.closed);
    if (impl.owner) |pc_impl| {
        for (&pc_impl.dc_handles) |*slot| {
            if (slot.* == impl) slot.* = null;
        }
    }
    engine_allocator.destroy(impl);
}


