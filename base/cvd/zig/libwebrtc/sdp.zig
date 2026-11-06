const std = @import("std");

pub const Error = error{
    EmptySdp,
    MissingVersion,
    InvalidSdpVersion,
    MissingOrigin,
    InvalidOrigin,
    MissingMediaSection,
    InvalidLineFormat,
    InvalidMediaLine,
    InvalidMediaKind,
    InvalidPort,
    InvalidSetupRole,
    InvalidDirection,
    InvalidRtpMap,
    InvalidFmtp,
    InvalidRtcpFb,
    InvalidSctpPort,
    InvalidMaxMessageSize,
    InvalidSsrc,
    NoCommonCodecs,
    OutOfMemory,
};

pub const SdpError = Error;

pub const Type = enum(u32) {
    offer = 0,
    pranswer = 1,
    answer = 2,
    rollback = 3,

    pub fn toString(self: Type) []const u8 {
        return switch (self) {
            .offer => "offer",
            .pranswer => "pranswer",
            .answer => "answer",
            .rollback => "rollback",
        };
    }

    pub fn fromString(s: []const u8) ?Type {
        if (std.ascii.eqlIgnoreCase(s, "offer")) return .offer;
        if (std.ascii.eqlIgnoreCase(s, "pranswer")) return .pranswer;
        if (std.ascii.eqlIgnoreCase(s, "answer")) return .answer;
        if (std.ascii.eqlIgnoreCase(s, "rollback")) return .rollback;
        return null;
    }
};

pub const Direction = enum {
    sendrecv,
    sendonly,
    recvonly,
    inactive,

    pub fn toString(self: Direction) []const u8 {
        return switch (self) {
            .sendrecv => "sendrecv",
            .sendonly => "sendonly",
            .recvonly => "recvonly",
            .inactive => "inactive",
        };
    }

    pub fn fromString(s: []const u8) ?Direction {
        if (std.ascii.eqlIgnoreCase(s, "sendrecv")) return .sendrecv;
        if (std.ascii.eqlIgnoreCase(s, "sendonly")) return .sendonly;
        if (std.ascii.eqlIgnoreCase(s, "recvonly")) return .recvonly;
        if (std.ascii.eqlIgnoreCase(s, "inactive")) return .inactive;
        return null;
    }

    pub fn answerDirection(self: Direction) Direction {
        return switch (self) {
            .sendrecv => .sendrecv,
            .sendonly => .recvonly,
            .recvonly => .sendonly,
            .inactive => .inactive,
        };
    }
};

pub const SetupRole = enum {
    actpass,
    active,
    passive,
    holdconn,

    pub fn toString(self: SetupRole) []const u8 {
        return switch (self) {
            .actpass => "actpass",
            .active => "active",
            .passive => "passive",
            .holdconn => "holdconn",
        };
    }

    pub fn fromString(s: []const u8) ?SetupRole {
        if (std.ascii.eqlIgnoreCase(s, "actpass")) return .actpass;
        if (std.ascii.eqlIgnoreCase(s, "active")) return .active;
        if (std.ascii.eqlIgnoreCase(s, "passive")) return .passive;
        if (std.ascii.eqlIgnoreCase(s, "holdconn")) return .holdconn;
        return null;
    }

    pub fn answerRole(remote_offer_role: SetupRole, preferred: SetupRole) SetupRole {
        return switch (remote_offer_role) {
            .actpass => if (preferred == .passive) .passive else .active,
            .passive => .active,
            .active => .passive,
            .holdconn => .holdconn,
        };
    }
};

pub const MediaKind = enum {
    video,
    audio,
    application,

    pub fn toString(self: MediaKind) []const u8 {
        return switch (self) {
            .video => "video",
            .audio => "audio",
            .application => "application",
        };
    }

    pub fn fromString(s: []const u8) ?MediaKind {
        if (std.ascii.eqlIgnoreCase(s, "video")) return .video;
        if (std.ascii.eqlIgnoreCase(s, "audio")) return .audio;
        if (std.ascii.eqlIgnoreCase(s, "application")) return .application;
        return null;
    }
};

pub const RtpCodec = struct {
    payload_type: u8,
    name: []const u8,
    clock_rate: u32,
    channels: u8 = 1,
    fmtp: []const u8 = "",
    rtcp_fb: []const []const u8 = &.{},

    pub fn h264PacketizationMode(self: RtpCodec) ?u8 {
        if (!std.ascii.eqlIgnoreCase(self.name, "H264")) return null;
        if (self.fmtp.len == 0) return 0;
        var it = std.mem.splitScalar(u8, self.fmtp, ';');
        while (it.next()) |raw_param| {
            const param = std.mem.trim(u8, raw_param, " \t");
            if (std.ascii.startsWithIgnoreCase(param, "packetization-mode=")) {
                const val = param["packetization-mode=".len..];
                return std.fmt.parseInt(u8, val, 10) catch null;
            }
        }
        return 0;
    }

    pub fn h264ProfileLevelId(self: RtpCodec) ?[]const u8 {
        if (!std.ascii.eqlIgnoreCase(self.name, "H264")) return null;
        var it = std.mem.splitScalar(u8, self.fmtp, ';');
        while (it.next()) |raw_param| {
            const param = std.mem.trim(u8, raw_param, " \t");
            if (std.ascii.startsWithIgnoreCase(param, "profile-level-id=")) {
                return param["profile-level-id=".len..];
            }
        }
        return null;
    }
};

pub const SsrcAttribute = struct {
    ssrc: u32,
    cname: []const u8 = "",
    msid: []const u8 = "",
};

pub const MediaSection = struct {
    kind: MediaKind,
    port: u16 = 9,
    proto: []const u8,
    mid: []const u8,
    direction: Direction = .sendrecv,
    rtcp_mux: bool = true,
    rtcp_rsize: bool = true,
    ice_ufrag: []const u8 = "",
    ice_pwd: []const u8 = "",
    ice_trickle: bool = true,
    fingerprint_alg: []const u8 = "sha-256",
    fingerprint: []const u8 = "",
    setup: SetupRole = .actpass,
    codecs: []const RtpCodec = &.{},
    sctp_port: ?u16 = null,
    max_message_size: ?u32 = null,
    ssrcs: []const SsrcAttribute = &.{},
    candidates: []const []const u8 = &.{},
};

pub const ParsedSdp = struct {
    allocator: std.mem.Allocator,
    arena: std.heap.ArenaAllocator,
    session_id: u64,
    session_version: u64 = 2,
    bundle_mids: []const []const u8,
    extmap_allow_mixed: bool = true,
    ice_ufrag: []const u8,
    ice_pwd: []const u8,
    fingerprint_alg: []const u8 = "sha-256",
    fingerprint: []const u8,
    setup: SetupRole = .actpass,
    media_sections: []MediaSection,

    pub fn deinit(self: *ParsedSdp) void {
        self.arena.deinit();
    }

    pub fn hasCodec(self: *const ParsedSdp, kind: MediaKind, codec_name: []const u8) bool {
        for (self.media_sections) |sec| {
            if (sec.kind != kind or sec.port == 0) continue;
            for (sec.codecs) |codec| {
                if (std.ascii.eqlIgnoreCase(codec.name, codec_name)) return true;
            }
        }
        return false;
    }

    pub fn serialize(self: *const ParsedSdp, allocator: std.mem.Allocator) ![]u8 {
        var out: std.ArrayList(u8) = .empty;
        errdefer out.deinit(allocator);

        var line_buf: [512]u8 = undefined;

        try out.appendSlice(allocator, "v=0\r\n");
        const origin_line = try std.fmt.bufPrint(
            &line_buf,
            "o=- {d} {d} IN IP4 127.0.0.1\r\n",
            .{ self.session_id, self.session_version },
        );
        try out.appendSlice(allocator, origin_line);
        try out.appendSlice(allocator, "s=-\r\n");
        try out.appendSlice(allocator, "t=0 0\r\n");

        if (self.bundle_mids.len > 0) {
            try out.appendSlice(allocator, "a=group:BUNDLE");
            for (self.bundle_mids) |mid| {
                try out.append(allocator, ' ');
                try out.appendSlice(allocator, mid);
            }
            try out.appendSlice(allocator, "\r\n");
        }

        if (self.extmap_allow_mixed) {
            try out.appendSlice(allocator, "a=extmap-allow-mixed\r\n");
        }
        try out.appendSlice(allocator, "a=msid-semantic: WMS *\r\n");

        for (self.media_sections) |sec| {
            if (sec.kind == .application) {
                const m_line = try std.fmt.bufPrint(
                    &line_buf,
                    "m=application {d} {s} webrtc-datachannel\r\n",
                    .{ sec.port, sec.proto },
                );
                try out.appendSlice(allocator, m_line);
            } else {
                const m_prefix = try std.fmt.bufPrint(
                    &line_buf,
                    "m={s} {d} {s}",
                    .{ sec.kind.toString(), sec.port, sec.proto },
                );
                try out.appendSlice(allocator, m_prefix);
                if (sec.codecs.len == 0) {
                    try out.appendSlice(allocator, " 0");
                } else {
                    for (sec.codecs) |codec| {
                        const pt_str = try std.fmt.bufPrint(&line_buf, " {d}", .{codec.payload_type});
                        try out.appendSlice(allocator, pt_str);
                    }
                }
                try out.appendSlice(allocator, "\r\n");
            }

            try out.appendSlice(allocator, "c=IN IP4 0.0.0.0\r\n");
            if (sec.kind != .application and sec.port != 0) {
                try out.appendSlice(allocator, "a=rtcp:9 IN IP4 0.0.0.0\r\n");
            }

            const ufrag = if (sec.ice_ufrag.len > 0) sec.ice_ufrag else self.ice_ufrag;
            const pwd = if (sec.ice_pwd.len > 0) sec.ice_pwd else self.ice_pwd;
            if (ufrag.len > 0) {
                try out.appendSlice(allocator, "a=ice-ufrag:");
                try out.appendSlice(allocator, ufrag);
                try out.appendSlice(allocator, "\r\n");
            }
            if (pwd.len > 0) {
                try out.appendSlice(allocator, "a=ice-pwd:");
                try out.appendSlice(allocator, pwd);
                try out.appendSlice(allocator, "\r\n");
            }
            if (sec.ice_trickle) {
                try out.appendSlice(allocator, "a=ice-options:trickle\r\n");
            }

            const fp_alg = if (sec.fingerprint_alg.len > 0) sec.fingerprint_alg else self.fingerprint_alg;
            const fp = if (sec.fingerprint.len > 0) sec.fingerprint else self.fingerprint;
            if (fp.len > 0) {
                try out.appendSlice(allocator, "a=fingerprint:");
                try out.appendSlice(allocator, fp_alg);
                try out.append(allocator, ' ');
                try out.appendSlice(allocator, fp);
                try out.appendSlice(allocator, "\r\n");
            }

            try out.appendSlice(allocator, "a=setup:");
            try out.appendSlice(allocator, sec.setup.toString());
            try out.appendSlice(allocator, "\r\n");

            if (sec.mid.len > 0) {
                try out.appendSlice(allocator, "a=mid:");
                try out.appendSlice(allocator, sec.mid);
                try out.appendSlice(allocator, "\r\n");
            }

            if (sec.kind == .application) {
                if (sec.sctp_port) |sp| {
                    const sp_line = try std.fmt.bufPrint(&line_buf, "a=sctp-port:{d}\r\n", .{sp});
                    try out.appendSlice(allocator, sp_line);
                }
                if (sec.max_message_size) |mms| {
                    const mms_line = try std.fmt.bufPrint(&line_buf, "a=max-message-size:{d}\r\n", .{mms});
                    try out.appendSlice(allocator, mms_line);
                }
            } else {
                try out.appendSlice(allocator, "a=");
                try out.appendSlice(allocator, sec.direction.toString());
                try out.appendSlice(allocator, "\r\n");

                if (sec.rtcp_mux) {
                    try out.appendSlice(allocator, "a=rtcp-mux\r\n");
                }
                if (sec.rtcp_rsize) {
                    try out.appendSlice(allocator, "a=rtcp-rsize\r\n");
                }

                for (sec.codecs) |codec| {
                    if (codec.channels > 1) {
                        const rtpmap_line = try std.fmt.bufPrint(
                            &line_buf,
                            "a=rtpmap:{d} {s}/{d}/{d}\r\n",
                            .{ codec.payload_type, codec.name, codec.clock_rate, codec.channels },
                        );
                        try out.appendSlice(allocator, rtpmap_line);
                    } else {
                        const rtpmap_line = try std.fmt.bufPrint(
                            &line_buf,
                            "a=rtpmap:{d} {s}/{d}\r\n",
                            .{ codec.payload_type, codec.name, codec.clock_rate },
                        );
                        try out.appendSlice(allocator, rtpmap_line);
                    }

                    for (codec.rtcp_fb) |fb| {
                        const fb_prefix = try std.fmt.bufPrint(&line_buf, "a=rtcp-fb:{d} ", .{codec.payload_type});
                        try out.appendSlice(allocator, fb_prefix);
                        try out.appendSlice(allocator, fb);
                        try out.appendSlice(allocator, "\r\n");
                    }

                    if (codec.fmtp.len > 0) {
                        const fmtp_prefix = try std.fmt.bufPrint(&line_buf, "a=fmtp:{d} ", .{codec.payload_type});
                        try out.appendSlice(allocator, fmtp_prefix);
                        try out.appendSlice(allocator, codec.fmtp);
                        try out.appendSlice(allocator, "\r\n");
                    }
                }

                for (sec.ssrcs) |ssrc_attr| {
                    if (ssrc_attr.msid.len > 0) {
                        try out.appendSlice(allocator, "a=msid:");
                        try out.appendSlice(allocator, ssrc_attr.msid);
                        try out.appendSlice(allocator, "\r\n");
                    }
                    if (ssrc_attr.cname.len > 0) {
                        const cname_prefix = try std.fmt.bufPrint(&line_buf, "a=ssrc:{d} cname:", .{ssrc_attr.ssrc});
                        try out.appendSlice(allocator, cname_prefix);
                        try out.appendSlice(allocator, ssrc_attr.cname);
                        try out.appendSlice(allocator, "\r\n");
                    }
                    if (ssrc_attr.msid.len > 0) {
                        const msid_prefix = try std.fmt.bufPrint(&line_buf, "a=ssrc:{d} msid:", .{ssrc_attr.ssrc});
                        try out.appendSlice(allocator, msid_prefix);
                        try out.appendSlice(allocator, ssrc_attr.msid);
                        try out.appendSlice(allocator, "\r\n");
                    }
                }
            }

            for (sec.candidates) |cand| {
                if (std.mem.startsWith(u8, cand, "a=candidate:")) {
                    try out.appendSlice(allocator, cand);
                    try out.appendSlice(allocator, "\r\n");
                } else if (std.mem.startsWith(u8, cand, "candidate:")) {
                    try out.appendSlice(allocator, "a=");
                    try out.appendSlice(allocator, cand);
                    try out.appendSlice(allocator, "\r\n");
                } else {
                    try out.appendSlice(allocator, "a=candidate:");
                    try out.appendSlice(allocator, cand);
                    try out.appendSlice(allocator, "\r\n");
                }
            }
        }

        return out.toOwnedSlice(allocator);
    }
};

pub const SessionDescription = struct {
    sdp_type: Type,
    sdp_text: []const u8,

    pub fn hasVideoCodec(self: SessionDescription, codec_name: []const u8) bool {
        return hasMediaSectionCodec(self.sdp_text, .video, codec_name);
    }

    pub fn hasAudioCodec(self: SessionDescription, codec_name: []const u8) bool {
        return hasMediaSectionCodec(self.sdp_text, .audio, codec_name);
    }

    pub fn parse(self: SessionDescription, allocator: std.mem.Allocator) !ParsedSdp {
        return parseSdp(allocator, self.sdp_text);
    }
};

fn hasMediaSectionCodec(sdp_text: []const u8, target_kind: MediaKind, codec_name: []const u8) bool {
    if (codec_name.len == 0 or sdp_text.len == 0) return false;
    var current_kind: ?MediaKind = null;
    var saw_any_media = false;

    var line_it = std.mem.splitScalar(u8, sdp_text, '\n');
    while (line_it.next()) |raw_line| {
        const line = std.mem.trim(u8, raw_line, "\r \t");
        if (line.len == 0) continue;
        if (std.mem.startsWith(u8, line, "m=")) {
            saw_any_media = true;
            const rest = line[2..];
            var tok_it = std.mem.tokenizeScalar(u8, rest, ' ');
            if (tok_it.next()) |kind_str| {
                current_kind = MediaKind.fromString(kind_str);
            } else {
                current_kind = null;
            }
            continue;
        }
        if (current_kind == target_kind and std.mem.startsWith(u8, line, "a=rtpmap:")) {
            const after_colon = line["a=rtpmap:".len..];
            var tok_it = std.mem.tokenizeScalar(u8, after_colon, ' ');
            _ = tok_it.next();
            if (tok_it.next()) |enc| {
                var slash_it = std.mem.splitScalar(u8, enc, '/');
                if (slash_it.next()) |name| {
                    if (std.ascii.eqlIgnoreCase(name, codec_name)) return true;
                }
            }
        }
    }

    if (!saw_any_media and target_kind == .video) {
        return containsIgnoreCase(sdp_text, codec_name);
    }
    return false;
}

fn containsIgnoreCase(haystack: []const u8, needle: []const u8) bool {
    if (needle.len == 0) return true;
    if (haystack.len < needle.len) return false;
    var i: usize = 0;
    while (i + needle.len <= haystack.len) : (i += 1) {
        if (std.ascii.eqlIgnoreCase(haystack[i .. i + needle.len], needle)) return true;
    }
    return false;
}

const CodecBuilder = struct {
    payload_type: u8,
    name: []const u8 = "",
    clock_rate: u32 = 0,
    channels: u8 = 1,
    fmtp: []const u8 = "",
    rtcp_fb: std.ArrayList([]const u8) = .empty,
};

const SsrcBuilder = struct {
    ssrc: u32,
    cname: []const u8 = "",
    msid: []const u8 = "",
};

const MediaSectionBuilder = struct {
    kind: MediaKind,
    port: u16 = 9,
    proto: []const u8,
    mid: []const u8 = "",
    direction: Direction = .sendrecv,
    rtcp_mux: bool = false,
    rtcp_rsize: bool = false,
    ice_ufrag: []const u8 = "",
    ice_pwd: []const u8 = "",
    ice_trickle: bool = false,
    fingerprint_alg: []const u8 = "",
    fingerprint: []const u8 = "",
    setup: ?SetupRole = null,
    sctp_port: ?u16 = null,
    max_message_size: ?u32 = null,
    codecs: std.ArrayList(CodecBuilder) = .empty,
    ssrcs: std.ArrayList(SsrcBuilder) = .empty,
    candidates: std.ArrayList([]const u8) = .empty,

    fn findOrAddCodec(self: *MediaSectionBuilder, a: std.mem.Allocator, pt: u8) !*CodecBuilder {
        for (self.codecs.items) |*c| {
            if (c.payload_type == pt) return c;
        }
        try self.codecs.append(a, .{ .payload_type = pt });
        return &self.codecs.items[self.codecs.items.len - 1];
    }

    fn findOrAddSsrc(self: *MediaSectionBuilder, a: std.mem.Allocator, ssrc_id: u32) !*SsrcBuilder {
        for (self.ssrcs.items) |*s| {
            if (s.ssrc == ssrc_id) return s;
        }
        try self.ssrcs.append(a, .{ .ssrc = ssrc_id });
        return &self.ssrcs.items[self.ssrcs.items.len - 1];
    }
};

fn isValidSdpTypeChar(ch: u8) bool {
    return switch (ch) {
        'v', 'o', 's', 'i', 'u', 'e', 'p', 'c', 'b', 't', 'r', 'z', 'k', 'a', 'm' => true,
        else => false,
    };
}

pub fn parseSdp(allocator: std.mem.Allocator, raw_sdp: []const u8) Error!ParsedSdp {
    if (raw_sdp.len == 0) return Error.EmptySdp;

    var arena = std.heap.ArenaAllocator.init(allocator);
    errdefer arena.deinit();
    const a = arena.allocator();

    var saw_version = false;
    var saw_origin = false;
    var session_id: u64 = 0;
    var session_version: u64 = 2;
    var bundle_mids: std.ArrayList([]const u8) = .empty;
    var extmap_allow_mixed: bool = false;
    var session_ice_ufrag: []const u8 = "";
    var session_ice_pwd: []const u8 = "";
    var session_ice_trickle: bool = false;
    var session_fp_alg: []const u8 = "sha-256";
    var session_fp: []const u8 = "";
    var session_setup: SetupRole = .actpass;

    var media_builders: std.ArrayList(MediaSectionBuilder) = .empty;

    var line_it = std.mem.splitScalar(u8, raw_sdp, '\n');
    while (line_it.next()) |raw_line| {
        const line = std.mem.trim(u8, raw_line, "\r \t");
        if (line.len == 0) continue;

        if (line.len < 2 or line[1] != '=' or !isValidSdpTypeChar(line[0])) {
            return Error.InvalidLineFormat;
        }

        const field_type = line[0];
        const value = line[2..];

        if (!saw_version) {
            if (field_type != 'v') return Error.MissingVersion;
            if (!std.mem.eql(u8, value, "0")) return Error.InvalidSdpVersion;
            saw_version = true;
            continue;
        }

        switch (field_type) {
            'v' => return Error.InvalidSdpVersion,
            'o' => {
                var tok_it = std.mem.tokenizeScalar(u8, value, ' ');
                _ = tok_it.next() orelse return Error.InvalidOrigin; // username
                const sid_str = tok_it.next() orelse return Error.InvalidOrigin;
                const sver_str = tok_it.next() orelse return Error.InvalidOrigin;
                _ = tok_it.next() orelse return Error.InvalidOrigin; // nettype
                _ = tok_it.next() orelse return Error.InvalidOrigin; // addrtype
                _ = tok_it.next() orelse return Error.InvalidOrigin; // unicast-address

                session_id = std.fmt.parseInt(u64, sid_str, 10) catch return Error.InvalidOrigin;
                session_version = std.fmt.parseInt(u64, sver_str, 10) catch return Error.InvalidOrigin;
                saw_origin = true;
            },
            's', 't', 'c', 'i', 'u', 'e', 'p', 'b', 'r', 'z', 'k' => {},
            'm' => {
                var tok_it = std.mem.tokenizeScalar(u8, value, ' ');
                const kind_str = tok_it.next() orelse return Error.InvalidMediaLine;
                const port_str = tok_it.next() orelse return Error.InvalidMediaLine;
                const proto_str = tok_it.next() orelse return Error.InvalidMediaLine;
                const first_fmt = tok_it.next() orelse return Error.InvalidMediaLine;

                const kind = MediaKind.fromString(kind_str) orelse return Error.InvalidMediaKind;

                var port_slash = std.mem.splitScalar(u8, port_str, '/');
                const base_port_str = port_slash.next() orelse return Error.InvalidPort;
                const port = std.fmt.parseInt(u16, base_port_str, 10) catch return Error.InvalidPort;

                var builder = MediaSectionBuilder{
                    .kind = kind,
                    .port = port,
                    .proto = try a.dupe(u8, proto_str),
                };

                if (kind != .application) {
                    if (std.fmt.parseInt(u8, first_fmt, 10)) |pt0| {
                        _ = try builder.findOrAddCodec(a, pt0);
                    } else |_| {
                        return Error.InvalidMediaLine;
                    }
                    while (tok_it.next()) |fmt_tok| {
                        const pt = std.fmt.parseInt(u8, fmt_tok, 10) catch return Error.InvalidMediaLine;
                        _ = try builder.findOrAddCodec(a, pt);
                    }
                }

                try media_builders.append(a, builder);
            },
            'a' => {
                const current_media: ?*MediaSectionBuilder = if (media_builders.items.len > 0)
                    &media_builders.items[media_builders.items.len - 1]
                else
                    null;

                if (std.mem.startsWith(u8, value, "group:BUNDLE")) {
                    const rest = value["group:BUNDLE".len..];
                    var mid_it = std.mem.tokenizeScalar(u8, rest, ' ');
                    while (mid_it.next()) |mid| {
                        try bundle_mids.append(a, try a.dupe(u8, mid));
                    }
                } else if (std.mem.eql(u8, value, "extmap-allow-mixed")) {
                    extmap_allow_mixed = true;
                } else if (std.mem.startsWith(u8, value, "ice-ufrag:")) {
                    const ufrag = try a.dupe(u8, value["ice-ufrag:".len..]);
                    if (ufrag.len == 0) return Error.InvalidLineFormat;
                    if (current_media) |m| {
                        m.ice_ufrag = ufrag;
                    } else {
                        session_ice_ufrag = ufrag;
                    }
                } else if (std.mem.startsWith(u8, value, "ice-pwd:")) {
                    const pwd = try a.dupe(u8, value["ice-pwd:".len..]);
                    if (pwd.len == 0) return Error.InvalidLineFormat;
                    if (current_media) |m| {
                        m.ice_pwd = pwd;
                    } else {
                        session_ice_pwd = pwd;
                    }
                } else if (std.mem.startsWith(u8, value, "ice-options:")) {
                    const opts_str = value["ice-options:".len..];
                    const has_trickle = std.mem.indexOf(u8, opts_str, "trickle") != null;
                    if (current_media) |m| {
                        m.ice_trickle = has_trickle;
                    } else {
                        session_ice_trickle = has_trickle;
                    }
                } else if (std.mem.startsWith(u8, value, "fingerprint:")) {
                    const fp_rest = value["fingerprint:".len..];
                    var fp_it = std.mem.tokenizeScalar(u8, fp_rest, ' ');
                    const alg = fp_it.next() orelse return Error.InvalidLineFormat;
                    const hash = fp_it.next() orelse return Error.InvalidLineFormat;
                    const dup_alg = try a.dupe(u8, alg);
                    const dup_hash = try a.dupe(u8, hash);
                    if (current_media) |m| {
                        m.fingerprint_alg = dup_alg;
                        m.fingerprint = dup_hash;
                    } else {
                        session_fp_alg = dup_alg;
                        session_fp = dup_hash;
                    }
                } else if (std.mem.startsWith(u8, value, "setup:")) {
                    const role_str = value["setup:".len..];
                    const role = SetupRole.fromString(role_str) orelse return Error.InvalidSetupRole;
                    if (current_media) |m| {
                        m.setup = role;
                    } else {
                        session_setup = role;
                    }
                } else if (std.mem.startsWith(u8, value, "mid:")) {
                    const mid_val = value["mid:".len..];
                    if (mid_val.len == 0) return Error.InvalidLineFormat;
                    if (current_media) |m| {
                        m.mid = try a.dupe(u8, mid_val);
                    }
                } else if (Direction.fromString(value)) |dir| {
                    if (current_media) |m| {
                        m.direction = dir;
                    }
                } else if (std.mem.eql(u8, value, "rtcp-mux")) {
                    if (current_media) |m| {
                        m.rtcp_mux = true;
                    }
                } else if (std.mem.eql(u8, value, "rtcp-rsize")) {
                    if (current_media) |m| {
                        m.rtcp_rsize = true;
                    }
                } else if (std.mem.startsWith(u8, value, "sctp-port:")) {
                    const sp_str = value["sctp-port:".len..];
                    const sp = std.fmt.parseInt(u16, sp_str, 10) catch return Error.InvalidSctpPort;
                    if (current_media) |m| {
                        m.sctp_port = sp;
                    }
                } else if (std.mem.startsWith(u8, value, "max-message-size:")) {
                    const mms_str = value["max-message-size:".len..];
                    const mms = std.fmt.parseInt(u32, mms_str, 10) catch return Error.InvalidMaxMessageSize;
                    if (current_media) |m| {
                        m.max_message_size = mms;
                    }
                } else if (std.mem.startsWith(u8, value, "rtpmap:")) {
                    const m = current_media orelse return Error.InvalidRtpMap;
                    const rest = value["rtpmap:".len..];
                    const sp_idx = std.mem.indexOfScalar(u8, rest, ' ') orelse return Error.InvalidRtpMap;
                    const pt_str = rest[0..sp_idx];
                    const enc_str = std.mem.trim(u8, rest[sp_idx + 1 ..], " \t");
                    const pt = std.fmt.parseInt(u8, pt_str, 10) catch return Error.InvalidRtpMap;

                    var slash_it = std.mem.splitScalar(u8, enc_str, '/');
                    const codec_name = slash_it.next() orelse return Error.InvalidRtpMap;
                    const clock_str = slash_it.next() orelse return Error.InvalidRtpMap;
                    if (codec_name.len == 0 or clock_str.len == 0) return Error.InvalidRtpMap;
                    const clock_rate = std.fmt.parseInt(u32, clock_str, 10) catch return Error.InvalidRtpMap;
                    const channels: u8 = if (slash_it.next()) |ch_str|
                        (std.fmt.parseInt(u8, ch_str, 10) catch return Error.InvalidRtpMap)
                    else
                        1;

                    const cb = try m.findOrAddCodec(a, pt);
                    cb.name = try a.dupe(u8, codec_name);
                    cb.clock_rate = clock_rate;
                    cb.channels = channels;
                } else if (std.mem.startsWith(u8, value, "fmtp:")) {
                    const m = current_media orelse return Error.InvalidFmtp;
                    const rest = value["fmtp:".len..];
                    const sp_idx = std.mem.indexOfScalar(u8, rest, ' ') orelse return Error.InvalidFmtp;
                    const pt_str = rest[0..sp_idx];
                    const params = std.mem.trim(u8, rest[sp_idx + 1 ..], " \t");
                    if (params.len == 0) return Error.InvalidFmtp;
                    const pt = std.fmt.parseInt(u8, pt_str, 10) catch return Error.InvalidFmtp;

                    const cb = try m.findOrAddCodec(a, pt);
                    cb.fmtp = try a.dupe(u8, params);
                } else if (std.mem.startsWith(u8, value, "rtcp-fb:")) {
                    const m = current_media orelse return Error.InvalidRtcpFb;
                    const rest = value["rtcp-fb:".len..];
                    const sp_idx = std.mem.indexOfScalar(u8, rest, ' ') orelse return Error.InvalidRtcpFb;
                    const pt_str = rest[0..sp_idx];
                    const fb_val = std.mem.trim(u8, rest[sp_idx + 1 ..], " \t");
                    if (fb_val.len == 0) return Error.InvalidRtcpFb;
                    const dup_fb = try a.dupe(u8, fb_val);

                    if (std.mem.eql(u8, pt_str, "*")) {
                        for (m.codecs.items) |*cb| {
                            try cb.rtcp_fb.append(a, dup_fb);
                        }
                    } else {
                        const pt = std.fmt.parseInt(u8, pt_str, 10) catch return Error.InvalidRtcpFb;
                        const cb = try m.findOrAddCodec(a, pt);
                        try cb.rtcp_fb.append(a, dup_fb);
                    }
                } else if (std.mem.startsWith(u8, value, "ssrc:")) {
                    const m = current_media orelse return Error.InvalidSsrc;
                    const rest = value["ssrc:".len..];
                    const sp_idx = std.mem.indexOfScalar(u8, rest, ' ') orelse return Error.InvalidSsrc;
                    const ssrc_str = rest[0..sp_idx];
                    const attr_rest = std.mem.trim(u8, rest[sp_idx + 1 ..], " \t");
                    const ssrc_id = std.fmt.parseInt(u32, ssrc_str, 10) catch return Error.InvalidSsrc;

                    const sb = try m.findOrAddSsrc(a, ssrc_id);
                    if (std.mem.startsWith(u8, attr_rest, "cname:")) {
                        sb.cname = try a.dupe(u8, attr_rest["cname:".len..]);
                    } else if (std.mem.startsWith(u8, attr_rest, "msid:")) {
                        sb.msid = try a.dupe(u8, attr_rest["msid:".len..]);
                    }
                } else if (std.mem.startsWith(u8, value, "candidate:")) {
                    const cand_body = value["candidate:".len..];
                    if (cand_body.len == 0) return Error.InvalidLineFormat;
                    if (current_media) |m| {
                        try m.candidates.append(a, try a.dupe(u8, value));
                    }
                }
            },
            else => {},
        }
    }

    if (!saw_version) return Error.MissingVersion;
    if (!saw_origin) return Error.MissingOrigin;
    if (media_builders.items.len == 0) return Error.MissingMediaSection;

    var sections = try a.alloc(MediaSection, media_builders.items.len);
    for (media_builders.items, 0..) |*mb, idx| {
        var valid_codecs: std.ArrayList(RtpCodec) = .empty;
        for (mb.codecs.items) |*cb| {
            if (cb.name.len == 0) continue;
            try valid_codecs.append(a, .{
                .payload_type = cb.payload_type,
                .name = cb.name,
                .clock_rate = cb.clock_rate,
                .channels = cb.channels,
                .fmtp = cb.fmtp,
                .rtcp_fb = try cb.rtcp_fb.toOwnedSlice(a),
            });
        }

        var ssrc_list = try a.alloc(SsrcAttribute, mb.ssrcs.items.len);
        for (mb.ssrcs.items, 0..) |sb, s_idx| {
            ssrc_list[s_idx] = .{
                .ssrc = sb.ssrc,
                .cname = sb.cname,
                .msid = sb.msid,
            };
        }

        const resolved_ufrag = if (mb.ice_ufrag.len > 0) mb.ice_ufrag else session_ice_ufrag;
        const resolved_pwd = if (mb.ice_pwd.len > 0) mb.ice_pwd else session_ice_pwd;
        const resolved_fp_alg = if (mb.fingerprint_alg.len > 0) mb.fingerprint_alg else session_fp_alg;
        const resolved_fp = if (mb.fingerprint.len > 0) mb.fingerprint else session_fp;
        const resolved_setup = mb.setup orelse session_setup;

        if (session_ice_ufrag.len == 0 and resolved_ufrag.len > 0) session_ice_ufrag = resolved_ufrag;
        if (session_ice_pwd.len == 0 and resolved_pwd.len > 0) session_ice_pwd = resolved_pwd;
        if (session_fp.len == 0 and resolved_fp.len > 0) {
            session_fp_alg = resolved_fp_alg;
            session_fp = resolved_fp;
        }
        if (idx == 0 and mb.setup != null) {
            session_setup = resolved_setup;
        }

        sections[idx] = .{
            .kind = mb.kind,
            .port = mb.port,
            .proto = mb.proto,
            .mid = mb.mid,
            .direction = mb.direction,
            .rtcp_mux = mb.rtcp_mux,
            .rtcp_rsize = mb.rtcp_rsize,
            .ice_ufrag = resolved_ufrag,
            .ice_pwd = resolved_pwd,
            .ice_trickle = mb.ice_trickle or session_ice_trickle,
            .fingerprint_alg = resolved_fp_alg,
            .fingerprint = resolved_fp,
            .setup = resolved_setup,
            .codecs = try valid_codecs.toOwnedSlice(a),
            .sctp_port = mb.sctp_port,
            .max_message_size = mb.max_message_size,
            .ssrcs = ssrc_list,
            .candidates = try mb.candidates.toOwnedSlice(a),
        };
    }

    return ParsedSdp{
        .allocator = allocator,
        .arena = arena,
        .session_id = session_id,
        .session_version = session_version,
        .bundle_mids = try bundle_mids.toOwnedSlice(a),
        .extmap_allow_mixed = extmap_allow_mixed,
        .ice_ufrag = session_ice_ufrag,
        .ice_pwd = session_ice_pwd,
        .fingerprint_alg = session_fp_alg,
        .fingerprint = session_fp,
        .setup = session_setup,
        .media_sections = sections,
    };
}

pub const OfferAnswerOptions = struct {
    session_id: u64 = 100001,
    ice_ufrag: []const u8,
    ice_pwd: []const u8,
    fingerprint: []const u8,
    setup: SetupRole = .actpass,
    include_video: bool = true,
    include_audio: bool = true,
    include_datachannel: bool = true,
    video_ssrc: u32 = 0x22334455,
    audio_ssrc: u32 = 0x55667788,
    candidates: []const []const u8 = &.{},
};

const default_video_rtcp_fb = [_][]const u8{
    "nack",
    "nack pli",
    "ccm fir",
    "goog-remb",
    "transport-cc",
};

const default_audio_rtcp_fb = [_][]const u8{
    "transport-cc",
};

const default_video_codecs = [_]RtpCodec{
    .{
        .payload_type = 96,
        .name = "VP8",
        .clock_rate = 90000,
        .channels = 1,
        .fmtp = "",
        .rtcp_fb = &default_video_rtcp_fb,
    },
    .{
        .payload_type = 102,
        .name = "H264",
        .clock_rate = 90000,
        .channels = 1,
        .fmtp = "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
        .rtcp_fb = &default_video_rtcp_fb,
    },
    .{
        .payload_type = 104,
        .name = "H264",
        .clock_rate = 90000,
        .channels = 1,
        .fmtp = "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f",
        .rtcp_fb = &default_video_rtcp_fb,
    },
};

const default_audio_codecs = [_]RtpCodec{
    .{
        .payload_type = 111,
        .name = "opus",
        .clock_rate = 48000,
        .channels = 2,
        .fmtp = "minptime=10;useinbandfec=1",
        .rtcp_fb = &default_audio_rtcp_fb,
    },
};

pub fn buildOfferSdp(allocator: std.mem.Allocator, opts: OfferAnswerOptions) ![]u8 {
    var sections_buf: [3]MediaSection = undefined;
    var mids_buf: [3][]const u8 = undefined;
    var section_count: usize = 0;

    const video_ssrcs = [_]SsrcAttribute{.{
        .ssrc = opts.video_ssrc,
        .cname = "cuttlefish-video",
        .msid = "display_0 display_0",
    }};

    const audio_ssrcs = [_]SsrcAttribute{.{
        .ssrc = opts.audio_ssrc,
        .cname = "cuttlefish-audio",
        .msid = "cuttlefish-stream audio_0",
    }};

    if (opts.include_video) {
        mids_buf[section_count] = "0";
        sections_buf[section_count] = .{
            .kind = .video,
            .port = 9,
            .proto = "UDP/TLS/RTP/SAVPF",
            .mid = "0",
            .direction = .sendrecv,
            .rtcp_mux = true,
            .rtcp_rsize = true,
            .ice_ufrag = opts.ice_ufrag,
            .ice_pwd = opts.ice_pwd,
            .ice_trickle = true,
            .fingerprint_alg = "sha-256",
            .fingerprint = opts.fingerprint,
            .setup = opts.setup,
            .codecs = &default_video_codecs,
            .ssrcs = &video_ssrcs,
            .candidates = opts.candidates,
        };
        section_count += 1;
    }

    if (opts.include_audio) {
        const mid = if (section_count == 0) "0" else "1";
        mids_buf[section_count] = mid;
        sections_buf[section_count] = .{
            .kind = .audio,
            .port = 9,
            .proto = "UDP/TLS/RTP/SAVPF",
            .mid = mid,
            .direction = .sendrecv,
            .rtcp_mux = true,
            .rtcp_rsize = true,
            .ice_ufrag = opts.ice_ufrag,
            .ice_pwd = opts.ice_pwd,
            .ice_trickle = true,
            .fingerprint_alg = "sha-256",
            .fingerprint = opts.fingerprint,
            .setup = opts.setup,
            .codecs = &default_audio_codecs,
            .ssrcs = &audio_ssrcs,
            .candidates = opts.candidates,
        };
        section_count += 1;
    }

    if (opts.include_datachannel) {
        const mid = switch (section_count) {
            0 => "0",
            1 => "1",
            else => "2",
        };
        mids_buf[section_count] = mid;
        sections_buf[section_count] = .{
            .kind = .application,
            .port = 9,
            .proto = "UDP/DTLS/SCTP",
            .mid = mid,
            .direction = .sendrecv,
            .ice_ufrag = opts.ice_ufrag,
            .ice_pwd = opts.ice_pwd,
            .ice_trickle = true,
            .fingerprint_alg = "sha-256",
            .fingerprint = opts.fingerprint,
            .setup = opts.setup,
            .sctp_port = 5000,
            .max_message_size = 262144,
            .candidates = opts.candidates,
        };
        section_count += 1;
    }

    const temp_parsed = ParsedSdp{
        .allocator = allocator,
        .arena = std.heap.ArenaAllocator.init(allocator),
        .session_id = opts.session_id,
        .session_version = 2,
        .bundle_mids = mids_buf[0..section_count],
        .extmap_allow_mixed = true,
        .ice_ufrag = opts.ice_ufrag,
        .ice_pwd = opts.ice_pwd,
        .fingerprint_alg = "sha-256",
        .fingerprint = opts.fingerprint,
        .setup = opts.setup,
        .media_sections = sections_buf[0..section_count],
    };

    return temp_parsed.serialize(allocator);
}

pub fn negotiateAnswerSdp(
    allocator: std.mem.Allocator,
    remote_offer_sdp: []const u8,
    opts: OfferAnswerOptions,
) ![]u8 {
    var offer = try parseSdp(allocator, remote_offer_sdp);
    defer offer.deinit();

    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const a = arena.allocator();

    var answer_sections = try a.alloc(MediaSection, offer.media_sections.len);
    var active_mids: std.ArrayList([]const u8) = .empty;

    const video_ssrcs = [_]SsrcAttribute{.{
        .ssrc = opts.video_ssrc,
        .cname = "cuttlefish-video",
        .msid = "display_0 display_0",
    }};

    const audio_ssrcs = [_]SsrcAttribute{.{
        .ssrc = opts.audio_ssrc,
        .cname = "cuttlefish-audio",
        .msid = "cuttlefish-stream audio_0",
    }};

    for (offer.media_sections, 0..) |remote_sec, idx| {
        const resolved_setup = SetupRole.answerRole(remote_sec.setup, opts.setup);
        const ans_dir = remote_sec.direction.answerDirection();

        switch (remote_sec.kind) {
            .video => {
                var matched_codecs: std.ArrayList(RtpCodec) = .empty;
                if (opts.include_video and remote_sec.port != 0) {
                    for (remote_sec.codecs) |rc| {
                        if (std.ascii.eqlIgnoreCase(rc.name, "VP8") and rc.clock_rate == 90000) {
                            try matched_codecs.append(a, .{
                                .payload_type = rc.payload_type,
                                .name = "VP8",
                                .clock_rate = 90000,
                                .channels = 1,
                                .fmtp = rc.fmtp,
                                .rtcp_fb = if (rc.rtcp_fb.len > 0) rc.rtcp_fb else &default_video_rtcp_fb,
                            });
                        } else if (std.ascii.eqlIgnoreCase(rc.name, "H264") and rc.clock_rate == 90000) {
                            const pkt_mode = rc.h264PacketizationMode() orelse continue;
                            if (pkt_mode != 0 and pkt_mode != 1) continue;
                            const default_fmtp = if (pkt_mode == 1)
                                "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f"
                            else
                                "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f";
                            try matched_codecs.append(a, .{
                                .payload_type = rc.payload_type,
                                .name = "H264",
                                .clock_rate = 90000,
                                .channels = 1,
                                .fmtp = if (rc.fmtp.len > 0) rc.fmtp else default_fmtp,
                                .rtcp_fb = if (rc.rtcp_fb.len > 0) rc.rtcp_fb else &default_video_rtcp_fb,
                            });
                        }
                    }
                }

                const active = matched_codecs.items.len > 0;
                if (active and remote_sec.mid.len > 0) {
                    try active_mids.append(a, remote_sec.mid);
                }

                answer_sections[idx] = .{
                    .kind = .video,
                    .port = if (active) 9 else 0,
                    .proto = remote_sec.proto,
                    .mid = remote_sec.mid,
                    .direction = if (active) ans_dir else .inactive,
                    .rtcp_mux = true,
                    .rtcp_rsize = true,
                    .ice_ufrag = opts.ice_ufrag,
                    .ice_pwd = opts.ice_pwd,
                    .ice_trickle = true,
                    .fingerprint_alg = "sha-256",
                    .fingerprint = opts.fingerprint,
                    .setup = resolved_setup,
                    .codecs = try matched_codecs.toOwnedSlice(a),
                    .ssrcs = if (active and (ans_dir == .sendrecv or ans_dir == .sendonly)) &video_ssrcs else &.{},
                    .candidates = opts.candidates,
                };
            },
            .audio => {
                var matched_codecs: std.ArrayList(RtpCodec) = .empty;
                if (opts.include_audio and remote_sec.port != 0) {
                    for (remote_sec.codecs) |rc| {
                        if (std.ascii.eqlIgnoreCase(rc.name, "opus") and rc.clock_rate == 48000) {
                            try matched_codecs.append(a, .{
                                .payload_type = rc.payload_type,
                                .name = "opus",
                                .clock_rate = 48000,
                                .channels = if (rc.channels > 0) rc.channels else 2,
                                .fmtp = if (rc.fmtp.len > 0) rc.fmtp else "minptime=10;useinbandfec=1",
                                .rtcp_fb = if (rc.rtcp_fb.len > 0) rc.rtcp_fb else &default_audio_rtcp_fb,
                            });
                        }
                    }
                }

                const active = matched_codecs.items.len > 0;
                if (active and remote_sec.mid.len > 0) {
                    try active_mids.append(a, remote_sec.mid);
                }

                answer_sections[idx] = .{
                    .kind = .audio,
                    .port = if (active) 9 else 0,
                    .proto = remote_sec.proto,
                    .mid = remote_sec.mid,
                    .direction = if (active) ans_dir else .inactive,
                    .rtcp_mux = true,
                    .rtcp_rsize = true,
                    .ice_ufrag = opts.ice_ufrag,
                    .ice_pwd = opts.ice_pwd,
                    .ice_trickle = true,
                    .fingerprint_alg = "sha-256",
                    .fingerprint = opts.fingerprint,
                    .setup = resolved_setup,
                    .codecs = try matched_codecs.toOwnedSlice(a),
                    .ssrcs = if (active and (ans_dir == .sendrecv or ans_dir == .sendonly)) &audio_ssrcs else &.{},
                    .candidates = opts.candidates,
                };
            },
            .application => {
                const active = opts.include_datachannel and remote_sec.port != 0;
                if (active and remote_sec.mid.len > 0) {
                    try active_mids.append(a, remote_sec.mid);
                }

                answer_sections[idx] = .{
                    .kind = .application,
                    .port = if (active) 9 else 0,
                    .proto = remote_sec.proto,
                    .mid = remote_sec.mid,
                    .direction = .sendrecv,
                    .ice_ufrag = opts.ice_ufrag,
                    .ice_pwd = opts.ice_pwd,
                    .ice_trickle = true,
                    .fingerprint_alg = "sha-256",
                    .fingerprint = opts.fingerprint,
                    .setup = resolved_setup,
                    .sctp_port = remote_sec.sctp_port orelse 5000,
                    .max_message_size = remote_sec.max_message_size orelse 262144,
                    .candidates = opts.candidates,
                };
            },
        }
    }

    var final_bundle_mids: std.ArrayList([]const u8) = .empty;
    if (offer.bundle_mids.len > 0) {
        for (offer.bundle_mids) |bmid| {
            for (active_mids.items) |amid| {
                if (std.mem.eql(u8, bmid, amid)) {
                    try final_bundle_mids.append(a, bmid);
                    break;
                }
            }
        }
    } else {
        try final_bundle_mids.appendSlice(a, active_mids.items);
    }

    const answer_parsed = ParsedSdp{
        .allocator = allocator,
        .arena = std.heap.ArenaAllocator.init(allocator),
        .session_id = opts.session_id,
        .session_version = 2,
        .bundle_mids = try final_bundle_mids.toOwnedSlice(a),
        .extmap_allow_mixed = offer.extmap_allow_mixed,
        .ice_ufrag = opts.ice_ufrag,
        .ice_pwd = opts.ice_pwd,
        .fingerprint_alg = "sha-256",
        .fingerprint = opts.fingerprint,
        .setup = if (answer_sections.len > 0) answer_sections[0].setup else .active,
        .media_sections = answer_sections,
    };

    return answer_parsed.serialize(allocator);
}

pub fn validateSdp(raw_sdp: []const u8) bool {
    var stack_buf: [65536]u8 = undefined;
    var fba = std.heap.FixedBufferAllocator.init(&stack_buf);
    var parsed = parseSdp(fba.allocator(), raw_sdp) catch return false;
    defer parsed.deinit();
    return parsed.media_sections.len > 0;
}

test "buildOfferSdp and parseSdp round-trip serialization" {
    const allocator = std.testing.allocator;
    const fp = "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";
    const candidates = [_][]const u8{
        "candidate:1 1 UDP 2122260223 192.168.1.10 15550 typ host",
    };

    const offer_sdp = try buildOfferSdp(allocator, .{
        .session_id = 424242,
        .ice_ufrag = "ufragOffer1",
        .ice_pwd = "pwdOfferSecret1234567890",
        .fingerprint = fp,
        .setup = .actpass,
        .video_ssrc = 0x12345678,
        .audio_ssrc = 0x87654321,
        .candidates = &candidates,
    });
    defer allocator.free(offer_sdp);

    try std.testing.expect(validateSdp(offer_sdp));

    const desc = SessionDescription{
        .sdp_type = .offer,
        .sdp_text = offer_sdp,
    };
    try std.testing.expectEqualStrings("offer", desc.sdp_type.toString());
    try std.testing.expectEqual(Type.offer, Type.fromString("offer").?);
    try std.testing.expect(desc.hasVideoCodec("VP8"));
    try std.testing.expect(desc.hasVideoCodec("H264"));
    try std.testing.expect(!desc.hasVideoCodec("opus"));
    try std.testing.expect(desc.hasAudioCodec("opus"));
    try std.testing.expect(!desc.hasAudioCodec("VP8"));

    var parsed = try desc.parse(allocator);
    defer parsed.deinit();

    try std.testing.expectEqual(@as(u64, 424242), parsed.session_id);
    try std.testing.expectEqual(@as(usize, 3), parsed.bundle_mids.len);
    try std.testing.expectEqualStrings("0", parsed.bundle_mids[0]);
    try std.testing.expectEqualStrings("1", parsed.bundle_mids[1]);
    try std.testing.expectEqualStrings("2", parsed.bundle_mids[2]);
    try std.testing.expect(parsed.extmap_allow_mixed);
    try std.testing.expectEqualStrings("ufragOffer1", parsed.ice_ufrag);
    try std.testing.expectEqualStrings("pwdOfferSecret1234567890", parsed.ice_pwd);
    try std.testing.expectEqualStrings("sha-256", parsed.fingerprint_alg);
    try std.testing.expectEqualStrings(fp, parsed.fingerprint);
    try std.testing.expectEqual(SetupRole.actpass, parsed.setup);
    try std.testing.expect(parsed.hasCodec(.video, "VP8"));
    try std.testing.expect(parsed.hasCodec(.video, "h264"));
    try std.testing.expect(parsed.hasCodec(.audio, "OPUS"));

    try std.testing.expectEqual(@as(usize, 3), parsed.media_sections.len);

    // Video media section
    const video_sec = parsed.media_sections[0];
    try std.testing.expectEqual(MediaKind.video, video_sec.kind);
    try std.testing.expectEqualStrings("0", video_sec.mid);
    try std.testing.expectEqual(Direction.sendrecv, video_sec.direction);
    try std.testing.expect(video_sec.rtcp_mux);
    try std.testing.expect(video_sec.rtcp_rsize);
    try std.testing.expect(video_sec.ice_trickle);
    try std.testing.expectEqual(@as(usize, 3), video_sec.codecs.len);
    try std.testing.expectEqual(@as(u8, 96), video_sec.codecs[0].payload_type);
    try std.testing.expectEqualStrings("VP8", video_sec.codecs[0].name);
    try std.testing.expectEqual(@as(u8, 102), video_sec.codecs[1].payload_type);
    try std.testing.expectEqualStrings("H264", video_sec.codecs[1].name);
    try std.testing.expectEqual(@as(?u8, 1), video_sec.codecs[1].h264PacketizationMode());
    try std.testing.expectEqualStrings("42e01f", video_sec.codecs[1].h264ProfileLevelId().?);
    try std.testing.expectEqual(@as(u8, 104), video_sec.codecs[2].payload_type);
    try std.testing.expectEqual(@as(?u8, 0), video_sec.codecs[2].h264PacketizationMode());
    try std.testing.expectEqual(@as(usize, 1), video_sec.ssrcs.len);
    try std.testing.expectEqual(@as(u32, 0x12345678), video_sec.ssrcs[0].ssrc);
    try std.testing.expectEqualStrings("cuttlefish-video", video_sec.ssrcs[0].cname);
    try std.testing.expectEqualStrings("display_0 display_0", video_sec.ssrcs[0].msid);
    try std.testing.expect(std.mem.indexOf(u8, offer_sdp, "a=msid:display_0 display_0\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, offer_sdp, "a=ssrc:305419896 msid:display_0 display_0\r\n") != null);
    try std.testing.expectEqual(@as(usize, 1), video_sec.candidates.len);
    try std.testing.expectEqualStrings(candidates[0], video_sec.candidates[0]);

    // Audio media section
    const audio_sec = parsed.media_sections[1];
    try std.testing.expectEqual(MediaKind.audio, audio_sec.kind);
    try std.testing.expectEqualStrings("1", audio_sec.mid);
    try std.testing.expectEqual(@as(usize, 1), audio_sec.codecs.len);
    try std.testing.expectEqual(@as(u8, 111), audio_sec.codecs[0].payload_type);
    try std.testing.expectEqualStrings("opus", audio_sec.codecs[0].name);
    try std.testing.expectEqual(@as(u32, 48000), audio_sec.codecs[0].clock_rate);
    try std.testing.expectEqual(@as(u8, 2), audio_sec.codecs[0].channels);
    try std.testing.expectEqualStrings("minptime=10;useinbandfec=1", audio_sec.codecs[0].fmtp);
    try std.testing.expectEqual(@as(usize, 1), audio_sec.ssrcs.len);
    try std.testing.expectEqual(@as(u32, 0x87654321), audio_sec.ssrcs[0].ssrc);

    // Application (DataChannel) media section
    const app_sec = parsed.media_sections[2];
    try std.testing.expectEqual(MediaKind.application, app_sec.kind);
    try std.testing.expectEqualStrings("2", app_sec.mid);
    try std.testing.expectEqualStrings("UDP/DTLS/SCTP", app_sec.proto);
    try std.testing.expectEqual(@as(?u16, 5000), app_sec.sctp_port);
    try std.testing.expectEqual(@as(?u32, 262144), app_sec.max_message_size);

    // Round-trip serialization
    const reserialized = try parsed.serialize(allocator);
    defer allocator.free(reserialized);
    try std.testing.expectEqualStrings(offer_sdp, reserialized);
}

test "negotiateAnswerSdp against browser JSEP offer with session-level attributes and codec filtering" {
    const allocator = std.testing.allocator;

    const browser_offer =
        "v=0\n" ++
        "o=- 8877665544 2 IN IP4 127.0.0.1\n" ++
        "s=-\n" ++
        "t=0 0\n" ++
        "a=group:BUNDLE video0 audio0 data0\n" ++
        "a=extmap-allow-mixed\n" ++
        "a=ice-ufrag:remoteUfrag\n" ++
        "a=ice-pwd:remotePasswordLongEnough123\n" ++
        "a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF:11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF\n" ++
        "a=setup:actpass\n" ++
        "m=video 9 UDP/TLS/RTP/SAVPF 96 98 102 108\n" ++
        "c=IN IP4 0.0.0.0\n" ++
        "a=mid:video0\n" ++
        "a=sendrecv\n" ++
        "a=rtcp-mux\n" ++
        "a=rtcp-rsize\n" ++
        "a=rtpmap:96 VP8/90000\n" ++
        "a=rtpmap:98 VP9/90000\n" ++
        "a=rtpmap:102 H264/90000\n" ++
        "a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\n" ++
        "a=rtpmap:108 H264/90000\n" ++
        "a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=2;profile-level-id=42e01f\n" ++
        "m=audio 9 UDP/TLS/RTP/SAVPF 0 111\n" ++
        "c=IN IP4 0.0.0.0\n" ++
        "a=mid:audio0\n" ++
        "a=sendonly\n" ++
        "a=rtcp-mux\n" ++
        "a=rtpmap:0 PCMU/8000\n" ++
        "a=rtpmap:111 OPUS/48000/2\n" ++
        "a=fmtp:111 minptime=10;useinbandfec=1\n" ++
        "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\n" ++
        "c=IN IP4 0.0.0.0\n" ++
        "a=mid:data0\n" ++
        "a=sctp-port:5000\n" ++
        "a=max-message-size:262144\n";

    const local_fp = "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00:FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00";
    const answer_sdp = try negotiateAnswerSdp(allocator, browser_offer, .{
        .session_id = 999001,
        .ice_ufrag = "localAnswerUfrag",
        .ice_pwd = "localAnswerPassword123456",
        .fingerprint = local_fp,
    });
    defer allocator.free(answer_sdp);

    var ans = try parseSdp(allocator, answer_sdp);
    defer ans.deinit();

    try std.testing.expectEqual(@as(usize, 3), ans.bundle_mids.len);
    try std.testing.expectEqualStrings("video0", ans.bundle_mids[0]);
    try std.testing.expectEqualStrings("audio0", ans.bundle_mids[1]);
    try std.testing.expectEqualStrings("data0", ans.bundle_mids[2]);

    // Video section: VP9 and H264 mode 2 must be filtered out; VP8 (96) and H264 mode 1 (102) kept.
    const v_sec = ans.media_sections[0];
    try std.testing.expectEqual(SetupRole.active, v_sec.setup);
    try std.testing.expectEqualStrings("video0", v_sec.mid);
    try std.testing.expectEqual(Direction.sendrecv, v_sec.direction);
    try std.testing.expectEqual(@as(usize, 2), v_sec.codecs.len);
    try std.testing.expectEqual(@as(u8, 96), v_sec.codecs[0].payload_type);
    try std.testing.expectEqualStrings("VP8", v_sec.codecs[0].name);
    try std.testing.expectEqual(@as(u8, 102), v_sec.codecs[1].payload_type);
    try std.testing.expectEqualStrings("H264", v_sec.codecs[1].name);
    try std.testing.expectEqual(@as(?u8, 1), v_sec.codecs[1].h264PacketizationMode());
    try std.testing.expectEqual(@as(usize, 1), v_sec.ssrcs.len);
    try std.testing.expectEqual(@as(u32, 0x22334455), v_sec.ssrcs[0].ssrc);
    try std.testing.expectEqualStrings("display_0 display_0", v_sec.ssrcs[0].msid);
    try std.testing.expect(std.mem.indexOf(u8, answer_sdp, "a=msid:display_0 display_0\r\n") != null);
    try std.testing.expect(!ans.hasCodec(.video, "VP9"));

    // Audio section: PCMU filtered out; opus (111) kept; remote sendonly -> local recvonly
    const a_sec = ans.media_sections[1];
    try std.testing.expectEqual(SetupRole.active, a_sec.setup);
    try std.testing.expectEqualStrings("audio0", a_sec.mid);
    try std.testing.expectEqual(Direction.recvonly, a_sec.direction);
    try std.testing.expectEqual(@as(usize, 1), a_sec.codecs.len);
    try std.testing.expectEqual(@as(u8, 111), a_sec.codecs[0].payload_type);
    try std.testing.expectEqualStrings("opus", a_sec.codecs[0].name);
    try std.testing.expectEqual(@as(usize, 0), a_sec.ssrcs.len);

    // DataChannel section
    const d_sec = ans.media_sections[2];
    try std.testing.expectEqual(SetupRole.active, d_sec.setup);
    try std.testing.expectEqualStrings("data0", d_sec.mid);
    try std.testing.expectEqual(@as(?u16, 5000), d_sec.sctp_port);

    // Also verify remote setup:active resolves to local setup:passive and H264 mode 0 matching
    const active_offer =
        "v=0\r\n" ++
        "o=- 112233 2 IN IP4 127.0.0.1\r\n" ++
        "s=-\r\n" ++
        "t=0 0\r\n" ++
        "a=group:BUNDLE v0\r\n" ++
        "m=video 9 UDP/TLS/RTP/SAVPF 104\r\n" ++
        "c=IN IP4 0.0.0.0\r\n" ++
        "a=mid:v0\r\n" ++
        "a=setup:active\r\n" ++
        "a=ice-ufrag:remAct\r\n" ++
        "a=ice-pwd:remActPassword1234567890\r\n" ++
        "a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF:11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF\r\n" ++
        "a=rtpmap:104 h264/90000\r\n" ++
        "a=fmtp:104 packetization-mode=0;profile-level-id=42e01f\r\n";

    const passive_answer_sdp = try negotiateAnswerSdp(allocator, active_offer, .{
        .session_id = 999002,
        .ice_ufrag = "localPassiveUfrag",
        .ice_pwd = "localPassivePassword123456",
        .fingerprint = local_fp,
    });
    defer allocator.free(passive_answer_sdp);

    var passive_ans = try parseSdp(allocator, passive_answer_sdp);
    defer passive_ans.deinit();
    try std.testing.expectEqual(SetupRole.passive, passive_ans.media_sections[0].setup);
    try std.testing.expectEqual(@as(usize, 1), passive_ans.media_sections[0].codecs.len);
    try std.testing.expectEqual(@as(?u8, 0), passive_ans.media_sections[0].codecs[0].h264PacketizationMode());
}

test "malformed SDP rejection and validateSdp" {
    const allocator = std.testing.allocator;

    try std.testing.expect(!validateSdp(""));
    try std.testing.expectError(Error.EmptySdp, parseSdp(allocator, ""));

    // Missing v=0
    const no_version = "o=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\nm=video 9 UDP/TLS/RTP/SAVPF 96\r\n";
    try std.testing.expect(!validateSdp(no_version));
    try std.testing.expectError(Error.MissingVersion, parseSdp(allocator, no_version));

    // Invalid version v=1
    const bad_version = "v=1\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\nm=video 9 UDP/TLS/RTP/SAVPF 96\r\n";
    try std.testing.expect(!validateSdp(bad_version));
    try std.testing.expectError(Error.InvalidSdpVersion, parseSdp(allocator, bad_version));

    // Missing m= section
    const no_media = "v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\n";
    try std.testing.expect(!validateSdp(no_media));
    try std.testing.expectError(Error.MissingMediaSection, parseSdp(allocator, no_media));

    // Invalid media kind
    const bad_media = "v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=slides 9 RTP/AVP 96\r\n";
    try std.testing.expect(!validateSdp(bad_media));
    try std.testing.expectError(Error.InvalidMediaKind, parseSdp(allocator, bad_media));

    // Invalid rtpmap
    const bad_rtpmap = "v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=video 9 UDP/TLS/RTP/SAVPF 96\r\na=rtpmap:96 VP8_no_clock\r\n";
    try std.testing.expect(!validateSdp(bad_rtpmap));
    try std.testing.expectError(Error.InvalidRtpMap, parseSdp(allocator, bad_rtpmap));
}
