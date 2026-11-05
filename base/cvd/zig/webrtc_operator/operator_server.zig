const std = @import("std");
const Allocator = std.mem.Allocator;
const protocol = @import("protocol.zig");
const os_socket = protocol.os_socket;
const registry_mod = @import("registry.zig");
const Registry = registry_mod.Registry;
const DeviceSink = registry_mod.DeviceSink;

pub const DEFAULT_HTTP_PORT: u16 = 8443;
pub const DEFAULT_ASSETS_DIR = "webrtc";
pub const DEFAULT_STUN_SERVER = "stun.l.google.com:19302";

pub const WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

fn containsIgnoreCase(haystack: []const u8, needle: []const u8) bool {
    if (needle.len == 0) return true;
    if (haystack.len < needle.len) return false;
    var i: usize = 0;
    while (i + needle.len <= haystack.len) : (i += 1) {
        if (std.ascii.eqlIgnoreCase(haystack[i .. i + needle.len], needle)) return true;
    }
    return false;
}

pub fn computeWebSocketAcceptKey(sec_websocket_key: []const u8) [28]u8 {
    var sha1 = std.crypto.hash.Sha1.init(.{});
    sha1.update(std.mem.trim(u8, sec_websocket_key, " \t\r\n"));
    sha1.update(WEBSOCKET_GUID);
    var digest: [std.crypto.hash.Sha1.digest_length]u8 = undefined;
    sha1.final(&digest);
    var encoded: [28]u8 = undefined;
    _ = std.base64.standard.Encoder.encode(&encoded, &digest);
    return encoded;
}

pub fn buildMaskedWebSocketTextFrame(allocator: Allocator, payload: []const u8, mask_key: [4]u8) ![]u8 {
    var frame: std.ArrayList(u8) = .empty;
    errdefer frame.deinit(allocator);

    try frame.append(allocator, 0x81); // FIN + text frame
    if (payload.len < 126) {
        try frame.append(allocator, 0x80 | @as(u8, @intCast(payload.len)));
    } else if (payload.len <= std.math.maxInt(u16)) {
        try frame.append(allocator, 0x80 | 126);
        var ext: [2]u8 = undefined;
        std.mem.writeInt(u16, &ext, @intCast(payload.len), .big);
        try frame.appendSlice(allocator, &ext);
    } else {
        try frame.append(allocator, 0x80 | 127);
        var ext: [8]u8 = undefined;
        std.mem.writeInt(u64, &ext, @intCast(payload.len), .big);
        try frame.appendSlice(allocator, &ext);
    }

    try frame.appendSlice(allocator, &mask_key);
    for (payload, 0..) |b, idx| {
        try frame.append(allocator, b ^ mask_key[idx & 3]);
    }
    return frame.toOwnedSlice(allocator);
}

pub const BufferedConnReader = struct {
    fd: i32,
    buf: [8192]u8 = undefined,
    pos: usize = 0,
    end: usize = 0,

    pub fn initWithRemainder(fd: i32, remainder: []const u8) BufferedConnReader {
        var r = BufferedConnReader{ .fd = fd };
        const copy_len = @min(remainder.len, r.buf.len);
        @memcpy(r.buf[0..copy_len], remainder[0..copy_len]);
        r.end = copy_len;
        return r;
    }

    pub fn readExact(self: *BufferedConnReader, dest: []u8) !void {
        var filled: usize = 0;
        while (filled < dest.len) {
            if (self.pos < self.end) {
                const avail = self.end - self.pos;
                const take = @min(avail, dest.len - filled);
                @memcpy(dest[filled .. filled + take], self.buf[self.pos .. self.pos + take]);
                self.pos += take;
                filled += take;
                continue;
            }
            self.pos = 0;
            self.end = 0;
            const n = try os_socket.recv(self.fd, &self.buf);
            if (n == 0) return error.EndOfStream;
            self.end = n;
        }
    }
};

pub const WebSocketFrame = struct {
    fin: bool,
    opcode: u8,
    payload: []u8,

    pub fn deinit(self: *const WebSocketFrame, allocator: Allocator) void {
        allocator.free(self.payload);
    }
};

pub fn readWebSocketFrame(allocator: Allocator, reader: *BufferedConnReader) !WebSocketFrame {
    var hdr2: [2]u8 = undefined;
    try reader.readExact(&hdr2);

    const fin = (hdr2[0] & 0x80) != 0;
    const opcode = hdr2[0] & 0x0f;
    const masked = (hdr2[1] & 0x80) != 0;
    const len7 = hdr2[1] & 0x7f;

    var payload_len: usize = len7;
    if (len7 == 126) {
        var ext2: [2]u8 = undefined;
        try reader.readExact(&ext2);
        payload_len = std.mem.readInt(u16, &ext2, .big);
    } else if (len7 == 127) {
        var ext8: [8]u8 = undefined;
        try reader.readExact(&ext8);
        const len64 = std.mem.readInt(u64, &ext8, .big);
        if (len64 > 16 * 1024 * 1024) return error.FrameTooLarge;
        payload_len = @intCast(len64);
    }

    var mask_key: [4]u8 = .{ 0, 0, 0, 0 };
    if (masked) {
        try reader.readExact(&mask_key);
    }

    const payload = try allocator.alloc(u8, payload_len);
    errdefer allocator.free(payload);

    if (payload_len > 0) {
        try reader.readExact(payload);
        if (masked) {
            for (payload, 0..) |*b, idx| {
                b.* ^= mask_key[idx & 3];
            }
        }
    }

    return .{
        .fin = fin,
        .opcode = opcode,
        .payload = payload,
    };
}

pub const OperatorServerConfig = struct {
    host_ipv4_be: u32 = 0, // 0.0.0.0 in network byte order
    port: u16 = DEFAULT_HTTP_PORT,
    assets_dir: []const u8 = DEFAULT_ASSETS_DIR,
    certs_dir: []const u8 = "webrtc/certs",
    use_secure_http: bool = false,
    stun_server: []const u8 = DEFAULT_STUN_SERVER,
};

pub const OperatorServer = struct {
    allocator: Allocator,
    registry: *Registry,
    config: OperatorServerConfig,
    listen_fd: i32 = -1,
    bound_port: u16 = 0,
    running: std.atomic.Value(bool) = .init(false),
    accept_thread: ?std.Thread = null,

    pub fn init(
        allocator: Allocator,
        registry: *Registry,
        config: OperatorServerConfig,
    ) OperatorServer {
        return .{
            .allocator = allocator,
            .registry = registry,
            .config = config,
            .bound_port = config.port,
        };
    }

    pub fn bindAndListen(self: *OperatorServer) !void {
        const fd = try os_socket.socketTcp();
        errdefer os_socket.close(fd);

        os_socket.setReuseAddr(fd);

        const addr = os_socket.initSockAddrIn(self.config.host_ipv4_be, self.config.port);
        try os_socket.bindIn(fd, &addr);
        try os_socket.listen(fd, 64);

        self.bound_port = os_socket.getsocknamePort(fd, self.config.port);
        self.listen_fd = fd;
    }

    pub fn start(self: *OperatorServer) !void {
        if (self.listen_fd < 0) {
            try self.bindAndListen();
        }
        self.running.store(true, .release);
        self.accept_thread = try std.Thread.spawn(.{}, acceptLoop, .{self});
    }

    pub fn stop(self: *OperatorServer) void {
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
        }
    }

    pub fn deinit(self: *OperatorServer) void {
        self.stop();
    }

    fn acceptLoop(self: *OperatorServer) void {
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

            const conn_thread = std.Thread.spawn(.{}, handleConnectionThread, .{ self, client_fd }) catch {
                os_socket.close(client_fd);
                continue;
            };
            conn_thread.detach();
        }
    }

    fn handleConnectionThread(self: *OperatorServer, client_fd: i32) void {
        defer os_socket.close(client_fd);
        self.handleConnection(client_fd) catch {};
    }

    fn handleConnection(self: *OperatorServer, client_fd: i32) !void {
        var req_buf: std.ArrayList(u8) = .empty;
        defer req_buf.deinit(self.allocator);

        var temp: [4096]u8 = undefined;
        var header_end_idx: ?usize = null;
        while (header_end_idx == null) {
            if (req_buf.items.len > 64 * 1024) return error.HeadersTooLarge;
            const n = try os_socket.recv(client_fd, &temp);
            if (n == 0) return;
            try req_buf.appendSlice(self.allocator, temp[0..n]);
            if (std.mem.indexOf(u8, req_buf.items, "\r\n\r\n")) |idx| {
                header_end_idx = idx;
            }
        }

        const hdr_end = header_end_idx.?;
        const header_block = req_buf.items[0..hdr_end];
        const body_start = hdr_end + 4;

        var line_it = std.mem.splitSequence(u8, header_block, "\r\n");
        const request_line = line_it.next() orelse return error.InvalidHttpRequest;

        var req_parts = std.mem.tokenizeScalar(u8, request_line, ' ');
        const method = req_parts.next() orelse return error.InvalidHttpRequest;
        const raw_target = req_parts.next() orelse return error.InvalidHttpRequest;
        const path = if (std.mem.indexOfScalar(u8, raw_target, '?')) |q_idx|
            raw_target[0..q_idx]
        else
            raw_target;

        var content_length: usize = 0;
        var is_ws_upgrade = false;
        var ws_key: ?[]const u8 = null;
        var ws_protocol: ?[]const u8 = null;

        while (line_it.next()) |line| {
            const colon = std.mem.indexOfScalar(u8, line, ':') orelse continue;
            const name = std.mem.trim(u8, line[0..colon], " \t");
            const val = std.mem.trim(u8, line[colon + 1 ..], " \t");

            if (std.ascii.eqlIgnoreCase(name, "Content-Length")) {
                content_length = std.fmt.parseInt(usize, val, 10) catch 0;
            } else if (std.ascii.eqlIgnoreCase(name, "Upgrade")) {
                if (containsIgnoreCase(val, "websocket")) {
                    is_ws_upgrade = true;
                }
            } else if (std.ascii.eqlIgnoreCase(name, "Sec-WebSocket-Key")) {
                ws_key = val;
            } else if (std.ascii.eqlIgnoreCase(name, "Sec-WebSocket-Protocol")) {
                ws_protocol = val;
            }
        }

        if (is_ws_upgrade and ws_key != null) {
            return self.handleWebSocketUpgrade(
                client_fd,
                path,
                ws_key.?,
                ws_protocol,
                req_buf.items[body_start..],
            );
        }

        if (content_length > 4 * 1024 * 1024) return error.PayloadTooLarge;
        while (req_buf.items.len - body_start < content_length) {
            const n = try os_socket.recv(client_fd, &temp);
            if (n == 0) break;
            try req_buf.appendSlice(self.allocator, temp[0..n]);
        }

        const avail_body = req_buf.items[body_start..];
        const body = avail_body[0..@min(avail_body.len, content_length)];

        try self.handleHttpRequest(client_fd, method, path, body);
    }

    fn handleWebSocketUpgrade(
        self: *OperatorServer,
        client_fd: i32,
        path: []const u8,
        sec_ws_key: []const u8,
        sec_ws_protocol: ?[]const u8,
        remainder: []const u8,
    ) !void {
        const is_client_ws = std.mem.eql(u8, path, "/connect_client");
        const is_device_ws = std.mem.eql(u8, path, "/register_device");
        if (!is_client_ws and !is_device_ws) {
            try sendHttpResponse(client_fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found", true);
            return;
        }

        const accept_key = computeWebSocketAcceptKey(sec_ws_key);
        const include_subproto = if (sec_ws_protocol) |proto|
            std.mem.indexOf(u8, proto, "webrtc-operator") != null
        else
            false;

        var resp_buf: [512]u8 = undefined;
        const resp = if (include_subproto)
            try std.fmt.bufPrint(
                &resp_buf,
                "HTTP/1.1 101 Switching Protocols\r\n" ++
                    "Upgrade: websocket\r\n" ++
                    "Connection: Upgrade\r\n" ++
                    "Sec-WebSocket-Accept: {s}\r\n" ++
                    "Sec-WebSocket-Protocol: webrtc-operator\r\n" ++
                    "\r\n",
                .{accept_key},
            )
        else
            try std.fmt.bufPrint(
                &resp_buf,
                "HTTP/1.1 101 Switching Protocols\r\n" ++
                    "Upgrade: websocket\r\n" ++
                    "Connection: Upgrade\r\n" ++
                    "Sec-WebSocket-Accept: {s}\r\n" ++
                    "\r\n",
                .{accept_key},
            );

        try registry_mod.sendAllRaw(client_fd, resp);

        var reader = BufferedConnReader.initWithRemainder(client_fd, remainder);
        if (is_client_ws) {
            self.runClientWebSocketLoop(client_fd, &reader);
        } else {
            self.runDeviceWebSocketLoop(client_fd, &reader);
        }
    }

    fn runClientWebSocketLoop(self: *OperatorServer, client_fd: i32, reader: *BufferedConnReader) void {
        var connected_client_id: ?u32 = null;
        defer {
            if (connected_client_id) |cid| {
                self.registry.disconnectClient(cid);
            }
        }

        var fragmented: std.ArrayList(u8) = .empty;
        defer fragmented.deinit(self.allocator);

        while (self.running.load(.acquire)) {
            const frame = readWebSocketFrame(self.allocator, reader) catch break;
            defer frame.deinit(self.allocator);

            switch (frame.opcode) {
                0x8 => { // Close
                    const close_frame = [_]u8{ 0x88, 0x00 };
                    registry_mod.sendAllRaw(client_fd, &close_frame) catch {};
                    break;
                },
                0x9 => { // Ping -> Pong
                    if (frame.payload.len < 126) {
                        var pong_hdr = [2]u8{ 0x8A, @intCast(frame.payload.len) };
                        registry_mod.sendAllRaw(client_fd, &pong_hdr) catch break;
                        if (frame.payload.len > 0) {
                            registry_mod.sendAllRaw(client_fd, frame.payload) catch break;
                        }
                    }
                    continue;
                },
                0xA => continue, // Pong
                0x1, 0x2, 0x0 => {
                    if (!frame.fin) {
                        fragmented.appendSlice(self.allocator, frame.payload) catch break;
                        continue;
                    }
                    const full_msg = if (fragmented.items.len > 0) blk: {
                        fragmented.appendSlice(self.allocator, frame.payload) catch break;
                        break :blk fragmented.items;
                    } else frame.payload;
                    defer fragmented.clearRetainingCapacity();

                    self.handleClientWsMessage(client_fd, full_msg, &connected_client_id);
                },
                else => continue,
            }
        }
    }

    fn handleClientWsMessage(
        self: *OperatorServer,
        client_fd: i32,
        raw_json: []const u8,
        connected_client_id: *?u32,
    ) void {
        var parsed = protocol.parseClientMessage(self.allocator, raw_json) catch {
            const err_msg = protocol.formatError(self.allocator, "Invalid client signaling message") catch return;
            defer self.allocator.free(err_msg);
            registry_mod.sendWebSocketTextFrame(client_fd, err_msg) catch {};
            return;
        };
        defer parsed.deinit(self.allocator);

        switch (parsed) {
            .connect => |conn| {
                if (connected_client_id.*) |old_cid| {
                    self.registry.disconnectClient(old_cid);
                    connected_client_id.* = null;
                }

                // Send server config first (or along with device_info) matching ClientWSHandler
                const cfg = protocol.formatConfig(self.allocator, self.config.stun_server) catch return;
                defer self.allocator.free(cfg);
                registry_mod.sendWebSocketTextFrame(client_fd, cfg) catch return;

                const res = self.registry.connectWebSocketClient(self.allocator, conn.device_id, client_fd) catch {
                    const err_msg = protocol.formatError(self.allocator, "Device not found") catch return;
                    defer self.allocator.free(err_msg);
                    registry_mod.sendWebSocketTextFrame(client_fd, err_msg) catch {};
                    return;
                };
                defer self.allocator.free(res.device_info_json);

                connected_client_id.* = res.client_id;

                const dev_info_msg = protocol.formatDeviceInfo(self.allocator, res.device_info_json) catch return;
                defer self.allocator.free(dev_info_msg);
                registry_mod.sendWebSocketTextFrame(client_fd, dev_info_msg) catch {};
            },
            .forward => |fwd| {
                const cid = connected_client_id.* orelse {
                    const err_msg = protocol.formatError(self.allocator, "Client not connected to a device") catch return;
                    defer self.allocator.free(err_msg);
                    registry_mod.sendWebSocketTextFrame(client_fd, err_msg) catch {};
                    return;
                };
                self.registry.forwardClientToDevice(cid, fwd.payload_json) catch {
                    const err_msg = protocol.formatError(self.allocator, "Failed to forward message to device") catch return;
                    defer self.allocator.free(err_msg);
                    registry_mod.sendWebSocketTextFrame(client_fd, err_msg) catch {};
                };
            },
            .client_poll => {},
        }
    }

    fn runDeviceWebSocketLoop(self: *OperatorServer, client_fd: i32, reader: *BufferedConnReader) void {
        var registered_device_id: ?[]u8 = null;
        defer {
            if (registered_device_id) |dev_id| {
                self.registry.unregisterDevice(dev_id);
                self.allocator.free(dev_id);
            }
        }

        var fragmented: std.ArrayList(u8) = .empty;
        defer fragmented.deinit(self.allocator);

        while (self.running.load(.acquire)) {
            const frame = readWebSocketFrame(self.allocator, reader) catch break;
            defer frame.deinit(self.allocator);

            switch (frame.opcode) {
                0x8 => {
                    const close_frame = [_]u8{ 0x88, 0x00 };
                    registry_mod.sendAllRaw(client_fd, &close_frame) catch {};
                    break;
                },
                0x9 => {
                    if (frame.payload.len < 126) {
                        var pong_hdr = [2]u8{ 0x8A, @intCast(frame.payload.len) };
                        registry_mod.sendAllRaw(client_fd, &pong_hdr) catch break;
                        if (frame.payload.len > 0) {
                            registry_mod.sendAllRaw(client_fd, frame.payload) catch break;
                        }
                    }
                    continue;
                },
                0xA => continue,
                0x1, 0x2, 0x0 => {
                    if (!frame.fin) {
                        fragmented.appendSlice(self.allocator, frame.payload) catch break;
                        continue;
                    }
                    const full_msg = if (fragmented.items.len > 0) blk: {
                        fragmented.appendSlice(self.allocator, frame.payload) catch break;
                        break :blk fragmented.items;
                    } else frame.payload;
                    defer fragmented.clearRetainingCapacity();

                    self.handleDeviceWsMessage(client_fd, full_msg, &registered_device_id);
                },
                else => continue,
            }
        }
    }

    fn handleDeviceWsMessage(
        self: *OperatorServer,
        client_fd: i32,
        raw_json: []const u8,
        registered_device_id: *?[]u8,
    ) void {
        const sink: DeviceSink = .{ .websocket_fd = client_fd };

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

                const cfg = protocol.formatConfig(self.allocator, self.config.stun_server) catch return;
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

    fn handleHttpRequest(
        self: *OperatorServer,
        client_fd: i32,
        method: []const u8,
        path: []const u8,
        body: []const u8,
    ) !void {
        if (std.mem.eql(u8, method, "OPTIONS")) {
            try sendHttpResponse(client_fd, "204 No Content", "text/plain; charset=utf-8", "", true);
            return;
        }

        if (std.mem.eql(u8, path, "/devices") or std.mem.eql(u8, path, "/list_devices")) {
            if (!std.mem.eql(u8, method, "GET")) {
                try sendHttpResponse(client_fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found", true);
                return;
            }
            const json = try self.registry.listDevicesJson(self.allocator);
            defer self.allocator.free(json);
            try sendHttpResponse(client_fd, "200 OK", "application/json", json, true);
            return;
        }

        if (std.mem.eql(u8, path, "/infra_config")) {
            if (!std.mem.eql(u8, method, "GET")) {
                try sendHttpResponse(client_fd, "405 Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed", true);
                return;
            }
            const json = try protocol.formatConfig(self.allocator, self.config.stun_server);
            defer self.allocator.free(json);
            try sendHttpResponse(client_fd, "200 OK", "application/json", json, true);
            return;
        }

        if (std.mem.eql(u8, path, "/connect")) {
            if (!std.mem.eql(u8, method, "POST")) {
                try sendHttpResponse(client_fd, "405 Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed", true);
                return;
            }
            const device_id = protocol.parseConnectHttpBody(self.allocator, body) catch {
                const err_json = try protocol.formatError(self.allocator, "Invalid connect request");
                defer self.allocator.free(err_json);
                try sendHttpResponse(client_fd, "400 Bad Request", "application/json", err_json, true);
                return;
            };
            defer self.allocator.free(device_id);

            const res = self.registry.connectPollingClient(self.allocator, device_id) catch {
                const err_json = try protocol.formatError(self.allocator, "Device not found");
                defer self.allocator.free(err_json);
                try sendHttpResponse(client_fd, "404 Not Found", "application/json", err_json, true);
                return;
            };
            defer self.allocator.free(res.device_info_json);

            const reply_json = try protocol.formatPollingConnectReply(
                self.allocator,
                &res.connection_id,
                res.device_info_json,
            );
            defer self.allocator.free(reply_json);
            try sendHttpResponse(client_fd, "200 OK", "application/json", reply_json, true);
            return;
        }

        if (std.mem.eql(u8, path, "/forward")) {
            if (!std.mem.eql(u8, method, "POST")) {
                try sendHttpResponse(client_fd, "405 Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed", true);
                return;
            }
            var fwd = protocol.parseForwardHttpBody(self.allocator, body) catch {
                const err_json = try protocol.formatError(self.allocator, "Invalid forward request");
                defer self.allocator.free(err_json);
                try sendHttpResponse(client_fd, "400 Bad Request", "application/json", err_json, true);
                return;
            };
            defer fwd.deinit(self.allocator);

            const messages_json = self.registry.forwardPollingClientAndDrain(
                self.allocator,
                fwd.connection_id,
                fwd.payload_json,
            ) catch {
                const err_json = try protocol.formatError(self.allocator, "Unknown connection_id or device disconnected");
                defer self.allocator.free(err_json);
                try sendHttpResponse(client_fd, "404 Not Found", "application/json", err_json, true);
                return;
            };
            defer self.allocator.free(messages_json);
            try sendHttpResponse(client_fd, "200 OK", "application/json", messages_json, true);
            return;
        }

        if (std.mem.eql(u8, path, "/poll_messages")) {
            if (!std.mem.eql(u8, method, "POST")) {
                try sendHttpResponse(client_fd, "405 Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed", true);
                return;
            }
            const conn_id = protocol.parsePollMessagesHttpBody(self.allocator, body) catch {
                const err_json = try protocol.formatError(self.allocator, "Invalid poll_messages request");
                defer self.allocator.free(err_json);
                try sendHttpResponse(client_fd, "400 Bad Request", "application/json", err_json, true);
                return;
            };
            defer self.allocator.free(conn_id);

            const messages_json = self.registry.drainPollMessages(self.allocator, conn_id) catch {
                const err_json = try protocol.formatError(self.allocator, "Unknown connection_id");
                defer self.allocator.free(err_json);
                try sendHttpResponse(client_fd, "404 Not Found", "application/json", err_json, true);
                return;
            };
            defer self.allocator.free(messages_json);
            try sendHttpResponse(client_fd, "200 OK", "application/json", messages_json, true);
            return;
        }

        if (!std.mem.eql(u8, method, "GET") and !std.mem.eql(u8, method, "HEAD")) {
            try sendHttpResponse(client_fd, "405 Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed", true);
            return;
        }

        try self.serveStaticAsset(client_fd, path);
    }

    fn serveStaticAsset(self: *OperatorServer, client_fd: i32, raw_path: []const u8) !void {
        const rel_path = if (std.mem.eql(u8, raw_path, "/"))
            "index.html"
        else
            std.mem.trimStart(u8, raw_path, "/");

        if (rel_path.len == 0 or
            std.mem.indexOf(u8, rel_path, "..") != null or
            std.mem.indexOfScalar(u8, rel_path, '\\') != null or
            std.mem.indexOfScalar(u8, rel_path, 0) != null)
        {
            try sendHttpResponse(client_fd, "403 Forbidden", "text/plain; charset=utf-8", "Forbidden", false);
            return;
        }

        var full_path_buf: [os_socket.PATH_MAX]u8 = undefined;
        const full_path = std.fmt.bufPrint(&full_path_buf, "{s}/{s}", .{ self.config.assets_dir, rel_path }) catch {
            try sendHttpResponse(client_fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found", false);
            return;
        };
        if (full_path.len + 1 >= full_path_buf.len) {
            try sendHttpResponse(client_fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found", false);
            return;
        }
        full_path_buf[full_path.len] = 0;

        const file_bytes = os_socket.readFileAlloc(self.allocator, @ptrCast(&full_path_buf), 16 * 1024 * 1024) catch {
            try sendHttpResponse(client_fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found", false);
            return;
        };
        defer self.allocator.free(file_bytes);

        const content_type = if (file_bytes.len >= 2 and file_bytes[0] == 0xFF and file_bytes[1] == 0xD8)
            "image/jpeg"
        else
            mimeTypeForPath(rel_path);
        try sendHttpResponse(client_fd, "200 OK", content_type, file_bytes, false);
    }
};

pub fn mimeTypeForPath(path: []const u8) []const u8 {
    if (std.mem.endsWith(u8, path, ".html")) return "text/html; charset=utf-8";
    if (std.mem.endsWith(u8, path, ".css")) return "text/css; charset=utf-8";
    if (std.mem.endsWith(u8, path, ".js")) return "text/javascript; charset=utf-8";
    if (std.mem.endsWith(u8, path, ".json")) return "application/json";
    if (std.mem.endsWith(u8, path, ".png")) return "image/png";
    if (std.mem.endsWith(u8, path, ".jpg") or std.mem.endsWith(u8, path, ".jpeg")) return "image/jpeg";
    if (std.mem.endsWith(u8, path, ".svg")) return "image/svg+xml";
    if (std.mem.endsWith(u8, path, ".ico")) return "image/x-icon";
    return "application/octet-stream";
}

fn sendHttpResponse(
    client_fd: i32,
    status: []const u8,
    content_type: []const u8,
    body: []const u8,
    include_cors: bool,
) !void {
    var hdr_buf: [1024]u8 = undefined;
    const hdr = if (include_cors)
        try std.fmt.bufPrint(
            &hdr_buf,
            "HTTP/1.1 {s}\r\n" ++
                "Content-Type: {s}\r\n" ++
                "Content-Length: {d}\r\n" ++
                "Cache-Control: no-store, no-cache, must-revalidate\r\n" ++
                "Access-Control-Allow-Origin: *\r\n" ++
                "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n" ++
                "Access-Control-Allow-Headers: Content-Type, Access-Control-Allow-Headers, Authorization, X-Requested-With, Accept\r\n" ++
                "Connection: close\r\n" ++
                "\r\n",
            .{ status, content_type, body.len },
        )
    else
        try std.fmt.bufPrint(
            &hdr_buf,
            "HTTP/1.1 {s}\r\n" ++
                "Content-Type: {s}\r\n" ++
                "Content-Length: {d}\r\n" ++
                "Cache-Control: no-store, no-cache, must-revalidate\r\n" ++
                "Connection: close\r\n" ++
                "\r\n",
            .{ status, content_type, body.len },
        );

    try registry_mod.sendAllRaw(client_fd, hdr);
    if (body.len > 0) {
        try registry_mod.sendAllRaw(client_fd, body);
    }
}
