const std = @import("std");
const linux = std.os.linux;
const Allocator = std.mem.Allocator;

pub const protocol = @import("protocol.zig");
pub const registry = @import("registry.zig");
pub const unix_device_server = @import("unix_device_server.zig");
pub const operator_server = @import("operator_server.zig");

pub const CliConfig = struct {
    http_server_port: u16 = operator_server.DEFAULT_HTTP_PORT,
    use_secure_http: bool = true,
    assets_dir: []const u8 = operator_server.DEFAULT_ASSETS_DIR,
    certs_dir: []const u8 = "webrtc/certs",
    stun_server: []const u8 = operator_server.DEFAULT_STUN_SERVER,
    operator_socket_path: []const u8 = unix_device_server.DEFAULT_SOCKET_PATH,
    help: bool = false,
};

pub fn parseCliArgs(args: []const []const u8) !CliConfig {
    var cfg = CliConfig{};
    var i: usize = 0;
    while (i < args.len) : (i += 1) {
        const arg = args[i];
        if (!std.mem.startsWith(u8, arg, "-")) continue;

        const stripped = std.mem.trimStart(u8, arg, "-");
        if (stripped.len == 0) continue;

        if (std.mem.eql(u8, stripped, "help") or std.mem.eql(u8, stripped, "h")) {
            cfg.help = true;
            continue;
        }

        if (std.mem.indexOfScalar(u8, stripped, '=')) |eq_idx| {
            const key = stripped[0..eq_idx];
            const val = stripped[eq_idx + 1 ..];
            try applyFlag(&cfg, key, val);
        } else {
            // Check boolean flags like -use_secure_http or -nouse_secure_http
            if (std.mem.eql(u8, stripped, "nouse_secure_http")) {
                cfg.use_secure_http = false;
                continue;
            }
            if (i + 1 < args.len and !std.mem.startsWith(u8, args[i + 1], "-")) {
                i += 1;
                try applyFlag(&cfg, stripped, args[i]);
            } else if (std.mem.eql(u8, stripped, "use_secure_http")) {
                cfg.use_secure_http = true;
            }
        }
    }
    return cfg;
}

fn applyFlag(cfg: *CliConfig, key: []const u8, val: []const u8) !void {
    if (std.mem.eql(u8, key, "http_server_port")) {
        cfg.http_server_port = try std.fmt.parseInt(u16, val, 10);
    } else if (std.mem.eql(u8, key, "use_secure_http")) {
        cfg.use_secure_http = parseBoolFlag(val);
    } else if (std.mem.eql(u8, key, "assets_dir")) {
        cfg.assets_dir = val;
    } else if (std.mem.eql(u8, key, "certs_dir")) {
        cfg.certs_dir = val;
    } else if (std.mem.eql(u8, key, "stun_server")) {
        cfg.stun_server = val;
    } else if (std.mem.eql(u8, key, "operator_socket_path")) {
        cfg.operator_socket_path = val;
    }
}

fn parseBoolFlag(val: []const u8) bool {
    if (std.ascii.eqlIgnoreCase(val, "true") or
        std.mem.eql(u8, val, "1") or
        std.ascii.eqlIgnoreCase(val, "yes") or
        std.ascii.eqlIgnoreCase(val, "y"))
    {
        return true;
    }
    return false;
}

pub fn main(init: std.process.Init) !void {
    const gpa = init.gpa;

    var arg_list: std.ArrayList([]const u8) = .empty;
    defer arg_list.deinit(gpa);

    var arg_it = std.process.Args.Iterator.init(init.minimal.args);
    _ = arg_it.skip(); // skip argv[0]
    while (arg_it.next()) |arg| {
        try arg_list.append(gpa, arg);
    }

    const cli = try parseCliArgs(arg_list.items);
    if (cli.help) {
        const help_msg =
            \\Usage: webrtc_operator [flags]
            \\  -http_server_port=<port>        Port for HTTP/HTTPS server (default: 8443)
            \\  -use_secure_http=<bool>         Whether to use HTTPS (default: true)
            \\  -assets_dir=<dir>               Directory with webp/static assets (default: webrtc)
            \\  -certs_dir=<dir>                Directory with TLS certificates (default: webrtc/certs)
            \\  -stun_server=<host:port>        STUN server address (default: stun.l.google.com:19302)
            \\  -operator_socket_path=<path>    UNIX socket path (default: /run/cuttlefish/operator)
            \\
        ;
        _ = linux.write(linux.STDOUT_FILENO, help_msg.ptr, help_msg.len);
        return;
    }

    var reg = registry.Registry.init(gpa);
    defer reg.deinit();

    var unix_srv = unix_device_server.UnixDeviceServer.init(
        gpa,
        &reg,
        cli.operator_socket_path,
        cli.stun_server,
    );
    defer unix_srv.deinit();

    // Start UNIX device server if parent directory exists/is writable; continue if unavailable
    unix_srv.start() catch |err| {
        std.log.warn("Could not start UNIX device server at {s}: {s}", .{
            cli.operator_socket_path,
            @errorName(err),
        });
    };

    var op_srv = operator_server.OperatorServer.init(gpa, &reg, .{
        .port = cli.http_server_port,
        .assets_dir = cli.assets_dir,
        .certs_dir = cli.certs_dir,
        .use_secure_http = cli.use_secure_http,
        .stun_server = cli.stun_server,
    });
    defer op_srv.deinit();

    try op_srv.start();

    if (op_srv.accept_thread) |thread| {
        thread.join();
        op_srv.accept_thread = null;
    }
}

// ============================================================================
// Tests
// ============================================================================

test "CLI flag parsing supports -flag=val, --flag=val, and -flag val" {
    const args1 = [_][]const u8{
        "-http_server_port=9443",
        "--use_secure_http=false",
        "-assets_dir=/opt/cf/webrtc",
        "--certs_dir=/opt/cf/certs",
        "-stun_server=stun.example.org:3478",
        "--operator_socket_path=.custom_operator.sock",
    };
    const cfg1 = try parseCliArgs(&args1);
    try std.testing.expectEqual(@as(u16, 9443), cfg1.http_server_port);
    try std.testing.expectEqual(false, cfg1.use_secure_http);
    try std.testing.expectEqualStrings("/opt/cf/webrtc", cfg1.assets_dir);
    try std.testing.expectEqualStrings("/opt/cf/certs", cfg1.certs_dir);
    try std.testing.expectEqualStrings("stun.example.org:3478", cfg1.stun_server);
    try std.testing.expectEqualStrings(".custom_operator.sock", cfg1.operator_socket_path);

    const args2 = [_][]const u8{
        "-http_server_port",
        "8080",
        "-use_secure_http",
        "true",
        "-assets_dir",
        "custom_assets",
        "-stun_server",
        "127.0.0.1:19302",
    };
    const cfg2 = try parseCliArgs(&args2);
    try std.testing.expectEqual(@as(u16, 8080), cfg2.http_server_port);
    try std.testing.expectEqual(true, cfg2.use_secure_http);
    try std.testing.expectEqualStrings("custom_assets", cfg2.assets_dir);
    try std.testing.expectEqualStrings("127.0.0.1:19302", cfg2.stun_server);
}

test "protocol.zig JSON formatting and parsing" {
    const allocator = std.testing.allocator;

    const cfg_json = try protocol.formatConfig(allocator, "stun.l.google.com:19302");
    defer allocator.free(cfg_json);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"config\",\"ice_servers\":[{\"urls\":[\"stun:stun.l.google.com:19302\"]}]}",
        cfg_json,
    );

    const dev_info_reply = try protocol.formatDeviceInfo(allocator, "{\"displays\":[]}");
    defer allocator.free(dev_info_reply);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"device_info\",\"device_info\":{\"displays\":[]}}",
        dev_info_reply,
    );

    const poll_reply = try protocol.formatPollingConnectReply(allocator, "secret123", "{\"displays\":[]}");
    defer allocator.free(poll_reply);
    try std.testing.expectEqualStrings(
        "{\"connection_id\":\"secret123\",\"message_type\":\"device_info\",\"device_info\":{\"displays\":[]}}",
        poll_reply,
    );

    const client_msg = try protocol.formatClientMsg(allocator, 7, "{\"sdp\":\"offer\"}");
    defer allocator.free(client_msg);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"client_msg\",\"client_id\":7,\"payload\":{\"sdp\":\"offer\"}}",
        client_msg,
    );

    const device_msg = try protocol.formatDeviceMsg(allocator, "{\"sdp\":\"answer\"}");
    defer allocator.free(device_msg);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"device_msg\",\"payload\":{\"sdp\":\"answer\"}}",
        device_msg,
    );

    const disc_msg = try protocol.formatClientDisconnected(allocator, 7);
    defer allocator.free(disc_msg);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"client_disconnected\",\"client_id\":7}",
        disc_msg,
    );

    const err_msg = try protocol.formatError(allocator, "bad request");
    defer allocator.free(err_msg);
    try std.testing.expectEqualStrings(
        "{\"error\":\"bad request\"}",
        err_msg,
    );

    var parsed_reg = try protocol.parseDeviceMessage(
        allocator,
        "{\"message_type\":\"register\",\"device_id\":\"cvd-1\",\"device_port\":8443,\"device_info\":{\"dpi\":320}}",
    );
    defer parsed_reg.deinit(allocator);
    switch (parsed_reg) {
        .register => |r| {
            try std.testing.expectEqualStrings("cvd-1", r.device_id);
            try std.testing.expectEqual(@as(u16, 8443), r.device_port);
            try std.testing.expectEqualStrings("{\"dpi\":320}", r.device_info_json);
        },
        else => return error.TestUnexpectedResult,
    }

    var parsed_fwd = try protocol.parseDeviceMessage(
        allocator,
        "{\"message_type\":\"forward\",\"client_id\":3,\"payload\":{\"ice\":\"candidate\"}}",
    );
    defer parsed_fwd.deinit(allocator);
    switch (parsed_fwd) {
        .forward => |f| {
            try std.testing.expectEqual(@as(u32, 3), f.client_id);
            try std.testing.expectEqualStrings("{\"ice\":\"candidate\"}", f.payload_json);
        },
        else => return error.TestUnexpectedResult,
    }
}

test "registry.zig device and client lifecycle" {
    const allocator = std.testing.allocator;

    const SinkCapture = struct {
        allocator: Allocator,
        messages: std.ArrayList([]u8) = .empty,

        fn deinit(self: *@This()) void {
            for (self.messages.items) |m| self.allocator.free(m);
            self.messages.deinit(self.allocator);
        }

        fn send(ctx: *anyopaque, data: []const u8) anyerror!void {
            const self: *@This() = @ptrCast(@alignCast(ctx));
            const copy = try self.allocator.dupe(u8, data);
            try self.messages.append(self.allocator, copy);
        }
    };

    var dev_capture = SinkCapture{ .allocator = allocator };
    defer dev_capture.deinit();

    var reg = registry.Registry.init(allocator);
    defer reg.deinit();

    try reg.registerDevice(
        "cvd-1",
        8443,
        "{\"name\":\"cuttlefish\"}",
        .{ .callback = .{ .ctx = &dev_capture, .send_fn = SinkCapture.send } },
    );

    const devices_json = try reg.listDevicesJson(allocator);
    defer allocator.free(devices_json);
    try std.testing.expectEqualStrings("[\"cvd-1\"]", devices_json);

    // Connect polling client
    const poll_res = try reg.connectPollingClient(allocator, "cvd-1");
    defer allocator.free(poll_res.device_info_json);
    try std.testing.expectEqual(@as(u32, 1), poll_res.client_id);
    try std.testing.expectEqualStrings("{\"name\":\"cuttlefish\"}", poll_res.device_info_json);

    // Forward from polling client to device
    const initial_drain = try reg.forwardPollingClientAndDrain(
        allocator,
        &poll_res.connection_id,
        "{\"type\":\"offer\"}",
    );
    defer allocator.free(initial_drain);
    try std.testing.expectEqualStrings("[]", initial_drain);
    try std.testing.expectEqual(@as(usize, 1), dev_capture.messages.items.len);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"client_msg\",\"client_id\":1,\"payload\":{\"type\":\"offer\"}}",
        dev_capture.messages.items[0],
    );

    // Forward from device to polling client and drain
    try reg.forwardDeviceToClient("cvd-1", 1, "{\"type\":\"answer\"}");
    const polled = try reg.drainPollMessages(allocator, &poll_res.connection_id);
    defer allocator.free(polled);
    try std.testing.expectEqualStrings(
        "[{\"message_type\":\"device_msg\",\"payload\":{\"type\":\"answer\"}}]",
        polled,
    );

    // Disconnect client notifies device
    reg.disconnectClient(1);
    try std.testing.expectEqual(@as(usize, 2), dev_capture.messages.items.len);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"client_disconnected\",\"client_id\":1}",
        dev_capture.messages.items[1],
    );
}

fn connectLoopbackTcp(port: u16) !i32 {
    const rc = linux.socket(linux.AF.INET, linux.SOCK.STREAM | linux.SOCK.CLOEXEC, 0);
    if (linux.errno(rc) != .SUCCESS) return error.SocketFailed;
    const fd: i32 = @intCast(rc);
    errdefer _ = linux.close(fd);

    var addr: linux.sockaddr.in = .{
        .family = linux.AF.INET,
        .port = std.mem.nativeToBig(u16, port),
        .addr = std.mem.nativeToBig(u32, 0x7f000001),
    };
    const conn_rc = linux.connect(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.in));
    if (linux.errno(conn_rc) != .SUCCESS) return error.ConnectFailed;
    return fd;
}

fn sendSimpleHttpRequest(allocator: Allocator, port: u16, raw_request: []const u8) ![]u8 {
    const fd = try connectLoopbackTcp(port);
    defer _ = linux.close(fd);

    try registry.sendAllRaw(fd, raw_request);

    var resp: std.ArrayList(u8) = .empty;
    errdefer resp.deinit(allocator);

    var buf: [4096]u8 = undefined;
    while (true) {
        const rc = linux.recvfrom(fd, &buf, buf.len, 0, null, null);
        switch (linux.errno(rc)) {
            .SUCCESS => {
                if (rc == 0) break;
                try resp.appendSlice(allocator, buf[0..rc]);
            },
            .INTR => continue,
            else => return error.ReadFailed,
        }
    }
    return resp.toOwnedSlice(allocator);
}

fn extractHttpBody(resp: []const u8) ![]const u8 {
    const sep = std.mem.indexOf(u8, resp, "\r\n\r\n") orelse return error.InvalidHttpResponse;
    return resp[sep + 4 ..];
}

fn readSeqPacket(allocator: Allocator, fd: i32) ![]u8 {
    var buf: [8192]u8 = undefined;
    while (true) {
        const rc = linux.recvfrom(fd, &buf, buf.len, 0, null, null);
        switch (linux.errno(rc)) {
            .SUCCESS => {
                if (rc == 0) return error.EndOfStream;
                return try allocator.dupe(u8, buf[0..rc]);
            },
            .INTR => continue,
            else => return error.SocketReadFailed,
        }
    }
}

test "live loopback unix_device_server (SOCK_SEQPACKET) and operator_server (HTTP + WebSocket)" {
    const allocator = std.testing.allocator;
    const sock_path = ".test_cf_operator.sock";
    defer unix_device_server.unlinkSocketPath(sock_path);

    var reg = registry.Registry.init(allocator);
    defer reg.deinit();

    var unix_srv = unix_device_server.UnixDeviceServer.init(
        allocator,
        &reg,
        sock_path,
        "stun.l.google.com:19302",
    );
    defer unix_srv.deinit();
    try unix_srv.start();
    try std.testing.expectEqual(unix_device_server.SocketMode.seqpacket, unix_srv.active_mode);

    var op_srv = operator_server.OperatorServer.init(allocator, &reg, .{
        .host_ipv4_be = std.mem.nativeToBig(u32, 0x7f000001),
        .port = 0, // ephemeral port
        .stun_server = "stun.l.google.com:19302",
    });
    defer op_srv.deinit();
    try op_srv.start();
    const port = op_srv.bound_port;
    try std.testing.expect(port > 0);

    // 1. Connect simulated device over UNIX SOCK_SEQPACKET
    const dev_rc = linux.socket(linux.AF.UNIX, linux.SOCK.SEQPACKET | linux.SOCK.CLOEXEC, 0);
    try std.testing.expectEqual(linux.E.SUCCESS, linux.errno(dev_rc));
    const dev_fd: i32 = @intCast(dev_rc);
    defer _ = linux.close(dev_fd);

    var un_addr: linux.sockaddr.un = .{
        .family = linux.AF.UNIX,
        .path = @splat(0),
    };
    @memcpy(un_addr.path[0..sock_path.len], sock_path);
    try std.testing.expectEqual(
        linux.E.SUCCESS,
        linux.errno(linux.connect(dev_fd, @ptrCast(&un_addr), @sizeOf(linux.sockaddr.un))),
    );

    const reg_pkt = "{\"message_type\":\"register\",\"device_id\":\"cvd-1\",\"device_port\":8443,\"device_info\":{\"displays\":[{\"width\":720,\"height\":1280}]}}";
    try registry.sendPacketRaw(dev_fd, reg_pkt);

    const cfg_pkt = try readSeqPacket(allocator, dev_fd);
    defer allocator.free(cfg_pkt);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"config\",\"ice_servers\":[{\"urls\":[\"stun:stun.l.google.com:19302\"]}]}",
        cfg_pkt,
    );

    // 2. Test GET /devices
    const devices_resp = try sendSimpleHttpRequest(
        allocator,
        port,
        "GET /devices HTTP/1.1\r\nHost: localhost\r\n\r\n",
    );
    defer allocator.free(devices_resp);
    try std.testing.expect(std.mem.startsWith(u8, devices_resp, "HTTP/1.1 200 OK\r\n"));
    try std.testing.expect(std.mem.indexOf(u8, devices_resp, "Access-Control-Allow-Origin: *\r\n") != null);
    try std.testing.expectEqualStrings("[\"cvd-1\"]", try extractHttpBody(devices_resp));

    // 3. Test GET /infra_config
    const infra_resp = try sendSimpleHttpRequest(
        allocator,
        port,
        "GET /infra_config HTTP/1.1\r\nHost: localhost\r\n\r\n",
    );
    defer allocator.free(infra_resp);
    try std.testing.expect(std.mem.startsWith(u8, infra_resp, "HTTP/1.1 200 OK\r\n"));
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"config\",\"ice_servers\":[{\"urls\":[\"stun:stun.l.google.com:19302\"]}]}",
        try extractHttpBody(infra_resp),
    );

    // 4. Test HTTP polling flow: POST /connect, POST /forward, POST /poll_messages
    const connect_body = "{\"device_id\":\"cvd-1\"}";
    const connect_req = try std.fmt.allocPrint(
        allocator,
        "POST /connect HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {d}\r\n\r\n{s}",
        .{ connect_body.len, connect_body },
    );
    defer allocator.free(connect_req);

    const connect_resp = try sendSimpleHttpRequest(allocator, port, connect_req);
    defer allocator.free(connect_resp);
    try std.testing.expect(std.mem.startsWith(u8, connect_resp, "HTTP/1.1 200 OK\r\n"));

    const connect_json_slice = try extractHttpBody(connect_resp);
    const connect_parsed = try std.json.parseFromSlice(std.json.Value, allocator, connect_json_slice, .{});
    defer connect_parsed.deinit();
    const conn_id = connect_parsed.value.object.get("connection_id").?.string;
    try std.testing.expectEqual(@as(usize, 64), conn_id.len);

    // Forward from polling client -> device
    const fwd_body = try std.fmt.allocPrint(
        allocator,
        "{{\"connection_id\":\"{s}\",\"payload\":{{\"type\":\"offer\",\"sdp\":\"v=0\"}}}}",
        .{conn_id},
    );
    defer allocator.free(fwd_body);
    const fwd_req = try std.fmt.allocPrint(
        allocator,
        "POST /forward HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {d}\r\n\r\n{s}",
        .{ fwd_body.len, fwd_body },
    );
    defer allocator.free(fwd_req);

    const fwd_resp = try sendSimpleHttpRequest(allocator, port, fwd_req);
    defer allocator.free(fwd_resp);
    try std.testing.expectEqualStrings("[]", try extractHttpBody(fwd_resp));

    // Verify UNIX device received client_msg for client_id 1
    const dev_recv_1 = try readSeqPacket(allocator, dev_fd);
    defer allocator.free(dev_recv_1);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"client_msg\",\"client_id\":1,\"payload\":{\"type\":\"offer\",\"sdp\":\"v=0\"}}",
        dev_recv_1,
    );

    // Send reply from UNIX device -> polling client 1
    try registry.sendPacketRaw(
        dev_fd,
        "{\"message_type\":\"forward\",\"client_id\":1,\"payload\":{\"type\":\"answer\",\"sdp\":\"v=0-ans\"}}",
    );

    // Poll messages
    const poll_body = try std.fmt.allocPrint(allocator, "{{\"connection_id\":\"{s}\"}}", .{conn_id});
    defer allocator.free(poll_body);
    const poll_req = try std.fmt.allocPrint(
        allocator,
        "POST /poll_messages HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {d}\r\n\r\n{s}",
        .{ poll_body.len, poll_body },
    );
    defer allocator.free(poll_req);

    var polled_ok = false;
    var attempt: usize = 0;
    while (attempt < 20) : (attempt += 1) {
        const poll_resp = try sendSimpleHttpRequest(allocator, port, poll_req);
        defer allocator.free(poll_resp);
        const body = try extractHttpBody(poll_resp);
        if (!std.mem.eql(u8, body, "[]")) {
            try std.testing.expectEqualStrings(
                "[{\"message_type\":\"device_msg\",\"payload\":{\"type\":\"answer\",\"sdp\":\"v=0-ans\"}}]",
                body,
            );
            polled_ok = true;
            break;
        }
        std.Thread.yield() catch {};
    }
    try std.testing.expect(polled_ok);

    // 5. Test WebSocket /connect_client upgrade & bidirectional frame relay
    const ws_fd = try connectLoopbackTcp(port);
    defer _ = linux.close(ws_fd);

    const upgrade_req =
        "GET /connect_client HTTP/1.1\r\n" ++
        "Host: localhost\r\n" ++
        "Upgrade: websocket\r\n" ++
        "Connection: Upgrade\r\n" ++
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" ++
        "Sec-WebSocket-Protocol: webrtc-operator\r\n" ++
        "Sec-WebSocket-Version: 13\r\n" ++
        "\r\n";
    try registry.sendAllRaw(ws_fd, upgrade_req);

    var upgrade_buf: [1024]u8 = undefined;
    var upgrade_len: usize = 0;
    var hdr_end: ?usize = null;
    while (hdr_end == null) {
        const rc = linux.recvfrom(ws_fd, upgrade_buf[upgrade_len..].ptr, upgrade_buf.len - upgrade_len, 0, null, null);
        try std.testing.expectEqual(linux.E.SUCCESS, linux.errno(rc));
        try std.testing.expect(rc > 0);
        upgrade_len += rc;
        hdr_end = std.mem.indexOf(u8, upgrade_buf[0..upgrade_len], "\r\n\r\n");
    }
    const upgrade_headers = upgrade_buf[0 .. hdr_end.? + 2];
    try std.testing.expect(std.mem.startsWith(u8, upgrade_headers, "HTTP/1.1 101 Switching Protocols\r\n"));
    try std.testing.expect(std.mem.indexOf(u8, upgrade_headers, "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, upgrade_headers, "Sec-WebSocket-Protocol: webrtc-operator\r\n") != null);

    var ws_reader = operator_server.BufferedConnReader.initWithRemainder(
        ws_fd,
        upgrade_buf[hdr_end.? + 4 .. upgrade_len],
    );

    // Send masked WebSocket connect message
    const ws_connect_frame = try operator_server.buildMaskedWebSocketTextFrame(
        allocator,
        "{\"message_type\":\"connect\",\"device_id\":\"cvd-1\"}",
        .{ 0x12, 0x34, 0x56, 0x78 },
    );
    defer allocator.free(ws_connect_frame);
    try registry.sendAllRaw(ws_fd, ws_connect_frame);

    // Receive config frame and device_info frame
    const frame1 = try operator_server.readWebSocketFrame(allocator, &ws_reader);
    defer frame1.deinit(allocator);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"config\",\"ice_servers\":[{\"urls\":[\"stun:stun.l.google.com:19302\"]}]}",
        frame1.payload,
    );

    const frame2 = try operator_server.readWebSocketFrame(allocator, &ws_reader);
    defer frame2.deinit(allocator);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"device_info\",\"device_info\":{\"displays\":[{\"width\":720,\"height\":1280}]}}",
        frame2.payload,
    );

    // Send masked WebSocket forward frame (client -> device)
    const ws_fwd_frame = try operator_server.buildMaskedWebSocketTextFrame(
        allocator,
        "{\"message_type\":\"forward\",\"payload\":{\"candidate\":\"cand-1\"}}",
        .{ 0xAA, 0xBB, 0xCC, 0xDD },
    );
    defer allocator.free(ws_fwd_frame);
    try registry.sendAllRaw(ws_fd, ws_fwd_frame);

    // Verify UNIX device receives client_msg for client_id 2
    const dev_recv_2 = try readSeqPacket(allocator, dev_fd);
    defer allocator.free(dev_recv_2);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"client_msg\",\"client_id\":2,\"payload\":{\"candidate\":\"cand-1\"}}",
        dev_recv_2,
    );

    // Send device -> WebSocket client 2
    try registry.sendPacketRaw(
        dev_fd,
        "{\"message_type\":\"forward\",\"client_id\":2,\"payload\":{\"candidate\":\"cand-2\"}}",
    );

    const frame3 = try operator_server.readWebSocketFrame(allocator, &ws_reader);
    defer frame3.deinit(allocator);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"device_msg\",\"payload\":{\"candidate\":\"cand-2\"}}",
        frame3.payload,
    );

    // Send WebSocket Close frame and verify device receives client_disconnected
    const close_masked = [_]u8{ 0x88, 0x80, 0x01, 0x02, 0x03, 0x04 };
    try registry.sendAllRaw(ws_fd, &close_masked);

    const dev_disc = try readSeqPacket(allocator, dev_fd);
    defer allocator.free(dev_disc);
    try std.testing.expectEqualStrings(
        "{\"message_type\":\"client_disconnected\",\"client_id\":2}",
        dev_disc,
    );

    // 6. Test CORS OPTIONS preflight, method restrictions, and path traversal protection
    const opt_resp = try sendSimpleHttpRequest(
        allocator,
        port,
        "OPTIONS /devices HTTP/1.1\r\nHost: localhost\r\n\r\n",
    );
    defer allocator.free(opt_resp);
    try std.testing.expect(std.mem.startsWith(u8, opt_resp, "HTTP/1.1 204 No Content\r\n"));

    const post_devices_resp = try sendSimpleHttpRequest(
        allocator,
        port,
        "POST /devices HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
    );
    defer allocator.free(post_devices_resp);
    try std.testing.expect(std.mem.startsWith(u8, post_devices_resp, "HTTP/1.1 404 Not Found\r\n"));

    const post_infra_resp = try sendSimpleHttpRequest(
        allocator,
        port,
        "POST /infra_config HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
    );
    defer allocator.free(post_infra_resp);
    try std.testing.expect(std.mem.startsWith(u8, post_infra_resp, "HTTP/1.1 405 Method Not Allowed\r\n"));

    const get_connect_resp = try sendSimpleHttpRequest(
        allocator,
        port,
        "GET /connect HTTP/1.1\r\nHost: localhost\r\n\r\n",
    );
    defer allocator.free(get_connect_resp);
    try std.testing.expect(std.mem.startsWith(u8, get_connect_resp, "HTTP/1.1 405 Method Not Allowed\r\n"));

    const traversal_resp = try sendSimpleHttpRequest(
        allocator,
        port,
        "GET /../main.zig HTTP/1.1\r\nHost: localhost\r\n\r\n",
    );
    defer allocator.free(traversal_resp);
    try std.testing.expect(std.mem.startsWith(u8, traversal_resp, "HTTP/1.1 403 Forbidden\r\n"));
}
