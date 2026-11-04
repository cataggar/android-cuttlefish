const std = @import("std");
const linux = std.os.linux;
const Allocator = std.mem.Allocator;
const protocol = @import("protocol.zig");

pub const MAX_POLL_QUEUE_LEN: usize = 1000;
pub const CONNECTION_ID_LEN: usize = 64;

/// Futex-backed thread-safe mutex compatible with Zig 0.17.0 without requiring an Io context.
pub const Mutex = struct {
    state: std.atomic.Value(u32) = .init(0), // 0 = unlocked, 1 = locked, 2 = contended

    pub fn lock(self: *Mutex) void {
        if (self.state.cmpxchgStrong(0, 1, .acquire, .monotonic) == null) return;
        while (self.state.swap(2, .acquire) != 0) {
            _ = linux.futex_4arg(
                @ptrCast(&self.state.raw),
                .{ .cmd = .WAIT, .private = true },
                2,
                null,
            );
        }
    }

    pub fn unlock(self: *Mutex) void {
        const prev = self.state.swap(0, .release);
        if (prev == 2) {
            _ = linux.futex_3arg(
                @ptrCast(&self.state.raw),
                .{ .cmd = .WAKE, .private = true },
                1,
            );
        }
    }
};

pub fn sendAllRaw(fd: i32, data: []const u8) !void {
    var sent: usize = 0;
    while (sent < data.len) {
        const rc = linux.sendto(fd, data[sent..].ptr, data.len - sent, linux.MSG.NOSIGNAL, null, 0);
        switch (linux.errno(rc)) {
            .SUCCESS => {
                if (rc == 0) return error.BrokenPipe;
                sent += rc;
            },
            .INTR => continue,
            .AGAIN => {
                std.Thread.yield() catch {};
                continue;
            },
            else => return error.SocketSendFailed,
        }
    }
}

pub fn sendPacketRaw(fd: i32, data: []const u8) !void {
    while (true) {
        const rc = linux.sendto(fd, data.ptr, data.len, linux.MSG.NOSIGNAL, null, 0);
        switch (linux.errno(rc)) {
            .SUCCESS => return,
            .INTR => continue,
            .AGAIN => {
                std.Thread.yield() catch {};
                continue;
            },
            else => return error.SocketSendFailed,
        }
    }
}

pub fn sendWebSocketTextFrame(fd: i32, payload: []const u8) !void {
    var header: [10]u8 = undefined;
    header[0] = 0x81; // FIN + text frame opcode (0x1)
    var header_len: usize = 2;
    if (payload.len < 126) {
        header[1] = @intCast(payload.len);
    } else if (payload.len <= std.math.maxInt(u16)) {
        header[1] = 126;
        std.mem.writeInt(u16, header[2..4], @intCast(payload.len), .big);
        header_len = 4;
    } else {
        header[1] = 127;
        std.mem.writeInt(u64, header[2..10], @intCast(payload.len), .big);
        header_len = 10;
    }
    try sendAllRaw(fd, header[0..header_len]);
    if (payload.len > 0) {
        try sendAllRaw(fd, payload);
    }
}

pub const CallbackSink = struct {
    ctx: *anyopaque,
    send_fn: *const fn (ctx: *anyopaque, data: []const u8) anyerror!void,
};

pub const DeviceSink = union(enum) {
    unix_seqpacket_fd: i32,
    unix_stream_fd: i32,
    websocket_fd: i32,
    callback: CallbackSink,

    pub fn send(self: DeviceSink, data: []const u8) !void {
        switch (self) {
            .unix_seqpacket_fd => |fd| try sendPacketRaw(fd, data),
            .unix_stream_fd => |fd| try sendAllRaw(fd, data),
            .websocket_fd => |fd| try sendWebSocketTextFrame(fd, data),
            .callback => |cb| try cb.send_fn(cb.ctx, data),
        }
    }
};

pub const PollingState = struct {
    connection_id: [CONNECTION_ID_LEN]u8,
    queue: std.ArrayList([]u8) = .empty,

    pub fn deinit(self: *PollingState, allocator: Allocator) void {
        for (self.queue.items) |msg| {
            allocator.free(msg);
        }
        self.queue.deinit(allocator);
    }
};

pub const ClientSink = union(enum) {
    websocket_fd: i32,
    polling: PollingState,
    callback: CallbackSink,
};

pub const DeviceEntry = struct {
    device_id: []u8,
    device_port: u16,
    device_info_json: []u8,
    sink: DeviceSink,

    pub fn deinit(self: *DeviceEntry, allocator: Allocator) void {
        allocator.free(self.device_id);
        allocator.free(self.device_info_json);
    }
};

pub const ClientEntry = struct {
    client_id: u32,
    device_id: []u8,
    sink: ClientSink,

    pub fn deinit(self: *ClientEntry, allocator: Allocator) void {
        allocator.free(self.device_id);
        switch (self.sink) {
            .polling => |*p| p.deinit(allocator),
            else => {},
        }
    }
};

fn generateConnectionId() [CONNECTION_ID_LEN]u8 {
    const charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    var random_bytes: [CONNECTION_ID_LEN]u8 = undefined;
    _ = linux.getrandom(&random_bytes, random_bytes.len, 0);
    var out: [CONNECTION_ID_LEN]u8 = undefined;
    for (random_bytes, 0..) |b, i| {
        out[i] = charset[b % charset.len];
    }
    return out;
}

pub const Registry = struct {
    allocator: Allocator,
    mutex: Mutex = .{},
    devices: std.ArrayList(DeviceEntry) = .empty,
    clients: std.ArrayList(ClientEntry) = .empty,
    next_client_id: u32 = 1,

    pub fn init(allocator: Allocator) Registry {
        return .{
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *Registry) void {
        self.mutex.lock();
        defer self.mutex.unlock();

        for (self.devices.items) |*dev| {
            dev.deinit(self.allocator);
        }
        self.devices.deinit(self.allocator);

        for (self.clients.items) |*cli| {
            cli.deinit(self.allocator);
        }
        self.clients.deinit(self.allocator);
    }

    pub fn registerDevice(
        self: *Registry,
        device_id: []const u8,
        device_port: u16,
        device_info_json: []const u8,
        sink: DeviceSink,
    ) !void {
        self.mutex.lock();
        defer self.mutex.unlock();

        for (self.devices.items) |dev| {
            if (std.mem.eql(u8, dev.device_id, device_id)) {
                return error.DeviceAlreadyRegistered;
            }
        }

        const id_copy = try self.allocator.dupe(u8, device_id);
        errdefer self.allocator.free(id_copy);
        const info_copy = try self.allocator.dupe(u8, device_info_json);
        errdefer self.allocator.free(info_copy);

        try self.devices.append(self.allocator, .{
            .device_id = id_copy,
            .device_port = device_port,
            .device_info_json = info_copy,
            .sink = sink,
        });
    }

    pub fn unregisterDevice(self: *Registry, device_id: []const u8) void {
        self.mutex.lock();
        defer self.mutex.unlock();

        var i: usize = 0;
        while (i < self.devices.items.len) : (i += 1) {
            if (std.mem.eql(u8, self.devices.items[i].device_id, device_id)) {
                var removed = self.devices.orderedRemove(i);
                removed.deinit(self.allocator);
                return;
            }
        }
    }

    pub fn listDevicesJson(self: *Registry, allocator: Allocator) ![]u8 {
        self.mutex.lock();
        defer self.mutex.unlock();

        var ids: std.ArrayList([]const u8) = .empty;
        defer ids.deinit(allocator);

        for (self.devices.items) |dev| {
            try ids.append(allocator, dev.device_id);
        }

        return protocol.formatDevicesList(allocator, ids.items);
    }

    pub fn getDeviceInfoJson(self: *Registry, allocator: Allocator, device_id: []const u8) !?[]u8 {
        self.mutex.lock();
        defer self.mutex.unlock();

        for (self.devices.items) |dev| {
            if (std.mem.eql(u8, dev.device_id, device_id)) {
                return try allocator.dupe(u8, dev.device_info_json);
            }
        }
        return null;
    }

    pub const WebSocketConnectResult = struct {
        client_id: u32,
        device_info_json: []u8,
    };

    pub fn connectWebSocketClient(
        self: *Registry,
        allocator: Allocator,
        device_id: []const u8,
        ws_fd: i32,
    ) !WebSocketConnectResult {
        return self.connectClientInternal(allocator, device_id, .{ .websocket_fd = ws_fd });
    }

    pub fn connectCallbackClient(
        self: *Registry,
        allocator: Allocator,
        device_id: []const u8,
        cb: CallbackSink,
    ) !WebSocketConnectResult {
        return self.connectClientInternal(allocator, device_id, .{ .callback = cb });
    }

    fn connectClientInternal(
        self: *Registry,
        allocator: Allocator,
        device_id: []const u8,
        sink: ClientSink,
    ) !WebSocketConnectResult {
        self.mutex.lock();
        defer self.mutex.unlock();

        const dev_info = blk: {
            for (self.devices.items) |dev| {
                if (std.mem.eql(u8, dev.device_id, device_id)) {
                    break :blk dev.device_info_json;
                }
            }
            return error.DeviceNotFound;
        };

        const dev_info_copy = try allocator.dupe(u8, dev_info);
        errdefer allocator.free(dev_info_copy);

        const dev_id_copy = try self.allocator.dupe(u8, device_id);
        errdefer self.allocator.free(dev_id_copy);

        const client_id = self.next_client_id;
        self.next_client_id +%= 1;
        if (self.next_client_id == 0) self.next_client_id = 1;

        try self.clients.append(self.allocator, .{
            .client_id = client_id,
            .device_id = dev_id_copy,
            .sink = sink,
        });

        return .{
            .client_id = client_id,
            .device_info_json = dev_info_copy,
        };
    }

    pub const PollingConnectResult = struct {
        client_id: u32,
        connection_id: [CONNECTION_ID_LEN]u8,
        device_info_json: []u8,
    };

    pub fn connectPollingClient(
        self: *Registry,
        allocator: Allocator,
        device_id: []const u8,
    ) !PollingConnectResult {
        self.mutex.lock();
        defer self.mutex.unlock();

        const dev_info = blk: {
            for (self.devices.items) |dev| {
                if (std.mem.eql(u8, dev.device_id, device_id)) {
                    break :blk dev.device_info_json;
                }
            }
            return error.DeviceNotFound;
        };

        const dev_info_copy = try allocator.dupe(u8, dev_info);
        errdefer allocator.free(dev_info_copy);

        const dev_id_copy = try self.allocator.dupe(u8, device_id);
        errdefer self.allocator.free(dev_id_copy);

        const conn_id = generateConnectionId();
        const client_id = self.next_client_id;
        self.next_client_id +%= 1;
        if (self.next_client_id == 0) self.next_client_id = 1;

        try self.clients.append(self.allocator, .{
            .client_id = client_id,
            .device_id = dev_id_copy,
            .sink = .{
                .polling = .{
                    .connection_id = conn_id,
                },
            },
        });

        return .{
            .client_id = client_id,
            .connection_id = conn_id,
            .device_info_json = dev_info_copy,
        };
    }

    /// Removes client and sends `{"message_type":"client_disconnected","client_id":<id>}` to its device.
    pub fn disconnectClient(self: *Registry, client_id: u32) void {
        self.mutex.lock();
        defer self.mutex.unlock();

        var found_device_id: ?[]u8 = null;
        defer if (found_device_id) |d| self.allocator.free(d);

        var i: usize = 0;
        while (i < self.clients.items.len) : (i += 1) {
            if (self.clients.items[i].client_id == client_id) {
                var removed = self.clients.orderedRemove(i);
                found_device_id = self.allocator.dupe(u8, removed.device_id) catch null;
                removed.deinit(self.allocator);
                break;
            }
        }

        const dev_id = found_device_id orelse return;
        for (self.devices.items) |dev| {
            if (std.mem.eql(u8, dev.device_id, dev_id)) {
                const msg = protocol.formatClientDisconnected(self.allocator, client_id) catch return;
                defer self.allocator.free(msg);
                dev.sink.send(msg) catch {};
                return;
            }
        }
    }

    /// Sends `{"message_type":"client_msg","client_id":<id>,"payload":<raw_json>}` to the associated device.
    pub fn forwardClientToDevice(self: *Registry, client_id: u32, payload_raw_json: []const u8) !void {
        self.mutex.lock();
        defer self.mutex.unlock();

        const dev_id = blk: {
            for (self.clients.items) |cli| {
                if (cli.client_id == client_id) {
                    break :blk cli.device_id;
                }
            }
            return error.ClientNotFound;
        };

        const dev_sink = blk: {
            for (self.devices.items) |dev| {
                if (std.mem.eql(u8, dev.device_id, dev_id)) {
                    break :blk dev.sink;
                }
            }
            return error.DeviceNotFound;
        };

        const msg = try protocol.formatClientMsg(self.allocator, client_id, payload_raw_json);
        defer self.allocator.free(msg);
        try dev_sink.send(msg);
    }

    /// Forwards a polling client's message to the device and drains any queued device messages.
    pub fn forwardPollingClientAndDrain(
        self: *Registry,
        allocator: Allocator,
        connection_id: []const u8,
        payload_raw_json: []const u8,
    ) ![]u8 {
        self.mutex.lock();
        defer self.mutex.unlock();

        const client_idx = self.findPollingClientIndexLocked(connection_id) orelse return error.ClientNotFound;
        const client_id = self.clients.items[client_idx].client_id;
        const dev_id = self.clients.items[client_idx].device_id;

        const dev_sink = blk: {
            for (self.devices.items) |dev| {
                if (std.mem.eql(u8, dev.device_id, dev_id)) {
                    break :blk dev.sink;
                }
            }
            return error.DeviceNotFound;
        };

        const msg = try protocol.formatClientMsg(self.allocator, client_id, payload_raw_json);
        defer self.allocator.free(msg);
        try dev_sink.send(msg);

        return self.drainPollingQueueLocked(allocator, client_idx);
    }

    /// Drains and returns queued device messages `[...]` for a polling client.
    pub fn drainPollMessages(
        self: *Registry,
        allocator: Allocator,
        connection_id: []const u8,
    ) ![]u8 {
        self.mutex.lock();
        defer self.mutex.unlock();

        const client_idx = self.findPollingClientIndexLocked(connection_id) orelse return error.ClientNotFound;
        return self.drainPollingQueueLocked(allocator, client_idx);
    }

    /// Sends `{"message_type":"device_msg","payload":<raw_json>}` to a connected client (or queues it for polling).
    pub fn forwardDeviceToClient(
        self: *Registry,
        device_id: []const u8,
        client_id: u32,
        payload_raw_json: []const u8,
    ) !void {
        self.mutex.lock();
        defer self.mutex.unlock();

        for (self.clients.items) |*cli| {
            if (cli.client_id == client_id) {
                if (device_id.len > 0 and !std.mem.eql(u8, cli.device_id, device_id)) {
                    return error.DeviceMismatch;
                }
                const msg = try protocol.formatDeviceMsg(self.allocator, payload_raw_json);
                switch (cli.sink) {
                    .websocket_fd => |fd| {
                        defer self.allocator.free(msg);
                        try sendWebSocketTextFrame(fd, msg);
                    },
                    .callback => |cb| {
                        defer self.allocator.free(msg);
                        try cb.send_fn(cb.ctx, msg);
                    },
                    .polling => |*poll_state| {
                        errdefer self.allocator.free(msg);
                        if (poll_state.queue.items.len >= MAX_POLL_QUEUE_LEN) {
                            self.allocator.free(msg);
                            return error.PollQueueFull;
                        }
                        try poll_state.queue.append(self.allocator, msg);
                    },
                }
                return;
            }
        }
        return error.ClientNotFound;
    }

    fn findPollingClientIndexLocked(self: *Registry, connection_id: []const u8) ?usize {
        if (connection_id.len != CONNECTION_ID_LEN) return null;
        for (self.clients.items, 0..) |cli, idx| {
            switch (cli.sink) {
                .polling => |p| {
                    if (std.mem.eql(u8, &p.connection_id, connection_id)) {
                        return idx;
                    }
                },
                else => {},
            }
        }
        return null;
    }

    fn drainPollingQueueLocked(self: *Registry, allocator: Allocator, client_idx: usize) ![]u8 {
        var poll_state = &self.clients.items[client_idx].sink.polling;
        defer {
            for (poll_state.queue.items) |item| {
                self.allocator.free(item);
            }
            poll_state.queue.clearRetainingCapacity();
        }
        return try protocol.formatJsonArrayFromRaw(allocator, poll_state.queue.items);
    }
};
