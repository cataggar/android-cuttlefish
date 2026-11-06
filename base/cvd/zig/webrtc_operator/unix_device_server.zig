const std = @import("std");
const builtin = @import("builtin");
const Allocator = std.mem.Allocator;
const protocol = @import("protocol.zig");
const os_socket = protocol.os_socket;
const registry_mod = @import("registry.zig");
const Registry = registry_mod.Registry;
const DeviceSink = registry_mod.DeviceSink;

pub const DEFAULT_SOCKET_PATH = "/run/cuttlefish/operator";
pub const MAX_DEVICE_PACKET_BYTES: usize = 256 * 1024;

pub const SocketMode = enum {
    seqpacket,
    stream,
};

pub const UnixDeviceServerConfig = struct {
    mode: SocketMode = if (builtin.os.tag == .macos) .stream else .seqpacket,
};

pub fn unlinkSocketPath(path: []const u8) void {
    if (path.len == 0 or path.len >= os_socket.PATH_MAX) return;
    var path_z: [os_socket.PATH_MAX]u8 = undefined;
    @memcpy(path_z[0..path.len], path);
    path_z[path.len] = 0;
    os_socket.unlink(@ptrCast(&path_z));
}

pub const UnixDeviceServer = struct {
    allocator: Allocator,
    registry: *Registry,
    socket_path: []const u8,
    stun_server: []const u8,
    config: UnixDeviceServerConfig = .{},
    requested_mode: SocketMode = .seqpacket,
    active_mode: SocketMode = if (builtin.os.tag == .macos) .stream else .seqpacket,
    listen_fd: i32 = -1,
    running: std.atomic.Value(bool) = .init(false),
    accept_thread: ?std.Thread = null,

    pub fn init(
        allocator: Allocator,
        registry: *Registry,
        socket_path: []const u8,
        stun_server: []const u8,
    ) UnixDeviceServer {
        const default_mode: SocketMode = if (builtin.os.tag == .macos) .stream else .seqpacket;
        return .{
            .allocator = allocator,
            .registry = registry,
            .socket_path = if (socket_path.len > 0) socket_path else DEFAULT_SOCKET_PATH,
            .stun_server = stun_server,
            .config = .{ .mode = default_mode },
            .requested_mode = .seqpacket,
            .active_mode = default_mode,
        };
    }

    pub fn initWithMode(
        allocator: Allocator,
        registry: *Registry,
        socket_path: []const u8,
        stun_server: []const u8,
        mode: SocketMode,
    ) UnixDeviceServer {
        const effective_mode: SocketMode = if (builtin.os.tag == .macos and mode == .seqpacket) .stream else mode;
        return .{
            .allocator = allocator,
            .registry = registry,
            .socket_path = if (socket_path.len > 0) socket_path else DEFAULT_SOCKET_PATH,
            .stun_server = stun_server,
            .config = .{ .mode = effective_mode },
            .requested_mode = mode,
            .active_mode = effective_mode,
        };
    }

    pub fn bindAndListen(self: *UnixDeviceServer) !void {
        if (self.socket_path.len == 0 or self.socket_path.len >= os_socket.MAX_UNIX_SOCKET_PATH) {
            return error.InvalidSocketPath;
        }

        unlinkSocketPath(self.socket_path);

        var fd: i32 = -1;
        if (builtin.os.tag != .macos and self.requested_mode == .seqpacket) {
            if (os_socket.socketUnixSeqPacket()) |sfd| {
                fd = sfd;
                self.active_mode = .seqpacket;
                self.config.mode = .seqpacket;
            } else |_| {}
        }

        if (fd < 0) {
            fd = try os_socket.socketUnixStream();
            self.active_mode = .stream;
            self.config.mode = .stream;
        }
        errdefer os_socket.close(fd);

        const addr = os_socket.initSockAddrUn(self.socket_path) catch return error.InvalidSocketPath;
        try os_socket.bindUn(fd, &addr);

        os_socket.listen(fd, 16) catch {
            unlinkSocketPath(self.socket_path);
            return error.ListenFailed;
        };

        self.listen_fd = fd;
    }

    pub fn start(self: *UnixDeviceServer) !void {
        if (self.listen_fd < 0) {
            try self.bindAndListen();
        }
        self.running.store(true, .release);
        self.accept_thread = try std.Thread.spawn(.{}, acceptLoop, .{self});
    }

    pub fn stop(self: *UnixDeviceServer) void {
        self.running.store(false, .release);
        if (self.listen_fd >= 0) {
            os_socket.shutdownRdwr(self.listen_fd);
        }
        if (self.accept_thread) |thread| {
            thread.join();
            self.accept_thread = null;
        }
        if (self.listen_fd >= 0) {
            os_socket.close(self.listen_fd);
            self.listen_fd = -1;
            unlinkSocketPath(self.socket_path);
        }
    }

    pub fn deinit(self: *UnixDeviceServer) void {
        self.stop();
    }

    fn acceptLoop(self: *UnixDeviceServer) void {
        while (self.running.load(.acquire)) {
            const poll_res = os_socket.pollIn(self.listen_fd, 50);
            if (!self.running.load(.acquire)) break;
            if (!poll_res.ready) continue;
            if ((poll_res.revents & (os_socket.POLLERR | os_socket.POLLHUP | os_socket.POLLNVAL)) != 0 and
                (poll_res.revents & os_socket.POLLIN) == 0)
            {
                if (!self.running.load(.acquire)) break;
                continue;
            }

            const client_fd = os_socket.accept(self.listen_fd) catch {
                if (!self.running.load(.acquire)) break;
                continue;
            };

            const conn_thread = std.Thread.spawn(.{}, handleClientThread, .{ self, client_fd }) catch {
                os_socket.close(client_fd);
                continue;
            };
            conn_thread.detach();
        }
    }

    fn handleClientThread(self: *UnixDeviceServer, client_fd: i32) void {
        var registered_device_id: ?[]u8 = null;
        var registered_sink: ?DeviceSink = null;
        defer {
            if (registered_device_id) |dev_id| {
                if (registered_sink) |sink| {
                    self.registry.unregisterDeviceSink(dev_id, sink);
                } else {
                    self.registry.unregisterDevice(dev_id);
                }
                self.allocator.free(dev_id);
            }
            os_socket.close(client_fd);
        }

        switch (self.active_mode) {
            .seqpacket => self.runSeqPacketLoop(client_fd, &registered_device_id, &registered_sink),
            .stream => self.runStreamLoop(client_fd, &registered_device_id, &registered_sink),
        }
    }

    fn runSeqPacketLoop(
        self: *UnixDeviceServer,
        client_fd: i32,
        registered_device_id: *?[]u8,
        registered_sink: *?DeviceSink,
    ) void {
        while (self.running.load(.acquire)) {
            const pkt_opt = os_socket.recvSeqPacket(self.allocator, client_fd, MAX_DEVICE_PACKET_BYTES) catch break;
            const pkt = pkt_opt orelse break;
            defer self.allocator.free(pkt);
            if (pkt.len == 0) continue;

            self.processDeviceJsonMessageWithSink(
                .{ .unix_seqpacket_fd = client_fd },
                pkt,
                registered_device_id,
                registered_sink,
            );
        }
    }

    fn runStreamLoop(
        self: *UnixDeviceServer,
        client_fd: i32,
        registered_device_id: *?[]u8,
        registered_sink: *?DeviceSink,
    ) void {
        var accum: std.ArrayList(u8) = .empty;
        defer accum.deinit(self.allocator);

        var temp: [8192]u8 = undefined;
        while (self.running.load(.acquire)) {
            const read_n = os_socket.recv(client_fd, &temp) catch break;
            if (read_n == 0) break;

            accum.appendSlice(self.allocator, temp[0..read_n]) catch break;

            while (accum.items.len > 0) {
                if (accum.items[0] == 0) {
                    // 4-byte big-endian length-prefixed frame (fallback for SOCK_SEQPACKET on macOS).
                    if (accum.items.len < 4) break;
                    const frame_len: usize = std.mem.readInt(u32, accum.items[0..4], .big);
                    if (frame_len == 0 or frame_len > MAX_DEVICE_PACKET_BYTES) return;
                    if (accum.items.len < 4 + frame_len) break;

                    const msg_end = 4 + frame_len;
                    const msg_copy = self.allocator.dupe(u8, accum.items[4..msg_end]) catch return;
                    defer self.allocator.free(msg_copy);

                    const remaining = accum.items.len - msg_end;
                    if (remaining > 0) {
                        std.mem.copyForwards(u8, accum.items[0..remaining], accum.items[msg_end..]);
                    }
                    accum.shrinkRetainingCapacity(remaining);

                    self.processDeviceJsonMessageWithSink(
                        .{ .unix_stream_framed_fd = client_fd },
                        msg_copy,
                        registered_device_id,
                        registered_sink,
                    );
                } else if (extractNextJsonObject(accum.items)) |span| {
                    const msg_end = span.end;
                    const msg_copy = self.allocator.dupe(u8, accum.items[span.start..span.end]) catch return;
                    defer self.allocator.free(msg_copy);

                    const remaining = accum.items.len - msg_end;
                    if (remaining > 0) {
                        std.mem.copyForwards(u8, accum.items[0..remaining], accum.items[msg_end..]);
                    }
                    accum.shrinkRetainingCapacity(remaining);

                    self.processDeviceJsonMessageWithSink(
                        .{ .unix_stream_fd = client_fd },
                        msg_copy,
                        registered_device_id,
                        registered_sink,
                    );
                } else {
                    break;
                }
            }
        }
    }

    fn processDeviceJsonMessageWithSink(
        self: *UnixDeviceServer,
        sink: DeviceSink,
        raw_json: []const u8,
        registered_device_id: *?[]u8,
        registered_sink: *?DeviceSink,
    ) void {
        var parsed = protocol.parseDeviceMessage(self.allocator, raw_json) catch {
            const err_msg = protocol.formatError(self.allocator, "Invalid device signaling message") catch return;
            defer self.allocator.free(err_msg);
            sink.send(err_msg) catch {};
            return;
        };
        defer parsed.deinit(self.allocator);

        switch (parsed) {
            .register => |reg| {
                if (registered_device_id.*) |old_id| {
                    if (registered_sink.*) |old_sink| {
                        self.registry.unregisterDeviceSink(old_id, old_sink);
                    } else {
                        self.registry.unregisterDevice(old_id);
                    }
                    self.allocator.free(old_id);
                    registered_device_id.* = null;
                    registered_sink.* = null;
                }

                self.registry.registerOrReplaceDevice(reg.device_id, reg.device_port, reg.device_info_json, sink) catch {
                    const err_msg = protocol.formatError(self.allocator, "Device already registered") catch return;
                    defer self.allocator.free(err_msg);
                    sink.send(err_msg) catch {};
                    return;
                };

                registered_device_id.* = self.allocator.dupe(u8, reg.device_id) catch null;
                registered_sink.* = sink;

                const cfg = protocol.formatConfig(self.allocator, self.stun_server) catch return;
                defer self.allocator.free(cfg);
                sink.send(cfg) catch {};
            },
            .forward => |fwd| {
                const dev_id = registered_device_id.* orelse "";
                self.registry.forwardDeviceToClient(dev_id, fwd.client_id, fwd.payload_json) catch {
                    const err_msg = protocol.formatError(self.allocator, "Failed to forward message to client") catch return;
                    defer self.allocator.free(err_msg);
                    sink.send(err_msg) catch {};
                };
            },
        }
    }
};

pub fn connectSeqpacketClient(server: *const UnixDeviceServer) !i32 {
    const fd = switch (server.config.mode) {
        .seqpacket => try os_socket.socketUnixSeqPacket(),
        .stream => try os_socket.socketUnixStream(),
    };
    errdefer os_socket.close(fd);

    const addr = try os_socket.initSockAddrUn(server.socket_path);
    try os_socket.connectUn(fd, &addr);
    return fd;
}

pub fn sendSeqpacketClientMsg(fd: i32, mode: SocketMode, payload: []const u8) !void {
    switch (mode) {
        .seqpacket => try registry_mod.sendPacketRaw(fd, payload),
        .stream => try registry_mod.sendStreamFramedRaw(fd, payload),
    }
}

pub fn recvSeqpacketClientMsg(allocator: Allocator, fd: i32, mode: SocketMode) ![]u8 {
    switch (mode) {
        .seqpacket => {
            var buf: [16384]u8 = undefined;
            const n = try os_socket.recv(fd, &buf);
            if (n == 0) return error.UnexpectedEof;
            return try allocator.dupe(u8, buf[0..n]);
        },
        .stream => {
            var hdr: [4]u8 = undefined;
            try readExactFd(fd, &hdr);
            if (hdr[0] == 0) {
                const frame_len: usize = std.mem.readInt(u32, &hdr, .big);
                if (frame_len == 0 or frame_len > MAX_DEVICE_PACKET_BYTES) return error.InvalidFrameLength;
                const payload = try allocator.alloc(u8, frame_len);
                errdefer allocator.free(payload);
                try readExactFd(fd, payload);
                return payload;
            } else {
                var accum: std.ArrayList(u8) = .empty;
                errdefer accum.deinit(allocator);
                try accum.appendSlice(allocator, &hdr);
                var one: [1]u8 = undefined;
                while (extractNextJsonObject(accum.items) == null) {
                    const n = try os_socket.recv(fd, &one);
                    if (n == 0) return error.UnexpectedEof;
                    try accum.append(allocator, one[0]);
                }
                return try accum.toOwnedSlice(allocator);
            }
        },
    }
}

fn readExactFd(fd: i32, out: []u8) !void {
    var offset: usize = 0;
    while (offset < out.len) {
        const n = try os_socket.recv(fd, out[offset..]);
        if (n == 0) return error.UnexpectedEof;
        offset += n;
    }
}

const JsonSpan = struct {
    start: usize,
    end: usize,
};

fn extractNextJsonObject(buf: []const u8) ?JsonSpan {
    var start: usize = 0;
    while (start < buf.len and buf[start] != '{') : (start += 1) {}
    if (start >= buf.len) return null;

    var depth: usize = 0;
    var in_string = false;
    var escaped = false;

    var i: usize = start;
    while (i < buf.len) : (i += 1) {
        const c = buf[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
            } else if (c == '{') {
                depth += 1;
            } else if (c == '}') {
                depth -= 1;
                if (depth == 0) {
                    return .{ .start = start, .end = i + 1 };
                }
            }
        }
    }
    return null;
}

test "unix_device_server: seqpacket/stream fallback and device registration roundtrip" {
    const allocator = std.testing.allocator;
    const sock_path = "cf_test_unix_dev.sock";
    defer unlinkSocketPath(sock_path);

    var reg = Registry.init(allocator);
    defer reg.deinit();

    var srv = UnixDeviceServer.init(allocator, &reg, sock_path, "stun.l.google.com:19302");
    defer srv.deinit();
    try srv.start();

    if (builtin.os.tag == .macos) {
        try std.testing.expectEqual(SocketMode.stream, srv.config.mode);
        try std.testing.expectEqual(SocketMode.stream, srv.active_mode);
    }

    const dev_fd = try connectSeqpacketClient(&srv);
    defer os_socket.close(dev_fd);

    const reg_json =
        \\{"message_type":"register","device_id":"cvd-u1","device_port":8443,"device_info":{"displays":[{"width":720,"height":1280}]}}
    ;
    try sendSeqpacketClientMsg(dev_fd, srv.config.mode, reg_json);

    const cfg_reply = try recvSeqpacketClientMsg(allocator, dev_fd, srv.config.mode);
    defer allocator.free(cfg_reply);
    try std.testing.expect(std.mem.indexOf(u8, cfg_reply, "\"message_type\":\"config\"") != null);
    try std.testing.expect(std.mem.indexOf(u8, cfg_reply, "stun:stun.l.google.com:19302") != null);
}

