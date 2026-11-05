const std = @import("std");

pub const DisplaySpec = struct {
    width: u32 = 720,
    height: u32 = 1280,
    dpi: u32 = 320,
    fps: u32 = 60,
};

pub const PortRange = struct {
    min_port: u16 = 15550,
    max_port: u16 = 15599,

    pub fn min(self: PortRange) u16 {
        return self.min_port;
    }

    pub fn max(self: PortRange) u16 {
        return self.max_port;
    }
};

pub const CliConfig = struct {
    device_id: []const u8 = "cf0",
    operator_socket_path: []const u8 = "/tmp/operator.sock",
    vsock_socket_path: []const u8 = "",
    guest_cid: u32 = 3,
    frames_vsock_port: u32 = 6900,
    guest_ip: []const u8 = "127.0.0.1",
    ice_host_address: []const u8 = "127.0.0.1",
    ice_host_ipv6: ?[]const u8 = null,
    udp_port_range: PortRange = .{},
    min_port: u16 = 15550,
    max_port: u16 = 15599,
    display0: DisplaySpec = .{},
    hwcomposer_socket_path: ?[]const u8 = null,
    uinput_socket_path: ?[]const u8 = null,
    operator_http_port: u16 = 8443,
    max_loops: usize = 0,
    help: bool = false,
};

pub const Config = CliConfig;
pub const StreamerConfig = CliConfig;

pub const CliError = error{
    InvalidFlagValue,
    InvalidPortRange,
    InvalidDisplaySpec,
    MissingFlagValue,
};

pub fn parsePortRange(val: []const u8) CliError!PortRange {
    const sep_idx = std.mem.indexOfScalar(u8, val, ':') orelse
        std.mem.indexOfScalar(u8, val, '-') orelse
        return error.InvalidPortRange;
    const min_str = std.mem.trim(u8, val[0..sep_idx], " \t");
    const max_str = std.mem.trim(u8, val[sep_idx + 1 ..], " \t");
    if (min_str.len == 0 or max_str.len == 0) return error.InvalidPortRange;

    const min_p = std.fmt.parseInt(u16, min_str, 10) catch return error.InvalidPortRange;
    const max_p = std.fmt.parseInt(u16, max_str, 10) catch return error.InvalidPortRange;
    if (min_p == 0 or min_p > max_p) return error.InvalidPortRange;

    return .{
        .min_port = min_p,
        .max_port = max_p,
    };
}

pub fn parseDisplaySpec(val: []const u8) CliError!DisplaySpec {
    if (val.len == 0) return error.InvalidDisplaySpec;
    var spec = DisplaySpec{};
    var saw_any = false;

    var it = std.mem.splitScalar(u8, val, ',');
    while (it.next()) |part_raw| {
        const part = std.mem.trim(u8, part_raw, " \t");
        if (part.len == 0) continue;
        const eq_idx = std.mem.indexOfScalar(u8, part, '=') orelse return error.InvalidDisplaySpec;
        const k = std.mem.trim(u8, part[0..eq_idx], " \t");
        const v = std.mem.trim(u8, part[eq_idx + 1 ..], " \t");
        if (k.len == 0 or v.len == 0) return error.InvalidDisplaySpec;

        const num = std.fmt.parseInt(u32, v, 10) catch return error.InvalidDisplaySpec;
        if (num == 0) return error.InvalidDisplaySpec;

        if (std.mem.eql(u8, k, "width") or std.mem.eql(u8, k, "w")) {
            spec.width = num;
            saw_any = true;
        } else if (std.mem.eql(u8, k, "height") or std.mem.eql(u8, k, "h")) {
            spec.height = num;
            saw_any = true;
        } else if (std.mem.eql(u8, k, "dpi")) {
            spec.dpi = num;
            saw_any = true;
        } else if (std.mem.eql(u8, k, "fps") or std.mem.eql(u8, k, "refresh_rate_hz")) {
            spec.fps = num;
            saw_any = true;
        } else {
            return error.InvalidDisplaySpec;
        }
    }

    if (!saw_any) return error.InvalidDisplaySpec;
    return spec;
}

pub fn parseCliArgs(args: []const []const u8) CliError!CliConfig {
    return parseArgs(args);
}

pub fn parseArgs(args: []const []const u8) CliError!CliConfig {
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
            try applyKeyValue(&cfg, key, val);
        } else {
            if (i + 1 >= args.len) return error.MissingFlagValue;
            i += 1;
            try applyKeyValue(&cfg, stripped, args[i]);
        }
    }
    return cfg;
}

fn applyKeyValue(cfg: *CliConfig, key: []const u8, val: []const u8) CliError!void {
    if (std.mem.eql(u8, key, "device_id") or std.mem.eql(u8, key, "device-id")) {
        if (val.len == 0) return error.InvalidFlagValue;
        cfg.device_id = val;
    } else if (std.mem.eql(u8, key, "operator_socket_path") or std.mem.eql(u8, key, "operator-socket-path")) {
        if (val.len == 0) return error.InvalidFlagValue;
        cfg.operator_socket_path = val;
    } else if (std.mem.eql(u8, key, "vsock_socket_path") or std.mem.eql(u8, key, "vsock-socket-path")) {
        cfg.vsock_socket_path = val;
    } else if (std.mem.eql(u8, key, "guest_cid") or std.mem.eql(u8, key, "guest-cid")) {
        cfg.guest_cid = std.fmt.parseInt(u32, val, 10) catch return error.InvalidFlagValue;
    } else if (std.mem.eql(u8, key, "frames_vsock_port") or std.mem.eql(u8, key, "frames-vsock-port")) {
        cfg.frames_vsock_port = std.fmt.parseInt(u32, val, 10) catch return error.InvalidFlagValue;
    } else if (std.mem.eql(u8, key, "guest_ip") or std.mem.eql(u8, key, "guest-ip")) {
        if (val.len == 0) return error.InvalidFlagValue;
        cfg.guest_ip = val;
    } else if (std.mem.eql(u8, key, "ice_host_address") or std.mem.eql(u8, key, "ice-host-address")) {
        if (val.len == 0) return error.InvalidFlagValue;
        cfg.ice_host_address = val;
    } else if (std.mem.eql(u8, key, "ice_host_ipv6") or std.mem.eql(u8, key, "ice-host-ipv6")) {
        cfg.ice_host_ipv6 = if (val.len > 0) val else null;
    } else if (std.mem.eql(u8, key, "udp_port_range") or std.mem.eql(u8, key, "udp-port-range")) {
        const range = try parsePortRange(val);
        cfg.udp_port_range = range;
        cfg.min_port = range.min_port;
        cfg.max_port = range.max_port;
    } else if (std.mem.eql(u8, key, "display0") or std.mem.eql(u8, key, "display")) {
        cfg.display0 = try parseDisplaySpec(val);
    } else if (std.mem.eql(u8, key, "hwcomposer_socket_path") or std.mem.eql(u8, key, "hwcomposer-socket-path")) {
        cfg.hwcomposer_socket_path = if (val.len > 0) val else null;
    } else if (std.mem.eql(u8, key, "uinput_socket_path") or std.mem.eql(u8, key, "uinput-socket-path")) {
        cfg.uinput_socket_path = if (val.len > 0) val else null;
    } else if (std.mem.eql(u8, key, "operator_http_port") or std.mem.eql(u8, key, "operator-http-port") or std.mem.eql(u8, key, "http_server_port")) {
        cfg.operator_http_port = std.fmt.parseInt(u16, val, 10) catch return error.InvalidFlagValue;
    } else if (std.mem.eql(u8, key, "max_loops") or std.mem.eql(u8, key, "max-loops")) {
        cfg.max_loops = std.fmt.parseInt(usize, val, 10) catch return error.InvalidFlagValue;
    }
}

test "cli default values match standalone cuttlefish contract" {
    const empty: []const []const u8 = &.{};
    const cfg = try parseArgs(empty);
    try std.testing.expectEqualStrings("cf0", cfg.device_id);
    try std.testing.expectEqualStrings("/tmp/operator.sock", cfg.operator_socket_path);
    try std.testing.expectEqualStrings("", cfg.vsock_socket_path);
    try std.testing.expectEqual(@as(u32, 3), cfg.guest_cid);
    try std.testing.expectEqual(@as(u32, 6900), cfg.frames_vsock_port);
    try std.testing.expectEqualStrings("127.0.0.1", cfg.guest_ip);
    try std.testing.expectEqualStrings("127.0.0.1", cfg.ice_host_address);
    try std.testing.expect(cfg.ice_host_ipv6 == null);
    try std.testing.expectEqual(@as(u16, 15550), cfg.udp_port_range.min_port);
    try std.testing.expectEqual(@as(u16, 15599), cfg.udp_port_range.max_port);
    try std.testing.expectEqual(@as(u32, 720), cfg.display0.width);
    try std.testing.expectEqual(@as(u32, 1280), cfg.display0.height);
    try std.testing.expectEqual(@as(u32, 320), cfg.display0.dpi);
    try std.testing.expectEqual(@as(u32, 60), cfg.display0.fps);
    try std.testing.expectEqual(@as(u16, 8443), cfg.operator_http_port);
    try std.testing.expectEqual(@as(usize, 0), cfg.max_loops);
}

test "cli parses full standalone streamer flag set with IPv4 and IPv6" {
    const args = [_][]const u8{
        "/usr/lib/cuttlefish/bin/webrtc",
        "--device_id=cf7",
        "--operator_socket_path=/var/lib/cuttlefish/cf7/operator.sock",
        "--vsock_socket_path=/var/lib/cuttlefish/cf7/vsock.sock",
        "--guest_cid=9",
        "--frames_vsock_port=6901",
        "--guest_ip=192.168.100.2",
        "--ice_host_address=198.51.100.1",
        "--ice_host_ipv6=fd00::1",
        "--udp_port_range=15550:15599",
        "--display0=width=1080,height=2400,dpi=420,fps=120",
        "--hwcomposer_socket_path=/ipc/hwcomposer.sock",
        "--uinput_socket_path=/ipc/uinputd.sock",
        "--operator_http_port=9443",
        "--max_loops=5",
    };

    const cfg = try parseArgs(&args);
    try std.testing.expectEqualStrings("cf7", cfg.device_id);
    try std.testing.expectEqualStrings("/var/lib/cuttlefish/cf7/operator.sock", cfg.operator_socket_path);
    try std.testing.expectEqualStrings("/var/lib/cuttlefish/cf7/vsock.sock", cfg.vsock_socket_path);
    try std.testing.expectEqual(@as(u32, 9), cfg.guest_cid);
    try std.testing.expectEqual(@as(u32, 6901), cfg.frames_vsock_port);
    try std.testing.expectEqualStrings("192.168.100.2", cfg.guest_ip);
    try std.testing.expectEqualStrings("198.51.100.1", cfg.ice_host_address);
    try std.testing.expectEqualStrings("fd00::1", cfg.ice_host_ipv6.?);
    try std.testing.expectEqual(@as(u16, 15550), cfg.udp_port_range.min());
    try std.testing.expectEqual(@as(u16, 15599), cfg.udp_port_range.max());
    try std.testing.expectEqual(@as(u32, 1080), cfg.display0.width);
    try std.testing.expectEqual(@as(u32, 2400), cfg.display0.height);
    try std.testing.expectEqual(@as(u32, 420), cfg.display0.dpi);
    try std.testing.expectEqual(@as(u32, 120), cfg.display0.fps);
    try std.testing.expectEqualStrings("/ipc/hwcomposer.sock", cfg.hwcomposer_socket_path.?);
    try std.testing.expectEqualStrings("/ipc/uinputd.sock", cfg.uinput_socket_path.?);
    try std.testing.expectEqual(@as(u16, 9443), cfg.operator_http_port);
    try std.testing.expectEqual(@as(usize, 5), cfg.max_loops);
}

test "cli supports single-dash and space-separated flags" {
    const args = [_][]const u8{
        "-device_id=cf1",
        "--operator_socket_path",
        ".op.sock",
        "-udp_port_range",
        "16000:16010",
        "--display0",
        "width=800,height=600,dpi=160,fps=30",
    };
    const cfg = try parseArgs(&args);
    try std.testing.expectEqualStrings("cf1", cfg.device_id);
    try std.testing.expectEqualStrings(".op.sock", cfg.operator_socket_path);
    try std.testing.expectEqual(@as(u16, 16000), cfg.min_port);
    try std.testing.expectEqual(@as(u16, 16010), cfg.max_port);
    try std.testing.expectEqual(@as(u32, 800), cfg.display0.width);
    try std.testing.expectEqual(@as(u32, 600), cfg.display0.height);
    try std.testing.expectEqual(@as(u32, 160), cfg.display0.dpi);
    try std.testing.expectEqual(@as(u32, 30), cfg.display0.fps);
}
