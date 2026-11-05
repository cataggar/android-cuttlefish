const std = @import("std");
const builtin = @import("builtin");
const linux = std.os.linux;
const Allocator = std.mem.Allocator;
const HmacSha1 = std.crypto.auth.hmac.HmacSha1;
const Crc32IsoHdlc = std.hash.Crc32;

const darwin_c = struct {
    pub const AF_INET: c_int = 2;
    pub const SOCK_DGRAM: c_int = 2;
    pub const MSG_DONTWAIT: c_int = 0x80;
    pub const EAGAIN: c_int = 35;
    pub const EADDRNOTAVAIL: c_int = 49;

    pub const SockAddrIn = extern struct {
        len: u8 = @sizeOf(SockAddrIn),
        family: u8 = 2,
        port: u16,
        addr: u32,
        zero: [8]u8 = .{ 0, 0, 0, 0, 0, 0, 0, 0 },
    };

    pub const PollFd = extern struct {
        fd: c_int,
        events: c_short,
        revents: c_short,
    };
    pub const POLLIN: c_short = 0x0001;

    extern "c" fn socket(domain: c_int, sock_type: c_int, protocol: c_int) c_int;
    extern "c" fn bind(s: c_int, addr: *const anyopaque, addrlen: u32) c_int;
    extern "c" fn getsockname(s: c_int, addr: *anyopaque, addrlen: *u32) c_int;
    extern "c" fn sendto(s: c_int, buf: *const anyopaque, len: usize, flags: c_int, dest_addr: *const anyopaque, dest_len: u32) isize;
    extern "c" fn recvfrom(s: c_int, buf: *anyopaque, len: usize, flags: c_int, src_addr: *anyopaque, addrlen: *u32) isize;
    extern "c" fn poll(fds: [*]PollFd, nfds: c_uint, timeout: c_int) c_int;
    extern "c" fn close(fd: c_int) c_int;
    extern "c" fn __error() *c_int;
};

pub const SocketFd = if (builtin.os.tag == .linux) linux.fd_t else c_int;

const UdpBindResult = enum {
    ok,
    addr_not_avail,
    failed,
};

fn createUdpSocket() Error!SocketFd {
    if (comptime builtin.os.tag == .linux) {
        const rc = linux.socket(linux.AF.INET, linux.SOCK.DGRAM | linux.SOCK.CLOEXEC, 0);
        if (linux.errno(rc) != .SUCCESS) return Error.SocketCreationFailed;
        return @intCast(rc);
    } else {
        const fd = darwin_c.socket(darwin_c.AF_INET, darwin_c.SOCK_DGRAM, 0);
        if (fd < 0) return Error.SocketCreationFailed;
        return fd;
    }
}

fn closeUdpSocket(fd: SocketFd) void {
    if (comptime builtin.os.tag == .linux) {
        _ = linux.close(fd);
    } else {
        _ = darwin_c.close(fd);
    }
}

fn bindUdpSocket(fd: SocketFd, ip: [4]u8, port: u16) UdpBindResult {
    const ip_be: u32 = @bitCast(ip);
    const port_be: u16 = std.mem.nativeToBig(u16, port);
    if (comptime builtin.os.tag == .linux) {
        var addr: linux.sockaddr.in = .{
            .family = linux.AF.INET,
            .port = port_be,
            .addr = ip_be,
        };
        const rc = linux.bind(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.in));
        return switch (linux.errno(rc)) {
            .SUCCESS => .ok,
            .ADDRNOTAVAIL => .addr_not_avail,
            else => .failed,
        };
    } else {
        var addr: darwin_c.SockAddrIn = .{
            .port = port_be,
            .addr = ip_be,
        };
        const rc = darwin_c.bind(fd, @ptrCast(&addr), @sizeOf(darwin_c.SockAddrIn));
        if (rc == 0) return .ok;
        const err = darwin_c.__error().*;
        if (err == darwin_c.EADDRNOTAVAIL) return .addr_not_avail;
        return .failed;
    }
}

fn getUdpBoundPort(fd: SocketFd) Error!u16 {
    if (comptime builtin.os.tag == .linux) {
        var bound_addr: linux.sockaddr.in = undefined;
        var addr_len: linux.socklen_t = @sizeOf(linux.sockaddr.in);
        if (linux.errno(linux.getsockname(fd, @ptrCast(&bound_addr), &addr_len)) != .SUCCESS) {
            return Error.BindFailed;
        }
        return std.mem.bigToNative(u16, bound_addr.port);
    } else {
        var bound_addr: darwin_c.SockAddrIn = undefined;
        var addr_len: u32 = @sizeOf(darwin_c.SockAddrIn);
        if (darwin_c.getsockname(fd, @ptrCast(&bound_addr), &addr_len) != 0) {
            return Error.BindFailed;
        }
        return std.mem.bigToNative(u16, bound_addr.port);
    }
}

pub const Error = error{
    InvalidCandidate,
    InvalidAddress,
    InvalidPort,
    InvalidTransport,
    InvalidCandidateType,
    InvalidStunPacket,
    InvalidMagicCookie,
    InvalidAttribute,
    IntegrityCheckFailed,
    FingerprintCheckFailed,
    UsernameMismatch,
    InvalidTurnChannel,
    BufferTooSmall,
    SocketCreationFailed,
    BindFailed,
    SendFailed,
    RecvFailed,
    NoSocketBound,
    PairIndexOutOfBounds,
    OutOfMemory,
};

/// UDP port range for ICE candidate gathering.
pub const PortRange = struct {
    min_port: u16 = 15550,
    max_port: u16 = 15599,
};

/// STUN / TURN server URI configuration.
pub const ServerConfig = struct {
    uri: [:0]const u8 = "stun:stun.l.google.com:19302",
};

/// Trickle ICE candidate descriptor matching `root.zig`.
pub const Candidate = struct {
    sdp_mid: [:0]const u8,
    sdp_mline_index: i32,
    candidate_sdp: [:0]const u8,
};

/// RFC 8445 Section 5.1.1.1 ICE candidate type.
pub const CandidateType = enum {
    host,
    srflx,
    prflx,
    relay,

    /// RFC 8445 Section 5.1.2.2 recommended type preferences.
    pub fn typePreference(self: CandidateType) u32 {
        return switch (self) {
            .host => 126,
            .prflx => 110,
            .srflx => 100,
            .relay => 0,
        };
    }

    pub fn toString(self: CandidateType) []const u8 {
        return switch (self) {
            .host => "host",
            .srflx => "srflx",
            .prflx => "prflx",
            .relay => "relay",
        };
    }

    pub fn fromString(s: []const u8) ?CandidateType {
        if (std.ascii.eqlIgnoreCase(s, "host")) return .host;
        if (std.ascii.eqlIgnoreCase(s, "srflx")) return .srflx;
        if (std.ascii.eqlIgnoreCase(s, "prflx")) return .prflx;
        if (std.ascii.eqlIgnoreCase(s, "relay")) return .relay;
        return null;
    }
};

/// Transport protocol for ICE candidates.
pub const TransportProtocol = enum {
    udp,
    tcp,

    pub fn toString(self: TransportProtocol) []const u8 {
        return switch (self) {
            .udp => "udp",
            .tcp => "tcp",
        };
    }

    pub fn fromString(s: []const u8) ?TransportProtocol {
        if (std.ascii.eqlIgnoreCase(s, "udp")) return .udp;
        if (std.ascii.eqlIgnoreCase(s, "tcp")) return .tcp;
        return null;
    }
};

pub fn parseIpv4(text: []const u8) Error![4]u8 {
    var out: [4]u8 = undefined;
    var iter = std.mem.splitScalar(u8, text, '.');
    var idx: usize = 0;
    while (iter.next()) |octet_str| {
        if (idx >= 4 or octet_str.len == 0) return Error.InvalidAddress;
        out[idx] = std.fmt.parseInt(u8, octet_str, 10) catch return Error.InvalidAddress;
        idx += 1;
    }
    if (idx != 4) return Error.InvalidAddress;
    return out;
}

/// Parsed SDP ICE candidate (`a=candidate:...` per RFC 8839 / RFC 8445).
pub const ParsedCandidate = struct {
    foundation: []const u8,
    component_id: u8 = 1,
    transport: TransportProtocol = .udp,
    priority: u32,
    address: [4]u8,
    port: u16,
    cand_type: CandidateType,
    raddr: ?[4]u8 = null,
    rport: ?u16 = null,
    generation: u32 = 0,
    ufrag: ?[]const u8 = null,

    /// RFC 8445 Section 5.1.2.1:
    /// priority = (2^24)*(type preference) + (2^8)*(local preference) + (2^0)*(256 - component ID)
    pub fn computePriority(cand_type: CandidateType, local_pref: u16, component_id: u8) u32 {
        const comp: u32 = if (component_id == 0) 1 else component_id;
        return (cand_type.typePreference() << 24) +
            (@as(u32, local_pref) << 8) +
            (256 - comp);
    }

    /// Parses an SDP candidate line (`a=candidate:...` or `candidate:...`).
    pub fn parse(sdp_line: []const u8) Error!ParsedCandidate {
        var trimmed = std.mem.trim(u8, sdp_line, " \t\r\n");
        if (std.mem.startsWith(u8, trimmed, "a=candidate:")) {
            trimmed = trimmed["a=candidate:".len..];
        } else if (std.mem.startsWith(u8, trimmed, "candidate:")) {
            trimmed = trimmed["candidate:".len..];
        } else {
            return Error.InvalidCandidate;
        }

        var tok = std.mem.tokenizeAny(u8, trimmed, " \t");
        const foundation = tok.next() orelse return Error.InvalidCandidate;
        const comp_str = tok.next() orelse return Error.InvalidCandidate;
        const trans_str = tok.next() orelse return Error.InvalidCandidate;
        const prio_str = tok.next() orelse return Error.InvalidCandidate;
        const addr_str = tok.next() orelse return Error.InvalidCandidate;
        const port_str = tok.next() orelse return Error.InvalidCandidate;
        const typ_kw = tok.next() orelse return Error.InvalidCandidate;
        const typ_str = tok.next() orelse return Error.InvalidCandidate;

        if (!std.mem.eql(u8, typ_kw, "typ")) return Error.InvalidCandidate;

        const component_id = std.fmt.parseInt(u8, comp_str, 10) catch return Error.InvalidCandidate;
        if (component_id == 0) return Error.InvalidCandidate;

        const transport = TransportProtocol.fromString(trans_str) orelse return Error.InvalidTransport;
        const priority = std.fmt.parseInt(u32, prio_str, 10) catch return Error.InvalidCandidate;
        const address = try parseIpv4(addr_str);
        const port = std.fmt.parseInt(u16, port_str, 10) catch return Error.InvalidPort;
        const cand_type = CandidateType.fromString(typ_str) orelse return Error.InvalidCandidateType;

        var parsed = ParsedCandidate{
            .foundation = foundation,
            .component_id = component_id,
            .transport = transport,
            .priority = priority,
            .address = address,
            .port = port,
            .cand_type = cand_type,
        };

        while (tok.next()) |key| {
            const val = tok.next() orelse break;
            if (std.mem.eql(u8, key, "raddr")) {
                parsed.raddr = try parseIpv4(val);
            } else if (std.mem.eql(u8, key, "rport")) {
                parsed.rport = std.fmt.parseInt(u16, val, 10) catch return Error.InvalidPort;
            } else if (std.mem.eql(u8, key, "generation")) {
                parsed.generation = std.fmt.parseInt(u32, val, 10) catch return Error.InvalidCandidate;
            } else if (std.mem.eql(u8, key, "ufrag")) {
                parsed.ufrag = val;
            }
        }

        return parsed;
    }

    /// Formats the candidate into an SDP `candidate:...` attribute value.
    pub fn formatSdp(self: ParsedCandidate, buf: []u8) Error![]const u8 {
        var pos: usize = 0;
        const base = std.fmt.bufPrint(
            buf[pos..],
            "candidate:{s} {d} {s} {d} {d}.{d}.{d}.{d} {d} typ {s}",
            .{
                self.foundation,
                self.component_id,
                self.transport.toString(),
                self.priority,
                self.address[0],
                self.address[1],
                self.address[2],
                self.address[3],
                self.port,
                self.cand_type.toString(),
            },
        ) catch return Error.BufferTooSmall;
        pos += base.len;

        if (self.raddr) |ra| {
            const raddr_part = std.fmt.bufPrint(
                buf[pos..],
                " raddr {d}.{d}.{d}.{d}",
                .{ ra[0], ra[1], ra[2], ra[3] },
            ) catch return Error.BufferTooSmall;
            pos += raddr_part.len;
        }

        if (self.rport) |rp| {
            const rport_part = std.fmt.bufPrint(
                buf[pos..],
                " rport {d}",
                .{rp},
            ) catch return Error.BufferTooSmall;
            pos += rport_part.len;
        }

        const gen_part = std.fmt.bufPrint(
            buf[pos..],
            " generation {d}",
            .{self.generation},
        ) catch return Error.BufferTooSmall;
        pos += gen_part.len;

        if (self.ufrag) |uf| {
            const uf_part = std.fmt.bufPrint(
                buf[pos..],
                " ufrag {s}",
                .{uf},
            ) catch return Error.BufferTooSmall;
            pos += uf_part.len;
        }

        return buf[0..pos];
    }
};

// ============================================================================
// STUN (RFC 8489) & TURN (RFC 8656) Packet Framing
// ============================================================================

pub const MAGIC_COOKIE: u32 = 0x2112A442;
pub const FINGERPRINT_XOR: u32 = 0x5354554e;
pub const STUN_HEADER_LEN: usize = 20;

pub const MessageType = enum(u16) {
    binding_request = 0x0001,
    binding_indication = 0x0011,
    binding_response = 0x0101,
    binding_error_response = 0x0111,
    allocate_request = 0x0003,
    allocate_response = 0x0103,
    create_permission_request = 0x0008,
    create_permission_response = 0x0108,
    channel_bind_request = 0x0009,
    channel_bind_response = 0x0109,
    send_indication = 0x0016,
    data_indication = 0x0017,
    _,
};

pub const AttrType = enum(u16) {
    mapped_address = 0x0001,
    username = 0x0006,
    message_integrity = 0x0008,
    error_code = 0x0009,
    channel_number = 0x000C,
    lifetime = 0x000D,
    xor_peer_address = 0x0012,
    data = 0x0013,
    xor_relayed_address = 0x0016,
    requested_transport = 0x0019,
    xor_mapped_address = 0x0020,
    priority = 0x0024,
    use_candidate = 0x0025,
    software = 0x8022,
    fingerprint = 0x8028,
    ice_controlled = 0x8029,
    ice_controlling = 0x802A,
    _,
};

pub const TransportEndpoint = struct {
    ip: [4]u8,
    port: u16,
};

/// Encodes an IPv4 endpoint into the 8-byte STUN XOR-MAPPED-ADDRESS / XOR-PEER-ADDRESS /
/// XOR-RELAYED-ADDRESS attribute value (RFC 8489 Section 14.2).
pub fn encodeXorAddress(endpoint: TransportEndpoint) [8]u8 {
    var out: [8]u8 = undefined;
    out[0] = 0x00;
    out[1] = 0x01; // IPv4 family
    const xor_port: u16 = endpoint.port ^ @as(u16, @truncate(MAGIC_COOKIE >> 16));
    std.mem.writeInt(u16, out[2..4], xor_port, .big);
    const ip_u32 = std.mem.readInt(u32, &endpoint.ip, .big);
    const xor_ip: u32 = ip_u32 ^ MAGIC_COOKIE;
    std.mem.writeInt(u32, out[4..8], xor_ip, .big);
    return out;
}

/// Decodes an 8-byte IPv4 XOR-MAPPED-ADDRESS / XOR-PEER-ADDRESS / XOR-RELAYED-ADDRESS value.
pub fn decodeXorAddress(attr_val: []const u8) Error!TransportEndpoint {
    if (attr_val.len < 8) return Error.InvalidAttribute;
    if (attr_val[1] != 0x01) return Error.InvalidAttribute; // Must be IPv4
    const xor_port = std.mem.readInt(u16, attr_val[2..4], .big);
    const port: u16 = xor_port ^ @as(u16, @truncate(MAGIC_COOKIE >> 16));
    const xor_ip = std.mem.readInt(u32, attr_val[4..8], .big);
    const ip_u32: u32 = xor_ip ^ MAGIC_COOKIE;
    var ip: [4]u8 = undefined;
    std.mem.writeInt(u32, &ip, ip_u32, .big);
    return .{ .ip = ip, .port = port };
}

pub const StunMessage = struct {
    msg_type: MessageType,
    transaction_id: [12]u8,
    username: ?[]const u8 = null,
    priority: ?u32 = null,
    use_candidate: bool = false,
    ice_controlling: ?u64 = null,
    ice_controlled: ?u64 = null,
    mapped_address: ?TransportEndpoint = null,
    xor_mapped_address: ?TransportEndpoint = null,
    xor_peer_address: ?TransportEndpoint = null,
    xor_relayed_address: ?TransportEndpoint = null,
    requested_transport: ?u8 = null,
    lifetime: ?u32 = null,
    channel_number: ?u16 = null,
    data: ?[]const u8 = null,
    software: ?[]const u8 = null,
    error_code: ?u16 = null,
    has_message_integrity: bool = false,
    message_integrity: [20]u8 = @splat(0),
    mi_offset: ?usize = null,
    has_fingerprint: bool = false,
    fingerprint: u32 = 0,
    fp_offset: ?usize = null,

    fn appendAttrHeader(out_buf: []u8, pos: *usize, attr_type: AttrType, attr_len: u16) Error!void {
        if (pos.* + 4 > out_buf.len) return Error.BufferTooSmall;
        std.mem.writeInt(u16, out_buf[pos.*..][0..2], @intFromEnum(attr_type), .big);
        std.mem.writeInt(u16, out_buf[pos.* + 2 ..][0..2], attr_len, .big);
        pos.* += 4;
    }

    fn appendAttrBytes(out_buf: []u8, pos: *usize, attr_type: AttrType, val: []const u8) Error!void {
        const padded_len = (val.len + 3) & ~@as(usize, 3);
        if (pos.* + 4 + padded_len > out_buf.len) return Error.BufferTooSmall;
        try appendAttrHeader(out_buf, pos, attr_type, @intCast(val.len));
        @memcpy(out_buf[pos.* .. pos.* + val.len], val);
        if (padded_len > val.len) {
            @memset(out_buf[pos.* + val.len .. pos.* + padded_len], 0);
        }
        pos.* += padded_len;
    }

    fn appendAttrU32(out_buf: []u8, pos: *usize, attr_type: AttrType, val: u32) Error!void {
        var b: [4]u8 = undefined;
        std.mem.writeInt(u32, &b, val, .big);
        try appendAttrBytes(out_buf, pos, attr_type, &b);
    }

    fn appendAttrU64(out_buf: []u8, pos: *usize, attr_type: AttrType, val: u64) Error!void {
        var b: [8]u8 = undefined;
        std.mem.writeInt(u64, &b, val, .big);
        try appendAttrBytes(out_buf, pos, attr_type, &b);
    }

    /// Encodes the STUN/TURN message into `out_buf`, optionally appending
    /// RFC 8489 `MESSAGE-INTEGRITY` (keyed by `integrity_key`) and `FINGERPRINT`.
    pub fn encode(
        self: StunMessage,
        out_buf: []u8,
        integrity_key: ?[]const u8,
        include_fingerprint: bool,
    ) Error![]const u8 {
        if (out_buf.len < STUN_HEADER_LEN) return Error.BufferTooSmall;

        std.mem.writeInt(u16, out_buf[0..2], @intFromEnum(self.msg_type), .big);
        std.mem.writeInt(u16, out_buf[2..4], 0, .big); // placeholder length
        std.mem.writeInt(u32, out_buf[4..8], MAGIC_COOKIE, .big);
        @memcpy(out_buf[8..20], &self.transaction_id);

        var pos: usize = STUN_HEADER_LEN;

        if (self.username) |u| {
            try appendAttrBytes(out_buf, &pos, .username, u);
        }
        if (self.priority) |p| {
            try appendAttrU32(out_buf, &pos, .priority, p);
        }
        if (self.use_candidate) {
            try appendAttrHeader(out_buf, &pos, .use_candidate, 0);
        }
        if (self.ice_controlling) |tb| {
            try appendAttrU64(out_buf, &pos, .ice_controlling, tb);
        }
        if (self.ice_controlled) |tb| {
            try appendAttrU64(out_buf, &pos, .ice_controlled, tb);
        }
        if (self.mapped_address) |ep| {
            var b: [8]u8 = undefined;
            b[0] = 0x00;
            b[1] = 0x01;
            std.mem.writeInt(u16, b[2..4], ep.port, .big);
            @memcpy(b[4..8], &ep.ip);
            try appendAttrBytes(out_buf, &pos, .mapped_address, &b);
        }
        if (self.xor_mapped_address) |ep| {
            const b = encodeXorAddress(ep);
            try appendAttrBytes(out_buf, &pos, .xor_mapped_address, &b);
        }
        if (self.xor_peer_address) |ep| {
            const b = encodeXorAddress(ep);
            try appendAttrBytes(out_buf, &pos, .xor_peer_address, &b);
        }
        if (self.xor_relayed_address) |ep| {
            const b = encodeXorAddress(ep);
            try appendAttrBytes(out_buf, &pos, .xor_relayed_address, &b);
        }
        if (self.requested_transport) |proto| {
            const b = [4]u8{ proto, 0, 0, 0 };
            try appendAttrBytes(out_buf, &pos, .requested_transport, &b);
        }
        if (self.lifetime) |lt| {
            try appendAttrU32(out_buf, &pos, .lifetime, lt);
        }
        if (self.channel_number) |ch| {
            var b = [4]u8{ 0, 0, 0, 0 };
            std.mem.writeInt(u16, b[0..2], ch, .big);
            try appendAttrBytes(out_buf, &pos, .channel_number, &b);
        }
        if (self.data) |d| {
            try appendAttrBytes(out_buf, &pos, .data, d);
        }
        if (self.software) |sw| {
            try appendAttrBytes(out_buf, &pos, .software, sw);
        }
        if (self.error_code) |code| {
            const class: u8 = @intCast((code / 100) & 0x07);
            const number: u8 = @intCast(code % 100);
            const b = [4]u8{ 0, 0, class, number };
            try appendAttrBytes(out_buf, &pos, .error_code, &b);
        }

        if (integrity_key) |key| {
            if (pos + 24 > out_buf.len) return Error.BufferTooSmall;
            const len_with_mi: u16 = @intCast((pos - STUN_HEADER_LEN) + 24);
            std.mem.writeInt(u16, out_buf[2..4], len_with_mi, .big);
            var mac: [HmacSha1.mac_length]u8 = undefined;
            HmacSha1.create(&mac, out_buf[0..pos], key);
            try appendAttrBytes(out_buf, &pos, .message_integrity, &mac);
        }

        if (include_fingerprint) {
            if (pos + 8 > out_buf.len) return Error.BufferTooSmall;
            const len_with_fp: u16 = @intCast((pos - STUN_HEADER_LEN) + 8);
            std.mem.writeInt(u16, out_buf[2..4], len_with_fp, .big);
            const crc = Crc32IsoHdlc.hash(out_buf[0..pos]) ^ FINGERPRINT_XOR;
            try appendAttrU32(out_buf, &pos, .fingerprint, crc);
        }

        std.mem.writeInt(u16, out_buf[2..4], @intCast(pos - STUN_HEADER_LEN), .big);
        return out_buf[0..pos];
    }

    /// Parses a STUN/TURN packet from `packet`.
    pub fn decode(packet: []const u8) Error!StunMessage {
        if (packet.len < STUN_HEADER_LEN) return Error.InvalidStunPacket;
        // Top two bits of STUN message header must be 0b00 (RFC 8489 Section 5)
        if ((packet[0] & 0xC0) != 0) return Error.InvalidStunPacket;

        const msg_type_raw = std.mem.readInt(u16, packet[0..2], .big);
        const msg_len = std.mem.readInt(u16, packet[2..4], .big);
        const cookie = std.mem.readInt(u32, packet[4..8], .big);
        if (cookie != MAGIC_COOKIE) return Error.InvalidMagicCookie;
        if ((msg_len & 0x03) != 0) return Error.InvalidStunPacket;
        if (STUN_HEADER_LEN + msg_len > packet.len) return Error.InvalidStunPacket;

        var msg = StunMessage{
            .msg_type = @enumFromInt(msg_type_raw),
            .transaction_id = packet[8..20].*,
        };

        const end = STUN_HEADER_LEN + msg_len;
        var pos: usize = STUN_HEADER_LEN;
        while (pos < end) {
            if (pos + 4 > end) return Error.InvalidAttribute;
            const attr_type_raw = std.mem.readInt(u16, packet[pos..][0..2], .big);
            const attr_len = std.mem.readInt(u16, packet[pos + 2 ..][0..2], .big);
            const attr_start = pos;
            pos += 4;
            if (pos + attr_len > end) return Error.InvalidAttribute;
            const val = packet[pos .. pos + attr_len];
            const padded_len = (attr_len + 3) & ~@as(usize, 3);
            if (pos + padded_len > end) return Error.InvalidAttribute;
            pos += padded_len;

            const attr_type: AttrType = @enumFromInt(attr_type_raw);
            switch (attr_type) {
                .username => msg.username = val,
                .priority => {
                    if (val.len != 4) return Error.InvalidAttribute;
                    msg.priority = std.mem.readInt(u32, val[0..4], .big);
                },
                .use_candidate => {
                    msg.use_candidate = true;
                },
                .ice_controlling => {
                    if (val.len != 8) return Error.InvalidAttribute;
                    msg.ice_controlling = std.mem.readInt(u64, val[0..8], .big);
                },
                .ice_controlled => {
                    if (val.len != 8) return Error.InvalidAttribute;
                    msg.ice_controlled = std.mem.readInt(u64, val[0..8], .big);
                },
                .mapped_address => {
                    if (val.len < 8 or val[1] != 0x01) return Error.InvalidAttribute;
                    msg.mapped_address = .{
                        .port = std.mem.readInt(u16, val[2..4], .big),
                        .ip = val[4..8].*,
                    };
                },
                .xor_mapped_address => {
                    msg.xor_mapped_address = try decodeXorAddress(val);
                },
                .xor_peer_address => {
                    msg.xor_peer_address = try decodeXorAddress(val);
                },
                .xor_relayed_address => {
                    msg.xor_relayed_address = try decodeXorAddress(val);
                },
                .requested_transport => {
                    if (val.len < 4) return Error.InvalidAttribute;
                    msg.requested_transport = val[0];
                },
                .lifetime => {
                    if (val.len != 4) return Error.InvalidAttribute;
                    msg.lifetime = std.mem.readInt(u32, val[0..4], .big);
                },
                .channel_number => {
                    if (val.len < 4) return Error.InvalidAttribute;
                    msg.channel_number = std.mem.readInt(u16, val[0..2], .big);
                },
                .data => msg.data = val,
                .software => msg.software = val,
                .error_code => {
                    if (val.len < 4) return Error.InvalidAttribute;
                    const class: u16 = val[2] & 0x07;
                    const number: u16 = val[3];
                    msg.error_code = class * 100 + number;
                },
                .message_integrity => {
                    if (val.len != 20) return Error.InvalidAttribute;
                    msg.has_message_integrity = true;
                    msg.message_integrity = val[0..20].*;
                    msg.mi_offset = attr_start;
                },
                .fingerprint => {
                    if (val.len != 4) return Error.InvalidAttribute;
                    msg.has_fingerprint = true;
                    msg.fingerprint = std.mem.readInt(u32, val[0..4], .big);
                    msg.fp_offset = attr_start;
                },
                _ => {},
            }
        }

        return msg;
    }

    /// Verifies RFC 8489 Section 14.5 `MESSAGE-INTEGRITY` HMAC-SHA1 over `raw_packet`.
    pub fn verifyMessageIntegrity(self: StunMessage, raw_packet: []const u8, key: []const u8) Error!void {
        if (!self.has_message_integrity) return Error.IntegrityCheckFailed;
        const mi_pos = self.mi_offset orelse return Error.IntegrityCheckFailed;
        if (mi_pos < STUN_HEADER_LEN or mi_pos + 24 > raw_packet.len) return Error.IntegrityCheckFailed;

        var adj_header: [STUN_HEADER_LEN]u8 = raw_packet[0..STUN_HEADER_LEN].*;
        const len_with_mi: u16 = @intCast((mi_pos - STUN_HEADER_LEN) + 24);
        std.mem.writeInt(u16, adj_header[2..4], len_with_mi, .big);

        var hmac = HmacSha1.init(key);
        hmac.update(&adj_header);
        hmac.update(raw_packet[STUN_HEADER_LEN..mi_pos]);
        var expected: [HmacSha1.mac_length]u8 = undefined;
        hmac.final(&expected);

        if (!std.crypto.timing_safe.eql([20]u8, expected, self.message_integrity)) {
            return Error.IntegrityCheckFailed;
        }
    }

    /// Verifies RFC 8489 Section 14.7 `FINGERPRINT` CRC-32 XOR `0x5354554e` over `raw_packet`.
    pub fn verifyFingerprint(self: StunMessage, raw_packet: []const u8) Error!void {
        if (!self.has_fingerprint) return Error.FingerprintCheckFailed;
        const fp_pos = self.fp_offset orelse return Error.FingerprintCheckFailed;
        if (fp_pos < STUN_HEADER_LEN or fp_pos + 8 > raw_packet.len) return Error.FingerprintCheckFailed;

        var adj_header: [STUN_HEADER_LEN]u8 = raw_packet[0..STUN_HEADER_LEN].*;
        const len_with_fp: u16 = @intCast((fp_pos - STUN_HEADER_LEN) + 8);
        std.mem.writeInt(u16, adj_header[2..4], len_with_fp, .big);

        var crc = Crc32IsoHdlc.init();
        crc.update(&adj_header);
        crc.update(raw_packet[STUN_HEADER_LEN..fp_pos]);
        const computed = crc.final() ^ FINGERPRINT_XOR;

        if (computed != self.fingerprint) {
            return Error.FingerprintCheckFailed;
        }
    }
};

/// RFC 8656 Section 12 TURN ChannelData 4-byte framing (`0x4000..0x7FFF`).
pub const TurnChannelData = struct {
    pub const MIN_CHANNEL: u16 = 0x4000;
    pub const MAX_CHANNEL: u16 = 0x7FFF;
    pub const HEADER_LEN: usize = 4;

    channel_number: u16,
    payload: []const u8,

    pub fn isChannelData(packet: []const u8) bool {
        if (packet.len < HEADER_LEN) return false;
        const ch = std.mem.readInt(u16, packet[0..2], .big);
        return ch >= MIN_CHANNEL and ch <= MAX_CHANNEL;
    }

    pub fn encode(self: TurnChannelData, out_buf: []u8) Error![]const u8 {
        if (self.channel_number < MIN_CHANNEL or self.channel_number > MAX_CHANNEL) {
            return Error.InvalidTurnChannel;
        }
        if (self.payload.len > std.math.maxInt(u16)) return Error.BufferTooSmall;
        const total_len = HEADER_LEN + self.payload.len;
        if (out_buf.len < total_len) return Error.BufferTooSmall;

        std.mem.writeInt(u16, out_buf[0..2], self.channel_number, .big);
        std.mem.writeInt(u16, out_buf[2..4], @intCast(self.payload.len), .big);
        @memcpy(out_buf[4..total_len], self.payload);
        return out_buf[0..total_len];
    }

    pub fn decode(packet: []const u8) Error!TurnChannelData {
        if (packet.len < HEADER_LEN) return Error.InvalidTurnChannel;
        const ch = std.mem.readInt(u16, packet[0..2], .big);
        if (ch < MIN_CHANNEL or ch > MAX_CHANNEL) return Error.InvalidTurnChannel;
        const length = std.mem.readInt(u16, packet[2..4], .big);
        if (HEADER_LEN + length > packet.len) return Error.InvalidTurnChannel;
        return .{
            .channel_number = ch,
            .payload = packet[4 .. 4 + length],
        };
    }
};

// ============================================================================
// ICE Agent, Port Range Gathering & Connectivity Check State Machine (RFC 8445)
// ============================================================================

pub const Role = enum {
    controlling,
    controlled,
};

pub const CheckState = enum {
    frozen,
    waiting,
    in_progress,
    succeeded,
    failed,
};

pub const IceConnectionState = enum {
    new,
    checking,
    connected,
    completed,
    failed,
    disconnected,
    closed,
};

pub const CandidatePair = struct {
    local: ParsedCandidate,
    remote: ParsedCandidate,
    pair_priority: u64,
    state: CheckState = .frozen,
    nominated: bool = false,

    /// RFC 8445 Section 6.1.2.3:
    /// pair priority = 2^32 * MIN(G, D) + 2 * MAX(G, D) + (G > D ? 1 : 0)
    pub fn computePairPriority(controlling_prio: u32, controlled_prio: u32, is_controlling: bool) u64 {
        const g: u64 = if (is_controlling) controlling_prio else controlled_prio;
        const d: u64 = if (is_controlling) controlled_prio else controlling_prio;
        return (@min(g, d) << 32) + (2 * @max(g, d)) + @as(u64, if (g > d) 1 else 0);
    }
};

pub const parseCandidate = ParsedCandidate.parse;

pub const AgentConfig = struct {
    role: Role = .controlling,
    local_ufrag: []const u8,
    local_pwd: []const u8,
    remote_ufrag: []const u8 = "",
    remote_pwd: []const u8 = "",
    tie_breaker: u64 = 0x0102030405060708,
    port_range: PortRange = .{},
    host_ipv4: [4]u8 = .{ 127, 0, 0, 1 },
    host_ipv6_str: ?[]const u8 = null,
    host_ipv6_text: ?[]const u8 = null,
};

pub const UdpRecvResult = struct {
    len: usize,
    data: []const u8,
    src_ip: [4]u8,
    src_port: u16,
};

pub const Agent = struct {
    allocator: Allocator,
    role: Role,
    local_ufrag: []const u8,
    local_pwd: []const u8,
    remote_ufrag: []const u8,
    remote_pwd: []const u8,
    tie_breaker: u64,
    port_range: PortRange,
    host_ipv4: [4]u8 = .{ 127, 0, 0, 1 },
    host_ipv6_str: ?[]const u8 = null,
    host_ipv6_text: ?[]const u8 = null,
    state: IceConnectionState = .new,
    local_candidates: std.ArrayList(ParsedCandidate) = .empty,
    remote_candidates: std.ArrayList(ParsedCandidate) = .empty,
    checklist: std.ArrayList(CandidatePair) = .empty,
    nominated_pair: ?CandidatePair = null,
    socket_fd: ?SocketFd = null,
    bound_port: ?u16 = null,
    owned_strings: std.ArrayList([]u8) = .empty,

    pub fn init(allocator: Allocator, config: AgentConfig) Agent {
        const ipv6 = config.host_ipv6_str orelse config.host_ipv6_text;
        return .{
            .allocator = allocator,
            .role = config.role,
            .local_ufrag = config.local_ufrag,
            .local_pwd = config.local_pwd,
            .remote_ufrag = config.remote_ufrag,
            .remote_pwd = config.remote_pwd,
            .tie_breaker = config.tie_breaker,
            .port_range = config.port_range,
            .host_ipv4 = config.host_ipv4,
            .host_ipv6_str = ipv6,
            .host_ipv6_text = ipv6,
        };
    }

    pub fn deinit(self: *Agent) void {
        if (self.socket_fd) |fd| {
            closeUdpSocket(fd);
            self.socket_fd = null;
        }
        for (self.owned_strings.items) |s| {
            self.allocator.free(s);
        }
        self.owned_strings.deinit(self.allocator);
        self.local_candidates.deinit(self.allocator);
        self.remote_candidates.deinit(self.allocator);
        self.checklist.deinit(self.allocator);
        self.state = .closed;
    }

    pub fn setRemoteCredentials(self: *Agent, ufrag: []const u8, pwd: []const u8) void {
        self.remote_ufrag = ufrag;
        self.remote_pwd = pwd;
    }

    pub fn setHostAddresses(self: *Agent, host_ipv4: [4]u8, host_ipv6: ?[]const u8) Error!void {
        self.host_ipv4 = host_ipv4;
        if (host_ipv6) |v6| {
            if (v6.len > 0) {
                const copy = try self.allocator.dupe(u8, v6);
                errdefer self.allocator.free(copy);
                try self.owned_strings.append(self.allocator, copy);
                self.host_ipv6_str = copy;
                self.host_ipv6_text = copy;
            } else {
                self.host_ipv6_str = null;
                self.host_ipv6_text = null;
            }
        } else {
            self.host_ipv6_str = null;
            self.host_ipv6_text = null;
        }
        for (self.local_candidates.items) |*lc| {
            lc.address = host_ipv4;
        }
        try self.rebuildChecklist();
    }

    /// Binds a real UDP socket within `port_range.min_port..port_range.max_port`
    /// (trying `host_ipv4` first, falling back to `127.0.0.1` if `EADDRNOTAVAIL` in a test,
    /// and falling back to an ephemeral port if all ports in range are busy) and records a
    /// `host` candidate advertising `self.host_ipv4:<port>`.
    pub fn gatherHostCandidates(self: *Agent) Error!void {
        if (self.socket_fd == null) {
            const fd = try createUdpSocket();
            errdefer closeUdpSocket(fd);

            var bound = false;
            var actual_port: u16 = 0;
            var bind_ip = self.host_ipv4;

            if (self.port_range.min_port <= self.port_range.max_port and self.port_range.min_port > 0) {
                var p: u32 = self.port_range.min_port;
                while (p <= self.port_range.max_port) : (p += 1) {
                    const port_u16: u16 = @intCast(p);
                    switch (bindUdpSocket(fd, bind_ip, port_u16)) {
                        .ok => {
                            bound = true;
                            actual_port = port_u16;
                            break;
                        },
                        .addr_not_avail => {
                            bind_ip = .{ 127, 0, 0, 1 };
                            if (bindUdpSocket(fd, bind_ip, port_u16) == .ok) {
                                bound = true;
                                actual_port = port_u16;
                                break;
                            }
                        },
                        .failed => {},
                    }
                }
            }

            if (!bound) {
                switch (bindUdpSocket(fd, bind_ip, 0)) {
                    .ok => {},
                    .addr_not_avail => {
                        bind_ip = .{ 127, 0, 0, 1 };
                        if (bindUdpSocket(fd, bind_ip, 0) != .ok) return Error.BindFailed;
                    },
                    .failed => return Error.BindFailed,
                }
                actual_port = try getUdpBoundPort(fd);
            }

            self.socket_fd = fd;
            self.bound_port = actual_port;
        }

        const port = self.bound_port orelse return Error.BindFailed;
        self.local_candidates.clearRetainingCapacity();
        const cand = ParsedCandidate{
            .foundation = "1",
            .component_id = 1,
            .transport = .udp,
            .priority = ParsedCandidate.computePriority(.host, 65535, 1),
            .address = self.host_ipv4,
            .port = port,
            .cand_type = .host,
            .generation = 0,
            .ufrag = self.local_ufrag,
        };
        try self.local_candidates.append(self.allocator, cand);
        try self.rebuildChecklist();
    }

    /// Parses a remote `a=candidate:...` line and forms sorted `CandidatePair`s in `checklist`.
    pub fn addRemoteCandidate(self: *Agent, cand_sdp: []const u8) Error!void {
        var parsed = try ParsedCandidate.parse(cand_sdp);

        const foundation_copy = try self.allocator.dupe(u8, parsed.foundation);
        errdefer self.allocator.free(foundation_copy);
        try self.owned_strings.append(self.allocator, foundation_copy);
        parsed.foundation = foundation_copy;

        if (parsed.ufrag) |uf| {
            const ufrag_copy = try self.allocator.dupe(u8, uf);
            errdefer self.allocator.free(ufrag_copy);
            try self.owned_strings.append(self.allocator, ufrag_copy);
            parsed.ufrag = ufrag_copy;
        }

        try self.remote_candidates.append(self.allocator, parsed);
        try self.rebuildChecklist();
    }

    fn rebuildChecklist(self: *Agent) Error!void {
        self.checklist.clearRetainingCapacity();
        const is_ctrl = (self.role == .controlling);
        for (self.local_candidates.items) |lc| {
            for (self.remote_candidates.items) |rc| {
                if (lc.component_id == rc.component_id and lc.transport == rc.transport) {
                    const pair_prio = CandidatePair.computePairPriority(
                        lc.priority,
                        rc.priority,
                        is_ctrl,
                    );
                    try self.checklist.append(self.allocator, .{
                        .local = lc,
                        .remote = rc,
                        .pair_priority = pair_prio,
                        .state = .waiting,
                        .nominated = false,
                    });
                }
            }
        }

        std.mem.sort(CandidatePair, self.checklist.items, {}, struct {
            fn lessThan(_: void, a: CandidatePair, b: CandidatePair) bool {
                return a.pair_priority > b.pair_priority;
            }
        }.lessThan);
    }

    /// Builds a signed + fingerprinted STUN Binding Request for the candidate pair at `pair_index`.
    pub fn buildConnectivityCheckRequest(
        self: *Agent,
        pair_index: usize,
        tx_id: [12]u8,
        use_candidate: bool,
        out_buf: []u8,
    ) Error![]const u8 {
        if (pair_index >= self.checklist.items.len) return Error.PairIndexOutOfBounds;
        var pair = &self.checklist.items[pair_index];
        pair.state = .in_progress;
        if (self.state == .new) {
            self.state = .checking;
        }

        var username_buf: [256]u8 = undefined;
        const username = std.fmt.bufPrint(
            &username_buf,
            "{s}:{s}",
            .{ self.remote_ufrag, self.local_ufrag },
        ) catch return Error.BufferTooSmall;

        const prflx_prio = ParsedCandidate.computePriority(.prflx, 65535, pair.local.component_id);
        const msg = StunMessage{
            .msg_type = .binding_request,
            .transaction_id = tx_id,
            .username = username,
            .priority = prflx_prio,
            .use_candidate = use_candidate and (self.role == .controlling),
            .ice_controlling = if (self.role == .controlling) self.tie_breaker else null,
            .ice_controlled = if (self.role == .controlled) self.tie_breaker else null,
        };

        return msg.encode(out_buf, self.remote_pwd, true);
    }

    /// Processes an incoming STUN packet (Binding Request or Binding Response) and updates
    /// the ICE checklist, nomination state, and connection state.
    pub fn handleStunPacket(
        self: *Agent,
        packet: []const u8,
        src_ip: [4]u8,
        src_port: u16,
        out_response_buf: []u8,
    ) Error!?[]const u8 {
        const msg = try StunMessage.decode(packet);

        switch (msg.msg_type) {
            .binding_request => {
                var expected_user_buf: [256]u8 = undefined;
                const expected_user = std.fmt.bufPrint(
                    &expected_user_buf,
                    "{s}:{s}",
                    .{ self.local_ufrag, self.remote_ufrag },
                ) catch return Error.BufferTooSmall;

                const actual_user = msg.username orelse return Error.UsernameMismatch;
                if (!std.mem.eql(u8, actual_user, expected_user)) {
                    return Error.UsernameMismatch;
                }

                try msg.verifyMessageIntegrity(packet, self.local_pwd);
                try msg.verifyFingerprint(packet);

                if (self.state == .new) {
                    self.state = .checking;
                }

                for (self.checklist.items) |*pair| {
                    if (std.mem.eql(u8, &pair.remote.address, &src_ip) and pair.remote.port == src_port) {
                        pair.state = .succeeded;
                        if (msg.use_candidate or self.role == .controlled) {
                            pair.nominated = true;
                            self.nominated_pair = pair.*;
                            self.state = .connected;
                        }
                        break;
                    }
                }

                const resp = StunMessage{
                    .msg_type = .binding_response,
                    .transaction_id = msg.transaction_id,
                    .xor_mapped_address = .{
                        .ip = src_ip,
                        .port = src_port,
                    },
                };
                const encoded = try resp.encode(out_response_buf, self.local_pwd, true);
                return encoded;
            },
            .binding_response => {
                try msg.verifyMessageIntegrity(packet, self.remote_pwd);
                try msg.verifyFingerprint(packet);
                _ = msg.xor_mapped_address orelse return Error.InvalidAttribute;

                for (self.checklist.items) |*pair| {
                    if (std.mem.eql(u8, &pair.remote.address, &src_ip) and pair.remote.port == src_port) {
                        pair.state = .succeeded;
                        if (self.role == .controlling) {
                            pair.nominated = true;
                            self.nominated_pair = pair.*;
                            self.state = .connected;
                        } else if (self.state == .checking) {
                            self.state = .connected;
                        }
                        break;
                    }
                }
                return null;
            },
            else => return null,
        }
    }

    /// Sends a raw UDP datagram via the agent's bound socket.
    /// Accepts either `(packet, dst_ip, dst_port)` or `(dst_ip, dst_port, packet)`.
    pub fn sendUdp(self: *const Agent, arg1: anytype, arg2: anytype, arg3: anytype) Error!void {
        if (@TypeOf(arg1) == [4]u8) {
            return self.sendUdpTo(arg1, arg2, arg3);
        } else {
            return self.sendUdpTo(arg2, arg3, arg1);
        }
    }

    pub fn sendUdpTo(self: *const Agent, dst_ip: [4]u8, dst_port: u16, packet: []const u8) Error!void {
        const fd = self.socket_fd orelse return Error.NoSocketBound;
        const ip_be: u32 = @bitCast(dst_ip);
        const port_be: u16 = std.mem.nativeToBig(u16, dst_port);
        if (comptime builtin.os.tag == .linux) {
            var dst_addr: linux.sockaddr.in = .{
                .family = linux.AF.INET,
                .port = port_be,
                .addr = ip_be,
            };
            const rc = linux.sendto(
                fd,
                packet.ptr,
                packet.len,
                0,
                @ptrCast(&dst_addr),
                @sizeOf(linux.sockaddr.in),
            );
            if (linux.errno(rc) != .SUCCESS) return Error.SendFailed;
        } else {
            var dst_addr: darwin_c.SockAddrIn = .{
                .port = port_be,
                .addr = ip_be,
            };
            const rc = darwin_c.sendto(
                fd,
                packet.ptr,
                packet.len,
                0,
                @ptrCast(&dst_addr),
                @sizeOf(darwin_c.SockAddrIn),
            );
            if (rc < 0) return Error.SendFailed;
        }
    }

    /// Receives a raw UDP datagram from the agent's bound socket (blocking).
    pub fn recvUdp(
        self: *const Agent,
        out_buf: []u8,
    ) Error!UdpRecvResult {
        const fd = self.socket_fd orelse return Error.NoSocketBound;
        if (comptime builtin.os.tag == .linux) {
            var src_addr: linux.sockaddr.in = undefined;
            var addr_len: linux.socklen_t = @sizeOf(linux.sockaddr.in);
            const rc = linux.recvfrom(
                fd,
                out_buf.ptr,
                out_buf.len,
                0,
                @ptrCast(&src_addr),
                &addr_len,
            );
            if (linux.errno(rc) != .SUCCESS) return Error.RecvFailed;
            const n: usize = @intCast(rc);
            const src_ip: [4]u8 = @bitCast(src_addr.addr);
            const src_port = std.mem.bigToNative(u16, src_addr.port);
            return .{
                .len = n,
                .data = out_buf[0..n],
                .src_ip = src_ip,
                .src_port = src_port,
            };
        } else {
            var src_addr: darwin_c.SockAddrIn = undefined;
            var addr_len: u32 = @sizeOf(darwin_c.SockAddrIn);
            const rc = darwin_c.recvfrom(
                fd,
                out_buf.ptr,
                out_buf.len,
                0,
                @ptrCast(&src_addr),
                &addr_len,
            );
            if (rc < 0) return Error.RecvFailed;
            const n: usize = @intCast(rc);
            const src_ip: [4]u8 = @bitCast(src_addr.addr);
            const src_port = std.mem.bigToNative(u16, src_addr.port);
            return .{
                .len = n,
                .data = out_buf[0..n],
                .src_ip = src_ip,
                .src_port = src_port,
            };
        }
    }

    /// Receives a raw UDP datagram without blocking (`MSG_DONTWAIT`).
    /// Returns `null` when no datagram is currently queued (`EAGAIN`/`EWOULDBLOCK`).
    pub fn recvUdpNonBlocking(
        self: *const Agent,
        out_buf: []u8,
    ) Error!?UdpRecvResult {
        const fd = self.socket_fd orelse return Error.NoSocketBound;
        if (comptime builtin.os.tag == .linux) {
            var src_addr: linux.sockaddr.in = undefined;
            var addr_len: linux.socklen_t = @sizeOf(linux.sockaddr.in);
            const rc = linux.recvfrom(
                fd,
                out_buf.ptr,
                out_buf.len,
                linux.MSG.DONTWAIT,
                @ptrCast(&src_addr),
                &addr_len,
            );
            switch (linux.errno(rc)) {
                .SUCCESS => {},
                .AGAIN => return null,
                else => return Error.RecvFailed,
            }
            const n: usize = @intCast(rc);
            const src_ip: [4]u8 = @bitCast(src_addr.addr);
            const src_port = std.mem.bigToNative(u16, src_addr.port);
            return .{
                .len = n,
                .data = out_buf[0..n],
                .src_ip = src_ip,
                .src_port = src_port,
            };
        } else {
            var src_addr: darwin_c.SockAddrIn = undefined;
            var addr_len: u32 = @sizeOf(darwin_c.SockAddrIn);
            var rc = darwin_c.recvfrom(
                fd,
                out_buf.ptr,
                out_buf.len,
                darwin_c.MSG_DONTWAIT,
                @ptrCast(&src_addr),
                &addr_len,
            );
            if (rc < 0) {
                const err = darwin_c.__error().*;
                if (err != darwin_c.EAGAIN) return Error.RecvFailed;
                var pfd = [_]darwin_c.PollFd{.{
                    .fd = fd,
                    .events = darwin_c.POLLIN,
                    .revents = 0,
                }};
                if (darwin_c.poll(&pfd, 1, 5) <= 0) return null;
                addr_len = @sizeOf(darwin_c.SockAddrIn);
                rc = darwin_c.recvfrom(
                    fd,
                    out_buf.ptr,
                    out_buf.len,
                    darwin_c.MSG_DONTWAIT,
                    @ptrCast(&src_addr),
                    &addr_len,
                );
                if (rc < 0) {
                    const err2 = darwin_c.__error().*;
                    if (err2 == darwin_c.EAGAIN) return null;
                    return Error.RecvFailed;
                }
            }
            const n: usize = @intCast(rc);
            const src_ip: [4]u8 = @bitCast(src_addr.addr);
            const src_port = std.mem.bigToNative(u16, src_addr.port);
            return .{
                .len = n,
                .data = out_buf[0..n],
                .src_ip = src_ip,
                .src_port = src_port,
            };
        }
    }
};

// ============================================================================
// Unit Tests
// ============================================================================

test "ParsedCandidate parse, formatSdp round-trip, and RFC 8445 priorities" {
    const host_prio = ParsedCandidate.computePriority(.host, 65535, 1);
    try std.testing.expectEqual(@as(u32, 2130706431), host_prio);

    const srflx_prio = ParsedCandidate.computePriority(.srflx, 65535, 1);
    try std.testing.expectEqual(@as(u32, 1694498815), srflx_prio);

    const raw_sdp = "a=candidate:842163049 1 udp 1694498815 203.0.113.5 54321 typ srflx raddr 192.168.1.10 rport 15550 generation 0 ufrag abcd";
    const parsed = try ParsedCandidate.parse(raw_sdp);
    try std.testing.expectEqualStrings("842163049", parsed.foundation);
    try std.testing.expectEqual(@as(u8, 1), parsed.component_id);
    try std.testing.expectEqual(TransportProtocol.udp, parsed.transport);
    try std.testing.expectEqual(@as(u32, 1694498815), parsed.priority);
    try std.testing.expectEqual([4]u8{ 203, 0, 113, 5 }, parsed.address);
    try std.testing.expectEqual(@as(u16, 54321), parsed.port);
    try std.testing.expectEqual(CandidateType.srflx, parsed.cand_type);
    try std.testing.expectEqual([4]u8{ 192, 168, 1, 10 }, parsed.raddr.?);
    try std.testing.expectEqual(@as(u16, 15550), parsed.rport.?);
    try std.testing.expectEqual(@as(u32, 0), parsed.generation);
    try std.testing.expectEqualStrings("abcd", parsed.ufrag.?);

    var sdp_buf: [256]u8 = undefined;
    const formatted = try parsed.formatSdp(&sdp_buf);
    const reparsed = try ParsedCandidate.parse(formatted);
    try std.testing.expectEqualStrings(parsed.foundation, reparsed.foundation);
    try std.testing.expectEqual(parsed.priority, reparsed.priority);
    try std.testing.expectEqual(parsed.address, reparsed.address);
    try std.testing.expectEqual(parsed.port, reparsed.port);
    try std.testing.expectEqual(parsed.cand_type, reparsed.cand_type);
    try std.testing.expectEqual(parsed.raddr.?, reparsed.raddr.?);
    try std.testing.expectEqual(parsed.rport.?, reparsed.rport.?);
    try std.testing.expectEqualStrings(parsed.ufrag.?, reparsed.ufrag.?);

    // Pair priority symmetry between controlling and controlled roles
    const p_ctrl = CandidatePair.computePairPriority(host_prio, srflx_prio, true);
    const p_ctld = CandidatePair.computePairPriority(srflx_prio, host_prio, false);
    try std.testing.expectEqual(p_ctrl, p_ctld);
}

test "STUN Binding Request/Response, XOR-MAPPED-ADDRESS, MESSAGE-INTEGRITY, and FINGERPRINT" {
    const tx_id = [12]u8{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    const req = StunMessage{
        .msg_type = .binding_request,
        .transaction_id = tx_id,
        .username = "remUfrag:locUfrag",
        .priority = 1862270975,
        .use_candidate = true,
        .ice_controlling = 0xCAFEBABE12345678,
    };

    var pkt_buf: [256]u8 = undefined;
    const encoded_req = try req.encode(&pkt_buf, "secret_ice_password_1234", true);

    const decoded_req = try StunMessage.decode(encoded_req);
    try std.testing.expectEqual(MessageType.binding_request, decoded_req.msg_type);
    try std.testing.expectEqual(tx_id, decoded_req.transaction_id);
    try std.testing.expectEqualStrings("remUfrag:locUfrag", decoded_req.username.?);
    try std.testing.expectEqual(@as(u32, 1862270975), decoded_req.priority.?);
    try std.testing.expect(decoded_req.use_candidate);
    try std.testing.expectEqual(@as(u64, 0xCAFEBABE12345678), decoded_req.ice_controlling.?);
    try std.testing.expect(decoded_req.has_message_integrity);
    try std.testing.expect(decoded_req.has_fingerprint);

    try decoded_req.verifyMessageIntegrity(encoded_req, "secret_ice_password_1234");
    try decoded_req.verifyFingerprint(encoded_req);

    // Wrong password must fail MESSAGE-INTEGRITY
    try std.testing.expectError(
        Error.IntegrityCheckFailed,
        decoded_req.verifyMessageIntegrity(encoded_req, "wrong_password"),
    );

    // Tampered payload byte must fail both MESSAGE-INTEGRITY and FINGERPRINT
    var tampered: [256]u8 = undefined;
    @memcpy(tampered[0..encoded_req.len], encoded_req);
    tampered[25] ^= 0xFF;
    const decoded_tampered = try StunMessage.decode(tampered[0..encoded_req.len]);
    try std.testing.expectError(
        Error.IntegrityCheckFailed,
        decoded_tampered.verifyMessageIntegrity(tampered[0..encoded_req.len], "secret_ice_password_1234"),
    );
    try std.testing.expectError(
        Error.FingerprintCheckFailed,
        decoded_tampered.verifyFingerprint(tampered[0..encoded_req.len]),
    );

    // STUN Binding Response with XOR-MAPPED-ADDRESS
    const resp = StunMessage{
        .msg_type = .binding_response,
        .transaction_id = tx_id,
        .xor_mapped_address = .{
            .ip = .{ 192, 0, 2, 44 },
            .port = 34780,
        },
    };
    var resp_buf: [256]u8 = undefined;
    const encoded_resp = try resp.encode(&resp_buf, "secret_ice_password_1234", true);
    const decoded_resp = try StunMessage.decode(encoded_resp);
    try decoded_resp.verifyMessageIntegrity(encoded_resp, "secret_ice_password_1234");
    try decoded_resp.verifyFingerprint(encoded_resp);
    try std.testing.expectEqual([4]u8{ 192, 0, 2, 44 }, decoded_resp.xor_mapped_address.?.ip);
    try std.testing.expectEqual(@as(u16, 34780), decoded_resp.xor_mapped_address.?.port);
}

test "TURN ALLOCATE attributes and TURN ChannelData 0x4000 framing" {
    const tx_id = [12]u8{ 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2 };
    const alloc_req = StunMessage{
        .msg_type = .allocate_request,
        .transaction_id = tx_id,
        .requested_transport = 17, // UDP
        .lifetime = 600,
    };

    var buf: [256]u8 = undefined;
    const enc_req = try alloc_req.encode(&buf, "turn_key", true);
    const dec_req = try StunMessage.decode(enc_req);
    try std.testing.expectEqual(MessageType.allocate_request, dec_req.msg_type);
    try std.testing.expectEqual(@as(u8, 17), dec_req.requested_transport.?);
    try std.testing.expectEqual(@as(u32, 600), dec_req.lifetime.?);

    const alloc_resp = StunMessage{
        .msg_type = .allocate_response,
        .transaction_id = tx_id,
        .xor_relayed_address = .{ .ip = .{ 198, 51, 100, 7 }, .port = 49152 },
        .xor_mapped_address = .{ .ip = .{ 203, 0, 113, 9 }, .port = 15551 },
        .lifetime = 600,
    };
    const enc_resp = try alloc_resp.encode(&buf, "turn_key", true);
    const dec_resp = try StunMessage.decode(enc_resp);
    try std.testing.expectEqual(MessageType.allocate_response, dec_resp.msg_type);
    try std.testing.expectEqual([4]u8{ 198, 51, 100, 7 }, dec_resp.xor_relayed_address.?.ip);
    try std.testing.expectEqual(@as(u16, 49152), dec_resp.xor_relayed_address.?.port);

    // TURN ChannelData framing
    const payload = "DTLS-SRTP-MEDIA-PACKET";
    const ch_frame = TurnChannelData{
        .channel_number = 0x4000,
        .payload = payload,
    };
    var ch_buf: [128]u8 = undefined;
    const enc_ch = try ch_frame.encode(&ch_buf);
    try std.testing.expect(TurnChannelData.isChannelData(enc_ch));
    const dec_ch = try TurnChannelData.decode(enc_ch);
    try std.testing.expectEqual(@as(u16, 0x4000), dec_ch.channel_number);
    try std.testing.expectEqualStrings(payload, dec_ch.payload);
}

test "Two-agent ICE loopback connectivity check, PortRange gathering, and nomination" {
    const allocator = std.testing.allocator;

    var agent_a = Agent.init(allocator, .{
        .role = .controlling,
        .local_ufrag = "ufragA",
        .local_pwd = "passwordA_0123456789",
        .remote_ufrag = "ufragB",
        .remote_pwd = "passwordB_0123456789",
        .tie_breaker = 0x8877665544332211,
        .port_range = .{ .min_port = 15550, .max_port = 15599 },
    });
    defer agent_a.deinit();

    var agent_b = Agent.init(allocator, .{
        .role = .controlled,
        .local_ufrag = "ufragB",
        .local_pwd = "passwordB_0123456789",
        .remote_ufrag = "ufragA",
        .remote_pwd = "passwordA_0123456789",
        .tie_breaker = 0x1122334455667788,
        .port_range = .{ .min_port = 15550, .max_port = 15599 },
    });
    defer agent_b.deinit();

    try agent_a.gatherHostCandidates();
    try agent_b.gatherHostCandidates();

    try std.testing.expectEqual(@as(usize, 1), agent_a.local_candidates.items.len);
    try std.testing.expectEqual(@as(usize, 1), agent_b.local_candidates.items.len);
    try std.testing.expect(agent_a.bound_port.? != agent_b.bound_port.?);

    var sdp_a_buf: [256]u8 = undefined;
    var sdp_b_buf: [256]u8 = undefined;
    const sdp_a = try agent_a.local_candidates.items[0].formatSdp(&sdp_a_buf);
    const sdp_b = try agent_b.local_candidates.items[0].formatSdp(&sdp_b_buf);

    try agent_a.addRemoteCandidate(sdp_b);
    try agent_b.addRemoteCandidate(sdp_a);

    try std.testing.expectEqual(@as(usize, 1), agent_a.checklist.items.len);
    try std.testing.expectEqual(@as(usize, 1), agent_b.checklist.items.len);
    try std.testing.expectEqual(
        agent_a.checklist.items[0].pair_priority,
        agent_b.checklist.items[0].pair_priority,
    );

    // Agent A builds and sends STUN Binding Request with USE-CANDIDATE over loopback UDP to Agent B
    const tx_id = [12]u8{ 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC };
    var req_buf: [256]u8 = undefined;
    const req_pkt = try agent_a.buildConnectivityCheckRequest(0, tx_id, true, &req_buf);
    try std.testing.expectEqual(IceConnectionState.checking, agent_a.state);

    const b_cand = agent_b.local_candidates.items[0];
    try agent_a.sendUdp(req_pkt, b_cand.address, b_cand.port);

    // Agent B receives the STUN Binding Request on its UDP socket and processes it
    var rx_b_buf: [256]u8 = undefined;
    const rx_b = try agent_b.recvUdp(&rx_b_buf);
    var resp_buf: [256]u8 = undefined;
    const maybe_resp = try agent_b.handleStunPacket(rx_b.data, rx_b.src_ip, rx_b.src_port, &resp_buf);
    try std.testing.expect(maybe_resp != null);
    try std.testing.expectEqual(IceConnectionState.connected, agent_b.state);
    try std.testing.expect(agent_b.nominated_pair != null);

    // Agent B sends the STUN Binding Success Response back to Agent A over UDP
    try agent_b.sendUdp(maybe_resp.?, rx_b.src_ip, rx_b.src_port);

    // Agent A receives the STUN Binding Response and transitions to connected with nominated pair
    var rx_a_buf: [256]u8 = undefined;
    const rx_a = try agent_a.recvUdp(&rx_a_buf);
    var unused_buf: [256]u8 = undefined;
    const follow_up = try agent_a.handleStunPacket(rx_a.data, rx_a.src_ip, rx_a.src_port, &unused_buf);
    try std.testing.expect(follow_up == null);
    try std.testing.expectEqual(IceConnectionState.connected, agent_a.state);
    try std.testing.expect(agent_a.nominated_pair != null);
    try std.testing.expectEqual(CheckState.succeeded, agent_a.checklist.items[0].state);
    try std.testing.expect(agent_a.checklist.items[0].nominated);
}

test "Explicit host_ipv4 candidate gathering within 15550..15599 and non-blocking UDP" {
    const allocator = std.testing.allocator;
    const host_ip = try parseIpv4("198.51.100.1");
    try std.testing.expectEqual([4]u8{ 198, 51, 100, 1 }, host_ip);

    var agent = Agent.init(allocator, .{
        .role = .controlling,
        .local_ufrag = "cfUfrag",
        .local_pwd = "cfPassword0123456789",
        .port_range = .{ .min_port = 15550, .max_port = 15599 },
        .host_ipv4 = host_ip,
        .host_ipv6_str = "fd00::1",
    });
    defer agent.deinit();

    try agent.gatherHostCandidates();
    try std.testing.expectEqual(@as(usize, 1), agent.local_candidates.items.len);
    const cand = agent.local_candidates.items[0];
    try std.testing.expectEqual([4]u8{ 198, 51, 100, 1 }, cand.address);
    try std.testing.expect(cand.port >= 15550 and cand.port <= 15599);
    try std.testing.expectEqualStrings("fd00::1", agent.host_ipv6_str.?);

    var empty_buf: [64]u8 = undefined;
    const maybe_pkt = try agent.recvUdpNonBlocking(&empty_buf);
    try std.testing.expect(maybe_pkt == null);
}

