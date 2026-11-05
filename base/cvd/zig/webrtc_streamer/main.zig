const std = @import("std");
const builtin = @import("builtin");

pub const cli = @import("cli.zig");
pub const operator_client = @import("operator_client.zig");
pub const frame_source = @import("frame_source.zig");
pub const input_sink = @import("input_sink.zig");

const is_linux = builtin.os.tag == .linux;
const linux = if (is_linux) std.os.linux else struct {};
const c = if (!is_linux) std.c else struct {};
const Allocator = std.mem.Allocator;

pub fn runWithArgs(allocator: Allocator, args: []const []const u8) !void {
    const cfg = try cli.parseArgs(args);
    if (cfg.help) return;

    var client = operator_client.OperatorClient.init(allocator, cfg);
    defer client.deinit();

    try client.connectWithRetry(50, 20);
    try client.sendRegistration();

    var frames = frame_source.FrameSource.init(
        cfg.display0.width,
        cfg.display0.height,
        cfg.hwcomposer_socket_path,
        cfg.vsock_socket_path,
    );
    const frame_bytes = frame_source.i420ByteSize(cfg.display0.width, cfg.display0.height);
    const i420_buf = try allocator.alloc(u8, frame_bytes);
    defer allocator.free(i420_buf);

    var loop_idx: usize = 0;
    while (cfg.max_loops == 0 or loop_idx < cfg.max_loops) : (loop_idx += 1) {
        frames.nextFrameI420(i420_buf);
        _ = client.pollUdpStunOnce() catch false;

        const maybe_msg = client.pollNextMessageAlloc(15) catch |err| switch (err) {
            error.ReadFailed, error.EndOfStream => break,
            else => return err,
        };
        if (maybe_msg) |msg| {
            defer allocator.free(msg);
            try client.handleOperatorMessage(msg);
            _ = client.pollUdpStunOnce() catch false;
        }
    }
}

pub const main = if (builtin.output_mode == .Lib)
    struct {
        fn cMainStub() callconv(.c) c_int {
            return 0;
        }
    }.cMainStub
else
    mainExe;

fn mainExe(init: std.process.Init) !void {
    const gpa = init.gpa;
    const arena = init.arena.allocator();
    const raw_args = try init.minimal.args.toSlice(arena);
    const args = if (raw_args.len > 1) raw_args[1..] else raw_args[0..0];
    try runWithArgs(gpa, args);
}

/// C ABI entrypoint so `cuttlefish/host/frontend/webrtc/main.cpp` can delegate directly
/// to the pure-Zig streamer when `--device_id` is passed.
pub export fn cf_zig_streamer_run(argc: c_int, argv: [*][*:0]const u8) callconv(.c) c_int {
    if (argc < 0) return -1;
    var debug_gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = debug_gpa.deinit();
    const allocator = debug_gpa.allocator();

    const count: usize = @intCast(argc);
    const slice = allocator.alloc([]const u8, count) catch return -1;
    defer allocator.free(slice);

    for (0..count) |idx| {
        slice[idx] = std.mem.span(argv[idx]);
    }

    const user_args = if (slice.len > 1) slice[1..] else slice[0..0];
    runWithArgs(allocator, user_args) catch return 1;
    return 0;
}

comptime {
    _ = cli;
    _ = operator_client;
    _ = frame_source;
    _ = input_sink;
    _ = &cf_zig_streamer_run;
}

// ============================================================================
// Mock UNIX Operator Socket Helpers for End-to-End Loopback Test
// ============================================================================

fn unlinkPath(path: []const u8) void {
    if (path.len == 0 or path.len >= 104) return;
    var path_z: [128]u8 = @splat(0);
    @memcpy(path_z[0..path.len], path);
    if (is_linux) {
        _ = linux.unlink(@ptrCast(&path_z));
    } else {
        _ = c.unlink(@ptrCast(&path_z));
    }
}

fn createUnixStreamListener(path: []const u8) !i32 {
    unlinkPath(path);
    if (is_linux) {
        const rc = linux.socket(linux.AF.UNIX, linux.SOCK.STREAM | linux.SOCK.CLOEXEC, 0);
        if (linux.errno(rc) != .SUCCESS) return error.SocketFailed;
        const fd: i32 = @intCast(rc);
        errdefer _ = linux.close(fd);

        var addr: linux.sockaddr.un = .{
            .family = linux.AF.UNIX,
            .path = @splat(0),
        };
        @memcpy(addr.path[0..path.len], path);
        if (linux.errno(linux.bind(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.un))) != .SUCCESS) {
            return error.BindFailed;
        }
        if (linux.errno(linux.listen(fd, 4)) != .SUCCESS) {
            return error.ListenFailed;
        }
        return fd;
    } else {
        const fd = c.socket(c.AF.UNIX, c.SOCK.STREAM, 0);
        if (fd < 0) return error.SocketFailed;
        errdefer _ = c.close(fd);

        var addr: c.sockaddr.un = std.mem.zeroes(c.sockaddr.un);
        addr.family = @intCast(c.AF.UNIX);
        addr.len = @sizeOf(c.sockaddr.un);
        const path_bytes: []u8 = @ptrCast(&addr.path);
        @memcpy(path_bytes[0..path.len], path);
        if (c.bind(fd, @ptrCast(&addr), @sizeOf(c.sockaddr.un)) != 0) {
            return error.BindFailed;
        }
        if (c.listen(fd, 4) != 0) {
            return error.ListenFailed;
        }
        return fd;
    }
}

fn acceptUnixClient(listen_fd: i32) !i32 {
    if (is_linux) {
        const rc = linux.accept4(listen_fd, null, null, linux.SOCK.CLOEXEC);
        if (linux.errno(rc) != .SUCCESS) return error.AcceptFailed;
        return @intCast(rc);
    } else {
        const fd = c.accept(listen_fd, null, null);
        if (fd < 0) return error.AcceptFailed;
        return fd;
    }
}

fn writeLenPrefixed(fd: i32, payload: []const u8) !void {
    var hdr: [4]u8 = undefined;
    std.mem.writeInt(u32, &hdr, @intCast(payload.len), .big);
    if (is_linux) {
        _ = linux.write(fd, &hdr, 4);
        _ = linux.write(fd, payload.ptr, payload.len);
    } else {
        _ = c.write(fd, &hdr, 4);
        _ = c.write(fd, payload.ptr, payload.len);
    }
}

fn readLenPrefixedAlloc(allocator: Allocator, fd: i32) ![]u8 {
    var accum: std.ArrayList(u8) = .empty;
    defer accum.deinit(allocator);

    while (true) {
        if (try operator_client.extractStreamMessage(allocator, &accum)) |msg| {
            return msg;
        }
        var buf: [2048]u8 = undefined;
        const n: usize = if (is_linux) blk: {
            const rc = linux.read(fd, &buf, buf.len);
            if (linux.errno(rc) != .SUCCESS or rc == 0) return error.EndOfStream;
            break :blk rc;
        } else blk: {
            const rc = c.read(fd, &buf, buf.len);
            if (rc <= 0) return error.EndOfStream;
            break :blk @intCast(rc);
        };
        try accum.appendSlice(allocator, buf[0..n]);
    }
}

test "end-to-end streamer registration, offer/answer/ICE signaling, and UDP STUN binding check" {
    const allocator = std.testing.allocator;
    const sock_path = ".cf_zig_streamer_e2e.sock";
    defer unlinkPath(sock_path);

    const listen_fd = try createUnixStreamListener(sock_path);
    defer operator_client.closeFd(listen_fd);

    const MockOperatorCtx = struct {
        listen_fd: i32,
        reg_msg: ?[]u8 = null,
        ans_msg: ?[]u8 = null,
        cand_msg: ?[]u8 = null,
        alloc: Allocator,

        fn run(ctx: *@This()) void {
            const conn_fd = acceptUnixClient(ctx.listen_fd) catch return;
            defer operator_client.closeFd(conn_fd);

            ctx.reg_msg = readLenPrefixedAlloc(ctx.alloc, conn_fd) catch return;

            writeLenPrefixed(conn_fd, "{\"type\":\"registered\",\"device_id\":\"cf0\"}") catch return;
            writeLenPrefixed(conn_fd, "{\"type\":\"connect\",\"client_id\":1}") catch return;

            const offer_json =
                "{\"type\":\"forward\",\"client_id\":1,\"payload\":{\"type\":\"offer\",\"sdp\":" ++
                "\"v=0\\r\\no=- 1 2 IN IP4 127.0.0.1\\r\\ns=-\\r\\nt=0 0\\r\\n" ++
                "a=ice-ufrag:remufrag\\r\\na=ice-pwd:rempwd123456789012345678\\r\\n" ++
                "a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF:11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF\\r\\n" ++
                "m=video 9 UDP/TLS/RTP/SAVPF 96\\r\\na=mid:0\\r\\n" ++
                "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\\r\\na=mid:1\\r\\n\"}}";
            writeLenPrefixed(conn_fd, offer_json) catch return;

            ctx.ans_msg = readLenPrefixedAlloc(ctx.alloc, conn_fd) catch return;
            ctx.cand_msg = readLenPrefixedAlloc(ctx.alloc, conn_fd) catch return;

            writeLenPrefixed(
                conn_fd,
                "{\"type\":\"forward\",\"client_id\":1,\"payload\":{\"type\":\"ice-candidate\",\"candidate\":\"candidate:1 1 udp 2130706431 127.0.0.1 50000 typ host\"}}",
            ) catch return;
        }
    };

    var mock_ctx = MockOperatorCtx{
        .listen_fd = listen_fd,
        .alloc = allocator,
    };
    defer {
        if (mock_ctx.reg_msg) |m| allocator.free(m);
        if (mock_ctx.ans_msg) |m| allocator.free(m);
        if (mock_ctx.cand_msg) |m| allocator.free(m);
    }

    const srv_thread = try std.Thread.spawn(.{}, MockOperatorCtx.run, .{&mock_ctx});

    const args = [_][]const u8{
        "--device_id=cf0",
        "--operator_socket_path=.cf_zig_streamer_e2e.sock",
        "--ice_host_address=127.0.0.1",
        "--ice_host_ipv6=fd00::1",
        "--udp_port_range=15650:15699",
        "--display0=width=720,height=1280,dpi=320,fps=60",
        "--max_loops=4",
    };

    const cfg = try cli.parseArgs(&args);
    var client = operator_client.OperatorClient.initWithFraming(allocator, cfg, .stream);
    defer client.deinit();

    try client.connectWithRetry(25, 10);
    try client.sendRegistration();

    var loops: usize = 0;
    while (loops < 4) : (loops += 1) {
        if ( try client.readNextMessageAlloc()) |msg| {
            defer allocator.free(msg);
            try client.handleOperatorMessage(msg);
        }
    }

    srv_thread.join();

    try std.testing.expect(client.registered);
    try std.testing.expect(mock_ctx.reg_msg != null);
    try std.testing.expect(std.mem.indexOf(u8, mock_ctx.reg_msg.?, "\"type\":\"register\"") != null);
    try std.testing.expect(std.mem.indexOf(u8, mock_ctx.reg_msg.?, "\"device_id\":\"cf0\"") != null);

    try std.testing.expect(mock_ctx.ans_msg != null);
    try std.testing.expect(std.mem.indexOf(u8, mock_ctx.ans_msg.?, "\"type\":\"answer\"") != null);
    try std.testing.expect(std.mem.indexOf(u8, mock_ctx.ans_msg.?, "m=video ") != null);
    try std.testing.expect(std.mem.indexOf(u8, mock_ctx.ans_msg.?, "webrtc-datachannel") != null);

    try std.testing.expect(mock_ctx.cand_msg != null);
    try std.testing.expect(std.mem.indexOf(u8, mock_ctx.cand_msg.?, "\"type\":\"ice-candidate\"") != null);

    const sess = client.findSession(1) orelse return error.TestUnexpectedResult;
    try std.testing.expect(sess.offer_received);
    try std.testing.expect(sess.answer_sent);
    try std.testing.expectEqualStrings("remufrag", sess.remote_sdp.ufrag());
    try std.testing.expectEqualStrings("127.0.0.1", sess.remoteCandidateIp());
    try std.testing.expectEqual(@as(u16, 50000), sess.remote_candidate_port);

    // Send a live UDP STUN Binding Request to the streamer's bound UDP port and verify response
    const peer_udp = try operator_client.bindUdpInRange(15700, 15750);
    defer operator_client.closeFd(peer_udp.fd);

    const tx_id = [12]u8{ 0xaa, 0xbb, 0xcc, 0xdd, 1, 2, 3, 4, 5, 6, 7, 8 };
    var stun_req_buf: [20]u8 = undefined;
    const stun_req = operator_client.buildStunBindingRequest(tx_id, &stun_req_buf);

    if (is_linux) {
        var dst: linux.sockaddr.in = .{
            .family = linux.AF.INET,
            .port = std.mem.nativeToBig(u16, client.bound_udp_port),
            .addr = std.mem.nativeToBig(u32, 0x7f000001),
        };
        _ = linux.sendto(peer_udp.fd, stun_req.ptr, stun_req.len, 0, @ptrCast(&dst), @sizeOf(linux.sockaddr.in));
    } else {
        var dst: c.sockaddr.in = std.mem.zeroes(c.sockaddr.in);
        dst.family = @intCast(c.AF.INET);
        dst.len = @sizeOf(c.sockaddr.in);
        dst.port = std.mem.nativeToBig(u16, client.bound_udp_port);
        dst.addr = std.mem.nativeToBig(u32, 0x7f000001);
        _ = c.sendto(peer_udp.fd, stun_req.ptr, stun_req.len, 0, @ptrCast(&dst), @sizeOf(c.sockaddr.in));
    }

    var got_stun = false;
    for (0..20) |_| {
        if (try client.pollUdpStunOnce()) {
            got_stun = true;
            break;
        }
        operator_client.sleepMs(5);
    }
    try std.testing.expect(got_stun);
    try std.testing.expectEqual(@as(usize, 1), client.stun_responses_sent);
}
