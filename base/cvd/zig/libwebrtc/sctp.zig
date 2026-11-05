const std = @import("std");
const c_types = @import("c_types.zig");
const c = c_types.c;

pub const Error = error{
    InitializationFailed,
    InvalidArgument,
    InvalidState,
    InternalError,
    Unsupported,
    BufferTooSmall,
    InvalidPacket,
    InvalidChecksum,
    InvalidChunk,
    InvalidParameter,
    InvalidDcepMessage,
    StreamNotFound,
    TooManyChannels,
    OutOfMemory,
};

pub const default_sctp_port: u16 = 5000;
pub const default_a_rwnd: u32 = 131072;
pub const default_max_payload_size: usize = 1200;
pub const default_state_cookie = "CF-ZIG-SCTP-COOKIE-RFC4960";

/// SCTP Chunk Types (RFC 4960 / RFC 9260 / RFC 6525 / RFC 3758).
pub const ChunkType = enum(u8) {
    data = 0,
    init = 1,
    init_ack = 2,
    sack = 3,
    heartbeat = 4,
    heartbeat_ack = 5,
    abort = 6,
    shutdown = 7,
    shutdown_ack = 8,
    err = 9,
    cookie_echo = 10,
    cookie_ack = 11,
    re_config = 130,
    forward_tsn = 192,
    _,
};

/// WebRTC SCTP Payload Protocol Identifiers (RFC 8831 / RFC 8832).
pub const Ppid = enum(u32) {
    dcep = 50,
    webrtc_string = 51,
    webrtc_binary_partial = 52,
    webrtc_binary = 53,
    webrtc_string_partial = 54,
    webrtc_string_empty = 56,
    webrtc_binary_empty = 57,
    _,
};

/// WebRTC DataChannel Establishment Protocol (DCEP, RFC 8832) message types.
pub const DcepMessageType = enum(u8) {
    ack = 0x02,
    open = 0x03,
};

/// WebRTC DCEP Channel Types (RFC 8832 Section 5.1).
pub const ChannelType = enum(u8) {
    reliable = 0x00,
    reliable_unordered = 0x80,
    partial_reliable_rexmit = 0x01,
    partial_reliable_rexmit_unordered = 0x81,
    partial_reliable_timed = 0x02,
    partial_reliable_timed_unordered = 0x82,
};

/// SCTP Association state machine states (RFC 4960 Section 4).
pub const AssociationState = enum {
    closed,
    cookie_wait,
    cookie_echoed,
    established,
    shutdown_pending,
    shutdown_sent,
    shutdown_received,
    shutdown_ack_sent,
};

/// DataChannel state matching `CF_WEBRTC_DC_STATE_*`.
pub const State = enum(u32) {
    connecting = 0,
    open = 1,
    closing = 2,
    closed = 3,
};

/// DataChannel configuration matching `root.zig` `sctp.Config`.
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

    pub fn sendText(self: *DataChannel, text: []const u8) c_types.Error!void {
        const ptr: [*]const u8 = if (text.len > 0) text.ptr else "";
        return c_types.mapCError(c.cf_webrtc_dc_send(self.handle, ptr, text.len, false));
    }

    pub fn sendBinary(self: *DataChannel, bytes: []const u8) c_types.Error!void {
        const ptr: [*]const u8 = if (bytes.len > 0) bytes.ptr else "";
        return c_types.mapCError(c.cf_webrtc_dc_send(self.handle, ptr, bytes.len, true));
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

/// Round `len` up to the next 4-byte boundary as required by SCTP chunk/TLV framing.
pub inline fn align4(len: usize) usize {
    return (len + 3) & ~@as(usize, 3);
}

/// Number of zero padding bytes needed to reach a 4-byte boundary.
pub inline fn pad4Len(len: usize) usize {
    return (4 - (len & 3)) & 3;
}

/// 12-byte SCTP Common Header (RFC 4960 Section 3.1).
pub const CommonHeader = struct {
    pub const byte_len: usize = 12;

    src_port: u16 = default_sctp_port,
    dst_port: u16 = default_sctp_port,
    verification_tag: u32,
    checksum: u32 = 0,

    pub fn encode(self: CommonHeader, out: *[byte_len]u8) void {
        std.mem.writeInt(u16, out[0..2], self.src_port, .big);
        std.mem.writeInt(u16, out[2..4], self.dst_port, .big);
        std.mem.writeInt(u32, out[4..8], self.verification_tag, .big);
        std.mem.writeInt(u32, out[8..12], self.checksum, .big);
    }

    pub fn decode(buf: []const u8) Error!CommonHeader {
        if (buf.len < byte_len) return Error.InvalidPacket;
        return .{
            .src_port = std.mem.readInt(u16, buf[0..2], .big),
            .dst_port = std.mem.readInt(u16, buf[2..4], .big),
            .verification_tag = std.mem.readInt(u32, buf[4..8], .big),
            .checksum = std.mem.readInt(u32, buf[8..12], .big),
        };
    }
};

pub const Crc32c = std.hash.crc.@"CRC-32/ISCSI";

/// Computes the Castagnoli CRC32c checksum of an SCTP packet with bytes 8..12
/// (the checksum field in CommonHeader) treated as all zeros.
pub fn computeChecksum(packet: []const u8) u32 {
    if (packet.len < CommonHeader.byte_len) {
        return Crc32c.hash(packet);
    }
    var crc = Crc32c.init();
    crc.update(packet[0..8]);
    const zero_checksum = [4]u8{ 0, 0, 0, 0 };
    crc.update(&zero_checksum);
    crc.update(packet[12..]);
    return crc.final();
}

/// Writes the computed Castagnoli CRC32c checksum into bytes 8..12 of `packet`.
pub fn writePacketChecksum(packet: []u8) Error!u32 {
    if (packet.len < CommonHeader.byte_len) return Error.InvalidPacket;
    const crc = computeChecksum(packet);
    std.mem.writeInt(u32, packet[8..12], crc, .big);
    return crc;
}

/// Verifies that an SCTP packet's header checksum matches its computed CRC32c.
pub fn verifyChecksum(packet: []const u8) bool {
    if (packet.len < CommonHeader.byte_len) return false;
    const header_crc = std.mem.readInt(u32, packet[8..12], .big);
    return computeChecksum(packet) == header_crc;
}

/// 4-byte SCTP Chunk Header (RFC 4960 Section 3.2).
pub const ChunkHeader = struct {
    pub const byte_len: usize = 4;

    chunk_type: ChunkType,
    flags: u8 = 0,
    length: u16,

    pub fn encode(self: ChunkHeader, out: *[byte_len]u8) void {
        out[0] = @intFromEnum(self.chunk_type);
        out[1] = self.flags;
        std.mem.writeInt(u16, out[2..4], self.length, .big);
    }

    pub fn decode(buf: []const u8) Error!ChunkHeader {
        if (buf.len < byte_len) return Error.InvalidChunk;
        const length = std.mem.readInt(u16, buf[2..4], .big);
        if (length < byte_len or buf.len < length) return Error.InvalidChunk;
        return .{
            .chunk_type = @enumFromInt(buf[0]),
            .flags = buf[1],
            .length = length,
        };
    }
};

const state_cookie_param_type: u16 = 0x0007;
const heartbeat_info_param_type: u16 = 0x0001;

/// SCTP INIT Chunk (Type 1) and INIT_ACK Chunk (Type 2) structure.
pub const InitChunk = struct {
    initiate_tag: u32,
    a_rwnd: u32 = default_a_rwnd,
    num_outbound_streams: u16 = 256,
    num_inbound_streams: u16 = 256,
    initial_tsn: u32,
    state_cookie: []const u8 = &.{},

    pub fn encodeAs(self: InitChunk, allocator: std.mem.Allocator, chunk_type: ChunkType) Error![]u8 {
        if (self.initiate_tag == 0 or self.num_outbound_streams == 0 or self.num_inbound_streams == 0) {
            return Error.InvalidChunk;
        }
        const cookie_tlv_unpadded: usize = if (self.state_cookie.len > 0) 4 + self.state_cookie.len else 0;
        const cookie_tlv_padded: usize = align4(cookie_tlv_unpadded);
        const unpadded_len: usize = 20 + cookie_tlv_unpadded;
        const total_len: usize = 20 + cookie_tlv_padded;

        var buf = allocator.alloc(u8, total_len) catch return Error.OutOfMemory;
        @memset(buf, 0);

        const hdr = ChunkHeader{
            .chunk_type = chunk_type,
            .flags = 0,
            .length = @intCast(unpadded_len),
        };
        hdr.encode(buf[0..4]);
        std.mem.writeInt(u32, buf[4..8], self.initiate_tag, .big);
        std.mem.writeInt(u32, buf[8..12], self.a_rwnd, .big);
        std.mem.writeInt(u16, buf[12..14], self.num_outbound_streams, .big);
        std.mem.writeInt(u16, buf[14..16], self.num_inbound_streams, .big);
        std.mem.writeInt(u32, buf[16..20], self.initial_tsn, .big);

        if (self.state_cookie.len > 0) {
            std.mem.writeInt(u16, buf[20..22], state_cookie_param_type, .big);
            std.mem.writeInt(u16, buf[22..24], @intCast(cookie_tlv_unpadded), .big);
            @memcpy(buf[24 .. 24 + self.state_cookie.len], self.state_cookie);
        }
        return buf;
    }

    pub fn encode(self: InitChunk, allocator: std.mem.Allocator) Error![]u8 {
        return self.encodeAs(allocator, .init);
    }

    pub fn encodeInitAck(self: InitChunk, allocator: std.mem.Allocator) Error![]u8 {
        if (self.state_cookie.len == 0) return Error.InvalidParameter;
        return self.encodeAs(allocator, .init_ack);
    }

    pub fn decode(chunk_buf: []const u8) Error!InitChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .init and hdr.chunk_type != .init_ack) return Error.InvalidChunk;
        if (hdr.length < 20) return Error.InvalidChunk;

        const initiate_tag = std.mem.readInt(u32, chunk_buf[4..8], .big);
        const a_rwnd = std.mem.readInt(u32, chunk_buf[8..12], .big);
        const num_outbound_streams = std.mem.readInt(u16, chunk_buf[12..14], .big);
        const num_inbound_streams = std.mem.readInt(u16, chunk_buf[14..16], .big);
        const initial_tsn = std.mem.readInt(u32, chunk_buf[16..20], .big);

        if (initiate_tag == 0 or num_outbound_streams == 0 or num_inbound_streams == 0) {
            return Error.InvalidChunk;
        }

        var cookie: []const u8 = &.{};
        var pos: usize = 20;
        const limit: usize = @min(@as(usize, hdr.length), chunk_buf.len);
        while (pos + 4 <= limit) {
            const param_type = std.mem.readInt(u16, chunk_buf[pos..][0..2], .big);
            const param_len = std.mem.readInt(u16, chunk_buf[pos + 2 ..][0..2], .big);
            if (param_len < 4 or pos + param_len > chunk_buf.len) return Error.InvalidParameter;
            if (param_type == state_cookie_param_type) {
                cookie = chunk_buf[pos + 4 .. pos + param_len];
            }
            pos += align4(param_len);
        }

        return .{
            .initiate_tag = initiate_tag,
            .a_rwnd = a_rwnd,
            .num_outbound_streams = num_outbound_streams,
            .num_inbound_streams = num_inbound_streams,
            .initial_tsn = initial_tsn,
            .state_cookie = cookie,
        };
    }
};

pub const InitAckChunk = InitChunk;

/// SCTP DATA Chunk (Type 0, RFC 4960 Section 3.3.1).
pub const DataChunk = struct {
    immediate: bool = false,
    unordered: bool = false,
    beginning: bool = true,
    ending: bool = true,
    tsn: u32,
    stream_id: u16,
    stream_seq: u16 = 0,
    ppid: u32,
    user_data: []const u8,

    pub fn flagsByte(self: DataChunk) u8 {
        var f: u8 = 0;
        if (self.immediate) f |= 0x08;
        if (self.unordered) f |= 0x04;
        if (self.beginning) f |= 0x02;
        if (self.ending) f |= 0x01;
        return f;
    }

    pub fn ppidEnum(self: DataChunk) Ppid {
        return @enumFromInt(self.ppid);
    }

    pub fn encode(self: DataChunk, allocator: std.mem.Allocator) Error![]u8 {
        const unpadded_len: usize = 16 + self.user_data.len;
        const total_len: usize = align4(unpadded_len);

        var buf = allocator.alloc(u8, total_len) catch return Error.OutOfMemory;
        @memset(buf, 0);

        const hdr = ChunkHeader{
            .chunk_type = .data,
            .flags = self.flagsByte(),
            .length = @intCast(unpadded_len),
        };
        hdr.encode(buf[0..4]);
        std.mem.writeInt(u32, buf[4..8], self.tsn, .big);
        std.mem.writeInt(u16, buf[8..10], self.stream_id, .big);
        std.mem.writeInt(u16, buf[10..12], self.stream_seq, .big);
        std.mem.writeInt(u32, buf[12..16], self.ppid, .big);
        if (self.user_data.len > 0) {
            @memcpy(buf[16 .. 16 + self.user_data.len], self.user_data);
        }
        return buf;
    }

    pub fn decode(chunk_buf: []const u8) Error!DataChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .data or hdr.length < 16) return Error.InvalidChunk;

        return .{
            .immediate = (hdr.flags & 0x08) != 0,
            .unordered = (hdr.flags & 0x04) != 0,
            .beginning = (hdr.flags & 0x02) != 0,
            .ending = (hdr.flags & 0x01) != 0,
            .tsn = std.mem.readInt(u32, chunk_buf[4..8], .big),
            .stream_id = std.mem.readInt(u16, chunk_buf[8..10], .big),
            .stream_seq = std.mem.readInt(u16, chunk_buf[10..12], .big),
            .ppid = std.mem.readInt(u32, chunk_buf[12..16], .big),
            .user_data = chunk_buf[16..hdr.length],
        };
    }
};

/// Gap Ack Block in a SACK chunk (RFC 4960 Section 3.3.4).
pub const GapAckBlock = struct {
    start: u16,
    end: u16,
};

/// SCTP SACK Chunk (Type 3, RFC 4960 Section 3.3.4).
pub const SackChunk = struct {
    cumulative_tsn_ack: u32,
    a_rwnd: u32 = default_a_rwnd,
    gap_blocks: []const GapAckBlock = &.{},
    dup_tsns: []const u32 = &.{},

    pub fn encode(self: SackChunk, allocator: std.mem.Allocator) Error![]u8 {
        const total_len: usize = 16 + self.gap_blocks.len * 4 + self.dup_tsns.len * 4;
        var buf = allocator.alloc(u8, total_len) catch return Error.OutOfMemory;

        const hdr = ChunkHeader{
            .chunk_type = .sack,
            .flags = 0,
            .length = @intCast(total_len),
        };
        hdr.encode(buf[0..4]);
        std.mem.writeInt(u32, buf[4..8], self.cumulative_tsn_ack, .big);
        std.mem.writeInt(u32, buf[8..12], self.a_rwnd, .big);
        std.mem.writeInt(u16, buf[12..14], @intCast(self.gap_blocks.len), .big);
        std.mem.writeInt(u16, buf[14..16], @intCast(self.dup_tsns.len), .big);

        var pos: usize = 16;
        for (self.gap_blocks) |gap| {
            std.mem.writeInt(u16, buf[pos..][0..2], gap.start, .big);
            std.mem.writeInt(u16, buf[pos + 2 ..][0..2], gap.end, .big);
            pos += 4;
        }
        for (self.dup_tsns) |dup| {
            std.mem.writeInt(u32, buf[pos..][0..4], dup, .big);
            pos += 4;
        }
        return buf;
    }

    pub fn decode(allocator: std.mem.Allocator, chunk_buf: []const u8) Error!SackChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .sack or hdr.length < 16) return Error.InvalidChunk;

        const cumulative_tsn_ack = std.mem.readInt(u32, chunk_buf[4..8], .big);
        const a_rwnd = std.mem.readInt(u32, chunk_buf[8..12], .big);
        const num_gaps = std.mem.readInt(u16, chunk_buf[12..14], .big);
        const num_dups = std.mem.readInt(u16, chunk_buf[14..16], .big);

        const expected_len: usize = 16 + @as(usize, num_gaps) * 4 + @as(usize, num_dups) * 4;
        if (hdr.length < expected_len) return Error.InvalidChunk;

        const gaps = allocator.alloc(GapAckBlock, num_gaps) catch return Error.OutOfMemory;
        errdefer allocator.free(gaps);

        const dups = allocator.alloc(u32, num_dups) catch return Error.OutOfMemory;

        var pos: usize = 16;
        for (0..num_gaps) |i| {
            gaps[i] = .{
                .start = std.mem.readInt(u16, chunk_buf[pos..][0..2], .big),
                .end = std.mem.readInt(u16, chunk_buf[pos + 2 ..][0..2], .big),
            };
            pos += 4;
        }
        for (0..num_dups) |i| {
            dups[i] = std.mem.readInt(u32, chunk_buf[pos..][0..4], .big);
            pos += 4;
        }

        return .{
            .cumulative_tsn_ack = cumulative_tsn_ack,
            .a_rwnd = a_rwnd,
            .gap_blocks = gaps,
            .dup_tsns = dups,
        };
    }

    pub fn deinit(self: *SackChunk, allocator: std.mem.Allocator) void {
        allocator.free(self.gap_blocks);
        allocator.free(self.dup_tsns);
    }
};

/// SCTP HEARTBEAT (Type 4) and HEARTBEAT_ACK (Type 5) Chunk.
pub const HeartbeatChunk = struct {
    info: []const u8,

    pub fn encodeAs(self: HeartbeatChunk, allocator: std.mem.Allocator, chunk_type: ChunkType) Error![]u8 {
        const param_unpadded: usize = 4 + self.info.len;
        const param_padded: usize = align4(param_unpadded);
        const unpadded_len: usize = 4 + param_unpadded;
        const total_len: usize = 4 + param_padded;

        var buf = allocator.alloc(u8, total_len) catch return Error.OutOfMemory;
        @memset(buf, 0);

        const hdr = ChunkHeader{
            .chunk_type = chunk_type,
            .flags = 0,
            .length = @intCast(unpadded_len),
        };
        hdr.encode(buf[0..4]);
        std.mem.writeInt(u16, buf[4..6], heartbeat_info_param_type, .big);
        std.mem.writeInt(u16, buf[6..8], @intCast(param_unpadded), .big);
        if (self.info.len > 0) {
            @memcpy(buf[8 .. 8 + self.info.len], self.info);
        }
        return buf;
    }

    pub fn encode(self: HeartbeatChunk, allocator: std.mem.Allocator) Error![]u8 {
        return self.encodeAs(allocator, .heartbeat);
    }

    pub fn encodeAck(self: HeartbeatChunk, allocator: std.mem.Allocator) Error![]u8 {
        return self.encodeAs(allocator, .heartbeat_ack);
    }

    pub fn decode(chunk_buf: []const u8) Error!HeartbeatChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .heartbeat and hdr.chunk_type != .heartbeat_ack) {
            return Error.InvalidChunk;
        }
        if (hdr.length < 8) return Error.InvalidChunk;
        const param_type = std.mem.readInt(u16, chunk_buf[4..6], .big);
        const param_len = std.mem.readInt(u16, chunk_buf[6..8], .big);
        if (param_type != heartbeat_info_param_type or param_len < 4 or 4 + param_len > hdr.length) {
            return Error.InvalidParameter;
        }
        return .{
            .info = chunk_buf[8 .. 4 + param_len],
        };
    }
};

pub const HeartbeatAckChunk = HeartbeatChunk;

/// SCTP SHUTDOWN Chunk (Type 7, RFC 4960 Section 3.3.8).
pub const ShutdownChunk = struct {
    cumulative_tsn_ack: u32,

    pub fn encode(self: ShutdownChunk, allocator: std.mem.Allocator) Error![]u8 {
        var buf = allocator.alloc(u8, 8) catch return Error.OutOfMemory;
        const hdr = ChunkHeader{
            .chunk_type = .shutdown,
            .flags = 0,
            .length = 8,
        };
        hdr.encode(buf[0..4]);
        std.mem.writeInt(u32, buf[4..8], self.cumulative_tsn_ack, .big);
        return buf;
    }

    pub fn decode(chunk_buf: []const u8) Error!ShutdownChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .shutdown or hdr.length != 8) return Error.InvalidChunk;
        return .{
            .cumulative_tsn_ack = std.mem.readInt(u32, chunk_buf[4..8], .big),
        };
    }
};

/// SCTP SHUTDOWN_ACK Chunk (Type 8, RFC 4960 Section 3.3.9).
pub const ShutdownAckChunk = struct {
    pub fn encode(allocator: std.mem.Allocator) Error![]u8 {
        var buf = allocator.alloc(u8, 4) catch return Error.OutOfMemory;
        const hdr = ChunkHeader{
            .chunk_type = .shutdown_ack,
            .flags = 0,
            .length = 4,
        };
        hdr.encode(buf[0..4]);
        return buf;
    }

    pub fn decode(chunk_buf: []const u8) Error!ShutdownAckChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .shutdown_ack or hdr.length != 4) return Error.InvalidChunk;
        return .{};
    }
};

/// SCTP COOKIE_ECHO Chunk (Type 10, RFC 4960 Section 3.3.11).
pub const CookieEchoChunk = struct {
    cookie: []const u8,

    pub fn encode(self: CookieEchoChunk, allocator: std.mem.Allocator) Error![]u8 {
        if (self.cookie.len == 0) return Error.InvalidChunk;
        const unpadded_len: usize = 4 + self.cookie.len;
        const total_len: usize = align4(unpadded_len);

        var buf = allocator.alloc(u8, total_len) catch return Error.OutOfMemory;
        @memset(buf, 0);

        const hdr = ChunkHeader{
            .chunk_type = .cookie_echo,
            .flags = 0,
            .length = @intCast(unpadded_len),
        };
        hdr.encode(buf[0..4]);
        @memcpy(buf[4 .. 4 + self.cookie.len], self.cookie);
        return buf;
    }

    pub fn decode(chunk_buf: []const u8) Error!CookieEchoChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .cookie_echo or hdr.length <= 4) return Error.InvalidChunk;
        return .{
            .cookie = chunk_buf[4..hdr.length],
        };
    }
};

/// SCTP COOKIE_ACK Chunk (Type 11, RFC 4960 Section 3.3.12).
pub const CookieAckChunk = struct {
    pub fn encode(allocator: std.mem.Allocator) Error![]u8 {
        var buf = allocator.alloc(u8, 4) catch return Error.OutOfMemory;
        const hdr = ChunkHeader{
            .chunk_type = .cookie_ack,
            .flags = 0,
            .length = 4,
        };
        hdr.encode(buf[0..4]);
        return buf;
    }

    pub fn decode(chunk_buf: []const u8) Error!CookieAckChunk {
        const hdr = try ChunkHeader.decode(chunk_buf);
        if (hdr.chunk_type != .cookie_ack or hdr.length != 4) return Error.InvalidChunk;
        return .{};
    }
};

/// Builds a complete SCTP packet (CommonHeader + chunks + Castagnoli CRC32c).
pub fn buildPacket(
    allocator: std.mem.Allocator,
    src_port: u16,
    dst_port: u16,
    verification_tag: u32,
    chunks: []const []const u8,
) Error![]u8 {
    var total_len: usize = CommonHeader.byte_len;
    for (chunks) |c_slice| {
        total_len += align4(c_slice.len);
    }

    var pkt = allocator.alloc(u8, total_len) catch return Error.OutOfMemory;
    @memset(pkt, 0);

    const hdr = CommonHeader{
        .src_port = src_port,
        .dst_port = dst_port,
        .verification_tag = verification_tag,
        .checksum = 0,
    };
    hdr.encode(pkt[0..12]);

    var pos: usize = CommonHeader.byte_len;
    for (chunks) |c_slice| {
        @memcpy(pkt[pos .. pos + c_slice.len], c_slice);
        pos += align4(c_slice.len);
    }

    _ = try writePacketChecksum(pkt);
    return pkt;
}

/// WebRTC DCEP DATA_CHANNEL_OPEN message (RFC 8832 Section 5.1).
pub const DcepOpenMessage = struct {
    pub const fixed_header_len: usize = 12;

    channel_type: ChannelType = .reliable,
    priority: u16 = 0,
    reliability_parameter: u32 = 0,
    label: []const u8,
    protocol: []const u8 = "",

    pub fn encodedLen(self: DcepOpenMessage) usize {
        return fixed_header_len + self.label.len + self.protocol.len;
    }

    pub fn encodeBuf(self: DcepOpenMessage, out: []u8) Error![]const u8 {
        const need = self.encodedLen();
        if (out.len < need) return Error.BufferTooSmall;

        out[0] = @intFromEnum(DcepMessageType.open);
        out[1] = @intFromEnum(self.channel_type);
        std.mem.writeInt(u16, out[2..4], self.priority, .big);
        std.mem.writeInt(u32, out[4..8], self.reliability_parameter, .big);
        std.mem.writeInt(u16, out[8..10], @intCast(self.label.len), .big);
        std.mem.writeInt(u16, out[10..12], @intCast(self.protocol.len), .big);

        const label_end = fixed_header_len + self.label.len;
        if (self.label.len > 0) {
            @memcpy(out[fixed_header_len..label_end], self.label);
        }
        if (self.protocol.len > 0) {
            @memcpy(out[label_end .. label_end + self.protocol.len], self.protocol);
        }
        return out[0..need];
    }

    pub fn encode(self: DcepOpenMessage, allocator: std.mem.Allocator) Error![]u8 {
        const buf = allocator.alloc(u8, self.encodedLen()) catch return Error.OutOfMemory;
        _ = try self.encodeBuf(buf);
        return buf;
    }

    pub fn decode(buf: []const u8) Error!DcepOpenMessage {
        if (buf.len < fixed_header_len) return Error.InvalidDcepMessage;
        if (buf[0] != @intFromEnum(DcepMessageType.open)) return Error.InvalidDcepMessage;

        const channel_type = std.enums.fromInt(ChannelType, buf[1]) orelse return Error.InvalidDcepMessage;
        const priority = std.mem.readInt(u16, buf[2..4], .big);
        const reliability_parameter = std.mem.readInt(u32, buf[4..8], .big);
        const label_len = std.mem.readInt(u16, buf[8..10], .big);
        const protocol_len = std.mem.readInt(u16, buf[10..12], .big);

        const total_len = fixed_header_len + @as(usize, label_len) + @as(usize, protocol_len);
        if (buf.len < total_len) return Error.InvalidDcepMessage;

        const label_end = fixed_header_len + @as(usize, label_len);
        return .{
            .channel_type = channel_type,
            .priority = priority,
            .reliability_parameter = reliability_parameter,
            .label = buf[fixed_header_len..label_end],
            .protocol = buf[label_end..total_len],
        };
    }
};

/// Encodes a 1-byte WebRTC DCEP DATA_CHANNEL_ACK message (`0x02`).
pub fn encodeDcepAck() [1]u8 {
    return .{@intFromEnum(DcepMessageType.ack)};
}

/// Decodes a 1-byte WebRTC DCEP DATA_CHANNEL_ACK message.
pub fn isDcepAck(buf: []const u8) bool {
    return buf.len >= 1 and buf[0] == @intFromEnum(DcepMessageType.ack);
}

/// Received DataChannel message stored or delivered by `Association`.
pub const ReceivedMessage = struct {
    stream_id: u16,
    is_binary: bool,
    data: []u8,

    pub fn deinit(self: *ReceivedMessage, allocator: std.mem.Allocator) void {
        allocator.free(self.data);
    }
};

/// Callbacks for a specific DataChannel stream or Association listener.
pub const ChannelCallbacks = struct {
    user_data: ?*anyopaque = null,
    on_state_change: ?*const fn (user_data: ?*anyopaque, stream_id: u16, new_state: State) void = null,
    on_message: ?*const fn (user_data: ?*anyopaque, stream_id: u16, data: []const u8, is_binary: bool) void = null,
};

/// Callbacks for Association-wide events (such as remote DataChannel creation via DCEP).
pub const AssociationCallbacks = struct {
    user_data: ?*anyopaque = null,
    on_channel_open: ?*const fn (user_data: ?*anyopaque, stream_id: u16, label: []const u8, protocol: []const u8) void = null,
    on_message: ?*const fn (user_data: ?*anyopaque, stream_id: u16, label: []const u8, data: []const u8, is_binary: bool) void = null,
};

/// Per-stream DataChannel state managed inside an `Association`.
pub const Channel = struct {
    stream_id: u16,
    label_buf: [128]u8 = @splat(0),
    label_len: usize = 0,
    protocol_buf: [128]u8 = @splat(0),
    protocol_len: usize = 0,
    ordered: bool = true,
    reliable: bool = true,
    state: State = .connecting,
    next_ssn: u16 = 0,
    dcep_open_queued: bool = false,
    callbacks: ChannelCallbacks = .{},
    rx_messages: std.ArrayList(ReceivedMessage) = .empty,

    // Reassembly state for multi-chunk fragmented DATA payloads on this stream.
    reassembly_buf: std.ArrayList(u8) = .empty,
    reassembling: bool = false,
    reassembly_ppid: u32 = 0,

    pub fn label(self: *const Channel) []const u8 {
        return self.label_buf[0..self.label_len];
    }

    pub fn protocol(self: *const Channel) []const u8 {
        return self.protocol_buf[0..self.protocol_len];
    }

    pub fn deinit(self: *Channel, allocator: std.mem.Allocator) void {
        for (self.rx_messages.items) |*msg| {
            msg.deinit(allocator);
        }
        self.rx_messages.deinit(allocator);
        self.reassembly_buf.deinit(allocator);
    }
};

/// Pure-Zig SCTP Association & WebRTC DataChannel Engine.
pub const Association = struct {
    allocator: std.mem.Allocator,
    local_port: u16 = default_sctp_port,
    remote_port: u16 = default_sctp_port,
    local_verification_tag: u32,
    remote_verification_tag: u32 = 0,
    next_tsn: u32,
    peer_cumulative_tsn: u32 = 0,
    has_peer_tsn: bool = false,
    a_rwnd: u32 = default_a_rwnd,
    peer_a_rwnd: u32 = default_a_rwnd,
    max_payload_size: usize = default_max_payload_size,
    state: AssociationState = .closed,
    next_auto_stream_id: u16 = 0,
    heartbeats_acked: usize = 0,
    sacks_received: usize = 0,
    last_acked_tsn: u32 = 0,
    callbacks: AssociationCallbacks = .{},

    outbound_packets: std.ArrayList([]u8) = .empty,
    channels: std.ArrayList(Channel) = .empty,

    pub const InitOptions = struct {
        local_port: u16 = default_sctp_port,
        remote_port: u16 = default_sctp_port,
        local_verification_tag: u32 = 0x10203040,
        initial_tsn: u32 = 1000,
        max_payload_size: usize = default_max_payload_size,
        is_client: bool = true,
        callbacks: AssociationCallbacks = .{},
    };

    pub fn init(allocator: std.mem.Allocator, opts: InitOptions) Association {
        return .{
            .allocator = allocator,
            .local_port = opts.local_port,
            .remote_port = opts.remote_port,
            .local_verification_tag = if (opts.local_verification_tag == 0) 0x10203040 else opts.local_verification_tag,
            .next_tsn = opts.initial_tsn,
            .max_payload_size = if (opts.max_payload_size == 0) default_max_payload_size else opts.max_payload_size,
            .next_auto_stream_id = if (opts.is_client) 0 else 1,
            .callbacks = opts.callbacks,
        };
    }

    pub fn deinit(self: *Association) void {
        for (self.outbound_packets.items) |pkt| {
            self.allocator.free(pkt);
        }
        self.outbound_packets.deinit(self.allocator);

        for (self.channels.items) |*ch| {
            ch.deinit(self.allocator);
        }
        self.channels.deinit(self.allocator);
    }

    fn queueSingleChunkPacket(self: *Association, verification_tag: u32, chunk_bytes: []const u8) Error!void {
        const chunks = [_][]const u8{chunk_bytes};
        const pkt = try buildPacket(
            self.allocator,
            self.local_port,
            self.remote_port,
            verification_tag,
            &chunks,
        );
        errdefer self.allocator.free(pkt);
        self.outbound_packets.append(self.allocator, pkt) catch return Error.OutOfMemory;
    }

    /// Creates an SCTP INIT packet, transitions state to `.cookie_wait`, and returns the packet bytes.
    pub fn createInitPacket(self: *Association) Error![]u8 {
        const init_chunk = InitChunk{
            .initiate_tag = self.local_verification_tag,
            .a_rwnd = self.a_rwnd,
            .num_outbound_streams = 256,
            .num_inbound_streams = 256,
            .initial_tsn = self.next_tsn,
        };
        const chunk_bytes = try init_chunk.encode(self.allocator);
        defer self.allocator.free(chunk_bytes);

        const chunks = [_][]const u8{chunk_bytes};
        const pkt = try buildPacket(
            self.allocator,
            self.local_port,
            self.remote_port,
            0, // RFC 4960: INIT packet must use verification_tag = 0
            &chunks,
        );
        self.state = .cookie_wait;
        return pkt;
    }

    /// Queues an SCTP INIT packet into the outbound packet queue and enters `.cookie_wait`.
    pub fn startHandshake(self: *Association) Error!void {
        const pkt = try self.createInitPacket();
        errdefer self.allocator.free(pkt);
        self.outbound_packets.append(self.allocator, pkt) catch return Error.OutOfMemory;
    }

    /// Pops the oldest outbound SCTP packet from the queue (caller owns the returned slice).
    pub fn pollOutboundPacket(self: *Association) ?[]u8 {
        if (self.outbound_packets.items.len == 0) return null;
        return self.outbound_packets.orderedRemove(0);
    }

    /// Queues an SCTP HEARTBEAT packet with the given info payload.
    pub fn sendHeartbeat(self: *Association, info: []const u8) Error!void {
        if (self.state != .established) return Error.InvalidState;
        const hb = HeartbeatChunk{ .info = info };
        const chunk_bytes = try hb.encode(self.allocator);
        defer self.allocator.free(chunk_bytes);
        try self.queueSingleChunkPacket(self.remote_verification_tag, chunk_bytes);
    }

    /// Initiates a graceful SCTP SHUTDOWN sequence.
    pub fn initiateShutdown(self: *Association) Error!void {
        if (self.state != .established and self.state != .shutdown_pending) return Error.InvalidState;
        const sd = ShutdownChunk{ .cumulative_tsn_ack = self.peer_cumulative_tsn };
        const chunk_bytes = try sd.encode(self.allocator);
        defer self.allocator.free(chunk_bytes);
        try self.queueSingleChunkPacket(self.remote_verification_tag, chunk_bytes);
        self.state = .shutdown_sent;
    }

    /// Finds a channel by `stream_id`.
    pub fn findChannel(self: *Association, stream_id: u16) ?*Channel {
        for (self.channels.items) |*ch| {
            if (ch.stream_id == stream_id) return ch;
        }
        return null;
    }

    /// Finds a channel by `label`.
    pub fn findChannelByLabel(self: *Association, label_str: []const u8) ?*Channel {
        for (self.channels.items) |*ch| {
            if (std.mem.eql(u8, ch.label(), label_str)) return ch;
        }
        return null;
    }

    fn ensureChannel(
        self: *Association,
        stream_id: u16,
        label_str: []const u8,
        protocol_str: []const u8,
        ordered: bool,
        reliable: bool,
        initial_state: State,
        callbacks: ChannelCallbacks,
    ) Error!*Channel {
        if (self.findChannel(stream_id)) |existing| {
            if (label_str.len > 0 and existing.label_len == 0) {
                const n = @min(label_str.len, existing.label_buf.len);
                @memcpy(existing.label_buf[0..n], label_str[0..n]);
                existing.label_len = n;
            }
            if (protocol_str.len > 0 and existing.protocol_len == 0) {
                const n = @min(protocol_str.len, existing.protocol_buf.len);
                @memcpy(existing.protocol_buf[0..n], protocol_str[0..n]);
                existing.protocol_len = n;
            }
            existing.ordered = ordered;
            existing.reliable = reliable;
            if (callbacks.on_message != null or callbacks.on_state_change != null) {
                existing.callbacks = callbacks;
            }
            return existing;
        }

        var ch = Channel{
            .stream_id = stream_id,
            .ordered = ordered,
            .reliable = reliable,
            .state = initial_state,
            .callbacks = callbacks,
        };
        const l_len = @min(label_str.len, ch.label_buf.len);
        @memcpy(ch.label_buf[0..l_len], label_str[0..l_len]);
        ch.label_len = l_len;

        const p_len = @min(protocol_str.len, ch.protocol_buf.len);
        @memcpy(ch.protocol_buf[0..p_len], protocol_str[0..p_len]);
        ch.protocol_len = p_len;

        self.channels.append(self.allocator, ch) catch return Error.OutOfMemory;
        return &self.channels.items[self.channels.items.len - 1];
    }

    fn queueDcepOpenForChannel(self: *Association, ch: *Channel) Error!void {
        const channel_type: ChannelType = if (ch.reliable)
            (if (ch.ordered) .reliable else .reliable_unordered)
        else
            (if (ch.ordered) .partial_reliable_rexmit else .partial_reliable_rexmit_unordered);

        const open_msg = DcepOpenMessage{
            .channel_type = channel_type,
            .priority = 0,
            .reliability_parameter = 0,
            .label = ch.label(),
            .protocol = ch.protocol(),
        };
        const dcep_payload = try open_msg.encode(self.allocator);
        defer self.allocator.free(dcep_payload);

        const ssn = ch.next_ssn;
        ch.next_ssn +%= 1;
        const tsn = self.next_tsn;
        self.next_tsn +%= 1;

        const data_chunk = DataChunk{
            .unordered = false,
            .beginning = true,
            .ending = true,
            .tsn = tsn,
            .stream_id = ch.stream_id,
            .stream_seq = ssn,
            .ppid = @intFromEnum(Ppid.dcep),
            .user_data = dcep_payload,
        };
        const chunk_bytes = try data_chunk.encode(self.allocator);
        defer self.allocator.free(chunk_bytes);

        try self.queueSingleChunkPacket(self.remote_verification_tag, chunk_bytes);
        ch.dcep_open_queued = true;
    }

    fn flushPendingDcepOpens(self: *Association) Error!void {
        const count = self.channels.items.len;
        for (0..count) |idx| {
            const ch = &self.channels.items[idx];
            if (!ch.dcep_open_queued and ch.state == .connecting) {
                try self.queueDcepOpenForChannel(ch);
            }
        }
    }

    /// Opens a WebRTC DataChannel via DCEP `DATA_CHANNEL_OPEN` and returns its `stream_id`.
    pub fn openDataChannel(
        self: *Association,
        label_str: []const u8,
        config: Config,
        callbacks: ChannelCallbacks,
    ) Error!u16 {
        if (label_str.len == 0) return Error.InvalidArgument;

        const stream_id: u16 = if (config.id >= 0)
            @intCast(config.id)
        else blk: {
            const id = self.next_auto_stream_id;
            self.next_auto_stream_id += 2;
            break :blk id;
        };

        const proto_slice: []const u8 = if (config.protocol) |p| p else "";
        const ch = try self.ensureChannel(
            stream_id,
            label_str,
            proto_slice,
            config.ordered,
            config.reliable,
            .connecting,
            callbacks,
        );

        if (self.state == .established and !ch.dcep_open_queued) {
            try self.queueDcepOpenForChannel(ch);
        }
        return stream_id;
    }

    /// Sets or updates callbacks on an existing DataChannel stream.
    pub fn setChannelCallbacks(self: *Association, stream_id: u16, callbacks: ChannelCallbacks) Error!void {
        const ch = self.findChannel(stream_id) orelse return Error.StreamNotFound;
        ch.callbacks = callbacks;
    }

    /// Sends a UTF-8 text message on `stream_id` (using `PPID 51` or `PPID 56` when empty).
    pub fn sendText(self: *Association, stream_id: u16, text: []const u8) Error!void {
        return self.sendPayload(stream_id, text, false);
    }

    /// Sends a binary message on `stream_id` (using `PPID 53` or `PPID 57` when empty).
    pub fn sendBinary(self: *Association, stream_id: u16, bytes: []const u8) Error!void {
        return self.sendPayload(stream_id, bytes, true);
    }

    fn sendPayload(self: *Association, stream_id: u16, payload: []const u8, is_binary: bool) Error!void {
        if (self.state != .established) return Error.InvalidState;
        const ch = self.findChannel(stream_id) orelse return Error.StreamNotFound;
        if (ch.state == .closing or ch.state == .closed) return Error.InvalidState;

        const unordered = !ch.ordered;
        const ssn: u16 = if (unordered) 0 else blk: {
            const s = ch.next_ssn;
            ch.next_ssn +%= 1;
            break :blk s;
        };

        // RFC 8831 Section 6.6: Empty messages use PPID 56 (string empty) or 57 (binary empty)
        // with a 1-byte zero user data payload inside the SCTP DATA chunk.
        if (payload.len == 0) {
            const ppid: Ppid = if (is_binary) .webrtc_binary_empty else .webrtc_string_empty;
            const zero_byte = [_]u8{0};
            const tsn = self.next_tsn;
            self.next_tsn +%= 1;

            const chunk = DataChunk{
                .unordered = unordered,
                .beginning = true,
                .ending = true,
                .tsn = tsn,
                .stream_id = stream_id,
                .stream_seq = ssn,
                .ppid = @intFromEnum(ppid),
                .user_data = &zero_byte,
            };
            const encoded = try chunk.encode(self.allocator);
            defer self.allocator.free(encoded);
            try self.queueSingleChunkPacket(self.remote_verification_tag, encoded);
            return;
        }

        const ppid: Ppid = if (is_binary) .webrtc_binary else .webrtc_string;
        const max_chunk = self.max_payload_size;
        var offset: usize = 0;

        while (offset < payload.len) {
            const remaining = payload.len - offset;
            const frag_len = @min(remaining, max_chunk);
            const is_first = (offset == 0);
            const is_last = (offset + frag_len == payload.len);

            const tsn = self.next_tsn;
            self.next_tsn +%= 1;

            const chunk = DataChunk{
                .unordered = unordered,
                .beginning = is_first,
                .ending = is_last,
                .tsn = tsn,
                .stream_id = stream_id,
                .stream_seq = ssn,
                .ppid = @intFromEnum(ppid),
                .user_data = payload[offset .. offset + frag_len],
            };
            const encoded = try chunk.encode(self.allocator);
            defer self.allocator.free(encoded);
            try self.queueSingleChunkPacket(self.remote_verification_tag, encoded);

            offset += frag_len;
        }
    }

    /// Processes an inbound SCTP packet and queues any protocol response packets.
    pub fn handleInboundPacket(self: *Association, packet: []const u8) Error!void {
        const hdr = try CommonHeader.decode(packet);
        if (!verifyChecksum(packet)) return Error.InvalidChecksum;

        var pos: usize = CommonHeader.byte_len;
        var saw_data_chunk = false;

        while (pos + ChunkHeader.byte_len <= packet.len) {
            const chunk_hdr = try ChunkHeader.decode(packet[pos..]);
            const chunk_slice = packet[pos .. pos + chunk_hdr.length];

            switch (chunk_hdr.chunk_type) {
                .init => {
                    if (hdr.verification_tag != 0) return Error.InvalidPacket;
                    const init_in = try InitChunk.decode(chunk_slice);
                    self.remote_verification_tag = init_in.initiate_tag;
                    self.peer_a_rwnd = init_in.a_rwnd;
                    self.peer_cumulative_tsn = init_in.initial_tsn -% 1;
                    self.has_peer_tsn = true;

                    const init_ack = InitAckChunk{
                        .initiate_tag = self.local_verification_tag,
                        .a_rwnd = self.a_rwnd,
                        .num_outbound_streams = init_in.num_inbound_streams,
                        .num_inbound_streams = init_in.num_outbound_streams,
                        .initial_tsn = self.next_tsn,
                        .state_cookie = default_state_cookie,
                    };
                    const ack_bytes = try init_ack.encodeInitAck(self.allocator);
                    defer self.allocator.free(ack_bytes);
                    try self.queueSingleChunkPacket(self.remote_verification_tag, ack_bytes);
                },
                .init_ack => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    const init_ack_in = try InitAckChunk.decode(chunk_slice);
                    if (init_ack_in.state_cookie.len == 0) return Error.InvalidParameter;

                    self.remote_verification_tag = init_ack_in.initiate_tag;
                    self.peer_a_rwnd = init_ack_in.a_rwnd;
                    self.peer_cumulative_tsn = init_ack_in.initial_tsn -% 1;
                    self.has_peer_tsn = true;

                    const ce = CookieEchoChunk{ .cookie = init_ack_in.state_cookie };
                    const ce_bytes = try ce.encode(self.allocator);
                    defer self.allocator.free(ce_bytes);
                    try self.queueSingleChunkPacket(self.remote_verification_tag, ce_bytes);
                    self.state = .cookie_echoed;
                },
                .cookie_echo => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    _ = try CookieEchoChunk.decode(chunk_slice);
                    const ca_bytes = try CookieAckChunk.encode(self.allocator);
                    defer self.allocator.free(ca_bytes);
                    try self.queueSingleChunkPacket(self.remote_verification_tag, ca_bytes);
                    self.state = .established;
                    try self.flushPendingDcepOpens();
                },
                .cookie_ack => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    _ = try CookieAckChunk.decode(chunk_slice);
                    self.state = .established;
                    try self.flushPendingDcepOpens();
                },
                .heartbeat => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    const hb_in = try HeartbeatChunk.decode(chunk_slice);
                    const hb_ack = HeartbeatAckChunk{ .info = hb_in.info };
                    const ack_bytes = try hb_ack.encodeAck(self.allocator);
                    defer self.allocator.free(ack_bytes);
                    try self.queueSingleChunkPacket(self.remote_verification_tag, ack_bytes);
                },
                .heartbeat_ack => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    _ = try HeartbeatAckChunk.decode(chunk_slice);
                    self.heartbeats_acked += 1;
                },
                .sack => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    var sack_in = try SackChunk.decode(self.allocator, chunk_slice);
                    defer sack_in.deinit(self.allocator);
                    self.last_acked_tsn = sack_in.cumulative_tsn_ack;
                    self.peer_a_rwnd = sack_in.a_rwnd;
                    self.sacks_received += 1;
                },
                .data => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    const data_in = try DataChunk.decode(chunk_slice);
                    if (!self.has_peer_tsn or data_in.tsn == self.peer_cumulative_tsn +% 1 or
                        @as(i32, @bitCast(data_in.tsn -% self.peer_cumulative_tsn)) > 0)
                    {
                        self.peer_cumulative_tsn = data_in.tsn;
                        self.has_peer_tsn = true;
                    }
                    saw_data_chunk = true;
                    try self.handleInboundDataChunk(data_in);
                },
                .shutdown => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    const sd_in = try ShutdownChunk.decode(chunk_slice);
                    self.last_acked_tsn = sd_in.cumulative_tsn_ack;
                    self.state = .shutdown_received;

                    const sa_bytes = try ShutdownAckChunk.encode(self.allocator);
                    defer self.allocator.free(sa_bytes);
                    try self.queueSingleChunkPacket(self.remote_verification_tag, sa_bytes);
                    self.state = .shutdown_ack_sent;
                },
                .shutdown_ack => {
                    if (hdr.verification_tag != self.local_verification_tag) return Error.InvalidPacket;
                    _ = try ShutdownAckChunk.decode(chunk_slice);
                    self.state = .closed;
                    for (self.channels.items) |*ch| {
                        ch.state = .closed;
                    }
                },
                else => {},
            }

            pos += align4(chunk_hdr.length);
        }

        if (saw_data_chunk) {
            const sack = SackChunk{
                .cumulative_tsn_ack = self.peer_cumulative_tsn,
                .a_rwnd = self.a_rwnd,
            };
            const sack_bytes = try sack.encode(self.allocator);
            defer self.allocator.free(sack_bytes);
            try self.queueSingleChunkPacket(self.remote_verification_tag, sack_bytes);
        }
    }

    fn handleInboundDataChunk(self: *Association, data_in: DataChunk) Error!void {
        if (data_in.beginning and data_in.ending) {
            return self.deliverCompleteMessage(data_in.stream_id, data_in.ppid, data_in.user_data);
        }

        const ch = try self.ensureChannel(
            data_in.stream_id,
            "",
            "",
            !data_in.unordered,
            true,
            .open,
            .{},
        );

        if (data_in.beginning) {
            ch.reassembly_buf.clearRetainingCapacity();
            ch.reassembling = true;
            ch.reassembly_ppid = data_in.ppid;
        } else if (!ch.reassembling) {
            return Error.InvalidChunk;
        }

        ch.reassembly_buf.appendSlice(self.allocator, data_in.user_data) catch return Error.OutOfMemory;

        if (data_in.ending) {
            ch.reassembling = false;
            const assembled = self.allocator.dupe(u8, ch.reassembly_buf.items) catch return Error.OutOfMemory;
            defer self.allocator.free(assembled);
            ch.reassembly_buf.clearRetainingCapacity();
            try self.deliverCompleteMessage(data_in.stream_id, ch.reassembly_ppid, assembled);
        }
    }

    fn deliverCompleteMessage(
        self: *Association,
        stream_id: u16,
        raw_ppid: u32,
        payload: []const u8,
    ) Error!void {
        const ppid: Ppid = @enumFromInt(raw_ppid);
        switch (ppid) {
            .dcep => {
                if (payload.len == 0) return Error.InvalidDcepMessage;
                const msg_type: DcepMessageType = std.enums.fromInt(DcepMessageType, payload[0]) orelse
                    return Error.InvalidDcepMessage;
                switch (msg_type) {
                    .open => {
                        const open_msg = try DcepOpenMessage.decode(payload);
                        const ordered = (open_msg.channel_type == .reliable or
                            open_msg.channel_type == .partial_reliable_rexmit or
                            open_msg.channel_type == .partial_reliable_timed);
                        const reliable = (open_msg.channel_type == .reliable or
                            open_msg.channel_type == .reliable_unordered);

                        const ch = try self.ensureChannel(
                            stream_id,
                            open_msg.label,
                            open_msg.protocol,
                            ordered,
                            reliable,
                            .open,
                            .{},
                        );
                        ch.state = .open;
                        ch.dcep_open_queued = true;

                        // Queue DCEP ACK on the same stream_id.
                        const ack_payload = encodeDcepAck();
                        const ssn = ch.next_ssn;
                        ch.next_ssn +%= 1;
                        const tsn = self.next_tsn;
                        self.next_tsn +%= 1;

                        const ack_chunk = DataChunk{
                            .unordered = false,
                            .beginning = true,
                            .ending = true,
                            .tsn = tsn,
                            .stream_id = stream_id,
                            .stream_seq = ssn,
                            .ppid = @intFromEnum(Ppid.dcep),
                            .user_data = &ack_payload,
                        };
                        const ack_bytes = try ack_chunk.encode(self.allocator);
                        defer self.allocator.free(ack_bytes);
                        try self.queueSingleChunkPacket(self.remote_verification_tag, ack_bytes);

                        if (ch.callbacks.on_state_change) |cb| {
                            cb(ch.callbacks.user_data, stream_id, .open);
                        }
                        if (self.callbacks.on_channel_open) |cb| {
                            cb(self.callbacks.user_data, stream_id, ch.label(), ch.protocol());
                        }
                    },
                    .ack => {
                        if (self.findChannel(stream_id)) |ch| {
                            ch.state = .open;
                            if (ch.callbacks.on_state_change) |cb| {
                                cb(ch.callbacks.user_data, stream_id, .open);
                            }
                        }
                    },
                }
            },
            .webrtc_string,
            .webrtc_string_partial,
            .webrtc_string_empty,
            .webrtc_binary,
            .webrtc_binary_partial,
            .webrtc_binary_empty,
            => {
                const is_binary = (ppid == .webrtc_binary or
                    ppid == .webrtc_binary_partial or
                    ppid == .webrtc_binary_empty);
                const is_empty = (ppid == .webrtc_string_empty or ppid == .webrtc_binary_empty);
                const effective_data: []const u8 = if (is_empty) &[_]u8{} else payload;

                const ch = try self.ensureChannel(
                    stream_id,
                    "",
                    "",
                    true,
                    true,
                    .open,
                    .{},
                );
                const duped = self.allocator.dupe(u8, effective_data) catch return Error.OutOfMemory;
                errdefer self.allocator.free(duped);
                ch.rx_messages.append(self.allocator, .{
                    .stream_id = stream_id,
                    .is_binary = is_binary,
                    .data = duped,
                }) catch return Error.OutOfMemory;

                if (ch.callbacks.on_message) |cb| {
                    cb(ch.callbacks.user_data, stream_id, effective_data, is_binary);
                }
                if (self.callbacks.on_message) |cb| {
                    cb(self.callbacks.user_data, stream_id, ch.label(), effective_data, is_binary);
                }
            },
            _ => {},
        }
    }
};

/// Exchanges all queued outbound SCTP packets between `assoc_a` and `assoc_b` until quiescent.
pub fn pumpLoopback(assoc_a: *Association, assoc_b: *Association) Error!void {
    var rounds: usize = 0;
    while (rounds < 1024) : (rounds += 1) {
        var progressed = false;

        while (assoc_a.pollOutboundPacket()) |pkt_a| {
            defer assoc_a.allocator.free(pkt_a);
            try assoc_b.handleInboundPacket(pkt_a);
            progressed = true;
        }

        while (assoc_b.pollOutboundPacket()) |pkt_b| {
            defer assoc_b.allocator.free(pkt_b);
            try assoc_a.handleInboundPacket(pkt_b);
            progressed = true;
        }

        if (!progressed) break;
    }
}

/// Performs the 4-way SCTP handshake (`INIT` -> `INIT_ACK` -> `COOKIE_ECHO` -> `COOKIE_ACK`)
/// between `assoc_a` and `assoc_b`.
pub fn connectLoopbackPair(assoc_a: *Association, assoc_b: *Association) Error!void {
    try assoc_a.startHandshake();
    try pumpLoopback(assoc_a, assoc_b);
}

test "SCTP CommonHeader and Castagnoli CRC32c calculation and tamper detection" {
    const allocator = std.testing.allocator;

    const cookie_ack = try CookieAckChunk.encode(allocator);
    defer allocator.free(cookie_ack);

    const chunks = [_][]const u8{cookie_ack};
    const pkt = try buildPacket(allocator, 5000, 5000, 0xCAFEBABE, &chunks);
    defer allocator.free(pkt);

    try std.testing.expectEqual(@as(usize, 16), pkt.len);
    const hdr = try CommonHeader.decode(pkt);
    try std.testing.expectEqual(@as(u16, 5000), hdr.src_port);
    try std.testing.expectEqual(@as(u16, 5000), hdr.dst_port);
    try std.testing.expectEqual(@as(u32, 0xCAFEBABE), hdr.verification_tag);
    try std.testing.expect(hdr.checksum != 0);
    try std.testing.expect(verifyChecksum(pkt));
    try std.testing.expectEqual(hdr.checksum, computeChecksum(pkt));

    // Flip a bit in the payload and verify tamper detection triggers
    pkt[12] ^= 0x01;
    try std.testing.expect(!verifyChecksum(pkt));
    pkt[12] ^= 0x01;
    try std.testing.expect(verifyChecksum(pkt));

    // Flip a bit in the verification tag and verify tamper detection triggers
    pkt[5] ^= 0x80;
    try std.testing.expect(!verifyChecksum(pkt));
}

test "WebRTC DCEP DATA_CHANNEL_OPEN and DATA_CHANNEL_ACK binary encoding and decoding" {
    const allocator = std.testing.allocator;

    const open_msg = DcepOpenMessage{
        .channel_type = .reliable,
        .priority = 256,
        .reliability_parameter = 0,
        .label = "adb-channel",
        .protocol = "webrtc-datachannel",
    };

    const encoded = try open_msg.encode(allocator);
    defer allocator.free(encoded);

    try std.testing.expectEqual(@as(u8, 0x03), encoded[0]);
    try std.testing.expectEqual(@as(u8, 0x00), encoded[1]);
    try std.testing.expectEqual(@as(usize, 12 + "adb-channel".len + "webrtc-datachannel".len), encoded.len);

    const decoded = try DcepOpenMessage.decode(encoded);
    try std.testing.expectEqual(ChannelType.reliable, decoded.channel_type);
    try std.testing.expectEqual(@as(u16, 256), decoded.priority);
    try std.testing.expectEqual(@as(u32, 0), decoded.reliability_parameter);
    try std.testing.expectEqualStrings("adb-channel", decoded.label);
    try std.testing.expectEqualStrings("webrtc-datachannel", decoded.protocol);

    const ack_bytes = encodeDcepAck();
    try std.testing.expectEqual(@as(usize, 1), ack_bytes.len);
    try std.testing.expectEqual(@as(u8, 0x02), ack_bytes[0]);
    try std.testing.expect(isDcepAck(&ack_bytes));
}

test "SCTP 4-way handshake (INIT -> INIT_ACK -> COOKIE_ECHO -> COOKIE_ACK), HEARTBEAT, and SHUTDOWN" {
    const allocator = std.testing.allocator;

    var assoc_a = Association.init(allocator, .{
        .local_verification_tag = 0x11112222,
        .initial_tsn = 100,
        .is_client = true,
    });
    defer assoc_a.deinit();

    var assoc_b = Association.init(allocator, .{
        .local_verification_tag = 0x33334444,
        .initial_tsn = 500,
        .is_client = false,
    });
    defer assoc_b.deinit();

    try std.testing.expectEqual(AssociationState.closed, assoc_a.state);
    try std.testing.expectEqual(AssociationState.closed, assoc_b.state);

    // Step 1: A emits INIT
    try assoc_a.startHandshake();
    try std.testing.expectEqual(AssociationState.cookie_wait, assoc_a.state);
    const init_pkt = assoc_a.pollOutboundPacket().?;
    defer allocator.free(init_pkt);

    // Step 2: B processes INIT and emits INIT_ACK
    try assoc_b.handleInboundPacket(init_pkt);
    const init_ack_pkt = assoc_b.pollOutboundPacket().?;
    defer allocator.free(init_ack_pkt);

    // Step 3: A processes INIT_ACK, transitions to cookie_echoed, and emits COOKIE_ECHO
    try assoc_a.handleInboundPacket(init_ack_pkt);
    try std.testing.expectEqual(AssociationState.cookie_echoed, assoc_a.state);
    const cookie_echo_pkt = assoc_a.pollOutboundPacket().?;
    defer allocator.free(cookie_echo_pkt);

    // Step 4: B processes COOKIE_ECHO, transitions to established, and emits COOKIE_ACK
    try assoc_b.handleInboundPacket(cookie_echo_pkt);
    try std.testing.expectEqual(AssociationState.established, assoc_b.state);
    const cookie_ack_pkt = assoc_b.pollOutboundPacket().?;
    defer allocator.free(cookie_ack_pkt);

    // Step 5: A processes COOKIE_ACK and transitions to established
    try assoc_a.handleInboundPacket(cookie_ack_pkt);
    try std.testing.expectEqual(AssociationState.established, assoc_a.state);

    // Test HEARTBEAT -> HEARTBEAT_ACK
    try assoc_a.sendHeartbeat("keepalive-probe-01");
    try pumpLoopback(&assoc_a, &assoc_b);
    try std.testing.expectEqual(@as(usize, 1), assoc_a.heartbeats_acked);

    // Test SHUTDOWN -> SHUTDOWN_ACK -> closed
    try assoc_a.initiateShutdown();
    try std.testing.expectEqual(AssociationState.shutdown_sent, assoc_a.state);
    try pumpLoopback(&assoc_a, &assoc_b);
    try std.testing.expectEqual(AssociationState.closed, assoc_a.state);
    try std.testing.expectEqual(AssociationState.shutdown_ack_sent, assoc_b.state);
}

test "Cuttlefish WebRTC DataChannels, fragmented binary (>1200 bytes), and empty messages (PPID 56/57)" {
    const allocator = std.testing.allocator;

    const cuttlefish_channels = [_][]const u8{
        "input-channel",
        "adb-channel",
        "control-channel",
        "sensors-channel",
        "lights-channel",
        "bluetooth-channel",
        "camera-data-channel",
    };

    var assoc_a = Association.init(allocator, .{
        .local_verification_tag = 0xAA000001,
        .initial_tsn = 1000,
        .max_payload_size = 1200,
        .is_client = true,
    });
    defer assoc_a.deinit();

    var assoc_b = Association.init(allocator, .{
        .local_verification_tag = 0xBB000002,
        .initial_tsn = 2000,
        .max_payload_size = 1200,
        .is_client = false,
    });
    defer assoc_b.deinit();

    try connectLoopbackPair(&assoc_a, &assoc_b);
    try std.testing.expectEqual(AssociationState.established, assoc_a.state);
    try std.testing.expectEqual(AssociationState.established, assoc_b.state);

    // Open all 7 Cuttlefish DataChannels via DCEP and exchange text + binary messages
    var stream_ids: [cuttlefish_channels.len]u16 = undefined;
    for (cuttlefish_channels, 0..) |ch_label, i| {
        stream_ids[i] = try assoc_a.openDataChannel(ch_label, .{}, .{});
    }
    try pumpLoopback(&assoc_a, &assoc_b);

    for (cuttlefish_channels, 0..) |ch_label, i| {
        const sid = stream_ids[i];
        const ch_a = assoc_a.findChannel(sid).?;
        const ch_b = assoc_b.findChannel(sid).?;
        try std.testing.expectEqual(State.open, ch_a.state);
        try std.testing.expectEqual(State.open, ch_b.state);
        try std.testing.expectEqualStrings(ch_label, ch_b.label());

        // Text message A -> B
        var txt_buf: [64]u8 = undefined;
        const msg_text = try std.fmt.bufPrint(&txt_buf, "hello-{s}", .{ch_label});
        try assoc_a.sendText(sid, msg_text);
        try pumpLoopback(&assoc_a, &assoc_b);

        try std.testing.expectEqual(@as(usize, 1), ch_b.rx_messages.items.len);
        try std.testing.expect(!ch_b.rx_messages.items[0].is_binary);
        try std.testing.expectEqualStrings(msg_text, ch_b.rx_messages.items[0].data);

        // Binary message B -> A
        const bin_data = [_]u8{ 0xAA, 0x55, @intCast(i), 0xFF };
        try assoc_b.sendBinary(sid, &bin_data);
        try pumpLoopback(&assoc_a, &assoc_b);

        try std.testing.expectEqual(@as(usize, 1), ch_a.rx_messages.items.len);
        try std.testing.expect(ch_a.rx_messages.items[0].is_binary);
        try std.testing.expectEqualSlices(u8, &bin_data, ch_a.rx_messages.items[0].data);
    }

    // Test multi-chunk fragmented binary payload (> 1200 bytes) on camera-data-channel
    const cam_sid = stream_ids[6];
    var large_payload: [3200]u8 = undefined;
    for (&large_payload, 0..) |*b, idx| {
        b.* = @truncate((idx * 37 + 13) & 0xFF);
    }
    try assoc_a.sendBinary(cam_sid, &large_payload);
    // 3200 bytes at max_payload_size=1200 produces 3 SCTP DATA packets (1200 + 1200 + 800)
    try std.testing.expectEqual(@as(usize, 3), assoc_a.outbound_packets.items.len);
    try pumpLoopback(&assoc_a, &assoc_b);

    const cam_ch_b = assoc_b.findChannel(cam_sid).?;
    try std.testing.expectEqual(@as(usize, 2), cam_ch_b.rx_messages.items.len);
    try std.testing.expect(cam_ch_b.rx_messages.items[1].is_binary);
    try std.testing.expectEqualSlices(u8, &large_payload, cam_ch_b.rx_messages.items[1].data);

    // Test empty text (PPID 56) and empty binary (PPID 57) messages on input-channel
    const input_sid = stream_ids[0];
    try assoc_a.sendText(input_sid, "");
    try assoc_a.sendBinary(input_sid, &.{});
    try pumpLoopback(&assoc_a, &assoc_b);

    const input_ch_b = assoc_b.findChannel(input_sid).?;
    try std.testing.expectEqual(@as(usize, 3), input_ch_b.rx_messages.items.len);
    try std.testing.expect(!input_ch_b.rx_messages.items[1].is_binary);
    try std.testing.expectEqual(@as(usize, 0), input_ch_b.rx_messages.items[1].data.len);
    try std.testing.expect(input_ch_b.rx_messages.items[2].is_binary);
    try std.testing.expectEqual(@as(usize, 0), input_ch_b.rx_messages.items[2].data.len);
}

test "SCTP SACK gap blocks, 4-byte chunk padding alignment, and callback listeners" {
    const allocator = std.testing.allocator;

    const gaps = [_]GapAckBlock{
        .{ .start = 2, .end = 4 },
        .{ .start = 7, .end = 9 },
    };
    const dups = [_]u32{ 1005, 1008 };
    const sack = SackChunk{
        .cumulative_tsn_ack = 1000,
        .a_rwnd = 65536,
        .gap_blocks = &gaps,
        .dup_tsns = &dups,
    };
    const sack_bytes = try sack.encode(allocator);
    defer allocator.free(sack_bytes);

    var decoded_sack = try SackChunk.decode(allocator, sack_bytes);
    defer decoded_sack.deinit(allocator);
    try std.testing.expectEqual(@as(u32, 1000), decoded_sack.cumulative_tsn_ack);
    try std.testing.expectEqual(@as(u32, 65536), decoded_sack.a_rwnd);
    try std.testing.expectEqual(@as(usize, 2), decoded_sack.gap_blocks.len);
    try std.testing.expectEqual(@as(u16, 2), decoded_sack.gap_blocks[0].start);
    try std.testing.expectEqual(@as(u16, 9), decoded_sack.gap_blocks[1].end);
    try std.testing.expectEqual(@as(usize, 2), decoded_sack.dup_tsns.len);
    try std.testing.expectEqual(@as(u32, 1008), decoded_sack.dup_tsns[1]);

    // Verify 4-byte padding alignment on odd-length DATA chunk (5 bytes -> 16+5=21 unpadded, 24 total)
    const data_chunk = DataChunk{
        .unordered = true,
        .beginning = true,
        .ending = true,
        .tsn = 42,
        .stream_id = 3,
        .stream_seq = 0,
        .ppid = @intFromEnum(Ppid.webrtc_string),
        .user_data = "hello",
    };
    const data_bytes = try data_chunk.encode(allocator);
    defer allocator.free(data_bytes);
    try std.testing.expectEqual(@as(usize, 24), data_bytes.len);

    const decoded_data = try DataChunk.decode(data_bytes);
    try std.testing.expect(decoded_data.unordered);
    try std.testing.expectEqualStrings("hello", decoded_data.user_data);
}

