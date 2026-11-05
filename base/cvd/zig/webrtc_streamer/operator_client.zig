const std = @import("std");
const builtin = @import("builtin");
const cli_mod = @import("cli.zig");
const input_sink_mod = @import("input_sink.zig");

const is_linux = builtin.os.tag == .linux;
const linux = if (is_linux) std.os.linux else struct {};
const c = if (!is_linux) std.c else struct {};
const Allocator = std.mem.Allocator;

pub const STUN_MAGIC_COOKIE: u32 = 0x2112A442;
pub const STUN_BINDING_REQUEST: u16 = 0x0001;
pub const STUN_BINDING_RESPONSE: u16 = 0x0101;
pub const STUN_ATTR_USERNAME: u16 = 0x0006;
pub const STUN_ATTR_MESSAGE_INTEGRITY: u16 = 0x0008;
pub const STUN_ATTR_XOR_MAPPED_ADDRESS: u16 = 0x0020;
pub const STUN_ATTR_FINGERPRINT: u16 = 0x8028;
pub const STUN_FINGERPRINT_XOR: u32 = 0x5354554e;

pub const DEFAULT_LOCAL_UFRAG = "cfzig001";
pub const DEFAULT_LOCAL_PWD = "cfzigpassword000000000001";
pub const DEFAULT_LOCAL_FINGERPRINT =
    "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";

pub const FramingMode = enum {
    auto,
    seqpacket,
    stream,
    raw_json,
};

pub const RemoteSdpInfo = struct {
    ufrag_buf: [64]u8 = @splat(0),
    ufrag_len: usize = 0,
    pwd_buf: [128]u8 = @splat(0),
    pwd_len: usize = 0,
    fingerprint_buf: [160]u8 = @splat(0),
    fingerprint_len: usize = 0,
    has_video: bool = true,
    has_application: bool = true,

    pub fn ufrag(self: *const RemoteSdpInfo) []const u8 {
        return self.ufrag_buf[0..self.ufrag_len];
    }

    pub fn pwd(self: *const RemoteSdpInfo) []const u8 {
        return self.pwd_buf[0..self.pwd_len];
    }

    pub fn fingerprint(self: *const RemoteSdpInfo) []const u8 {
        return self.fingerprint_buf[0..self.fingerprint_len];
    }
};

pub const ClientSession = struct {
    client_id: u32,
    connected: bool = true,
    offer_received: bool = false,
    offer_sent: bool = false,
    answer_sent: bool = false,
    answer_received: bool = false,
    remote_sdp: RemoteSdpInfo = .{},
    remote_candidate_ip_buf: [64]u8 = @splat(0),
    remote_candidate_ip_len: usize = 0,
    remote_candidate_port: u16 = 0,

    pub fn remoteCandidateIp(self: *const ClientSession) []const u8 {
        return self.remote_candidate_ip_buf[0..self.remote_candidate_ip_len];
    }
};

pub fn pollReadable(fd: i32, timeout_ms: i32) bool {
    if (fd < 0) return false;
    if (is_linux) {
        var pfd = [1]linux.pollfd{.{
            .fd = fd,
            .events = linux.POLL.IN,
            .revents = 0,
        }};
        const rc = linux.poll(&pfd, 1, timeout_ms);
        if (linux.errno(rc) != .SUCCESS or rc == 0) return false;
        return (pfd[0].revents & (linux.POLL.IN | linux.POLL.HUP | linux.POLL.ERR)) != 0;
    } else {
        var pfd = [1]c.pollfd{.{
            .fd = fd,
            .events = c.POLL.IN,
            .revents = 0,
        }};
        const rc = c.poll(&pfd, 1, timeout_ms);
        if (rc <= 0) return false;
        return (pfd[0].revents & (c.POLL.IN | c.POLL.HUP | c.POLL.ERR)) != 0;
    }
}

pub fn closeFd(fd: i32) void {
    if (fd < 0) return;
    if (is_linux) {
        _ = linux.close(fd);
    } else {
        _ = c.close(fd);
    }
}

pub fn sleepMs(ms: u64) void {
    const sec: isize = @intCast(ms / 1000);
    const nsec: isize = @intCast((ms % 1000) * std.time.ns_per_ms);
    if (is_linux) {
        var req: linux.timespec = .{ .sec = sec, .nsec = nsec };
        _ = linux.nanosleep(&req, null);
    } else {
        var req: c.timespec = .{ .sec = sec, .nsec = nsec };
        _ = c.nanosleep(&req, null);
    }
}

pub fn parseIpv4BigEndian(ip_str: []const u8) !u32 {
    var parts = std.mem.splitScalar(u8, ip_str, '.');
    var octets: [4]u8 = undefined;
    var idx: usize = 0;
    while (parts.next()) |part| : (idx += 1) {
        if (idx >= 4) return error.InvalidIpAddress;
        octets[idx] = std.fmt.parseInt(u8, part, 10) catch return error.InvalidIpAddress;
    }
    if (idx != 4) return error.InvalidIpAddress;
    return std.mem.readInt(u32, &octets, .big);
}

pub fn parseRemoteSdp(raw_sdp: []const u8) RemoteSdpInfo {
    var info = RemoteSdpInfo{};
    var lines = std.mem.splitScalar(u8, raw_sdp, '\n');
    while (lines.next()) |raw_line| {
        const line = std.mem.trimEnd(u8, raw_line, "\r ");
        if (std.mem.startsWith(u8, line, "a=ice-ufrag:")) {
            const v = line["a=ice-ufrag:".len..];
            const n = @min(v.len, info.ufrag_buf.len);
            @memcpy(info.ufrag_buf[0..n], v[0..n]);
            info.ufrag_len = n;
        } else if (std.mem.startsWith(u8, line, "a=ice-pwd:")) {
            const v = line["a=ice-pwd:".len..];
            const n = @min(v.len, info.pwd_buf.len);
            @memcpy(info.pwd_buf[0..n], v[0..n]);
            info.pwd_len = n;
        } else if (std.mem.startsWith(u8, line, "a=fingerprint:")) {
            const v = line["a=fingerprint:".len..];
            const n = @min(v.len, info.fingerprint_buf.len);
            @memcpy(info.fingerprint_buf[0..n], v[0..n]);
            info.fingerprint_len = n;
        } else if (std.mem.startsWith(u8, line, "m=video ")) {
            info.has_video = true;
        } else if (std.mem.startsWith(u8, line, "m=application ")) {
            info.has_application = true;
        }
    }
    return info;
}

pub const ParsedIceCandidate = struct {
    address: []const u8,
    port: u16,
};

/// Parses `candidate:<foundation> <component> <protocol> <priority> <ip> <port> typ <type>...`
pub fn parseIceCandidateLine(raw_cand: []const u8) ?ParsedIceCandidate {
    var s = std.mem.trim(u8, raw_cand, " \t\r\n");
    if (std.mem.startsWith(u8, s, "a=")) s = s[2..];
    if (std.mem.startsWith(u8, s, "candidate:")) s = s["candidate:".len..];

    var tok = std.mem.tokenizeAny(u8, s, " \t");
    _ = tok.next() orelse return null; // foundation
    _ = tok.next() orelse return null; // component
    _ = tok.next() orelse return null; // protocol
    _ = tok.next() orelse return null; // priority
    const ip = tok.next() orelse return null;
    const port_str = tok.next() orelse return null;
    const port = std.fmt.parseInt(u16, port_str, 10) catch return null;
    return .{ .address = ip, .port = port };
}

pub fn formatRegisterMessage(allocator: Allocator, cfg: cli_mod.CliConfig) ![]u8 {
    return std.fmt.allocPrint(
        allocator,
        "{{\"type\":\"register\",\"message_type\":\"register\",\"device_id\":\"{s}\",\"port\":{d},\"device_port\":{d}," ++
            "\"info\":{{\"displays\":[{{\"id\":\"0\",\"width\":{d},\"height\":{d},\"x_res\":{d},\"y_res\":{d},\"dpi\":{d},\"fps\":{d}}}],\"touchpads\":[],\"audio_streams\":[],\"custom_control_panel_buttons\":[],\"mouse_enabled\":true,\"gamepad_enabled\":false,\"adb_port\":6520,\"hardware\":{{\"cpus\":4,\"memory_mb\":4096,\"camera_passthrough\":false}},\"memory_mb\":4096,\"cpus\":4}}," ++
            "\"device_info\":{{\"displays\":[{{\"id\":\"0\",\"width\":{d},\"height\":{d},\"x_res\":{d},\"y_res\":{d},\"dpi\":{d},\"fps\":{d}}}],\"touchpads\":[],\"audio_streams\":[],\"custom_control_panel_buttons\":[],\"mouse_enabled\":true,\"gamepad_enabled\":false,\"adb_port\":6520,\"hardware\":{{\"cpus\":4,\"memory_mb\":4096,\"camera_passthrough\":false}},\"memory_mb\":4096,\"cpus\":4}}}}",
        .{
            cfg.device_id,
            cfg.operator_http_port,
            cfg.operator_http_port,
            cfg.display0.width,
            cfg.display0.height,
            cfg.display0.width,
            cfg.display0.height,
            cfg.display0.dpi,
            cfg.display0.fps,
            cfg.display0.width,
            cfg.display0.height,
            cfg.display0.width,
            cfg.display0.height,
            cfg.display0.dpi,
            cfg.display0.fps,
        },
    );
}

pub fn buildSdpOffer(
    allocator: Allocator,
    ice_host_address: []const u8,
    ice_host_ipv6: ?[]const u8,
    bound_udp_port: u16,
    local_ufrag: []const u8,
    local_pwd: []const u8,
    local_fingerprint: []const u8,
) ![]u8 {
    return buildSdpWithSetup(
        allocator,
        ice_host_address,
        ice_host_ipv6,
        bound_udp_port,
        local_ufrag,
        local_pwd,
        local_fingerprint,
        "actpass",
    );
}

pub fn buildSdpAnswer(
    allocator: Allocator,
    ice_host_address: []const u8,
    ice_host_ipv6: ?[]const u8,
    bound_udp_port: u16,
    local_ufrag: []const u8,
    local_pwd: []const u8,
    local_fingerprint: []const u8,
) ![]u8 {
    return buildSdpWithSetup(
        allocator,
        ice_host_address,
        ice_host_ipv6,
        bound_udp_port,
        local_ufrag,
        local_pwd,
        local_fingerprint,
        "passive",
    );
}

fn buildSdpWithSetup(
    allocator: Allocator,
    ice_host_address: []const u8,
    ice_host_ipv6: ?[]const u8,
    bound_udp_port: u16,
    local_ufrag: []const u8,
    local_pwd: []const u8,
    local_fingerprint: []const u8,
    setup_role: []const u8,
) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(allocator);

    const cand_v4 = try std.fmt.allocPrint(
        allocator,
        "a=candidate:1 1 udp 2130706431 {s} {d} typ host\r\n",
        .{ ice_host_address, bound_udp_port },
    );
    defer allocator.free(cand_v4);

    const cand_v6 = if (ice_host_ipv6) |v6|
        try std.fmt.allocPrint(
            allocator,
            "a=candidate:2 1 udp 2130706430 {s} {d} typ host\r\n",
            .{ v6, bound_udp_port },
        )
    else
        try allocator.dupe(u8, "");
    defer allocator.free(cand_v6);

    const sdp_str = try std.fmt.allocPrint(
        allocator,
        "v=0\r\n" ++
            "o=- 4611731400430051336 2 IN IP4 {s}\r\n" ++
            "s=-\r\n" ++
            "t=0 0\r\n" ++
            "a=group:BUNDLE 0 1\r\n" ++
            "a=msid-semantic: WMS display_0\r\n" ++
            "m=video {d} UDP/TLS/RTP/SAVPF 96 102\r\n" ++
            "c=IN IP4 {s}\r\n" ++
            "a=rtcp:9 IN IP4 0.0.0.0\r\n" ++
            "a=ice-ufrag:{s}\r\n" ++
            "a=ice-pwd:{s}\r\n" ++
            "a=ice-options:trickle\r\n" ++
            "a=fingerprint:sha-256 {s}\r\n" ++
            "a=setup:{s}\r\n" ++
            "a=mid:0\r\n" ++
            "a=sendonly\r\n" ++
            "a=msid:display_0 display_0_track\r\n" ++
            "a=rtcp-mux\r\n" ++
            "a=rtcp-rsize\r\n" ++
            "a=rtpmap:96 VP8/90000\r\n" ++
            "a=rtpmap:102 H264/90000\r\n" ++
            "a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n" ++
            "a=ssrc:1001 cname:cuttlefish-video\r\n" ++
            "a=ssrc:1001 msid:display_0 display_0_track\r\n" ++
            "{s}{s}" ++
            "m=application {d} UDP/DTLS/SCTP webrtc-datachannel\r\n" ++
            "c=IN IP4 {s}\r\n" ++
            "a=ice-ufrag:{s}\r\n" ++
            "a=ice-pwd:{s}\r\n" ++
            "a=ice-options:trickle\r\n" ++
            "a=fingerprint:sha-256 {s}\r\n" ++
            "a=setup:{s}\r\n" ++
            "a=mid:1\r\n" ++
            "a=sctp-port:5000\r\n" ++
            "a=max-message-size:262144\r\n" ++
            "{s}{s}",
        .{
            ice_host_address,
            bound_udp_port,
            ice_host_address,
            local_ufrag,
            local_pwd,
            local_fingerprint,
            setup_role,
            cand_v4,
            cand_v6,
            bound_udp_port,
            ice_host_address,
            local_ufrag,
            local_pwd,
            local_fingerprint,
            setup_role,
            cand_v4,
            cand_v6,
        },
    );
    defer allocator.free(sdp_str);

    try out.appendSlice(allocator, sdp_str);
    return out.toOwnedSlice(allocator);
}

pub fn formatForwardOfferMessage(allocator: Allocator, client_id: u32, offer_sdp: []const u8) ![]u8 {
    var escaped: std.Io.Writer.Allocating = .init(allocator);
    defer escaped.deinit();
    try std.json.Stringify.value(offer_sdp, .{}, &escaped.writer);
    return std.fmt.allocPrint(
        allocator,
        "{{\"type\":\"forward\",\"message_type\":\"forward\",\"client_id\":{d},\"payload\":{{\"type\":\"offer\",\"sdp\":{s}}}}}",
        .{ client_id, escaped.written() },
    );
}

pub fn formatForwardAnswerMessage(allocator: Allocator, client_id: u32, answer_sdp: []const u8) ![]u8 {
    var escaped: std.Io.Writer.Allocating = .init(allocator);
    defer escaped.deinit();
    try std.json.Stringify.value(answer_sdp, .{}, &escaped.writer);
    return std.fmt.allocPrint(
        allocator,
        "{{\"type\":\"forward\",\"message_type\":\"forward\",\"client_id\":{d},\"payload\":{{\"type\":\"answer\",\"sdp\":{s}}}}}",
        .{ client_id, escaped.written() },
    );
}

pub fn formatForwardCandidateMessage(
    allocator: Allocator,
    client_id: u32,
    ice_host_address: []const u8,
    bound_udp_port: u16,
) ![]u8 {
    return std.fmt.allocPrint(
        allocator,
        "{{\"type\":\"forward\",\"message_type\":\"forward\",\"client_id\":{d},\"payload\":{{\"type\":\"ice-candidate\",\"candidate\":\"candidate:1 1 udp 2130706431 {s} {d} typ host\",\"mid\":\"0\",\"mLineIndex\":0}}}}",
        .{ client_id, ice_host_address, bound_udp_port },
    );
}

/// Checks if a UDP payload is an RFC 8489 STUN Binding Request.
pub fn isStunBindingRequest(pkt: []const u8) bool {
    if (pkt.len < 20) return false;
    const msg_type = std.mem.readInt(u16, pkt[0..2], .big);
    const cookie = std.mem.readInt(u32, pkt[4..8], .big);
    return msg_type == STUN_BINDING_REQUEST and cookie == STUN_MAGIC_COOKIE;
}

/// Builds an RFC 8489 STUN Binding Success Response with `XOR-MAPPED-ADDRESS`,
/// `MESSAGE-INTEGRITY` (HMAC-SHA1), and `FINGERPRINT` (CRC-32 XOR `0x5354554e`).
pub fn buildStunBindingResponse(
    request_pkt: []const u8,
    integrity_key: []const u8,
    mapped_ipv4_host_order: u32,
    mapped_port: u16,
    out: []u8,
) ![]const u8 {
    if (!isStunBindingRequest(request_pkt)) return error.InvalidStunRequest;
    if (out.len < 64) return error.BufferTooSmall;

    std.mem.writeInt(u16, out[0..2], STUN_BINDING_RESPONSE, .big);
    std.mem.writeInt(u16, out[2..4], 0, .big);
    std.mem.writeInt(u32, out[4..8], STUN_MAGIC_COOKIE, .big);
    @memcpy(out[8..20], request_pkt[8..20]);
    var pos: usize = 20;

    // XOR-MAPPED-ADDRESS (12 bytes)
    std.mem.writeInt(u16, out[pos..][0..2], STUN_ATTR_XOR_MAPPED_ADDRESS, .big);
    std.mem.writeInt(u16, out[pos + 2 ..][0..2], 8, .big);
    out[pos + 4] = 0;
    out[pos + 5] = 0x01; // IPv4
    const xport: u16 = mapped_port ^ @as(u16, @truncate(STUN_MAGIC_COOKIE >> 16));
    const xaddr: u32 = mapped_ipv4_host_order ^ STUN_MAGIC_COOKIE;
    std.mem.writeInt(u16, out[pos + 6 ..][0..2], xport, .big);
    std.mem.writeInt(u32, out[pos + 8 ..][0..4], xaddr, .big);
    pos += 12;

    // MESSAGE-INTEGRITY (24 bytes): length field includes MESSAGE-INTEGRITY (24 bytes)
    const len_with_mi: u16 = @intCast((pos - 20) + 24);
    std.mem.writeInt(u16, out[2..4], len_with_mi, .big);
    var mac: [20]u8 = undefined;
    std.crypto.auth.hmac.HmacSha1.create(&mac, out[0..pos], integrity_key);
    std.mem.writeInt(u16, out[pos..][0..2], STUN_ATTR_MESSAGE_INTEGRITY, .big);
    std.mem.writeInt(u16, out[pos + 2 ..][0..2], 20, .big);
    @memcpy(out[pos + 4 ..][0..20], &mac);
    pos += 24;

    // FINGERPRINT (8 bytes): length field includes FINGERPRINT (8 bytes)
    const len_with_fp: u16 = @intCast((pos - 20) + 8);
    std.mem.writeInt(u16, out[2..4], len_with_fp, .big);
    const crc = std.hash.Crc32.hash(out[0..pos]) ^ STUN_FINGERPRINT_XOR;
    std.mem.writeInt(u16, out[pos..][0..2], STUN_ATTR_FINGERPRINT, .big);
    std.mem.writeInt(u16, out[pos + 2 ..][0..2], 4, .big);
    std.mem.writeInt(u32, out[pos + 4 ..][0..4], crc, .big);
    pos += 8;

    return out[0..pos];
}

/// Builds a minimal RFC 8489 STUN Binding Request for testing.
pub fn buildStunBindingRequest(tx_id: [12]u8, out: *[20]u8) []const u8 {
    std.mem.writeInt(u16, out[0..2], STUN_BINDING_REQUEST, .big);
    std.mem.writeInt(u16, out[2..4], 0, .big);
    std.mem.writeInt(u32, out[4..8], STUN_MAGIC_COOKIE, .big);
    @memcpy(out[8..20], &tx_id);
    return out[0..20];
}

pub fn bindUdpInRange(min_port: u16, max_port: u16) !struct { fd: i32, port: u16 } {
    if (is_linux) {
        const rc = linux.socket(linux.AF.INET, linux.SOCK.DGRAM | linux.SOCK.NONBLOCK | linux.SOCK.CLOEXEC, 0);
        if (linux.errno(rc) != .SUCCESS) return error.UdpSocketFailed;
        const fd: i32 = @intCast(rc);
        errdefer _ = linux.close(fd);

        var p: u32 = min_port;
        while (p <= max_port) : (p += 1) {
            var addr: linux.sockaddr.in = .{
                .family = linux.AF.INET,
                .port = std.mem.nativeToBig(u16, @intCast(p)),
                .addr = 0,
            };
            if (linux.errno(linux.bind(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.in))) == .SUCCESS) {
                return .{ .fd = fd, .port = @intCast(p) };
            }
        }
        // Fallback to ephemeral port
        var any_addr: linux.sockaddr.in = .{ .family = linux.AF.INET, .port = 0, .addr = 0 };
        if (linux.errno(linux.bind(fd, @ptrCast(&any_addr), @sizeOf(linux.sockaddr.in))) != .SUCCESS) {
            return error.UdpBindFailed;
        }
        var bound_addr: linux.sockaddr.in = undefined;
        var addr_len: linux.socklen_t = @sizeOf(linux.sockaddr.in);
        _ = linux.getsockname(fd, @ptrCast(&bound_addr), &addr_len);
        return .{ .fd = fd, .port = std.mem.bigToNative(u16, bound_addr.port) };
    } else {
        const fd = c.socket(c.AF.INET, c.SOCK.DGRAM, 0);
        if (fd < 0) return error.UdpSocketFailed;
        errdefer _ = c.close(fd);

        const nonblock_flag: c_int = @intCast(@as(u32, @bitCast(c.O{ .NONBLOCK = true })));
        const fl = c.fcntl(fd, c.F.GETFL, @as(c_int, 0));
        if (fl >= 0) _ = c.fcntl(fd, c.F.SETFL, fl | nonblock_flag);

        var p: u32 = min_port;
        while (p <= max_port) : (p += 1) {
            var addr: c.sockaddr.in = std.mem.zeroes(c.sockaddr.in);
            addr.family = @intCast(c.AF.INET);
            addr.len = @sizeOf(c.sockaddr.in);
            addr.port = std.mem.nativeToBig(u16, @intCast(p));
            addr.addr = 0;
            if (c.bind(fd, @ptrCast(&addr), @sizeOf(c.sockaddr.in)) == 0) {
                return .{ .fd = fd, .port = @intCast(p) };
            }
        }
        var any_addr: c.sockaddr.in = std.mem.zeroes(c.sockaddr.in);
        any_addr.family = @intCast(c.AF.INET);
        any_addr.len = @sizeOf(c.sockaddr.in);
        if (c.bind(fd, @ptrCast(&any_addr), @sizeOf(c.sockaddr.in)) != 0) {
            return error.UdpBindFailed;
        }
        var bound_addr: c.sockaddr.in = std.mem.zeroes(c.sockaddr.in);
        var addr_len: c.socklen_t = @sizeOf(c.sockaddr.in);
        _ = c.getsockname(fd, @ptrCast(&bound_addr), &addr_len);
        return .{ .fd = fd, .port = std.mem.bigToNative(u16, bound_addr.port) };
    }
}

pub const OperatorClient = struct {
    allocator: Allocator,
    config: cli_mod.CliConfig,
    sock_fd: i32 = -1,
    udp_fd: i32 = -1,
    bound_udp_port: u16 = 0,
    framing_mode: FramingMode = .auto,
    active_framing: FramingMode = .stream,
    registered: bool = false,
    local_ufrag: []const u8 = DEFAULT_LOCAL_UFRAG,
    local_pwd: []const u8 = DEFAULT_LOCAL_PWD,
    local_fingerprint: []const u8 = DEFAULT_LOCAL_FINGERPRINT,
    sessions: std.ArrayList(ClientSession) = .empty,
    stream_accum: std.ArrayList(u8) = .empty,
    stun_responses_sent: usize = 0,
    input_sink: input_sink_mod.InputSink = .{},

    pub fn init(allocator: Allocator, cfg: cli_mod.CliConfig) OperatorClient {
        return .{
            .allocator = allocator,
            .config = cfg,
            .input_sink = input_sink_mod.InputSink.init(cfg.uinput_socket_path),
        };
    }

    pub fn initWithFraming(allocator: Allocator, cfg: cli_mod.CliConfig, mode: FramingMode) OperatorClient {
        return .{
            .allocator = allocator,
            .config = cfg,
            .framing_mode = mode,
            .input_sink = input_sink_mod.InputSink.init(cfg.uinput_socket_path),
        };
    }

    pub fn deinit(self: *OperatorClient) void {
        closeFd(self.sock_fd);
        self.sock_fd = -1;
        closeFd(self.udp_fd);
        self.udp_fd = -1;
        self.sessions.deinit(self.allocator);
        self.stream_accum.deinit(self.allocator);
    }

    pub fn bindMediaUdp(self: *OperatorClient) !u16 {
        if (self.udp_fd >= 0) return self.bound_udp_port;
        const res = try bindUdpInRange(
            self.config.udp_port_range.min_port,
            self.config.udp_port_range.max_port,
        );
        self.udp_fd = res.fd;
        self.bound_udp_port = res.port;
        return res.port;
    }

    pub fn connectWithRetry(self: *OperatorClient, max_attempts: usize, backoff_ms: u64) !void {
        _ = try self.bindMediaUdp();
        var attempt: usize = 0;
        const limit = if (max_attempts == 0) @as(usize, 1) else max_attempts;
        while (attempt < limit) : (attempt += 1) {
            if (self.tryConnectOnce()) |_| {
                return;
            } else |err| {
                if (attempt + 1 >= limit) return err;
                sleepMs(backoff_ms);
            }
        }
    }

    fn tryConnectOnce(self: *OperatorClient) !void {
        const path = self.config.operator_socket_path;
        if (is_linux) {
            if (path.len == 0 or path.len >= 108) return error.InvalidSocketPath;
            var addr: linux.sockaddr.un = .{
                .family = linux.AF.UNIX,
                .path = @splat(0),
            };
            @memcpy(addr.path[0..path.len], path);

            if (self.framing_mode == .auto or self.framing_mode == .seqpacket) {
                const rc = linux.socket(linux.AF.UNIX, linux.SOCK.SEQPACKET | linux.SOCK.CLOEXEC, 0);
                if (linux.errno(rc) == .SUCCESS) {
                    const fd: i32 = @intCast(rc);
                    if (linux.errno(linux.connect(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.un))) == .SUCCESS) {
                        self.sock_fd = fd;
                        self.active_framing = .seqpacket;
                        return;
                    }
                    _ = linux.close(fd);
                }
                if (self.framing_mode == .seqpacket) return error.ConnectFailed;
            }

            const rc = linux.socket(linux.AF.UNIX, linux.SOCK.STREAM | linux.SOCK.CLOEXEC, 0);
            if (linux.errno(rc) != .SUCCESS) return error.SocketFailed;
            const fd: i32 = @intCast(rc);
            if (linux.errno(linux.connect(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.un))) != .SUCCESS) {
                _ = linux.close(fd);
                return error.ConnectFailed;
            }
            self.sock_fd = fd;
            self.active_framing = if (self.framing_mode == .raw_json) .raw_json else .stream;
        } else {
            if (path.len == 0 or path.len >= 104) return error.InvalidSocketPath;
            const fd = c.socket(c.AF.UNIX, c.SOCK.STREAM, 0);
            if (fd < 0) return error.SocketFailed;

            var addr: c.sockaddr.un = std.mem.zeroes(c.sockaddr.un);
            addr.family = @intCast(c.AF.UNIX);
            addr.len = @sizeOf(c.sockaddr.un);
            const path_bytes: []u8 = @ptrCast(&addr.path);
            @memcpy(path_bytes[0..path.len], path);

            if (c.connect(fd, @ptrCast(&addr), @sizeOf(c.sockaddr.un)) != 0) {
                _ = c.close(fd);
                return error.ConnectFailed;
            }
            self.sock_fd = fd;
            self.active_framing = if (self.framing_mode == .raw_json) .raw_json else .stream;
        }
    }

    pub fn sendRegistration(self: *OperatorClient) !void {
        const reg_json = try formatRegisterMessage(self.allocator, self.config);
        defer self.allocator.free(reg_json);
        try self.sendMessage(reg_json);
    }

    pub fn sendMessage(self: *OperatorClient, payload: []const u8) !void {
        if (self.sock_fd < 0) return error.NotConnected;
        switch (self.active_framing) {
            .seqpacket, .raw_json => try writeAllRaw(self.sock_fd, payload),
            .stream, .auto => {
                // Ensure none of the 4 big-endian length bytes equals 0x7b ('{') so both
                // 4-byte BE length-prefixed readers and brace-scanning stream readers work.
                const needs_space = ((payload.len & 0xff) == 0x7b) or (((payload.len >> 8) & 0xff) == 0x7b);
                const msg_len: usize = payload.len + @intFromBool(needs_space);
                var frame = try self.allocator.alloc(u8, 4 + msg_len);
                defer self.allocator.free(frame);
                std.mem.writeInt(u32, frame[0..4], @intCast(msg_len), .big);
                @memcpy(frame[4 .. 4 + payload.len], payload);
                if (needs_space) frame[4 + payload.len] = ' ';
                try writeAllRaw(self.sock_fd, frame);
            },
        }
    }

    pub fn readNextMessageAlloc(self: *OperatorClient) !?[]u8 {
        if (self.sock_fd < 0) return error.NotConnected;
        if (self.active_framing == .seqpacket) {
            var buf: [16384]u8 = undefined;
            const n = try readSomeRaw(self.sock_fd, &buf);
            if (n == 0) return null;
            return try self.allocator.dupe(u8, buf[0..n]);
        }

        while (true) {
            if (try extractStreamMessage(self.allocator, &self.stream_accum)) |msg| {
                return msg;
            }
            var temp: [4096]u8 = undefined;
            const n = try readSomeRaw(self.sock_fd, &temp);
            if (n == 0) return null;
            try self.stream_accum.appendSlice(self.allocator, temp[0..n]);
        }
    }

    /// Non-blocking / timeout-bounded message poll used by the live streamer main loop
    /// so UNIX socket reads never starve UDP STUN / media polling.
    /// Returns `error.EndOfStream` when the peer closes the UNIX socket.
    pub fn pollNextMessageAlloc(self: *OperatorClient, timeout_ms: i32) !?[]u8 {
        if (self.sock_fd < 0) return error.NotConnected;
        if (self.active_framing == .seqpacket) {
            if (!pollReadable(self.sock_fd, timeout_ms)) return null;
            var buf: [16384]u8 = undefined;
            const n = try readSomeRaw(self.sock_fd, &buf);
            if (n == 0) return error.EndOfStream;
            return try self.allocator.dupe(u8, buf[0..n]);
        }

        if (try extractStreamMessage(self.allocator, &self.stream_accum)) |msg| {
            return msg;
        }
        if (!pollReadable(self.sock_fd, timeout_ms)) return null;

        var temp: [4096]u8 = undefined;
        const n = try readSomeRaw(self.sock_fd, &temp);
        if (n == 0) return error.EndOfStream;
        try self.stream_accum.appendSlice(self.allocator, temp[0..n]);
        return try extractStreamMessage(self.allocator, &self.stream_accum);
    }

    pub fn findOrCreateSession(self: *OperatorClient, client_id: u32) !*ClientSession {
        for (self.sessions.items) |*sess| {
            if (sess.client_id == client_id) return sess;
        }
        try self.sessions.append(self.allocator, .{ .client_id = client_id });
        return &self.sessions.items[self.sessions.items.len - 1];
    }

    pub fn findSession(self: *const OperatorClient, client_id: u32) ?*const ClientSession {
        for (self.sessions.items) |*sess| {
            if (sess.client_id == client_id) return sess;
        }
        return null;
    }

    pub fn handleOperatorMessage(self: *OperatorClient, raw_json: []const u8) !void {
        const trimmed = std.mem.trim(u8, raw_json, " \t\r\n");
        if (trimmed.len == 0) return;

        const parsed = std.json.parseFromSlice(std.json.Value, self.allocator, trimmed, .{}) catch {
            return error.InvalidJson;
        };
        defer parsed.deinit();

        const root = switch (parsed.value) {
            .object => |obj| obj,
            else => return error.InvalidJson,
        };

        const type_val = root.get("type") orelse root.get("message_type") orelse return;
        const msg_type = switch (type_val) {
            .string => |s| s,
            else => return,
        };

        if (std.mem.eql(u8, msg_type, "registered") or std.mem.eql(u8, msg_type, "config")) {
            self.registered = true;
            return;
        }

        const cid = extractClientId(root) orelse 0;

        if (std.mem.eql(u8, msg_type, "connect")) {
            _ = try self.findOrCreateSession(cid);
            return;
        }

        if (std.mem.eql(u8, msg_type, "client_disconnected") or std.mem.eql(u8, msg_type, "disconnect")) {
            if (cid != 0) {
                for (self.sessions.items) |*s| {
                    if (s.client_id == cid) s.connected = false;
                }
            }
            return;
        }

        if (std.mem.eql(u8, msg_type, "request-offer")) {
            try self.handleRequestOfferForClient(cid);
            return;
        }

        if (std.mem.eql(u8, msg_type, "offer")) {
            if (root.get("sdp")) |sdp_v| {
                if (sdp_v == .string) {
                    try self.handleOfferForClient(cid, sdp_v.string);
                }
            }
            return;
        }

        if (std.mem.eql(u8, msg_type, "answer")) {
            if (root.get("sdp")) |sdp_v| {
                if (sdp_v == .string) {
                    try self.handleAnswerForClient(cid, sdp_v.string);
                }
            }
            return;
        }

        if (std.mem.eql(u8, msg_type, "ice-candidate") or std.mem.eql(u8, msg_type, "ice") or std.mem.eql(u8, msg_type, "candidate")) {
            if (root.get("candidate")) |cand_v| {
                if (cand_v == .string) {
                    try self.handleRemoteCandidateForClient(cid, cand_v.string);
                }
            }
            return;
        }

        if (std.mem.eql(u8, msg_type, "forward") or std.mem.eql(u8, msg_type, "client_msg")) {
            _ = try self.findOrCreateSession(cid);
            const payload_val = root.get("payload") orelse return;
            if (payload_val != .object) return;
            const payload = payload_val.object;
            const inner_type_v = payload.get("type") orelse payload.get("message_type");
            const inner_type = if (inner_type_v) |v| (if (v == .string) v.string else "") else "";

            if (std.mem.eql(u8, inner_type, "request-offer")) {
                try self.handleRequestOfferForClient(cid);
            } else if (std.mem.eql(u8, inner_type, "offer") or (inner_type.len == 0 and payload.get("sdp") != null)) {
                if (payload.get("sdp")) |sdp_v| {
                    if (sdp_v == .string) {
                        try self.handleOfferForClient(cid, sdp_v.string);
                    }
                }
            } else if (std.mem.eql(u8, inner_type, "answer")) {
                if (payload.get("sdp")) |sdp_v| {
                    if (sdp_v == .string) {
                        try self.handleAnswerForClient(cid, sdp_v.string);
                    }
                }
            } else if (std.mem.eql(u8, inner_type, "ice-candidate") or
                std.mem.eql(u8, inner_type, "ice") or
                std.mem.eql(u8, inner_type, "candidate") or
                payload.get("candidate") != null)
            {
                if (payload.get("candidate")) |cand_v| {
                    switch (cand_v) {
                        .string => |s| try self.handleRemoteCandidateForClient(cid, s),
                        .object => |cobj| {
                            if (cobj.get("candidate")) |inner_c| {
                                if (inner_c == .string) try self.handleRemoteCandidateForClient(cid, inner_c.string);
                            }
                        },
                        else => {},
                    }
                }
            } else if (std.mem.eql(u8, inner_type, "data-channel")) {
                if (payload.get("data")) |data_v| {
                    if (data_v == .string) {
                        _ = self.input_sink.handleDataChannelJson(self.allocator, data_v.string) catch {};
                    }
                }
            }
        }
    }

    fn handleRequestOfferForClient(self: *OperatorClient, client_id: u32) !void {
        _ = try self.bindMediaUdp();
        const sess = try self.findOrCreateSession(client_id);

        const offer_sdp = try buildSdpOffer(
            self.allocator,
            self.config.ice_host_address,
            self.config.ice_host_ipv6,
            self.bound_udp_port,
            self.local_ufrag,
            self.local_pwd,
            self.local_fingerprint,
        );
        defer self.allocator.free(offer_sdp);

        const offer_msg = try formatForwardOfferMessage(self.allocator, client_id, offer_sdp);
        defer self.allocator.free(offer_msg);
        try self.sendMessage(offer_msg);

        const cand_msg = try formatForwardCandidateMessage(
            self.allocator,
            client_id,
            self.config.ice_host_address,
            self.bound_udp_port,
        );
        defer self.allocator.free(cand_msg);
        try self.sendMessage(cand_msg);

        sess.offer_sent = true;
    }

    fn handleAnswerForClient(self: *OperatorClient, client_id: u32, answer_sdp: []const u8) !void {
        const sess = try self.findOrCreateSession(client_id);
        sess.answer_received = true;
        sess.remote_sdp = parseRemoteSdp(answer_sdp);
    }

    fn handleOfferForClient(self: *OperatorClient, client_id: u32, offer_sdp: []const u8) !void {
        _ = try self.bindMediaUdp();
        const sess = try self.findOrCreateSession(client_id);
        sess.offer_received = true;
        sess.remote_sdp = parseRemoteSdp(offer_sdp);

        const answer_sdp = try buildSdpAnswer(
            self.allocator,
            self.config.ice_host_address,
            self.config.ice_host_ipv6,
            self.bound_udp_port,
            self.local_ufrag,
            self.local_pwd,
            self.local_fingerprint,
        );
        defer self.allocator.free(answer_sdp);

        const ans_msg = try formatForwardAnswerMessage(self.allocator, client_id, answer_sdp);
        defer self.allocator.free(ans_msg);
        try self.sendMessage(ans_msg);

        const cand_msg = try formatForwardCandidateMessage(
            self.allocator,
            client_id,
            self.config.ice_host_address,
            self.bound_udp_port,
        );
        defer self.allocator.free(cand_msg);
        try self.sendMessage(cand_msg);

        sess.answer_sent = true;
    }

    fn handleRemoteCandidateForClient(self: *OperatorClient, client_id: u32, cand_str: []const u8) !void {
        const sess = try self.findOrCreateSession(client_id);
        if (parseIceCandidateLine(cand_str)) |parsed| {
            const n = @min(parsed.address.len, sess.remote_candidate_ip_buf.len);
            @memcpy(sess.remote_candidate_ip_buf[0..n], parsed.address[0..n]);
            sess.remote_candidate_ip_len = n;
            sess.remote_candidate_port = parsed.port;
        }
    }

    /// Polls the bound UDP socket once and responds to any incoming STUN Binding Request.
    pub fn pollUdpStunOnce(self: *OperatorClient) !bool {
        if (self.udp_fd < 0) return false;
        if (!pollReadable(self.udp_fd, 0)) return false;
        var buf: [2048]u8 = undefined;
        var resp_buf: [128]u8 = undefined;

        if (is_linux) {
            var src_addr: linux.sockaddr.in = std.mem.zeroes(linux.sockaddr.in);
            var addr_len: linux.socklen_t = @sizeOf(linux.sockaddr.in);
            const rc = linux.recvfrom(self.udp_fd, &buf, buf.len, 0, @ptrCast(&src_addr), &addr_len);
            if (linux.errno(rc) != .SUCCESS or rc == 0) return false;
            const pkt = buf[0..rc];
            if (!isStunBindingRequest(pkt)) return false;

            const src_ip_host = std.mem.bigToNative(u32, src_addr.addr);
            const src_port_host = std.mem.bigToNative(u16, src_addr.port);
            const resp = try buildStunBindingResponse(pkt, self.local_pwd, src_ip_host, src_port_host, &resp_buf);
            _ = linux.sendto(self.udp_fd, resp.ptr, resp.len, 0, @ptrCast(&src_addr), addr_len);
            self.stun_responses_sent += 1;
            return true;
        } else {
            var src_addr: c.sockaddr.in = std.mem.zeroes(c.sockaddr.in);
            var addr_len: c.socklen_t = @sizeOf(c.sockaddr.in);
            const rc = c.recvfrom(self.udp_fd, &buf, buf.len, 0, @ptrCast(&src_addr), &addr_len);
            if (rc <= 0) return false;
            const pkt = buf[0..@intCast(rc)];
            if (!isStunBindingRequest(pkt)) return false;

            const src_ip_host = std.mem.bigToNative(u32, src_addr.addr);
            const src_port_host = std.mem.bigToNative(u16, src_addr.port);
            const resp = try buildStunBindingResponse(pkt, self.local_pwd, src_ip_host, src_port_host, &resp_buf);
            _ = c.sendto(self.udp_fd, resp.ptr, resp.len, 0, @ptrCast(&src_addr), addr_len);
            self.stun_responses_sent += 1;
            return true;
        }
    }
};

fn extractClientId(root: std.json.ObjectMap) ?u32 {
    const v = root.get("client_id") orelse return null;
    return switch (v) {
        .integer => |i| if (i >= 0 and i <= std.math.maxInt(u32)) @intCast(i) else null,
        .string => |s| std.fmt.parseInt(u32, s, 10) catch null,
        else => null,
    };
}

fn writeAllRaw(fd: i32, data: []const u8) !void {
    var off: usize = 0;
    while (off < data.len) {
        if (is_linux) {
            const rc = linux.sendto(fd, data[off..].ptr, data.len - off, 0, null, 0);
            switch (linux.errno(rc)) {
                .SUCCESS => {
                    if (rc == 0) return error.WriteFailed;
                    off += rc;
                },
                .INTR => continue,
                else => return error.WriteFailed,
            }
        } else {
            const rc = c.write(fd, data[off..].ptr, data.len - off);
            if (rc > 0) {
                off += @intCast(rc);
            } else {
                return error.WriteFailed;
            }
        }
    }
}

fn readSomeRaw(fd: i32, buf: []u8) !usize {
    while (true) {
        if (is_linux) {
            const rc = linux.recvfrom(fd, buf.ptr, buf.len, 0, null, null);
            switch (linux.errno(rc)) {
                .SUCCESS => return rc,
                .INTR => continue,
                else => return error.ReadFailed,
            }
        } else {
            const rc = c.read(fd, buf.ptr, buf.len);
            if (rc >= 0) return @intCast(rc);
            return error.ReadFailed;
        }
    }
}

/// Extracts either a 4-byte big-endian length-prefixed JSON message or a raw balanced JSON object
/// from `accum`.
pub fn extractStreamMessage(allocator: Allocator, accum: *std.ArrayList(u8)) !?[]u8 {
    if (accum.items.len == 0) return null;

    // Case 1: 4-byte big-endian length prefix (first byte is 0x00 for all messages < 16 MiB)
    if (accum.items[0] != '{' and !std.ascii.isWhitespace(accum.items[0])) {
        if (accum.items.len < 4) return null;
        const msg_len: usize = std.mem.readInt(u32, accum.items[0..4], .big);
        if (msg_len > 4 * 1024 * 1024) return error.FrameTooLarge;
        if (accum.items.len < 4 + msg_len) return null;

        const out = try allocator.dupe(u8, std.mem.trim(u8, accum.items[4 .. 4 + msg_len], " \t\r\n"));
        const rem = accum.items.len - (4 + msg_len);
        if (rem > 0) {
            std.mem.copyForwards(u8, accum.items[0..rem], accum.items[4 + msg_len ..]);
        }
        accum.shrinkRetainingCapacity(rem);
        return out;
    }

    // Case 2: Raw JSON object on stream
    var start: usize = 0;
    while (start < accum.items.len and accum.items[start] != '{') : (start += 1) {}
    if (start >= accum.items.len) {
        accum.clearRetainingCapacity();
        return null;
    }

    var depth: usize = 0;
    var in_string = false;
    var escaped = false;
    var i: usize = start;
    while (i < accum.items.len) : (i += 1) {
        const ch = accum.items[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
        } else if (ch == '{') {
            depth += 1;
        } else if (ch == '}') {
            depth -= 1;
            if (depth == 0) {
                const end = i + 1;
                const out = try allocator.dupe(u8, accum.items[start..end]);
                const rem = accum.items.len - end;
                if (rem > 0) {
                    std.mem.copyForwards(u8, accum.items[0..rem], accum.items[end..]);
                }
                accum.shrinkRetainingCapacity(rem);
                return out;
            }
        }
    }
    return null;
}

test "STUN Binding Request and RFC 8489 Success Response roundtrip" {
    const tx_id = [12]u8{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    var req_buf: [20]u8 = undefined;
    const req = buildStunBindingRequest(tx_id, &req_buf);
    try std.testing.expect(isStunBindingRequest(req));

    var resp_buf: [128]u8 = undefined;
    const resp = try buildStunBindingResponse(req, DEFAULT_LOCAL_PWD, 0x7f000001, 54321, &resp_buf);
    try std.testing.expectEqual(@as(usize, 64), resp.len);
    try std.testing.expectEqual(STUN_BINDING_RESPONSE, std.mem.readInt(u16, resp[0..2], .big));
    try std.testing.expectEqual(@as(u16, 44), std.mem.readInt(u16, resp[2..4], .big));
    try std.testing.expectEqual(STUN_MAGIC_COOKIE, std.mem.readInt(u32, resp[4..8], .big));
    try std.testing.expectEqualSlices(u8, &tx_id, resp[8..20]);

    // Verify FINGERPRINT attribute at end of response
    const fp_attr_type = std.mem.readInt(u16, resp[56..58], .big);
    const fp_attr_len = std.mem.readInt(u16, resp[58..60], .big);
    const fp_val = std.mem.readInt(u32, resp[60..64], .big);
    try std.testing.expectEqual(STUN_ATTR_FINGERPRINT, fp_attr_type);
    try std.testing.expectEqual(@as(u16, 4), fp_attr_len);
    const expected_crc = std.hash.Crc32.hash(resp[0..56]) ^ STUN_FINGERPRINT_XOR;
    try std.testing.expectEqual(expected_crc, fp_val);
}

test "SDP Answer generation includes required ICE, DTLS, VP8/H264, SCTP, and host candidates" {
    const allocator = std.testing.allocator;
    const sdp = try buildSdpAnswer(
        allocator,
        "198.51.100.1",
        "fd00::1",
        15550,
        DEFAULT_LOCAL_UFRAG,
        DEFAULT_LOCAL_PWD,
        DEFAULT_LOCAL_FINGERPRINT,
    );
    defer allocator.free(sdp);

    try std.testing.expect(std.mem.indexOf(u8, sdp, "a=ice-ufrag:cfzig001\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, sdp, "a=ice-pwd:cfzigpassword000000000001\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, sdp, "a=fingerprint:sha-256 ") != null);
    try std.testing.expect(std.mem.indexOf(u8, sdp, "a=setup:passive\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, sdp, "m=video 15550 UDP/TLS/RTP/SAVPF 96 102\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, sdp, "m=application 15550 UDP/DTLS/SCTP webrtc-datachannel\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, sdp, "a=candidate:1 1 udp 2130706431 198.51.100.1 15550 typ host\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, sdp, "a=candidate:2 1 udp 2130706430 fd00::1 15550 typ host\r\n") != null);
}

test "SDP Offer and Register message include browser UI required fields" {
    const allocator = std.testing.allocator;
    const offer = try buildSdpOffer(
        allocator,
        "127.0.0.1",
        null,
        15550,
        DEFAULT_LOCAL_UFRAG,
        DEFAULT_LOCAL_PWD,
        DEFAULT_LOCAL_FINGERPRINT,
    );
    defer allocator.free(offer);
    try std.testing.expect(std.mem.indexOf(u8, offer, "a=setup:actpass\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, offer, "a=msid:display_0 display_0_track\r\n") != null);

    const cfg = cli_mod.CliConfig{};
    const reg = try formatRegisterMessage(allocator, cfg);
    defer allocator.free(reg);
    try std.testing.expect(std.mem.indexOf(u8, reg, "\"custom_control_panel_buttons\":[]") != null);
    try std.testing.expect(std.mem.indexOf(u8, reg, "\"hardware\":{\"cpus\":4,\"memory_mb\":4096,\"camera_passthrough\":false}") != null);
    try std.testing.expect(std.mem.indexOf(u8, reg, "\"audio_streams\":[]") != null);
    try std.testing.expect(std.mem.indexOf(u8, reg, "\"mouse_enabled\":true") != null);
}

