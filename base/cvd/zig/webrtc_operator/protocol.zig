const std = @import("std");
const builtin = @import("builtin");
const Allocator = std.mem.Allocator;

// Signaling field keys matching cuttlefish/host/frontend/webrtc_operator/constants/signaling_constants.h
pub const kTypeField = "message_type";
pub const kDeviceInfoField = "device_info";
pub const kDeviceIdField = "device_id";
pub const kClientIdField = "client_id";
pub const kPayloadField = "payload";
pub const kServersField = "ice_servers";
pub const kConnectionIdField = "connection_id";
pub const kDevicePortField = "device_port";
pub const kUrlsField = "urls";
pub const kUsernameField = "username";
pub const kCredentialField = "credential";
pub const kCredentialTypeField = "credentialType";

pub const Fields = struct {
    pub const message_type = kTypeField;
    pub const device_info = kDeviceInfoField;
    pub const device_id = kDeviceIdField;
    pub const client_id = kClientIdField;
    pub const payload = kPayloadField;
    pub const ice_servers = kServersField;
    pub const connection_id = kConnectionIdField;
    pub const device_port = kDevicePortField;
    pub const urls = kUrlsField;
    pub const username = kUsernameField;
    pub const credential = kCredentialField;
    pub const credentialType = kCredentialTypeField;
};

// Signaling message types matching signaling_constants.h
pub const kRegisterType = "register";
pub const kForwardType = "forward";
pub const kConfigType = "config";
pub const kConnectType = "connect";
pub const kDeviceInfoType = "device_info";
pub const kClientMessageType = "client_msg";
pub const kClientDisconnectType = "client_disconnected";
pub const kDeviceMessageType = "device_msg";
pub const kClientPollType = "client_poll";

pub const MessageTypes = struct {
    pub const register = kRegisterType;
    pub const forward = kForwardType;
    pub const config = kConfigType;
    pub const connect = kConnectType;
    pub const device_info = kDeviceInfoType;
    pub const client_msg = kClientMessageType;
    pub const client_disconnected = kClientDisconnectType;
    pub const device_msg = kDeviceMessageType;
    pub const client_poll = kClientPollType;
};

pub const ProtocolError = error{
    InvalidJson,
    MissingMessageType,
    UnknownMessageType,
    MissingDeviceId,
    MissingClientId,
    InvalidClientId,
    MissingPayload,
    MissingConnectionId,
};

pub fn stringifyValue(allocator: Allocator, value: std.json.Value) ![]u8 {
    var out: std.Io.Writer.Allocating = .init(allocator);
    defer out.deinit();
    try std.json.Stringify.value(value, .{}, &out.writer);
    return try allocator.dupe(u8, out.written());
}

pub fn stringifyString(allocator: Allocator, str: []const u8) ![]u8 {
    var out: std.Io.Writer.Allocating = .init(allocator);
    defer out.deinit();
    try std.json.Stringify.value(str, .{}, &out.writer);
    return try allocator.dupe(u8, out.written());
}

/// Server config: `{"message_type":"config","ice_servers":[{"urls":["stun:<stun_server>"]}]}`
pub fn formatConfig(allocator: Allocator, stun_server: []const u8) ![]u8 {
    if (stun_server.len == 0) {
        return std.fmt.allocPrint(
            allocator,
            "{{\"{s}\":\"{s}\",\"{s}\":[]}}",
            .{ kTypeField, kConfigType, kServersField },
        );
    }
    const has_prefix = std.mem.startsWith(u8, stun_server, "stun:") or std.mem.startsWith(u8, stun_server, "turn:");
    if (has_prefix) {
        return std.fmt.allocPrint(
            allocator,
            "{{\"{s}\":\"{s}\",\"{s}\":[{{\"{s}\":[\"{s}\"]}}]}}",
            .{ kTypeField, kConfigType, kServersField, kUrlsField, stun_server },
        );
    }
    return std.fmt.allocPrint(
        allocator,
        "{{\"{s}\":\"{s}\",\"{s}\":[{{\"{s}\":[\"stun:{s}\"]}}]}}",
        .{ kTypeField, kConfigType, kServersField, kUrlsField, stun_server },
    );
}

/// Device info reply: `{"message_type":"device_info","device_info":<raw_json>}`
pub fn formatDeviceInfo(allocator: Allocator, device_info_raw_json: []const u8) ![]u8 {
    return std.fmt.allocPrint(
        allocator,
        "{{\"{s}\":\"{s}\",\"{s}\":{s}}}",
        .{ kTypeField, kDeviceInfoType, kDeviceInfoField, device_info_raw_json },
    );
}

/// Polling connect reply: `{"connection_id":"<secret>","message_type":"device_info","device_info":<raw_json>}`
pub fn formatPollingConnectReply(allocator: Allocator, connection_id: []const u8, device_info_raw_json: []const u8) ![]u8 {
    return std.fmt.allocPrint(
        allocator,
        "{{\"{s}\":\"{s}\",\"{s}\":\"{s}\",\"{s}\":{s}}}",
        .{ kConnectionIdField, connection_id, kTypeField, kDeviceInfoType, kDeviceInfoField, device_info_raw_json },
    );
}

/// Forward to device: `{"message_type":"client_msg","client_id":<id>,"payload":<raw_json>}`
pub fn formatClientMsg(allocator: Allocator, client_id: u32, payload_raw_json: []const u8) ![]u8 {
    return std.fmt.allocPrint(
        allocator,
        "{{\"{s}\":\"{s}\",\"{s}\":{d},\"{s}\":{s}}}",
        .{ kTypeField, kClientMessageType, kClientIdField, client_id, kPayloadField, payload_raw_json },
    );
}

/// Forward to client: `{"message_type":"device_msg","payload":<raw_json>}`
pub fn formatDeviceMsg(allocator: Allocator, payload_raw_json: []const u8) ![]u8 {
    return std.fmt.allocPrint(
        allocator,
        "{{\"{s}\":\"{s}\",\"{s}\":{s}}}",
        .{ kTypeField, kDeviceMessageType, kPayloadField, payload_raw_json },
    );
}

/// Client disconnected: `{"message_type":"client_disconnected","client_id":<id>}`
pub fn formatClientDisconnected(allocator: Allocator, client_id: u32) ![]u8 {
    return std.fmt.allocPrint(
        allocator,
        "{{\"{s}\":\"{s}\",\"{s}\":{d}}}",
        .{ kTypeField, kClientDisconnectType, kClientIdField, client_id },
    );
}

/// Error reply: `{"error":"<msg>"}`
pub fn formatError(allocator: Allocator, msg: []const u8) ![]u8 {
    const escaped = try stringifyString(allocator, msg);
    defer allocator.free(escaped);
    return std.fmt.allocPrint(allocator, "{{\"error\":{s}}}", .{escaped});
}

/// Formats a list of device IDs as a JSON array: `["id1","id2"]`
pub fn formatDevicesList(allocator: Allocator, device_ids: []const []const u8) ![]u8 {
    var out: std.Io.Writer.Allocating = .init(allocator);
    defer out.deinit();
    try std.json.Stringify.value(device_ids, .{}, &out.writer);
    return try allocator.dupe(u8, out.written());
}

/// Formats a slice of pre-serialized JSON items as a JSON array: `[<item1>,<item2>,...]`
pub fn formatJsonArrayFromRaw(allocator: Allocator, raw_items: []const []const u8) ![]u8 {
    var list: std.ArrayList(u8) = .empty;
    errdefer list.deinit(allocator);
    try list.append(allocator, '[');
    for (raw_items, 0..) |item, idx| {
        if (idx > 0) try list.append(allocator, ',');
        try list.appendSlice(allocator, item);
    }
    try list.append(allocator, ']');
    return list.toOwnedSlice(allocator);
}

pub const DeviceRegisterMsg = struct {
    device_id: []u8,
    device_port: u16,
    device_info_json: []u8,

    pub fn deinit(self: *const DeviceRegisterMsg, allocator: Allocator) void {
        allocator.free(self.device_id);
        allocator.free(self.device_info_json);
    }
};

pub const DeviceForwardMsg = struct {
    client_id: u32,
    payload_json: []u8,

    pub fn deinit(self: *const DeviceForwardMsg, allocator: Allocator) void {
        allocator.free(self.payload_json);
    }
};

pub const ParsedDeviceMessage = union(enum) {
    register: DeviceRegisterMsg,
    forward: DeviceForwardMsg,

    pub fn deinit(self: *const ParsedDeviceMessage, allocator: Allocator) void {
        switch (self.*) {
            .register => |*r| r.deinit(allocator),
            .forward => |*f| f.deinit(allocator),
        }
    }
};

pub fn parseDeviceMessage(allocator: Allocator, raw_json: []const u8) !ParsedDeviceMessage {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, raw_json, .{}) catch {
        return error.InvalidJson;
    };
    defer parsed.deinit();

    const root = switch (parsed.value) {
        .object => |obj| obj,
        else => return error.InvalidJson,
    };

    const msg_type_val = root.get(kTypeField) orelse return error.MissingMessageType;
    const msg_type = switch (msg_type_val) {
        .string => |s| s,
        else => return error.MissingMessageType,
    };

    if (std.mem.eql(u8, msg_type, kRegisterType)) {
        const device_id_val = root.get(kDeviceIdField) orelse return error.MissingDeviceId;
        const device_id_str = switch (device_id_val) {
            .string => |s| if (s.len > 0) s else return error.MissingDeviceId,
            else => return error.MissingDeviceId,
        };

        var port: u16 = 0;
        if (root.get(kDevicePortField)) |port_val| {
            switch (port_val) {
                .integer => |i| {
                    if (i >= 0 and i <= std.math.maxInt(u16)) {
                        port = @intCast(i);
                    }
                },
                else => {},
            }
        }

        const device_id_owned = try allocator.dupe(u8, device_id_str);
        errdefer allocator.free(device_id_owned);

        const info_json = if (root.get(kDeviceInfoField)) |info_val|
            try stringifyValue(allocator, info_val)
        else
            try allocator.dupe(u8, "{}");

        return .{
            .register = .{
                .device_id = device_id_owned,
                .device_port = port,
                .device_info_json = info_json,
            },
        };
    } else if (std.mem.eql(u8, msg_type, kForwardType)) {
        const client_id_val = root.get(kClientIdField) orelse return error.MissingClientId;
        const client_id: u32 = switch (client_id_val) {
            .integer => |i| if (i > 0 and i <= std.math.maxInt(u32)) @intCast(i) else return error.InvalidClientId,
            else => return error.InvalidClientId,
        };

        const payload_val = root.get(kPayloadField) orelse return error.MissingPayload;
        const payload_json = try stringifyValue(allocator, payload_val);

        return .{
            .forward = .{
                .client_id = client_id,
                .payload_json = payload_json,
            },
        };
    } else {
        return error.UnknownMessageType;
    }
}

pub const ParsedClientMessage = union(enum) {
    connect: struct {
        device_id: []u8,
    },
    forward: struct {
        payload_json: []u8,
    },
    client_poll: struct {
        connection_id: ?[]u8,
    },

    pub fn deinit(self: *const ParsedClientMessage, allocator: Allocator) void {
        switch (self.*) {
            .connect => |c| allocator.free(c.device_id),
            .forward => |f| allocator.free(f.payload_json),
            .client_poll => |p| if (p.connection_id) |cid| allocator.free(cid),
        }
    }
};

pub fn parseClientMessage(allocator: Allocator, raw_json: []const u8) !ParsedClientMessage {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, raw_json, .{}) catch {
        return error.InvalidJson;
    };
    defer parsed.deinit();

    const root = switch (parsed.value) {
        .object => |obj| obj,
        else => return error.InvalidJson,
    };

    const msg_type_val = root.get(kTypeField) orelse return error.MissingMessageType;
    const msg_type = switch (msg_type_val) {
        .string => |s| s,
        else => return error.MissingMessageType,
    };

    if (std.mem.eql(u8, msg_type, kConnectType)) {
        const device_id_val = root.get(kDeviceIdField) orelse return error.MissingDeviceId;
        const device_id_str = switch (device_id_val) {
            .string => |s| if (s.len > 0) s else return error.MissingDeviceId,
            else => return error.MissingDeviceId,
        };
        return .{
            .connect = .{
                .device_id = try allocator.dupe(u8, device_id_str),
            },
        };
    } else if (std.mem.eql(u8, msg_type, kForwardType)) {
        const payload_val = root.get(kPayloadField) orelse return error.MissingPayload;
        return .{
            .forward = .{
                .payload_json = try stringifyValue(allocator, payload_val),
            },
        };
    } else if (std.mem.eql(u8, msg_type, kClientPollType)) {
        var cid: ?[]u8 = null;
        if (root.get(kConnectionIdField)) |cid_val| {
            if (cid_val == .string) {
                cid = try allocator.dupe(u8, cid_val.string);
            }
        }
        return .{
            .client_poll = .{
                .connection_id = cid,
            },
        };
    } else {
        return error.UnknownMessageType;
    }
}

pub fn parseConnectHttpBody(allocator: Allocator, raw_json: []const u8) ![]u8 {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, raw_json, .{}) catch {
        return error.InvalidJson;
    };
    defer parsed.deinit();

    const root = switch (parsed.value) {
        .object => |obj| obj,
        else => return error.InvalidJson,
    };

    const device_id_val = root.get(kDeviceIdField) orelse return error.MissingDeviceId;
    const device_id_str = switch (device_id_val) {
        .string => |s| if (s.len > 0) s else return error.MissingDeviceId,
        else => return error.MissingDeviceId,
    };
    return try allocator.dupe(u8, device_id_str);
}

pub const ForwardHttpBody = struct {
    connection_id: []u8,
    payload_json: []u8,

    pub fn deinit(self: *const ForwardHttpBody, allocator: Allocator) void {
        allocator.free(self.connection_id);
        allocator.free(self.payload_json);
    }
};

pub fn parseForwardHttpBody(allocator: Allocator, raw_json: []const u8) !ForwardHttpBody {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, raw_json, .{}) catch {
        return error.InvalidJson;
    };
    defer parsed.deinit();

    const root = switch (parsed.value) {
        .object => |obj| obj,
        else => return error.InvalidJson,
    };

    const conn_val = root.get(kConnectionIdField) orelse return error.MissingConnectionId;
    const conn_str = switch (conn_val) {
        .string => |s| if (s.len > 0) s else return error.MissingConnectionId,
        else => return error.MissingConnectionId,
    };

    const payload_val = root.get(kPayloadField) orelse return error.MissingPayload;
    const conn_owned = try allocator.dupe(u8, conn_str);
    errdefer allocator.free(conn_owned);
    const payload_owned = try stringifyValue(allocator, payload_val);

    return .{
        .connection_id = conn_owned,
        .payload_json = payload_owned,
    };
}

pub fn parsePollMessagesHttpBody(allocator: Allocator, raw_json: []const u8) ![]u8 {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, raw_json, .{}) catch {
        return error.InvalidJson;
    };
    defer parsed.deinit();

    const root = switch (parsed.value) {
        .object => |obj| obj,
        else => return error.InvalidJson,
    };

    const conn_val = root.get(kConnectionIdField) orelse return error.MissingConnectionId;
    const conn_str = switch (conn_val) {
        .string => |s| if (s.len > 0) s else return error.MissingConnectionId,
        else => return error.MissingConnectionId,
    };
    return try allocator.dupe(u8, conn_str);
}

pub const os_socket = struct {
    pub const is_linux = builtin.os.tag == .linux;
    const linux = std.os.linux;

    pub const PATH_MAX: usize = if (is_linux) linux.PATH_MAX else 1024;
    pub const MAX_UNIX_SOCKET_PATH: usize = if (is_linux) 108 else 104;

    pub const AF_UNIX: i32 = 1;
    pub const AF_INET: i32 = 2;
    pub const SOCK_STREAM: i32 = 1;
    pub const SOCK_SEQPACKET: i32 = 5;

    pub const SOL_SOCKET: i32 = if (is_linux) linux.SOL.SOCKET else 0xffff;
    pub const SO_REUSEADDR: i32 = if (is_linux) linux.SO.REUSEADDR else 0x0004;
    pub const SO_NOSIGPIPE: i32 = 0x1022;

    pub const POLLIN: i16 = 0x0001;
    pub const POLLERR: i16 = 0x0008;
    pub const POLLHUP: i16 = 0x0010;
    pub const POLLNVAL: i16 = 0x0020;

    pub const SockAddrIn = if (is_linux)
        linux.sockaddr.in
    else
        extern struct {
            len: u8 = @sizeOf(@This()),
            family: u8 = 2,
            port: u16,
            addr: u32,
            zero: [8]u8 = @splat(0),
        };

    pub const SockAddrUn = if (is_linux)
        linux.sockaddr.un
    else
        extern struct {
            len: u8 = @sizeOf(@This()),
            family: u8 = 1,
            path: [104]u8 = @splat(0),
        };

    pub const PollFd = if (is_linux) linux.pollfd else std.c.pollfd;

    extern "c" fn arc4random_buf(buf: [*]u8, nbytes: usize) void;

    pub fn initSockAddrIn(host_ipv4_be: u32, port_native: u16) SockAddrIn {
        if (is_linux) {
            return .{
                .family = linux.AF.INET,
                .port = std.mem.nativeToBig(u16, port_native),
                .addr = host_ipv4_be,
                .zero = @splat(0),
            };
        } else {
            return .{
                .len = @sizeOf(SockAddrIn),
                .family = @intCast(AF_INET),
                .port = std.mem.nativeToBig(u16, port_native),
                .addr = host_ipv4_be,
                .zero = @splat(0),
            };
        }
    }

    pub fn initSockAddrUn(socket_path: []const u8) !SockAddrUn {
        if (socket_path.len == 0 or socket_path.len >= MAX_UNIX_SOCKET_PATH) {
            return error.SocketPathTooLong;
        }
        var addr: SockAddrUn = if (is_linux) .{
            .family = linux.AF.UNIX,
            .path = @splat(0),
        } else .{
            .len = @sizeOf(SockAddrUn),
            .family = @intCast(AF_UNIX),
            .path = @splat(0),
        };
        @memcpy(addr.path[0..socket_path.len], socket_path);
        return addr;
    }

    pub fn configureDarwinSocket(fd: i32) void {
        if (!is_linux) {
            const F_SETFD: c_int = 2;
            const FD_CLOEXEC: c_int = 1;
            _ = std.c.fcntl(fd, F_SETFD, FD_CLOEXEC);
            const one: c_int = 1;
            _ = std.c.setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, @sizeOf(c_int));
        }
    }

    pub fn socketTcp() !i32 {
        if (is_linux) {
            const rc = linux.socket(linux.AF.INET, linux.SOCK.STREAM | linux.SOCK.CLOEXEC, 0);
            if (linux.errno(rc) != .SUCCESS) return error.SocketCreationFailed;
            return @intCast(rc);
        } else {
            const fd = std.c.socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) return error.SocketCreationFailed;
            configureDarwinSocket(fd);
            return fd;
        }
    }

    pub fn socketUnixSeqPacket() !i32 {
        if (is_linux) {
            const rc = linux.socket(linux.AF.UNIX, linux.SOCK.SEQPACKET | linux.SOCK.CLOEXEC, 0);
            if (linux.errno(rc) != .SUCCESS) return error.SocketCreationFailed;
            return @intCast(rc);
        } else {
            const fd = std.c.socket(AF_UNIX, SOCK_SEQPACKET, 0);
            if (fd < 0) return error.SocketCreationFailed;
            configureDarwinSocket(fd);
            return fd;
        }
    }

    pub fn socketUnixStream() !i32 {
        if (is_linux) {
            const rc = linux.socket(linux.AF.UNIX, linux.SOCK.STREAM | linux.SOCK.CLOEXEC, 0);
            if (linux.errno(rc) != .SUCCESS) return error.SocketCreationFailed;
            return @intCast(rc);
        } else {
            const fd = std.c.socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) return error.SocketCreationFailed;
            configureDarwinSocket(fd);
            return fd;
        }
    }

    pub fn setReuseAddr(fd: i32) void {
        if (is_linux) {
            const one: i32 = 1;
            _ = linux.setsockopt(fd, linux.SOL.SOCKET, linux.SO.REUSEADDR, std.mem.asBytes(&one), @sizeOf(i32));
        } else {
            const one: c_int = 1;
            _ = std.c.setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, @sizeOf(c_int));
        }
    }

    pub fn bindIn(fd: i32, addr: *const SockAddrIn) !void {
        if (is_linux) {
            const rc = linux.bind(fd, @ptrCast(addr), @sizeOf(SockAddrIn));
            if (linux.errno(rc) != .SUCCESS) return error.BindFailed;
        } else {
            const rc = std.c.bind(fd, @ptrCast(addr), @sizeOf(SockAddrIn));
            if (rc != 0) return error.BindFailed;
        }
    }

    pub fn bindUn(fd: i32, addr: *const SockAddrUn) !void {
        if (is_linux) {
            const rc = linux.bind(fd, @ptrCast(addr), @sizeOf(SockAddrUn));
            if (linux.errno(rc) != .SUCCESS) return error.BindFailed;
        } else {
            const rc = std.c.bind(fd, @ptrCast(addr), @sizeOf(SockAddrUn));
            if (rc != 0) return error.BindFailed;
        }
    }

    pub fn listen(fd: i32, backlog: u31) !void {
        if (is_linux) {
            const rc = linux.listen(fd, backlog);
            if (linux.errno(rc) != .SUCCESS) return error.ListenFailed;
        } else {
            const rc = std.c.listen(fd, backlog);
            if (rc != 0) return error.ListenFailed;
        }
    }

    pub fn getsocknamePort(fd: i32, fallback_port: u16) u16 {
        var bound_addr: SockAddrIn = undefined;
        var addr_len: u32 = @sizeOf(SockAddrIn);
        if (is_linux) {
            if (linux.errno(linux.getsockname(fd, @ptrCast(&bound_addr), &addr_len)) == .SUCCESS) {
                return std.mem.bigToNative(u16, bound_addr.port);
            }
        } else {
            if (std.c.getsockname(fd, @ptrCast(&bound_addr), &addr_len) == 0) {
                return std.mem.bigToNative(u16, bound_addr.port);
            }
        }
        return fallback_port;
    }

    pub fn accept(listen_fd: i32) !i32 {
        if (is_linux) {
            const rc = linux.accept4(listen_fd, null, null, linux.SOCK.CLOEXEC);
            if (linux.errno(rc) != .SUCCESS) return error.AcceptFailed;
            return @intCast(rc);
        } else {
            const fd = std.c.accept(listen_fd, null, null);
            if (fd < 0) return error.AcceptFailed;
            configureDarwinSocket(fd);
            return fd;
        }
    }

    pub fn connectIn(fd: i32, addr: *const SockAddrIn) !void {
        if (is_linux) {
            const rc = linux.connect(fd, @ptrCast(addr), @sizeOf(SockAddrIn));
            if (linux.errno(rc) != .SUCCESS) return error.ConnectFailed;
        } else {
            const rc = std.c.connect(fd, @ptrCast(addr), @sizeOf(SockAddrIn));
            if (rc != 0) return error.ConnectFailed;
        }
    }

    pub fn connectUn(fd: i32, addr: *const SockAddrUn) !void {
        if (is_linux) {
            const rc = linux.connect(fd, @ptrCast(addr), @sizeOf(SockAddrUn));
            if (linux.errno(rc) != .SUCCESS) return error.ConnectFailed;
        } else {
            const rc = std.c.connect(fd, @ptrCast(addr), @sizeOf(SockAddrUn));
            if (rc != 0) return error.ConnectFailed;
        }
    }

    pub fn pollIn(fd: i32, timeout_ms: i32) struct { ready: bool, revents: i16 } {
        var pfd = [1]PollFd{
            .{
                .fd = fd,
                .events = POLLIN,
                .revents = 0,
            },
        };
        if (is_linux) {
            const rc = linux.poll(&pfd, 1, timeout_ms);
            if (linux.errno(rc) != .SUCCESS or rc == 0) return .{ .ready = false, .revents = 0 };
            return .{ .ready = true, .revents = pfd[0].revents };
        } else {
            const rc = std.c.poll(&pfd, 1, timeout_ms);
            if (rc <= 0) return .{ .ready = false, .revents = 0 };
            return .{ .ready = true, .revents = pfd[0].revents };
        }
    }

    pub fn recv(fd: i32, buf: []u8) !usize {
        if (is_linux) {
            while (true) {
                const rc = linux.recvfrom(fd, buf.ptr, buf.len, 0, null, null);
                switch (linux.errno(rc)) {
                    .SUCCESS => return @intCast(rc),
                    .INTR => continue,
                    else => return error.SocketReadFailed,
                }
            }
        } else {
            while (true) {
                const rc = std.c.recvfrom(fd, buf.ptr, buf.len, 0, null, null);
                if (rc >= 0) return @intCast(rc);
                const err = std.c._errno().*;
                if (err == @intFromEnum(std.c.E.INTR)) continue;
                return error.SocketReadFailed;
            }
        }
    }

    pub fn recvSeqPacket(allocator: Allocator, fd: i32, max_packet_bytes: usize) !?[]u8 {
        if (is_linux) {
            var peek_byte: [1]u8 = undefined;
            const peek_rc = linux.recvfrom(
                fd,
                &peek_byte,
                peek_byte.len,
                linux.MSG.PEEK | linux.MSG.TRUNC,
                null,
                null,
            );
            switch (linux.errno(peek_rc)) {
                .SUCCESS => {},
                .INTR => return try allocator.alloc(u8, 0),
                else => return null,
            }
            if (peek_rc == 0) return null;

            const pkt_len: usize = @min(@as(usize, @intCast(peek_rc)), max_packet_bytes);
            const buf = try allocator.alloc(u8, pkt_len);
            errdefer allocator.free(buf);

            const read_rc = linux.recvfrom(fd, buf.ptr, buf.len, 0, null, null);
            if (linux.errno(read_rc) != .SUCCESS or read_rc == 0) {
                allocator.free(buf);
                return null;
            }
            const actual_len: usize = @min(@as(usize, @intCast(read_rc)), buf.len);
            return buf[0..actual_len];
        } else {
            const buf = try allocator.alloc(u8, max_packet_bytes);
            errdefer allocator.free(buf);
            const n = recv(fd, buf) catch {
                allocator.free(buf);
                return null;
            };
            if (n == 0) {
                allocator.free(buf);
                return null;
            }
            const out = try allocator.dupe(u8, buf[0..n]);
            allocator.free(buf);
            return out;
        }
    }

    pub fn sendAll(fd: i32, data: []const u8) !void {
        if (is_linux) {
            var offset: usize = 0;
            while (offset < data.len) {
                const rc = linux.sendto(
                    fd,
                    data[offset..].ptr,
                    data.len - offset,
                    linux.MSG.NOSIGNAL,
                    null,
                    0,
                );
                switch (linux.errno(rc)) {
                    .SUCCESS => {
                        if (rc == 0) return error.ConnectionClosed;
                        offset += @intCast(rc);
                    },
                    .INTR, .AGAIN => continue,
                    else => return error.SocketWriteFailed,
                }
            }
        } else {
            configureDarwinSocket(fd);
            var offset: usize = 0;
            while (offset < data.len) {
                const rc = std.c.sendto(
                    fd,
                    data[offset..].ptr,
                    data.len - offset,
                    0,
                    null,
                    0,
                );
                if (rc > 0) {
                    offset += @intCast(rc);
                    continue;
                }
                if (rc == 0) return error.ConnectionClosed;
                const err = std.c._errno().*;
                if (err == @intFromEnum(std.c.E.INTR) or err == @intFromEnum(std.c.E.AGAIN)) continue;
                return error.SocketWriteFailed;
            }
        }
    }

    pub fn sendPacket(fd: i32, data: []const u8) !void {
        if (is_linux) {
            while (true) {
                const rc = linux.sendto(
                    fd,
                    data.ptr,
                    data.len,
                    linux.MSG.NOSIGNAL,
                    null,
                    0,
                );
                switch (linux.errno(rc)) {
                    .SUCCESS => return,
                    .INTR, .AGAIN => continue,
                    else => return error.SocketWriteFailed,
                }
            }
        } else {
            try sendAll(fd, data);
        }
    }

    pub fn shutdownRdwr(fd: i32) void {
        if (is_linux) {
            _ = linux.shutdown(fd, linux.SHUT.RDWR);
        } else {
            _ = std.c.shutdown(fd, 2);
        }
    }

    pub fn close(fd: i32) void {
        if (is_linux) {
            _ = linux.close(fd);
        } else {
            _ = std.c.close(fd);
        }
    }

    pub fn unlink(path_z: [*:0]const u8) void {
        if (is_linux) {
            _ = linux.unlink(path_z);
        } else {
            _ = std.c.unlink(path_z);
        }
    }

    pub fn writeStdout(bytes: []const u8) void {
        if (is_linux) {
            _ = linux.write(linux.STDOUT_FILENO, bytes.ptr, bytes.len);
        } else {
            _ = std.c.write(1, bytes.ptr, bytes.len);
        }
    }

    pub fn readFileAlloc(allocator: Allocator, path_z: [*:0]const u8, max_bytes: usize) ![]u8 {
        if (is_linux) {
            const open_rc = linux.openat(
                linux.AT.FDCWD,
                path_z,
                .{ .ACCMODE = .RDONLY, .CLOEXEC = true },
                0,
            );
            if (linux.errno(open_rc) != .SUCCESS) return error.FileNotFound;
            const file_fd: i32 = @intCast(open_rc);
            defer _ = linux.close(file_fd);

            var content: std.ArrayListUnmanaged(u8) = .empty;
            errdefer content.deinit(allocator);

            var read_buf: [8192]u8 = undefined;
            while (content.items.len < max_bytes) {
                const rrc = linux.read(file_fd, &read_buf, read_buf.len);
                if (linux.errno(rrc) != .SUCCESS) return error.ReadError;
                const n: usize = @intCast(rrc);
                if (n == 0) break;
                try content.appendSlice(allocator, read_buf[0..n]);
            }
            return try content.toOwnedSlice(allocator);
        } else {
            const file_fd = std.c.open(path_z, .{ .ACCMODE = .RDONLY, .CLOEXEC = true });
            if (file_fd < 0) return error.FileNotFound;
            defer _ = std.c.close(file_fd);

            var content: std.ArrayListUnmanaged(u8) = .empty;
            errdefer content.deinit(allocator);

            var read_buf: [8192]u8 = undefined;
            while (content.items.len < max_bytes) {
                const rrc = std.c.read(file_fd, &read_buf, read_buf.len);
                if (rrc < 0) return error.ReadError;
                const n: usize = @intCast(rrc);
                if (n == 0) break;
                try content.appendSlice(allocator, read_buf[0..n]);
            }
            return try content.toOwnedSlice(allocator);
        }
    }

    pub fn fillRandom(buf: []u8) void {
        if (is_linux) {
            _ = linux.getrandom(buf.ptr, buf.len, 0);
        } else {
            arc4random_buf(buf.ptr, buf.len);
        }
    }
};
