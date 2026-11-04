const std = @import("std");
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
