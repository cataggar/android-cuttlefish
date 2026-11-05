const std = @import("std");

pub const DEFAULT_MTU: usize = 1200;
pub const RTP_VERSION: u2 = 2;
pub const RTP_FIXED_HEADER_LEN: usize = 12;
pub const ONE_BYTE_EXT_PROFILE: u16 = 0xBEDE;

pub const Error = error{
    BufferTooSmall,
    InvalidRtpPacket,
    UnsupportedRtpVersion,
    InvalidExtensionLength,
    InvalidPadding,
    InvalidRtcpPacket,
    UnexpectedRtcpType,
    InvalidVp8Descriptor,
    MissingStartPacket,
    InvalidH264Packet,
    NalUnitTooLarge,
    InvalidOpusPacket,
};

/// RFC 3550 / RFC 8285 RTP Header.
pub const Header = struct {
    version: u2 = RTP_VERSION,
    padding: bool = false,
    extension: bool = false,
    csrc_count: u4 = 0,
    marker: bool = false,
    payload_type: u7,
    sequence_number: u16,
    timestamp: u32,
    ssrc: u32,
    csrcs: [15]u32 = @splat(0),
    extension_profile: u16 = 0,
    extension_data: []const u8 = "",

    pub fn serializedSize(self: Header) Error!usize {
        var size: usize = RTP_FIXED_HEADER_LEN + (@as(usize, self.csrc_count) * 4);
        if (self.extension) {
            if (self.extension_data.len % 4 != 0) return error.InvalidExtensionLength;
            size += 4 + self.extension_data.len;
        }
        return size;
    }
};

/// An owned RTP packet produced by codec packetizers.
pub const OwnedRtpPacket = struct {
    header: Header,
    payload: []u8,

    pub fn asPacket(self: OwnedRtpPacket) Packet {
        return .{
            .header = self.header,
            .payload = self.payload,
        };
    }

    pub fn deinit(self: OwnedRtpPacket, allocator: std.mem.Allocator) void {
        allocator.free(self.payload);
    }
};

pub fn freeOwnedPackets(allocator: std.mem.Allocator, packets: []OwnedRtpPacket) void {
    for (packets) |pkt| {
        pkt.deinit(allocator);
    }
    allocator.free(packets);
}

/// RFC 3550 RTP Packet view.
pub const Packet = struct {
    header: Header,
    payload: []const u8,

    pub fn parse(raw: []const u8) Error!Packet {
        if (raw.len < RTP_FIXED_HEADER_LEN) return error.InvalidRtpPacket;

        const b0 = raw[0];
        const b1 = raw[1];
        const version: u2 = @intCast(b0 >> 6);
        if (version != RTP_VERSION) return error.UnsupportedRtpVersion;

        const padding = ((b0 >> 5) & 0x01) != 0;
        const extension = ((b0 >> 4) & 0x01) != 0;
        const csrc_count: u4 = @intCast(b0 & 0x0F);
        const marker = (b1 >> 7) != 0;
        const payload_type: u7 = @intCast(b1 & 0x7F);

        const sequence_number = std.mem.readInt(u16, raw[2..4], .big);
        const timestamp = std.mem.readInt(u32, raw[4..8], .big);
        const ssrc = std.mem.readInt(u32, raw[8..12], .big);

        var offset: usize = RTP_FIXED_HEADER_LEN;
        const csrc_bytes = @as(usize, csrc_count) * 4;
        if (raw.len < offset + csrc_bytes) return error.InvalidRtpPacket;

        var csrcs: [15]u32 = @splat(0);
        for (0..csrc_count) |i| {
            const start = offset + i * 4;
            csrcs[i] = std.mem.readInt(u32, raw[start..][0..4], .big);
        }
        offset += csrc_bytes;

        var extension_profile: u16 = 0;
        var extension_data: []const u8 = "";
        if (extension) {
            if (raw.len < offset + 4) return error.InvalidRtpPacket;
            extension_profile = std.mem.readInt(u16, raw[offset..][0..2], .big);
            const ext_words = std.mem.readInt(u16, raw[offset + 2 ..][0..2], .big);
            offset += 4;
            const ext_bytes = @as(usize, ext_words) * 4;
            if (raw.len < offset + ext_bytes) return error.InvalidRtpPacket;
            extension_data = raw[offset .. offset + ext_bytes];
            offset += ext_bytes;
        }

        var payload_end: usize = raw.len;
        if (padding) {
            if (raw.len <= offset) return error.InvalidPadding;
            const pad_len: usize = raw[raw.len - 1];
            if (pad_len == 0 or offset + pad_len > raw.len) return error.InvalidPadding;
            payload_end = raw.len - pad_len;
        }

        return .{
            .header = .{
                .version = version,
                .padding = padding,
                .extension = extension,
                .csrc_count = csrc_count,
                .marker = marker,
                .payload_type = payload_type,
                .sequence_number = sequence_number,
                .timestamp = timestamp,
                .ssrc = ssrc,
                .csrcs = csrcs,
                .extension_profile = extension_profile,
                .extension_data = extension_data,
            },
            .payload = raw[offset..payload_end],
        };
    }

    pub fn serialize(self: Packet, out_buf: []u8) Error![]const u8 {
        const hdr_size = try self.header.serializedSize();
        const total_size = hdr_size + self.payload.len;
        if (out_buf.len < total_size) return error.BufferTooSmall;

        out_buf[0] = (@as(u8, self.header.version) << 6) |
            (@as(u8, @intFromBool(self.header.padding)) << 5) |
            (@as(u8, @intFromBool(self.header.extension)) << 4) |
            @as(u8, self.header.csrc_count);
        out_buf[1] = (@as(u8, @intFromBool(self.header.marker)) << 7) |
            @as(u8, self.header.payload_type);
        std.mem.writeInt(u16, out_buf[2..4], self.header.sequence_number, .big);
        std.mem.writeInt(u32, out_buf[4..8], self.header.timestamp, .big);
        std.mem.writeInt(u32, out_buf[8..12], self.header.ssrc, .big);

        var offset: usize = RTP_FIXED_HEADER_LEN;
        for (0..self.header.csrc_count) |i| {
            std.mem.writeInt(u32, out_buf[offset..][0..4], self.header.csrcs[i], .big);
            offset += 4;
        }

        if (self.header.extension) {
            const ext_words: u16 = @intCast(self.header.extension_data.len / 4);
            std.mem.writeInt(u16, out_buf[offset..][0..2], self.header.extension_profile, .big);
            std.mem.writeInt(u16, out_buf[offset + 2 ..][0..2], ext_words, .big);
            offset += 4;
            @memcpy(out_buf[offset .. offset + self.header.extension_data.len], self.header.extension_data);
            offset += self.header.extension_data.len;
        }

        @memcpy(out_buf[offset .. offset + self.payload.len], self.payload);
        offset += self.payload.len;
        return out_buf[0..offset];
    }
};

/// Monotonic RTP sequence number and sender stats tracker.
pub const RtpSequencer = struct {
    ssrc: u32,
    payload_type: u7,
    next_seq: u16 = 1,
    packets_sent: u64 = 0,
    bytes_sent: u64 = 0,

    pub fn nextHeader(self: *RtpSequencer, timestamp: u32, marker: bool) Header {
        const seq = self.next_seq;
        self.next_seq +%= 1;
        self.packets_sent += 1;
        return .{
            .payload_type = self.payload_type,
            .sequence_number = seq,
            .timestamp = timestamp,
            .ssrc = self.ssrc,
            .marker = marker,
        };
    }

    pub fn recordBytes(self: *RtpSequencer, payload_bytes: usize) void {
        self.bytes_sent += payload_bytes;
    }
};

// ============================================================================
// RTCP Framing & Feedback (RFC 3550 & RFC 4585)
// ============================================================================

pub const RtcpPacketType = enum(u8) {
    sr = 200,
    rr = 201,
    sdes = 202,
    bye = 203,
    app = 204,
    rtpfb = 205,
    psfb = 206,
    _,
};

pub const RtcpFeedbackFmt = struct {
    pub const NACK: u5 = 1;
    pub const TWCC: u5 = 15;
    pub const PLI: u5 = 1;
    pub const FIR: u5 = 4;
};

pub const ReportBlock = struct {
    ssrc: u32,
    fraction_lost: u8 = 0,
    cumulative_lost: u24 = 0,
    extended_highest_seq: u32,
    jitter: u32 = 0,
    lsr: u32 = 0,
    dlsr: u32 = 0,

    pub const SIZE: usize = 24;

    pub fn encode(self: ReportBlock, out: *[SIZE]u8) void {
        std.mem.writeInt(u32, out[0..4], self.ssrc, .big);
        out[4] = self.fraction_lost;
        out[5] = @intCast((self.cumulative_lost >> 16) & 0xFF);
        out[6] = @intCast((self.cumulative_lost >> 8) & 0xFF);
        out[7] = @intCast(self.cumulative_lost & 0xFF);
        std.mem.writeInt(u32, out[8..12], self.extended_highest_seq, .big);
        std.mem.writeInt(u32, out[12..16], self.jitter, .big);
        std.mem.writeInt(u32, out[16..20], self.lsr, .big);
        std.mem.writeInt(u32, out[20..24], self.dlsr, .big);
    }

    pub fn decode(raw: *const [SIZE]u8) ReportBlock {
        const cum_lost: u24 = (@as(u24, raw[5]) << 16) | (@as(u24, raw[6]) << 8) | @as(u24, raw[7]);
        return .{
            .ssrc = std.mem.readInt(u32, raw[0..4], .big),
            .fraction_lost = raw[4],
            .cumulative_lost = cum_lost,
            .extended_highest_seq = std.mem.readInt(u32, raw[8..12], .big),
            .jitter = std.mem.readInt(u32, raw[12..16], .big),
            .lsr = std.mem.readInt(u32, raw[16..20], .big),
            .dlsr = std.mem.readInt(u32, raw[20..24], .big),
        };
    }
};

pub const SenderReport = struct {
    sender_ssrc: u32,
    ntp_timestamp: u64,
    rtp_timestamp: u32,
    sender_packet_count: u32,
    sender_octet_count: u32,
    reports: []const ReportBlock = &.{},

    pub fn deinit(self: SenderReport, allocator: std.mem.Allocator) void {
        if (self.reports.len > 0) allocator.free(self.reports);
    }
};

pub const ReceiverReport = struct {
    sender_ssrc: u32,
    reports: []const ReportBlock = &.{},

    pub fn deinit(self: ReceiverReport, allocator: std.mem.Allocator) void {
        if (self.reports.len > 0) allocator.free(self.reports);
    }
};

fn writeRtcpHeader(out: *[4]u8, count_or_fmt: u5, pt: RtcpPacketType, total_bytes: usize) void {
    out[0] = (@as(u8, RTP_VERSION) << 6) | @as(u8, count_or_fmt);
    out[1] = @intFromEnum(pt);
    const length_words: u16 = @intCast((total_bytes / 4) - 1);
    std.mem.writeInt(u16, out[2..4], length_words, .big);
}

const RtcpCommonHeader = struct {
    count_or_fmt: u5,
    padding: bool,
    packet_type: RtcpPacketType,
    total_bytes: usize,
};

fn parseRtcpHeader(raw: []const u8) Error!RtcpCommonHeader {
    if (raw.len < 8 or (raw.len % 4) != 0) return error.InvalidRtcpPacket;
    const version: u2 = @intCast(raw[0] >> 6);
    if (version != RTP_VERSION) return error.InvalidRtcpPacket;
    const padding = ((raw[0] >> 5) & 0x01) != 0;
    const count_or_fmt: u5 = @intCast(raw[0] & 0x1F);
    const pt: RtcpPacketType = @enumFromInt(raw[1]);
    const length_words = std.mem.readInt(u16, raw[2..4], .big);
    const total_bytes = (@as(usize, length_words) + 1) * 4;
    if (raw.len < total_bytes) return error.InvalidRtcpPacket;
    return .{
        .count_or_fmt = count_or_fmt,
        .padding = padding,
        .packet_type = pt,
        .total_bytes = total_bytes,
    };
}

pub fn encodeSr(sr: SenderReport, out_buf: []u8) Error![]const u8 {
    if (sr.reports.len > 31) return error.InvalidRtcpPacket;
    const total_bytes: usize = 28 + sr.reports.len * ReportBlock.SIZE;
    if (out_buf.len < total_bytes) return error.BufferTooSmall;

    writeRtcpHeader(out_buf[0..4], @intCast(sr.reports.len), .sr, total_bytes);
    std.mem.writeInt(u32, out_buf[4..8], sr.sender_ssrc, .big);
    std.mem.writeInt(u64, out_buf[8..16], sr.ntp_timestamp, .big);
    std.mem.writeInt(u32, out_buf[16..20], sr.rtp_timestamp, .big);
    std.mem.writeInt(u32, out_buf[20..24], sr.sender_packet_count, .big);
    std.mem.writeInt(u32, out_buf[24..28], sr.sender_octet_count, .big);

    var offset: usize = 28;
    for (sr.reports) |rb| {
        rb.encode(out_buf[offset..][0..ReportBlock.SIZE]);
        offset += ReportBlock.SIZE;
    }
    return out_buf[0..total_bytes];
}

pub fn decodeSr(allocator: std.mem.Allocator, raw: []const u8) (Error || std.mem.Allocator.Error)!SenderReport {
    const hdr = try parseRtcpHeader(raw);
    if (hdr.packet_type != .sr) return error.UnexpectedRtcpType;
    const rc = @as(usize, hdr.count_or_fmt);
    const expected_len = 28 + rc * ReportBlock.SIZE;
    if (hdr.total_bytes < expected_len) return error.InvalidRtcpPacket;

    const reports: []ReportBlock = if (rc > 0) try allocator.alloc(ReportBlock, rc) else &.{};
    var offset: usize = 28;
    for (0..rc) |i| {
        reports[i] = ReportBlock.decode(raw[offset..][0..ReportBlock.SIZE]);
        offset += ReportBlock.SIZE;
    }

    return .{
        .sender_ssrc = std.mem.readInt(u32, raw[4..8], .big),
        .ntp_timestamp = std.mem.readInt(u64, raw[8..16], .big),
        .rtp_timestamp = std.mem.readInt(u32, raw[16..20], .big),
        .sender_packet_count = std.mem.readInt(u32, raw[20..24], .big),
        .sender_octet_count = std.mem.readInt(u32, raw[24..28], .big),
        .reports = reports,
    };
}

pub fn encodeRr(rr: ReceiverReport, out_buf: []u8) Error![]const u8 {
    if (rr.reports.len > 31) return error.InvalidRtcpPacket;
    const total_bytes: usize = 8 + rr.reports.len * ReportBlock.SIZE;
    if (out_buf.len < total_bytes) return error.BufferTooSmall;

    writeRtcpHeader(out_buf[0..4], @intCast(rr.reports.len), .rr, total_bytes);
    std.mem.writeInt(u32, out_buf[4..8], rr.sender_ssrc, .big);

    var offset: usize = 8;
    for (rr.reports) |rb| {
        rb.encode(out_buf[offset..][0..ReportBlock.SIZE]);
        offset += ReportBlock.SIZE;
    }
    return out_buf[0..total_bytes];
}

pub fn decodeRr(allocator: std.mem.Allocator, raw: []const u8) (Error || std.mem.Allocator.Error)!ReceiverReport {
    const hdr = try parseRtcpHeader(raw);
    if (hdr.packet_type != .rr) return error.UnexpectedRtcpType;
    const rc = @as(usize, hdr.count_or_fmt);
    const expected_len = 8 + rc * ReportBlock.SIZE;
    if (hdr.total_bytes < expected_len) return error.InvalidRtcpPacket;

    const reports: []ReportBlock = if (rc > 0) try allocator.alloc(ReportBlock, rc) else &.{};
    var offset: usize = 8;
    for (0..rc) |i| {
        reports[i] = ReportBlock.decode(raw[offset..][0..ReportBlock.SIZE]);
        offset += ReportBlock.SIZE;
    }

    return .{
        .sender_ssrc = std.mem.readInt(u32, raw[4..8], .big),
        .reports = reports,
    };
}

/// Picture Loss Indication (RFC 4585 Section 6.3.1, PSFB FMT=1).
pub fn encodePli(sender_ssrc: u32, media_ssrc: u32, out_buf: []u8) Error![]const u8 {
    if (out_buf.len < 12) return error.BufferTooSmall;
    writeRtcpHeader(out_buf[0..4], RtcpFeedbackFmt.PLI, .psfb, 12);
    std.mem.writeInt(u32, out_buf[4..8], sender_ssrc, .big);
    std.mem.writeInt(u32, out_buf[8..12], media_ssrc, .big);
    return out_buf[0..12];
}

pub fn isPli(raw: []const u8) bool {
    const hdr = parseRtcpHeader(raw) catch return false;
    return hdr.packet_type == .psfb and hdr.count_or_fmt == RtcpFeedbackFmt.PLI and hdr.total_bytes == 12;
}

/// Full Intra Request (RFC 5104 Section 4.3.1, PSFB FMT=4).
pub fn encodeFir(sender_ssrc: u32, media_ssrc: u32, seq_nr: u8, out_buf: []u8) Error![]const u8 {
    if (out_buf.len < 20) return error.BufferTooSmall;
    writeRtcpHeader(out_buf[0..4], RtcpFeedbackFmt.FIR, .psfb, 20);
    std.mem.writeInt(u32, out_buf[4..8], sender_ssrc, .big);
    std.mem.writeInt(u32, out_buf[8..12], 0, .big);
    std.mem.writeInt(u32, out_buf[12..16], media_ssrc, .big);
    out_buf[16] = seq_nr;
    out_buf[17] = 0;
    out_buf[18] = 0;
    out_buf[19] = 0;
    return out_buf[0..20];
}

pub fn isFir(raw: []const u8) bool {
    const hdr = parseRtcpHeader(raw) catch return false;
    return hdr.packet_type == .psfb and hdr.count_or_fmt == RtcpFeedbackFmt.FIR and hdr.total_bytes >= 20;
}

/// Generic NACK (RFC 4585 Section 6.2.1, RTPFB FMT=1).
/// Encodes lost sequence numbers into (PID, BLP) 32-bit FCI words.
pub fn encodeNack(
    sender_ssrc: u32,
    media_ssrc: u32,
    lost_seqs: []const u16,
    out_buf: []u8,
) Error![]const u8 {
    if (lost_seqs.len == 0) return error.InvalidRtcpPacket;
    if (out_buf.len < 16) return error.BufferTooSmall;

    std.mem.writeInt(u32, out_buf[4..8], sender_ssrc, .big);
    std.mem.writeInt(u32, out_buf[8..12], media_ssrc, .big);

    var offset: usize = 12;
    var i: usize = 0;
    while (i < lost_seqs.len) {
        if (offset + 4 > out_buf.len) return error.BufferTooSmall;
        const pid = lost_seqs[i];
        var blp: u16 = 0;
        i += 1;

        while (i < lost_seqs.len) {
            const diff = lost_seqs[i] -% pid;
            if (diff == 0) {
                i += 1;
                continue;
            }
            if (diff <= 16) {
                const bit: u4 = @intCast(diff - 1);
                blp |= (@as(u16, 1) << bit);
                i += 1;
            } else {
                break;
            }
        }

        std.mem.writeInt(u16, out_buf[offset..][0..2], pid, .big);
        std.mem.writeInt(u16, out_buf[offset + 2 ..][0..2], blp, .big);
        offset += 4;
    }

    writeRtcpHeader(out_buf[0..4], RtcpFeedbackFmt.NACK, .rtpfb, offset);
    return out_buf[0..offset];
}

/// Decodes a Generic NACK packet and returns the allocated list of lost sequence numbers.
pub fn decodeNack(
    allocator: std.mem.Allocator,
    raw: []const u8,
) (Error || std.mem.Allocator.Error)![]u16 {
    const hdr = try parseRtcpHeader(raw);
    if (hdr.packet_type != .rtpfb or hdr.count_or_fmt != RtcpFeedbackFmt.NACK) {
        return error.UnexpectedRtcpType;
    }
    if (hdr.total_bytes < 16 or (hdr.total_bytes % 4) != 0) {
        return error.InvalidRtcpPacket;
    }

    var lost: std.ArrayList(u16) = .empty;
    errdefer lost.deinit(allocator);

    var offset: usize = 12;
    while (offset < hdr.total_bytes) : (offset += 4) {
        const pid = std.mem.readInt(u16, raw[offset..][0..2], .big);
        const blp = std.mem.readInt(u16, raw[offset + 2 ..][0..2], .big);
        try lost.append(allocator, pid);
        for (0..16) |bit| {
            if ((blp & (@as(u16, 1) << @intCast(bit))) != 0) {
                try lost.append(allocator, pid +% (@as(u16, @intCast(bit)) + 1));
            }
        }
    }

    return lost.toOwnedSlice(allocator);
}

// ============================================================================
// VP8 RTP Packetizer & Depacketizer (RFC 7741)
// ============================================================================

pub const Vp8PayloadDescriptor = struct {
    non_reference: bool = false,
    start_of_partition: bool = false,
    partition_index: u3 = 0,
    picture_id: ?u15 = null,
    tl0_pic_idx: ?u8 = null,
    tid: ?u2 = null,
    layer_sync: bool = false,
    key_idx: ?u5 = null,
    descriptor_len: usize = 1,
    is_keyframe: bool = false,
};

pub const Vp8Packetizer = struct {
    sequencer: *RtpSequencer,
    max_payload_len: usize = DEFAULT_MTU,
    picture_id: u15 = 0,

    pub const DESCRIPTOR_LEN: usize = 4; // Byte0 (X=1) + ExtByte (I=1) + 2-byte PictureID (M=1)

    pub fn packetize(
        self: *Vp8Packetizer,
        allocator: std.mem.Allocator,
        frame: []const u8,
        timestamp: u32,
        non_reference: bool,
    ) (Error || std.mem.Allocator.Error)![]OwnedRtpPacket {
        if (frame.len == 0) return error.InvalidVp8Descriptor;
        if (self.max_payload_len <= DESCRIPTOR_LEN) return error.BufferTooSmall;

        const max_chunk = self.max_payload_len - DESCRIPTOR_LEN;
        const packet_count = (frame.len + max_chunk - 1) / max_chunk;
        const pic_id = self.picture_id;
        self.picture_id +%= 1;

        var packets: std.ArrayList(OwnedRtpPacket) = .empty;
        errdefer {
            for (packets.items) |pkt| pkt.deinit(allocator);
            packets.deinit(allocator);
        }

        var offset: usize = 0;
        var pkt_idx: usize = 0;
        while (offset < frame.len) : (pkt_idx += 1) {
            const chunk_len = @min(max_chunk, frame.len - offset);
            const is_first = (pkt_idx == 0);
            const is_last = (offset + chunk_len == frame.len);

            var payload = try allocator.alloc(u8, DESCRIPTOR_LEN + chunk_len);
            errdefer allocator.free(payload);

            // RFC 7741 Section 4.2: X=1 (0x80), N bit (0x20), S bit (0x10 on first packet), PID=0
            payload[0] = 0x80 |
                (if (non_reference) @as(u8, 0x20) else 0) |
                (if (is_first) @as(u8, 0x10) else 0);
            // Extension byte: I=1 (0x80), L=0, T=0, K=0
            payload[1] = 0x80;
            // 15-bit PictureID with M=1 (0x80)
            payload[2] = 0x80 | @as(u8, @intCast((pic_id >> 8) & 0x7F));
            payload[3] = @as(u8, @intCast(pic_id & 0xFF));
            @memcpy(payload[DESCRIPTOR_LEN..], frame[offset .. offset + chunk_len]);

            const hdr = self.sequencer.nextHeader(timestamp, is_last);
            self.sequencer.recordBytes(payload.len);
            try packets.append(allocator, .{
                .header = hdr,
                .payload = payload,
            });

            offset += chunk_len;
        }

        std.debug.assert(packets.items.len == packet_count);
        return packets.toOwnedSlice(allocator);
    }
};

pub const Vp8Depacketizer = struct {
    frame_buf: std.ArrayList(u8) = .empty,
    is_keyframe: bool = false,
    picture_id: ?u15 = null,
    in_frame: bool = false,

    pub const Frame = struct {
        data: []u8,
        is_keyframe: bool,
        picture_id: ?u15,

        pub fn deinit(self: Frame, allocator: std.mem.Allocator) void {
            allocator.free(self.data);
        }
    };

    pub fn deinit(self: *Vp8Depacketizer, allocator: std.mem.Allocator) void {
        self.frame_buf.deinit(allocator);
    }

    pub fn parseDescriptor(payload: []const u8) Error!Vp8PayloadDescriptor {
        if (payload.len == 0) return error.InvalidVp8Descriptor;

        const b0 = payload[0];
        const has_ext = (b0 & 0x80) != 0;
        const non_ref = (b0 & 0x20) != 0;
        const start_of_partition = (b0 & 0x10) != 0;
        const part_idx: u3 = @intCast(b0 & 0x07);

        var offset: usize = 1;
        var picture_id: ?u15 = null;
        var tl0_pic_idx: ?u8 = null;
        var tid: ?u2 = null;
        var layer_sync = false;
        var key_idx: ?u5 = null;

        if (has_ext) {
            if (payload.len <= offset) return error.InvalidVp8Descriptor;
            const ext = payload[offset];
            offset += 1;

            const has_i = (ext & 0x80) != 0;
            const has_l = (ext & 0x40) != 0;
            const has_t = (ext & 0x20) != 0;
            const has_k = (ext & 0x10) != 0;

            if (has_i) {
                if (payload.len <= offset) return error.InvalidVp8Descriptor;
                const pid_b0 = payload[offset];
                offset += 1;
                if ((pid_b0 & 0x80) != 0) {
                    if (payload.len <= offset) return error.InvalidVp8Descriptor;
                    const pid_b1 = payload[offset];
                    offset += 1;
                    picture_id = (@as(u15, @intCast(pid_b0 & 0x7F)) << 8) | @as(u15, pid_b1);
                } else {
                    picture_id = @intCast(pid_b0 & 0x7F);
                }
            }

            if (has_l) {
                if (payload.len <= offset) return error.InvalidVp8Descriptor;
                tl0_pic_idx = payload[offset];
                offset += 1;
            }

            if (has_t or has_k) {
                if (payload.len <= offset) return error.InvalidVp8Descriptor;
                const tk = payload[offset];
                offset += 1;
                if (has_t) {
                    tid = @intCast(tk >> 6);
                    layer_sync = (tk & 0x20) != 0;
                }
                if (has_k) {
                    key_idx = @intCast(tk & 0x1F);
                }
            }
        }

        if (payload.len <= offset) return error.InvalidVp8Descriptor;

        // RFC 7741 Section 4.3: Bit 0 of first VP8 payload byte (when S=1 and PID=0) is P (0 = keyframe)
        const is_keyframe = start_of_partition and part_idx == 0 and ((payload[offset] & 0x01) == 0);

        return .{
            .non_reference = non_ref,
            .start_of_partition = start_of_partition,
            .partition_index = part_idx,
            .picture_id = picture_id,
            .tl0_pic_idx = tl0_pic_idx,
            .tid = tid,
            .layer_sync = layer_sync,
            .key_idx = key_idx,
            .descriptor_len = offset,
            .is_keyframe = is_keyframe,
        };
    }

    pub fn push(
        self: *Vp8Depacketizer,
        allocator: std.mem.Allocator,
        packet: Packet,
    ) (Error || std.mem.Allocator.Error)!?Frame {
        const desc = try parseDescriptor(packet.payload);
        if (desc.start_of_partition and desc.partition_index == 0) {
            self.frame_buf.clearRetainingCapacity();
            self.is_keyframe = desc.is_keyframe;
            self.picture_id = desc.picture_id;
            self.in_frame = true;
        } else if (!self.in_frame) {
            return error.MissingStartPacket;
        }

        try self.frame_buf.appendSlice(allocator, packet.payload[desc.descriptor_len..]);

        if (packet.header.marker) {
            self.in_frame = false;
            return Frame{
                .data = try self.frame_buf.toOwnedSlice(allocator),
                .is_keyframe = self.is_keyframe,
                .picture_id = self.picture_id,
            };
        }
        return null;
    }
};

// ============================================================================
// H.264 RTP Packetizer & Depacketizer (RFC 6184)
// ============================================================================

pub const H264PacketizationMode = enum {
    non_interleaved,
    single_nal_unit,
};

pub const H264NalType = struct {
    pub const SLICE: u5 = 1;
    pub const IDR: u5 = 5;
    pub const SEI: u5 = 6;
    pub const SPS: u5 = 7;
    pub const PPS: u5 = 8;
    pub const AUD: u5 = 9;
    pub const STAP_A: u5 = 24;
    pub const FU_A: u5 = 28;
};

pub const ANNEXB_START_CODE = [_]u8{ 0x00, 0x00, 0x00, 0x01 };

fn findStartCode(data: []const u8, from: usize) ?struct { pos: usize, len: usize } {
    var i = from;
    while (i + 3 <= data.len) : (i += 1) {
        if (data[i] == 0x00 and data[i + 1] == 0x00) {
            if (data[i + 2] == 0x01) {
                return .{ .pos = i, .len = 3 };
            }
            if (i + 4 <= data.len and data[i + 2] == 0x00 and data[i + 3] == 0x01) {
                return .{ .pos = i, .len = 4 };
            }
        }
    }
    return null;
}

/// Splits an Annex-B bitstream (`00 00 01` or `00 00 00 01` start codes, or raw single NAL)
/// into slices pointing to individual NAL units.
pub fn splitAnnexBNals(
    allocator: std.mem.Allocator,
    data: []const u8,
) std.mem.Allocator.Error![][]const u8 {
    var nals: std.ArrayList([]const u8) = .empty;
    errdefer nals.deinit(allocator);

    const first = findStartCode(data, 0) orelse {
        if (data.len > 0) {
            try nals.append(allocator, data);
        }
        return nals.toOwnedSlice(allocator);
    };

    var nal_start = first.pos + first.len;
    while (nal_start < data.len) {
        if (findStartCode(data, nal_start)) |next_sc| {
            if (next_sc.pos > nal_start) {
                try nals.append(allocator, data[nal_start..next_sc.pos]);
            }
            nal_start = next_sc.pos + next_sc.len;
        } else {
            try nals.append(allocator, data[nal_start..]);
            break;
        }
    }

    return nals.toOwnedSlice(allocator);
}

fn isAggregatableNal(nal: []const u8) bool {
    if (nal.len == 0) return false;
    const nal_type: u5 = @intCast(nal[0] & 0x1F);
    return nal_type == H264NalType.SPS or
        nal_type == H264NalType.PPS or
        nal_type == H264NalType.SEI or
        nal_type == H264NalType.AUD;
}

pub const H264Packetizer = struct {
    sequencer: *RtpSequencer,
    mode: H264PacketizationMode = .non_interleaved,
    max_payload_len: usize = DEFAULT_MTU,

    pub fn packetize(
        self: *H264Packetizer,
        allocator: std.mem.Allocator,
        annexb_or_raw: []const u8,
        timestamp: u32,
    ) (Error || std.mem.Allocator.Error)![]OwnedRtpPacket {
        const nals = try splitAnnexBNals(allocator, annexb_or_raw);
        defer allocator.free(nals);
        if (nals.len == 0) return error.InvalidH264Packet;

        var payloads: std.ArrayList([]u8) = .empty;
        errdefer {
            for (payloads.items) |p| allocator.free(p);
            payloads.deinit(allocator);
        }

        switch (self.mode) {
            .single_nal_unit => {
                for (nals) |nal| {
                    if (nal.len == 0) continue;
                    if (nal.len > self.max_payload_len) return error.NalUnitTooLarge;
                    const buf = try allocator.dupe(u8, nal);
                    try payloads.append(allocator, buf);
                }
            },
            .non_interleaved => {
                var i: usize = 0;
                while (i < nals.len) {
                    const nal = nals[i];
                    if (nal.len == 0) {
                        i += 1;
                        continue;
                    }

                    // Try STAP-A aggregation for consecutive small parameter-set NALs
                    if (isAggregatableNal(nal) and (1 + 2 + nal.len <= self.max_payload_len)) {
                        var group_end = i + 1;
                        var stap_len: usize = 1 + 2 + nal.len;
                        var max_nri: u8 = nal[0] & 0x60;
                        var forbidden_zero: u8 = nal[0] & 0x80;

                        while (group_end < nals.len) {
                            const next_nal = nals[group_end];
                            if (!isAggregatableNal(next_nal)) break;
                            if (stap_len + 2 + next_nal.len > self.max_payload_len) break;
                            stap_len += 2 + next_nal.len;
                            if ((next_nal[0] & 0x60) > max_nri) max_nri = next_nal[0] & 0x60;
                            forbidden_zero |= (next_nal[0] & 0x80);
                            group_end += 1;
                        }

                        if (group_end - i >= 2) {
                            var stap = try allocator.alloc(u8, stap_len);
                            errdefer allocator.free(stap);
                            stap[0] = forbidden_zero | max_nri | @as(u8, H264NalType.STAP_A);
                            var offset: usize = 1;
                            for (nals[i..group_end]) |part| {
                                std.mem.writeInt(u16, stap[offset..][0..2], @intCast(part.len), .big);
                                offset += 2;
                                @memcpy(stap[offset .. offset + part.len], part);
                                offset += part.len;
                            }
                            try payloads.append(allocator, stap);
                            i = group_end;
                            continue;
                        }
                    }

                    if (nal.len <= self.max_payload_len) {
                        // Single NAL Unit packet (types 1..23)
                        const buf = try allocator.dupe(u8, nal);
                        try payloads.append(allocator, buf);
                    } else {
                        // FU-A fragmentation (type 28)
                        if (self.max_payload_len <= 2) return error.BufferTooSmall;
                        const nal_hdr = nal[0];
                        const fu_indicator: u8 = (nal_hdr & 0xE0) | @as(u8, H264NalType.FU_A);
                        const orig_type: u8 = nal_hdr & 0x1F;
                        const body = nal[1..];
                        const max_frag = self.max_payload_len - 2;

                        var offset: usize = 0;
                        while (offset < body.len) {
                            const frag_len = @min(max_frag, body.len - offset);
                            const is_start = (offset == 0);
                            const is_end = (offset + frag_len == body.len);
                            const fu_header: u8 = (if (is_start) @as(u8, 0x80) else 0) |
                                (if (is_end) @as(u8, 0x40) else 0) |
                                orig_type;

                            var frag = try allocator.alloc(u8, 2 + frag_len);
                            errdefer allocator.free(frag);
                            frag[0] = fu_indicator;
                            frag[1] = fu_header;
                            @memcpy(frag[2..], body[offset .. offset + frag_len]);
                            try payloads.append(allocator, frag);
                            offset += frag_len;
                        }
                    }
                    i += 1;
                }
            },
        }

        if (payloads.items.len == 0) return error.InvalidH264Packet;

        var packets = try allocator.alloc(OwnedRtpPacket, payloads.items.len);
        for (payloads.items, 0..) |pl, idx| {
            const is_last = (idx + 1 == payloads.items.len);
            const hdr = self.sequencer.nextHeader(timestamp, is_last);
            self.sequencer.recordBytes(pl.len);
            packets[idx] = .{
                .header = hdr,
                .payload = pl,
            };
        }
        payloads.deinit(allocator);
        return packets;
    }
};

pub const H264Depacketizer = struct {
    access_unit_buf: std.ArrayList(u8) = .empty,
    fu_buf: std.ArrayList(u8) = .empty,
    in_fu: bool = false,

    pub fn deinit(self: *H264Depacketizer, allocator: std.mem.Allocator) void {
        self.access_unit_buf.deinit(allocator);
        self.fu_buf.deinit(allocator);
    }

    pub fn push(
        self: *H264Depacketizer,
        allocator: std.mem.Allocator,
        packet: Packet,
    ) (Error || std.mem.Allocator.Error)!?[]u8 {
        if (packet.payload.len == 0) return error.InvalidH264Packet;
        const b0 = packet.payload[0];
        const nal_type: u5 = @intCast(b0 & 0x1F);

        switch (nal_type) {
            1...23 => {
                if (self.in_fu) return error.InvalidH264Packet;
                try self.access_unit_buf.appendSlice(allocator, &ANNEXB_START_CODE);
                try self.access_unit_buf.appendSlice(allocator, packet.payload);
            },
            H264NalType.STAP_A => {
                if (self.in_fu) return error.InvalidH264Packet;
                if (packet.payload.len < 3) return error.InvalidH264Packet;
                var offset: usize = 1;
                while (offset < packet.payload.len) {
                    if (offset + 2 > packet.payload.len) return error.InvalidH264Packet;
                    const nal_len = std.mem.readInt(u16, packet.payload[offset..][0..2], .big);
                    offset += 2;
                    if (nal_len == 0 or offset + nal_len > packet.payload.len) {
                        return error.InvalidH264Packet;
                    }
                    try self.access_unit_buf.appendSlice(allocator, &ANNEXB_START_CODE);
                    try self.access_unit_buf.appendSlice(allocator, packet.payload[offset .. offset + nal_len]);
                    offset += nal_len;
                }
            },
            H264NalType.FU_A => {
                if (packet.payload.len < 3) return error.InvalidH264Packet;
                const fu_indicator = packet.payload[0];
                const fu_header = packet.payload[1];
                const start_bit = (fu_header & 0x80) != 0;
                const end_bit = (fu_header & 0x40) != 0;
                const orig_nal_hdr = (fu_indicator & 0xE0) | (fu_header & 0x1F);

                if (start_bit and end_bit) return error.InvalidH264Packet;

                if (start_bit) {
                    self.fu_buf.clearRetainingCapacity();
                    try self.fu_buf.appendSlice(allocator, &ANNEXB_START_CODE);
                    try self.fu_buf.append(allocator, orig_nal_hdr);
                    try self.fu_buf.appendSlice(allocator, packet.payload[2..]);
                    self.in_fu = true;
                } else {
                    if (!self.in_fu) return error.MissingStartPacket;
                    try self.fu_buf.appendSlice(allocator, packet.payload[2..]);
                    if (end_bit) {
                        try self.access_unit_buf.appendSlice(allocator, self.fu_buf.items);
                        self.fu_buf.clearRetainingCapacity();
                        self.in_fu = false;
                    }
                }
            },
            else => return error.InvalidH264Packet,
        }

        if (packet.header.marker) {
            if (self.in_fu) return error.InvalidH264Packet;
            return try self.access_unit_buf.toOwnedSlice(allocator);
        }
        return null;
    }
};

// ============================================================================
// Opus RTP Packetizer & Depacketizer (RFC 7587 & RFC 6716)
// ============================================================================

pub const OpusFrameInfo = struct {
    config: u5,
    stereo: bool,
    frame_count_code: u2,
    samples_per_frame_48k: u32,
};

/// Parses the RFC 6716 Section 3.1 Opus TOC byte.
pub fn parseOpusToc(toc_byte: u8) OpusFrameInfo {
    const config: u5 = @intCast(toc_byte >> 3);
    const stereo = ((toc_byte >> 2) & 0x01) != 0;
    const frame_count_code: u2 = @intCast(toc_byte & 0x03);

    const samples_per_frame_48k: u32 = switch (config) {
        // SILK-only NB/MB/WB: 10, 20, 40, 60 ms
        0...11 => switch (@as(u2, @intCast(config & 0x03))) {
            0 => 480,
            1 => 960,
            2 => 1920,
            3 => 2880,
        },
        // Hybrid SWB/FB: 10, 20 ms
        12...15 => if ((config & 0x01) == 0) 480 else 960,
        // CELT-only NB/WB/SWB/FB: 2.5, 5, 10, 20 ms
        16...31 => switch (@as(u2, @intCast(config & 0x03))) {
            0 => 120,
            1 => 240,
            2 => 480,
            3 => 960,
        },
    };

    return .{
        .config = config,
        .stereo = stereo,
        .frame_count_code = frame_count_code,
        .samples_per_frame_48k = samples_per_frame_48k,
    };
}

pub const OpusPacketizer = struct {
    sequencer: *RtpSequencer,
    timestamp: u32 = 0,

    pub fn packetize(
        self: *OpusPacketizer,
        allocator: std.mem.Allocator,
        opus_frame: []const u8,
        marker: bool,
    ) (Error || std.mem.Allocator.Error)!OwnedRtpPacket {
        if (opus_frame.len == 0) return error.InvalidOpusPacket;
        const info = parseOpusToc(opus_frame[0]);
        const ts = self.timestamp;
        self.timestamp +%= info.samples_per_frame_48k;

        const payload = try allocator.dupe(u8, opus_frame);
        const hdr = self.sequencer.nextHeader(ts, marker);
        self.sequencer.recordBytes(payload.len);
        return .{
            .header = hdr,
            .payload = payload,
        };
    }
};

pub const OpusDepacketizer = struct {
    pub const DecodedOpusPacket = struct {
        info: OpusFrameInfo,
        payload: []const u8,
        timestamp: u32,
        sequence_number: u16,
    };

    pub fn depacketize(packet: Packet) Error!DecodedOpusPacket {
        if (packet.payload.len == 0) return error.InvalidOpusPacket;
        return .{
            .info = parseOpusToc(packet.payload[0]),
            .payload = packet.payload,
            .timestamp = packet.header.timestamp,
            .sequence_number = packet.header.sequence_number,
        };
    }
};

// ============================================================================
// Unit Tests
// ============================================================================

test "RTP Packet.serialize and Packet.parse round-trip with CSRC and extension" {
    const ext_bytes = [_]u8{ 0x10, 0xAA, 0x21, 0xBB, 0xCC, 0x00, 0x00, 0x00 };
    const payload = "hello-rtp-payload";

    var csrcs: [15]u32 = @splat(0);
    csrcs[0] = 0x11223344;
    csrcs[1] = 0x55667788;

    const original = Packet{
        .header = .{
            .version = 2,
            .padding = false,
            .extension = true,
            .csrc_count = 2,
            .marker = true,
            .payload_type = 96,
            .sequence_number = 4242,
            .timestamp = 90000,
            .ssrc = 0xCAFEBABE,
            .csrcs = csrcs,
            .extension_profile = ONE_BYTE_EXT_PROFILE,
            .extension_data = &ext_bytes,
        },
        .payload = payload,
    };

    var wire_buf: [256]u8 = undefined;
    const wire = try original.serialize(&wire_buf);

    const parsed = try Packet.parse(wire);
    try std.testing.expectEqual(@as(u2, 2), parsed.header.version);
    try std.testing.expectEqual(false, parsed.header.padding);
    try std.testing.expectEqual(true, parsed.header.extension);
    try std.testing.expectEqual(@as(u4, 2), parsed.header.csrc_count);
    try std.testing.expectEqual(true, parsed.header.marker);
    try std.testing.expectEqual(@as(u7, 96), parsed.header.payload_type);
    try std.testing.expectEqual(@as(u16, 4242), parsed.header.sequence_number);
    try std.testing.expectEqual(@as(u32, 90000), parsed.header.timestamp);
    try std.testing.expectEqual(@as(u32, 0xCAFEBABE), parsed.header.ssrc);
    try std.testing.expectEqual(@as(u32, 0x11223344), parsed.header.csrcs[0]);
    try std.testing.expectEqual(@as(u32, 0x55667788), parsed.header.csrcs[1]);
    try std.testing.expectEqual(ONE_BYTE_EXT_PROFILE, parsed.header.extension_profile);
    try std.testing.expectEqualSlices(u8, &ext_bytes, parsed.header.extension_data);
    try std.testing.expectEqualSlices(u8, payload, parsed.payload);
}

test "RTCP SR, RR, PLI, FIR, and NACK PID+BLP round-trip" {
    const allocator = std.testing.allocator;
    var buf: [256]u8 = undefined;

    // 1. Sender Report (SR) with 1 report block
    const rb = ReportBlock{
        .ssrc = 0x12345678,
        .fraction_lost = 12,
        .cumulative_lost = 0x010203,
        .extended_highest_seq = 65540,
        .jitter = 35,
        .lsr = 0xAABBCCDD,
        .dlsr = 0x00010000,
    };
    const sr = SenderReport{
        .sender_ssrc = 0xABCDEF01,
        .ntp_timestamp = 0x1122334455667788,
        .rtp_timestamp = 180000,
        .sender_packet_count = 250,
        .sender_octet_count = 250000,
        .reports = &[_]ReportBlock{rb},
    };
    const sr_wire = try encodeSr(sr, &buf);
    const decoded_sr = try decodeSr(allocator, sr_wire);
    defer decoded_sr.deinit(allocator);
    try std.testing.expectEqual(sr.sender_ssrc, decoded_sr.sender_ssrc);
    try std.testing.expectEqual(sr.ntp_timestamp, decoded_sr.ntp_timestamp);
    try std.testing.expectEqual(sr.rtp_timestamp, decoded_sr.rtp_timestamp);
    try std.testing.expectEqual(sr.sender_packet_count, decoded_sr.sender_packet_count);
    try std.testing.expectEqual(sr.sender_octet_count, decoded_sr.sender_octet_count);
    try std.testing.expectEqual(@as(usize, 1), decoded_sr.reports.len);
    try std.testing.expectEqual(rb.ssrc, decoded_sr.reports[0].ssrc);
    try std.testing.expectEqual(rb.fraction_lost, decoded_sr.reports[0].fraction_lost);
    try std.testing.expectEqual(rb.cumulative_lost, decoded_sr.reports[0].cumulative_lost);
    try std.testing.expectEqual(rb.extended_highest_seq, decoded_sr.reports[0].extended_highest_seq);

    // 2. Receiver Report (RR)
    const rr = ReceiverReport{
        .sender_ssrc = 0x99887766,
        .reports = &[_]ReportBlock{rb},
    };
    const rr_wire = try encodeRr(rr, &buf);
    const decoded_rr = try decodeRr(allocator, rr_wire);
    defer decoded_rr.deinit(allocator);
    try std.testing.expectEqual(rr.sender_ssrc, decoded_rr.sender_ssrc);
    try std.testing.expectEqual(@as(usize, 1), decoded_rr.reports.len);
    try std.testing.expectEqual(rb.lsr, decoded_rr.reports[0].lsr);
    try std.testing.expectEqual(rb.dlsr, decoded_rr.reports[0].dlsr);

    // 3. PLI & FIR
    const pli_wire = try encodePli(0x1111, 0x2222, &buf);
    try std.testing.expect(isPli(pli_wire));
    try std.testing.expect(!isFir(pli_wire));

    const fir_wire = try encodeFir(0x1111, 0x2222, 7, &buf);
    try std.testing.expect(isFir(fir_wire));
    try std.testing.expect(!isPli(fir_wire));

    // 4. Generic NACK with multiple lost sequence numbers spanning multiple FCI words
    const lost_seqs = [_]u16{ 100, 101, 105, 116, 120, 121, 136 };
    const nack_wire = try encodeNack(0x1111, 0x2222, &lost_seqs, &buf);
    // 100..116 fits in 1st FCI word (PID=100, BLP bits 0, 4, 15); 120..136 fits in 2nd FCI word
    try std.testing.expectEqual(@as(usize, 20), nack_wire.len);

    const decoded_nack = try decodeNack(allocator, nack_wire);
    defer allocator.free(decoded_nack);
    try std.testing.expectEqualSlices(u16, &lost_seqs, decoded_nack);
}

test "VP8 packetization and depacketization of multi-MTU keyframe" {
    const allocator = std.testing.allocator;
    var seq = RtpSequencer{ .ssrc = 0x10203040, .payload_type = 96 };
    var packetizer = Vp8Packetizer{
        .sequencer = &seq,
        .max_payload_len = 1200,
        .picture_id = 42,
    };
    var depacketizer = Vp8Depacketizer{};
    defer depacketizer.deinit(allocator);

    // Build a 3500-byte VP8 keyframe (first byte bit 0 == 0 => keyframe)
    var vp8_frame: [3500]u8 = undefined;
    vp8_frame[0] = 0x10; // P=0 (keyframe), version=0, show_frame=1
    for (1..vp8_frame.len) |i| {
        vp8_frame[i] = @truncate(i *% 37 +% 13);
    }

    const packets = try packetizer.packetize(allocator, &vp8_frame, 90000, false);
    defer freeOwnedPackets(allocator, packets);

    try std.testing.expectEqual(@as(usize, 3), packets.len);
    try std.testing.expectEqual(@as(u15, 43), packetizer.picture_id);

    var reassembled: ?Vp8Depacketizer.Frame = null;
    for (packets, 0..) |pkt, idx| {
        const is_last = (idx + 1 == packets.len);
        try std.testing.expectEqual(is_last, pkt.header.marker);
        try std.testing.expect(pkt.payload.len <= 1200);

        const desc = try Vp8Depacketizer.parseDescriptor(pkt.payload);
        try std.testing.expectEqual(idx == 0, desc.start_of_partition);
        try std.testing.expectEqual(@as(?u15, 42), desc.picture_id);

        if (try depacketizer.push(allocator, pkt.asPacket())) |frame| {
            reassembled = frame;
        }
    }

    try std.testing.expect(reassembled != null);
    defer reassembled.?.deinit(allocator);
    try std.testing.expect(reassembled.?.is_keyframe);
    try std.testing.expectEqual(@as(?u15, 42), reassembled.?.picture_id);
    try std.testing.expectEqualSlices(u8, &vp8_frame, reassembled.?.data);
}

test "H.264 Annex-B splitting, STAP-A SPS+PPS, FU-A IDR fragmentation, and depacketization" {
    const allocator = std.testing.allocator;

    // Construct an Annex-B access unit: SPS (type 7) + PPS (type 8) + large IDR slice (type 5, > 3000 bytes)
    const sps = [_]u8{ 0x67, 0x42, 0xC0, 0x1F, 0xDA, 0x01, 0x40, 0x16, 0xEC, 0x04, 0x40 };
    const pps = [_]u8{ 0x68, 0xCE, 0x3C, 0x80 };
    var idr: [3400]u8 = undefined;
    idr[0] = 0x65; // NRI=3, Type=5 (IDR)
    for (1..idr.len) |i| {
        idr[i] = @truncate((i *% 73 +% 19) | 0x01); // avoid accidental 00 00 01 start code inside payload
    }

    var annexb: std.ArrayList(u8) = .empty;
    defer annexb.deinit(allocator);
    try annexb.appendSlice(allocator, &ANNEXB_START_CODE);
    try annexb.appendSlice(allocator, &sps);
    try annexb.appendSlice(allocator, &ANNEXB_START_CODE);
    try annexb.appendSlice(allocator, &pps);
    try annexb.appendSlice(allocator, &ANNEXB_START_CODE);
    try annexb.appendSlice(allocator, &idr);

    // Verify splitAnnexBNals
    const split_nals = try splitAnnexBNals(allocator, annexb.items);
    defer allocator.free(split_nals);
    try std.testing.expectEqual(@as(usize, 3), split_nals.len);
    try std.testing.expectEqualSlices(u8, &sps, split_nals[0]);
    try std.testing.expectEqualSlices(u8, &pps, split_nals[1]);
    try std.testing.expectEqualSlices(u8, &idr, split_nals[2]);

    var seq = RtpSequencer{ .ssrc = 0xAABBCCDD, .payload_type = 102 };
    var packetizer = H264Packetizer{
        .sequencer = &seq,
        .mode = .non_interleaved,
        .max_payload_len = 1200,
    };

    const packets = try packetizer.packetize(allocator, annexb.items, 180000);
    defer freeOwnedPackets(allocator, packets);

    // Packet 0 must be STAP-A (type 24) aggregating SPS+PPS; packets 1..3 must be FU-A (type 28) for IDR
    try std.testing.expectEqual(@as(usize, 4), packets.len);
    try std.testing.expectEqual(H264NalType.STAP_A, @as(u5, @intCast(packets[0].payload[0] & 0x1F)));
    try std.testing.expectEqual(H264NalType.FU_A, @as(u5, @intCast(packets[1].payload[0] & 0x1F)));
    try std.testing.expect((packets[1].payload[1] & 0x80) != 0); // Start bit set on first FU-A
    try std.testing.expect((packets[3].payload[1] & 0x40) != 0); // End bit set on last FU-A

    var depacketizer = H264Depacketizer{};
    defer depacketizer.deinit(allocator);

    var reconstructed: ?[]u8 = null;
    for (packets, 0..) |pkt, idx| {
        const is_last = (idx + 1 == packets.len);
        try std.testing.expectEqual(is_last, pkt.header.marker);
        if (try depacketizer.push(allocator, pkt.asPacket())) |au| {
            reconstructed = au;
        }
    }

    try std.testing.expect(reconstructed != null);
    defer allocator.free(reconstructed.?);
    try std.testing.expectEqualSlices(u8, annexb.items, reconstructed.?);
}

test "Opus TOC byte inspection and RTP packetization" {
    const allocator = std.testing.allocator;

    // Config 31 (CELT FB 20ms => 960 samples), stereo=true, code=0 => (31 << 3) | (1 << 2) | 0 = 0xFC
    const toc_20ms_stereo: u8 = 0xFC;
    const info_20ms = parseOpusToc(toc_20ms_stereo);
    try std.testing.expectEqual(@as(u5, 31), info_20ms.config);
    try std.testing.expectEqual(true, info_20ms.stereo);
    try std.testing.expectEqual(@as(u2, 0), info_20ms.frame_count_code);
    try std.testing.expectEqual(@as(u32, 960), info_20ms.samples_per_frame_48k);

    // Config 12 (Hybrid SWB 10ms => 480 samples), stereo=false, code=0 => (12 << 3) = 0x60
    const toc_10ms_mono: u8 = 0x60;
    const info_10ms = parseOpusToc(toc_10ms_mono);
    try std.testing.expectEqual(@as(u5, 12), info_10ms.config);
    try std.testing.expectEqual(false, info_10ms.stereo);
    try std.testing.expectEqual(@as(u32, 480), info_10ms.samples_per_frame_48k);

    var seq = RtpSequencer{ .ssrc = 0x55667788, .payload_type = 111 };
    var packetizer = OpusPacketizer{ .sequencer = &seq, .timestamp = 1000 };

    const frame1 = [_]u8{ toc_20ms_stereo, 0xAA, 0xBB, 0xCC };
    const pkt1 = try packetizer.packetize(allocator, &frame1, true);
    defer pkt1.deinit(allocator);
    try std.testing.expectEqual(@as(u32, 1000), pkt1.header.timestamp);
    try std.testing.expectEqual(@as(u32, 1960), packetizer.timestamp);

    const frame2 = [_]u8{ toc_10ms_mono, 0xDD, 0xEE };
    const pkt2 = try packetizer.packetize(allocator, &frame2, false);
    defer pkt2.deinit(allocator);
    try std.testing.expectEqual(@as(u32, 1960), pkt2.header.timestamp);
    try std.testing.expectEqual(@as(u32, 2440), packetizer.timestamp);

    const decoded = try OpusDepacketizer.depacketize(pkt1.asPacket());
    try std.testing.expectEqual(@as(u32, 960), decoded.info.samples_per_frame_48k);
    try std.testing.expectEqualSlices(u8, &frame1, decoded.payload);
}
