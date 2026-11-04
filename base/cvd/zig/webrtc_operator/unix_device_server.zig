const std = @import("std");
const linux = std.os.linux;
const Allocator = std.mem.Allocator;
const protocol = @import("protocol.zig");
const registry_mod = @import("registry.zig");
const Registry = registry_mod.Registry;
const DeviceSink = registry_mod.DeviceSink;

pub const DEFAULT_SOCKET_PATH = "/run/cuttlefish/operator";

pub const SocketMode = enum {
    seqpacket,
    stream,
};

pub fn unlinkSocketPath(path: []const u8) void {
    if (path.len == 0 or path.len >= linux.PATH_MAX) return;
    var path_z: [linux.PATH_MAX]u8 = undefined;
    @memcpy(path_z[0..path.len], path);
    path_z[path.len] = 0;
    _ = linux.unlink(@ptrCast(&path_z));
}

pub const UnixDeviceServer = struct {
    allocator: Allocator,
    registry: *Registry,
    socket_path: []const u8,
    stun_server: []const u8,
    requested_mode: SocketMode = .seqpacket,
    active_mode: SocketMode = .seqpacket,
    listen_fd: i32 = -1,
    running: std.atomic.Value(bool) = .init(false),
    accept_thread: ?std.Thread = null,

    pub fn init(
        allocator: Allocator,
        registry: *Registry,
        socket_path: []const u8,
        stun_server: []const u8,
    ) UnixDeviceServer {
        return .{
            .allocator = allocator,
            .registry = registry,
            .socket_path = if (socket_path.len > 0) socket_path else DEFAULT_SOCKET_PATH,
            .stun_server = stun_server,
        };
    }

    pub fn initWithMode(
        allocator: Allocator,
        registry: *Registry,
        socket_path: []const u8,
        stun_server: []const u8,
        mode: SocketMode,
    ) UnixDeviceServer {
        return .{
            .allocator = allocator,
            .registry = registry,
            .socket_path = if (socket_path.len > 0) socket_path else DEFAULT_SOCKET_PATH,
            .stun_server = stun_server,
            .requested_mode = mode,
            .active_mode = mode,
        };
    }

    pub fn bindAndListen(self: *UnixDeviceServer) !void {
        if (self.socket_path.len == 0 or self.socket_path.len >= 108) {
            return error.InvalidSocketPath;
        }

        unlinkSocketPath(self.socket_path);

        var fd: i32 = -1;
        if (self.requested_mode == .seqpacket) {
            const rc = linux.socket(linux.AF.UNIX, linux.SOCK.SEQPACKET | linux.SOCK.CLOEXEC, 0);
            if (linux.errno(rc) == .SUCCESS) {
                fd = @intCast(rc);
                self.active_mode = .seqpacket;
            }
        }

        if (fd < 0) {
            const rc = linux.socket(linux.AF.UNIX, linux.SOCK.STREAM | linux.SOCK.CLOEXEC, 0);
            if (linux.errno(rc) != .SUCCESS) {
                return error.SocketCreationFailed;
            }
            fd = @intCast(rc);
            self.active_mode = .stream;
        }
        errdefer _ = linux.close(fd);

        var addr: linux.sockaddr.un = .{
            .family = linux.AF.UNIX,
            .path = @splat(0),
        };
        @memcpy(addr.path[0..self.socket_path.len], self.socket_path);

        const bind_rc = linux.bind(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.un));
        if (linux.errno(bind_rc) != .SUCCESS) {
            return error.BindFailed;
        }

        const listen_rc = linux.listen(fd, 16);
        if (linux.errno(listen_rc) != .SUCCESS) {
            unlinkSocketPath(self.socket_path);
            return error.ListenFailed;
        }

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
            _ = linux.shutdown(self.listen_fd, linux.SHUT.RDWR);
        }
        if (self.accept_thread) |thread| {
            thread.join();
            self.accept_thread = null;
        }
        if (self.listen_fd >= 0) {
            _ = linux.close(self.listen_fd);
            self.listen_fd = -1;
            unlinkSocketPath(self.socket_path);
        }
    }

    pub fn deinit(self: *UnixDeviceServer) void {
        self.stop();
    }

    fn acceptLoop(self: *UnixDeviceServer) void {
        while (self.running.load(.acquire)) {
            var pfd = [1]linux.pollfd{
                .{
                    .fd = self.listen_fd,
                    .events = linux.POLL.IN,
                    .revents = 0,
                },
            };
            const poll_rc = linux.poll(&pfd, 1, 50);
            if (!self.running.load(.acquire)) break;
            if (linux.errno(poll_rc) != .SUCCESS or poll_rc == 0) continue;
            if ((pfd[0].revents & (linux.POLL.ERR | linux.POLL.HUP | linux.POLL.NVAL)) != 0 and
                (pfd[0].revents & linux.POLL.IN) == 0)
            {
                if (!self.running.load(.acquire)) break;
                continue;
            }

            const client_rc = linux.accept4(self.listen_fd, null, null, linux.SOCK.CLOEXEC);
            if (linux.errno(client_rc) != .SUCCESS) {
                if (!self.running.load(.acquire)) break;
                continue;
            }
            const client_fd: i32 = @intCast(client_rc);

            const conn_thread = std.Thread.spawn(.{}, handleClientThread, .{ self, client_fd }) catch {
                _ = linux.close(client_fd);
                continue;
            };
            conn_thread.detach();
        }
    }

    fn handleClientThread(self: *UnixDeviceServer, client_fd: i32) void {
        var registered_device_id: ?[]u8 = null;
        defer {
            if (registered_device_id) |dev_id| {
                self.registry.unregisterDevice(dev_id);
                self.allocator.free(dev_id);
            }
            _ = linux.close(client_fd);
        }

        switch (self.active_mode) {
            .seqpacket => self.runSeqPacketLoop(client_fd, &registered_device_id),
            .stream => self.runStreamLoop(client_fd, &registered_device_id),
        }
    }

    fn runSeqPacketLoop(self: *UnixDeviceServer, client_fd: i32, registered_device_id: *?[]u8) void {
        while (self.running.load(.acquire)) {
            var peek_byte: [1]u8 = undefined;
            const size_rc = linux.recvfrom(
                client_fd,
                &peek_byte,
                0,
                linux.MSG.PEEK | linux.MSG.TRUNC,
                null,
                null,
            );
            switch (linux.errno(size_rc)) {
                .SUCCESS => {},
                .INTR => continue,
                else => break,
            }
            if (size_rc == 0) break; // EOF

            const pkt_len: usize = size_rc;
            const buf = self.allocator.alloc(u8, pkt_len) catch break;
            defer self.allocator.free(buf);

            const read_rc = linux.recvfrom(client_fd, buf.ptr, buf.len, 0, null, null);
            switch (linux.errno(read_rc)) {
                .SUCCESS => {},
                .INTR => continue,
                else => break,
            }
            if (read_rc == 0) break;

            const actual_len = @min(buf.len, read_rc);
            self.processDeviceJsonMessage(client_fd, buf[0..actual_len], registered_device_id);
        }
    }

    fn runStreamLoop(self: *UnixDeviceServer, client_fd: i32, registered_device_id: *?[]u8) void {
        var accum: std.ArrayList(u8) = .empty;
        defer accum.deinit(self.allocator);

        var temp: [8192]u8 = undefined;
        while (self.running.load(.acquire)) {
            const read_rc = linux.recvfrom(client_fd, &temp, temp.len, 0, null, null);
            switch (linux.errno(read_rc)) {
                .SUCCESS => {},
                .INTR => continue,
                else => break,
            }
            if (read_rc == 0) break;

            accum.appendSlice(self.allocator, temp[0..read_rc]) catch break;

            while (extractNextJsonObject(accum.items)) |span| {
                const msg_end = span.end;
                const msg_copy = self.allocator.dupe(u8, accum.items[span.start..span.end]) catch break;
                defer self.allocator.free(msg_copy);

                const remaining = accum.items.len - msg_end;
                if (remaining > 0) {
                    std.mem.copyForwards(u8, accum.items[0..remaining], accum.items[msg_end..]);
                }
                accum.shrinkRetainingCapacity(remaining);

                self.processDeviceJsonMessage(client_fd, msg_copy, registered_device_id);
            }
        }
    }

    fn processDeviceJsonMessage(
        self: *UnixDeviceServer,
        client_fd: i32,
        raw_json: []const u8,
        registered_device_id: *?[]u8,
    ) void {
        const sink: DeviceSink = switch (self.active_mode) {
            .seqpacket => .{ .unix_seqpacket_fd = client_fd },
            .stream => .{ .unix_stream_fd = client_fd },
        };

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
                    self.registry.unregisterDevice(old_id);
                    self.allocator.free(old_id);
                    registered_device_id.* = null;
                }

                self.registry.registerDevice(reg.device_id, reg.device_port, reg.device_info_json, sink) catch {
                    const err_msg = protocol.formatError(self.allocator, "Device already registered") catch return;
                    defer self.allocator.free(err_msg);
                    sink.send(err_msg) catch {};
                    return;
                };

                registered_device_id.* = self.allocator.dupe(u8, reg.device_id) catch null;

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
