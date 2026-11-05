const std = @import("std");
pub const sources = @import("slim_sources.zig");
pub const c_types = @import("c_types.zig");
pub const c = c_types.c;
pub const Error = c_types.Error;
pub const mapCError = c_types.mapCError;

pub const sdp = @import("sdp.zig");
pub const ice = @import("ice.zig");
pub const dtls_srtp = @import("dtls_srtp.zig");
pub const sctp = @import("sctp.zig");
pub const rtp = @import("rtp.zig");
pub const pc = @import("pc.zig");

pub const SubsystemBackendKind = pc.SubsystemBackendKind;
pub const SubsystemMode = pc.SubsystemMode;
pub const ParseSdpHookFn = pc.ParseSdpHookFn;
pub const ValidateIceCandidateHookFn = pc.ValidateIceCandidateHookFn;
pub const Backend = pc.Backend;
pub const BackendConfig = pc.BackendConfig;
pub const default_backend = pc.default_backend;

pub const codecs = pc.codecs;
pub const media = pc.media;
pub const Factory = pc.Factory;
pub const PeerConnection = pc.PeerConnection;
pub const VideoSource = media.VideoSource;
pub const AudioSource = media.AudioSource;
pub const DataChannel = sctp.DataChannel;

comptime {
    _ = sdp;
    _ = ice;
    _ = dtls_srtp;
    _ = sctp;
    _ = rtp;
    _ = pc;
    _ = &pc.cf_webrtc_factory_create;
    _ = &pc.cf_webrtc_factory_destroy;
    _ = &pc.cf_webrtc_factory_supported_video_codecs;
    _ = &pc.cf_webrtc_factory_supported_audio_codecs;
    _ = &pc.cf_webrtc_video_source_create;
    _ = &pc.cf_webrtc_video_source_push_i420_frame;
    _ = &pc.cf_webrtc_video_source_push_abgr_frame;
    _ = &pc.cf_webrtc_video_source_push_encoded_h264;
    _ = &pc.cf_webrtc_video_source_frames_pushed;
    _ = &pc.cf_webrtc_video_source_encoded_bytes_pushed;
    _ = &pc.cf_webrtc_video_source_destroy;
    _ = &pc.cf_webrtc_audio_source_create;
    _ = &pc.cf_webrtc_audio_source_push_pcm_frame;
    _ = &pc.cf_webrtc_audio_source_frames_pushed;
    _ = &pc.cf_webrtc_audio_source_destroy;
    _ = &pc.cf_webrtc_pc_create;
    _ = &pc.cf_webrtc_pc_add_video_track;
    _ = &pc.cf_webrtc_pc_add_audio_track;
    _ = &pc.cf_webrtc_pc_create_offer;
    _ = &pc.cf_webrtc_pc_create_answer;
    _ = &pc.cf_webrtc_pc_set_local_description;
    _ = &pc.cf_webrtc_pc_set_remote_description;
    _ = &pc.cf_webrtc_pc_add_ice_candidate;
    _ = &pc.cf_webrtc_pc_create_data_channel;
    _ = &pc.cf_webrtc_dc_set_callbacks;
    _ = &pc.cf_webrtc_dc_send;
    _ = &pc.cf_webrtc_dc_state;
    _ = &pc.cf_webrtc_dc_label;
    _ = &pc.cf_webrtc_dc_close;
    _ = &pc.cf_webrtc_dc_destroy;
    _ = &pc.cf_webrtc_pc_state;
    _ = &pc.cf_webrtc_pc_signaling_state;
    _ = &pc.cf_webrtc_pc_close;
    _ = &pc.cf_webrtc_pc_destroy;
}

test {
    _ = sdp;
    _ = ice;
    _ = dtls_srtp;
    _ = sctp;
    _ = rtp;
    _ = pc;
}

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
    try std.testing.expect(pc_a.dtlsHandshakeComplete());
    try std.testing.expect(pc_b.dtlsHandshakeComplete());
    try std.testing.expectEqual(SubsystemBackendKind.pure_zig, default_backend.sdp_backend);
    try std.testing.expectEqual(SubsystemBackendKind.pure_zig, default_backend.ice_backend);
    try std.testing.expectEqual(SubsystemBackendKind.pure_zig, default_backend.dtls_srtp_backend);
    try std.testing.expectEqual(SubsystemBackendKind.pure_zig, default_backend.sctp_backend);
    try std.testing.expectEqual(SubsystemBackendKind.pure_zig, default_backend.rtp_backend);
    try std.testing.expectEqual(SubsystemBackendKind.pure_zig, default_backend.codecs_backend);
    try std.testing.expectEqual(SubsystemBackendKind.pure_zig, default_backend.pc_backend);
    try std.testing.expectEqual(sctp.State.open, dc_a.state());
    try std.testing.expect(ctx_b.remote_dc != null);

    // Send text message from Peer A -> Peer B over DTLS-SCTP
    try dc_a.sendText("touch:down:120,240");
    try std.testing.expectEqual(@as(usize, 1), ctx_b.rx_count);
    try std.testing.expect(!ctx_b.rx_binary);
    try std.testing.expectEqualStrings("touch:down:120,240", ctx_b.rx_buf[0..ctx_b.rx_len]);

    // Send binary message from Peer B -> Peer A over DTLS-SCTP
    var remote_dc = ctx_b.remote_dc.?;
    const bin_payload = [_]u8{ 0xCA, 0xFE, 0xBA, 0xBE, 0x01, 0x02 };
    try remote_dc.sendBinary(&bin_payload);
    try std.testing.expectEqual(@as(usize, 1), ctx_a.rx_count);
    try std.testing.expect(ctx_a.rx_binary);
    try std.testing.expectEqualSlices(u8, &bin_payload, ctx_a.rx_buf[0..ctx_a.rx_len]);

    // Push media frames on Peer A's tracks and verify RTP packetization + SRTP delivery to Peer B
    const h264_idr = [_]u8{
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xe0, 0x1f,
        0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80,
        0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x21,
    };
    try video_src.pushEncodedH264(&h264_idr, true, 66_666);
    try std.testing.expect(video_src.rtpPacketsGenerated() >= 2);

    const pcm: [480 * 2]i16 = @splat(256);
    try audio_src.pushPcmFrame(&pcm, 480, 48000, 2, 20);
    try std.testing.expect(audio_src.rtpPacketsGenerated() >= 1);

    try std.testing.expect(pc_a.srtpPacketsSent() >= 3);
    try std.testing.expect(pc_b.srtpPacketsReceived() >= 3);
}
