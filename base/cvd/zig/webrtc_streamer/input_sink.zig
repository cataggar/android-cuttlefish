const std = @import("std");
const builtin = @import("builtin");
const is_linux = builtin.os.tag == .linux;
const linux = if (is_linux) std.os.linux else struct {};
const c = if (!is_linux) std.c else struct {};

pub const MSG_TOUCH_EVENT: u8 = 0x01;
pub const MSG_KEY_EVENT: u8 = 0x02;

pub const TOUCH_ACTION_DOWN: u8 = 0;
pub const TOUCH_ACTION_UP: u8 = 1;
pub const TOUCH_ACTION_MOVE: u8 = 2;

pub const KEY_ACTION_DOWN: u8 = 0;
pub const KEY_ACTION_UP: u8 = 1;

pub const TOUCH_PACKET_SIZE: usize = 9;
pub const KEY_PACKET_SIZE: usize = 7;
pub const DEFAULT_PRESSURE: u16 = 50;

pub const TouchEvent = struct {
    action: u8,
    pointer_id: u8 = 0,
    x: u16,
    y: u16,
    pressure: u16 = DEFAULT_PRESSURE,
};

pub const KeyEvent = struct {
    action: u8,
    keycode: u32,
    meta: u8 = 0,
};

pub const InputEvent = union(enum) {
    touch: TouchEvent,
    key: KeyEvent,
};

pub const InputError = error{
    InvalidJson,
    MissingEventType,
    UnknownEventType,
    MissingCoordinates,
    MissingKeycode,
    BufferTooSmall,
    SocketFailed,
    ConnectFailed,
    WriteFailed,
};

fn parseDownValue(val: std.json.Value) bool {
    return switch (val) {
        .bool => |b| b,
        .integer => |i| i != 0,
        .float => |f| f != 0.0,
        .string => |s| std.mem.eql(u8, s, "1") or
            std.ascii.eqlIgnoreCase(s, "true") or
            std.ascii.eqlIgnoreCase(s, "down") or
            std.ascii.eqlIgnoreCase(s, "keydown"),
        .array => |arr| if (arr.items.len > 0) parseDownValue(arr.items[0]) else false,
        else => false,
    };
}

fn parseI32Clamped(val: std.json.Value) ?i32 {
    return switch (val) {
        .integer => |i| blk: {
            if (i <= std.math.minInt(i32)) break :blk std.math.minInt(i32);
            if (i >= std.math.maxInt(i32)) break :blk std.math.maxInt(i32);
            break :blk @intCast(i);
        },
        .float => |f| blk: {
            if (f <= -2147483648.0) break :blk std.math.minInt(i32);
            if (f >= 2147483647.0) break :blk std.math.maxInt(i32);
            break :blk @intFromFloat(f);
        },
        .array => |arr| if (arr.items.len > 0) parseI32Clamped(arr.items[0]) else null,
        else => null,
    };
}

fn parseU16Clamped(val: std.json.Value) ?u16 {
    return switch (val) {
        .integer => |i| blk: {
            if (i <= 0) break :blk 0;
            if (i >= std.math.maxInt(u16)) break :blk std.math.maxInt(u16);
            break :blk @intCast(i);
        },
        .float => |f| blk: {
            if (f <= 0.0) break :blk 0;
            if (f >= 65535.0) break :blk 65535;
            break :blk @intFromFloat(f);
        },
        .array => |arr| if (arr.items.len > 0) parseU16Clamped(arr.items[0]) else null,
        else => null,
    };
}

fn parseKeycodeString(s: []const u8) ?u32 {
    if (std.fmt.parseInt(u32, s, 10)) |n| return n else |_| {}
    if (std.ascii.eqlIgnoreCase(s, "home")) return 3;
    if (std.ascii.eqlIgnoreCase(s, "back") or std.ascii.eqlIgnoreCase(s, "Escape")) return 4;
    if (std.ascii.eqlIgnoreCase(s, "menu") or std.ascii.eqlIgnoreCase(s, "overview")) return 187;
    if (std.ascii.eqlIgnoreCase(s, "power")) return 26;
    if (std.ascii.eqlIgnoreCase(s, "volumeup") or std.ascii.eqlIgnoreCase(s, "volup")) return 24;
    if (std.ascii.eqlIgnoreCase(s, "volumedown") or std.ascii.eqlIgnoreCase(s, "voldown")) return 25;
    if (std.ascii.eqlIgnoreCase(s, "Enter") or std.ascii.eqlIgnoreCase(s, "NumpadEnter")) return 66;
    if (std.ascii.eqlIgnoreCase(s, "Backspace")) return 67;
    if (std.ascii.eqlIgnoreCase(s, "Tab")) return 61;
    if (std.ascii.eqlIgnoreCase(s, "Space")) return 62;
    if (std.ascii.eqlIgnoreCase(s, "ArrowUp")) return 19;
    if (std.ascii.eqlIgnoreCase(s, "ArrowDown")) return 20;
    if (std.ascii.eqlIgnoreCase(s, "ArrowLeft")) return 21;
    if (std.ascii.eqlIgnoreCase(s, "ArrowRight")) return 22;
    if (s.len == 4 and std.mem.startsWith(u8, s, "Key")) {
        const ch = std.ascii.toUpper(s[3]);
        if (ch >= 'A' and ch <= 'Z') return 29 + @as(u32, ch - 'A');
    }
    if (s.len == 6 and std.mem.startsWith(u8, s, "Digit")) {
        const ch = s[5];
        if (ch >= '0' and ch <= '9') return 7 + @as(u32, ch - '0');
    }
    return null;
}

fn parseU32Clamped(val: std.json.Value) ?u32 {
    return switch (val) {
        .integer => |i| blk: {
            if (i <= 0) break :blk 0;
            if (i >= std.math.maxInt(u32)) break :blk std.math.maxInt(u32);
            break :blk @intCast(i);
        },
        .float => |f| blk: {
            if (f <= 0.0) break :blk 0;
            if (f >= 4294967295.0) break :blk 4294967295;
            break :blk @intFromFloat(f);
        },
        .string => |s| parseKeycodeString(s),
        else => null,
    };
}

/// Parses a Cuttlefish WebRTC DataChannel JSON input event (`touch`, `multi-touch`, `mouse`, `keyboard`/`key`, or `device-control`).
pub fn parseInputJson(allocator: std.mem.Allocator, raw_json: []const u8) InputError!InputEvent {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, raw_json, .{}) catch {
        return error.InvalidJson;
    };
    defer parsed.deinit();

    const root = switch (parsed.value) {
        .object => |obj| obj,
        else => return error.InvalidJson,
    };

    if (root.get("command")) |cmd_val| {
        const keycode = parseU32Clamped(cmd_val) orelse return error.MissingKeycode;
        const is_down = if (root.get("button_state")) |bs|
            parseDownValue(bs)
        else if (root.get("down")) |dv|
            parseDownValue(dv)
        else
            true;
        return .{
            .key = .{
                .action = if (is_down) KEY_ACTION_DOWN else KEY_ACTION_UP,
                .keycode = keycode,
                .meta = 0,
            },
        };
    }

    const type_val = root.get("type") orelse root.get("event_type") orelse root.get("message_type") orelse return error.MissingEventType;
    const ev_type = switch (type_val) {
        .string => |s| s,
        else => return error.MissingEventType,
    };

    if (std.mem.eql(u8, ev_type, "mouseMove")) {
        const dx = if (root.get("x")) |xv| (parseI32Clamped(xv) orelse 0) else 0;
        const dy = if (root.get("y")) |yv| (parseI32Clamped(yv) orelse 0) else 0;
        return .{
            .touch = .{
                .action = TOUCH_ACTION_MOVE,
                .pointer_id = 0,
                .x = @intCast(std.math.clamp(540 + dx, 0, 65535)),
                .y = @intCast(std.math.clamp(1205 + dy, 0, 65535)),
                .pressure = 0,
            },
        };
    }

    if (std.mem.eql(u8, ev_type, "mouseButton") or std.mem.eql(u8, ev_type, "mouseWheel")) {
        const is_down = if (root.get("down")) |dv| parseDownValue(dv) else false;
        return .{
            .touch = .{
                .action = if (is_down) TOUCH_ACTION_DOWN else TOUCH_ACTION_UP,
                .pointer_id = 0,
                .x = 540,
                .y = 1205,
                .pressure = if (is_down) DEFAULT_PRESSURE else 0,
            },
        };
    }

    if (std.mem.eql(u8, ev_type, "touch") or
        std.mem.eql(u8, ev_type, "multi-touch") or
        std.mem.eql(u8, ev_type, "mouse") or
        std.mem.eql(u8, ev_type, "wheel"))
    {
        const x_val = root.get("x") orelse return error.MissingCoordinates;
        const y_val = root.get("y") orelse return error.MissingCoordinates;
        const x = parseU16Clamped(x_val) orelse return error.MissingCoordinates;
        const y = parseU16Clamped(y_val) orelse return error.MissingCoordinates;

        const is_down = if (root.get("down")) |dv|
            parseDownValue(dv)
        else if (root.get("button")) |bv|
            parseDownValue(bv)
        else
            false;

        var pointer_id: u8 = 0;
        if (root.get("id") orelse root.get("slot") orelse root.get("pointer_id")) |id_val| {
            if (parseU16Clamped(id_val)) |pid| {
                pointer_id = @intCast(@min(pid, 255));
            }
        }

        var action: u8 = if (is_down) TOUCH_ACTION_DOWN else TOUCH_ACTION_UP;
        if (root.get("action")) |act_val| {
            switch (act_val) {
                .integer => |i| {
                    if (i >= 0 and i <= 2) action = @intCast(i);
                },
                .string => |s| {
                    if (std.ascii.eqlIgnoreCase(s, "move")) action = TOUCH_ACTION_MOVE else if (std.ascii.eqlIgnoreCase(s, "down")) action = TOUCH_ACTION_DOWN else if (std.ascii.eqlIgnoreCase(s, "up")) action = TOUCH_ACTION_UP;
                },
                else => {},
            }
        }

        const pressure: u16 = if (action == TOUCH_ACTION_UP)
            0
        else if (root.get("pressure")) |pv|
            (parseU16Clamped(pv) orelse DEFAULT_PRESSURE)
        else
            DEFAULT_PRESSURE;

        return .{
            .touch = .{
                .action = action,
                .pointer_id = pointer_id,
                .x = x,
                .y = y,
                .pressure = pressure,
            },
        };
    }

    if (std.mem.eql(u8, ev_type, "keyboard") or std.mem.eql(u8, ev_type, "key")) {
        const kc_val = root.get("keycode") orelse root.get("code") orelse root.get("key_code") orelse return error.MissingKeycode;
        const keycode = parseU32Clamped(kc_val) orelse return error.MissingKeycode;
        const is_down = if (root.get("down")) |dv|
            parseDownValue(dv)
        else if (root.get("event_type")) |et|
            parseDownValue(et)
        else
            false;
        var meta: u8 = 0;
        if (root.get("meta")) |mv| {
            if (parseU16Clamped(mv)) |m| meta = @intCast(@min(m, 255));
        }
        return .{
            .key = .{
                .action = if (is_down) KEY_ACTION_DOWN else KEY_ACTION_UP,
                .keycode = keycode,
                .meta = meta,
            },
        };
    }

    return error.UnknownEventType;
}

/// Encodes a TouchEvent into the 9-byte binary input wire format:
/// `[0]=0x01, [1]=action, [2]=pointer_id, [3..5]=x LE, [5..7]=y LE, [7..9]=pressure LE`.
pub fn encodeTouchPacket(ev: TouchEvent) [TOUCH_PACKET_SIZE]u8 {
    var pkt: [TOUCH_PACKET_SIZE]u8 = undefined;
    pkt[0] = MSG_TOUCH_EVENT;
    pkt[1] = ev.action;
    pkt[2] = ev.pointer_id;
    std.mem.writeInt(u16, pkt[3..5], ev.x, .little);
    std.mem.writeInt(u16, pkt[5..7], ev.y, .little);
    std.mem.writeInt(u16, pkt[7..9], ev.pressure, .little);
    return pkt;
}

/// Encodes a KeyEvent into the 7-byte binary input wire format:
/// `[0]=0x02, [1]=action, [2..6]=keycode LE, [6]=meta`.
pub fn encodeKeyPacket(ev: KeyEvent) [KEY_PACKET_SIZE]u8 {
    var pkt: [KEY_PACKET_SIZE]u8 = undefined;
    pkt[0] = MSG_KEY_EVENT;
    pkt[1] = ev.action;
    std.mem.writeInt(u32, pkt[2..6], ev.keycode, .little);
    pkt[6] = ev.meta;
    return pkt;
}

/// Formats an InputEvent into `uinputd.sock` / minitouch text commands.
pub fn formatUinputCommand(ev: InputEvent, buf: []u8) InputError![]const u8 {
    return switch (ev) {
        .touch => |t| switch (t.action) {
            TOUCH_ACTION_DOWN => std.fmt.bufPrint(
                buf,
                "d {d} {d} {d} {d}\nc\n",
                .{ t.pointer_id, t.x, t.y, if (t.pressure == 0) DEFAULT_PRESSURE else t.pressure },
            ) catch error.BufferTooSmall,
            TOUCH_ACTION_MOVE => std.fmt.bufPrint(
                buf,
                "m {d} {d} {d} {d}\nc\n",
                .{ t.pointer_id, t.x, t.y, if (t.pressure == 0) DEFAULT_PRESSURE else t.pressure },
            ) catch error.BufferTooSmall,
            else => std.fmt.bufPrint(
                buf,
                "u {d}\nc\n",
                .{t.pointer_id},
            ) catch error.BufferTooSmall,
        },
        .key => |k| std.fmt.bufPrint(
            buf,
            "k {d} {d}\nc\n",
            .{ k.keycode, if (k.action == KEY_ACTION_DOWN) @as(u8, 1) else @as(u8, 0) },
        ) catch error.BufferTooSmall,
    };
}

pub const InputSink = struct {
    uinput_socket_path: ?[]const u8 = null,
    pointer_down: bool = false,
    mouse_x: i32 = 540,
    mouse_y: i32 = 1205,
    mouse_down: bool = false,
    mouse_dirty: bool = false,

    pub fn init(uinput_socket_path: ?[]const u8) InputSink {
        return .{ .uinput_socket_path = uinput_socket_path };
    }

    pub fn flushPendingMouse(self: *InputSink) void {
        if (!self.mouse_dirty) return;
        self.mouse_dirty = false;
        const sock_path = self.uinput_socket_path orelse return;
        if (sock_path.len == 0) return;

        if (self.mouse_x < 0 or self.mouse_y < 0) {
            writeUnixStreamPacket(sock_path, "p 2000 3000 0\nc\n") catch {};
            return;
        }
        const mx: u16 = @intCast(std.math.clamp(self.mouse_x, 0, 1079));
        const my: u16 = @intCast(std.math.clamp(self.mouse_y, 0, 2409));
        var cmd_buf: [128]u8 = undefined;
        const cmd = if (self.mouse_down)
            std.fmt.bufPrint(&cmd_buf, "p {d} {d} 1\nm 0 {d} {d} {d}\nc\n", .{ mx, my, mx, my, DEFAULT_PRESSURE }) catch ""
        else
            std.fmt.bufPrint(&cmd_buf, "p {d} {d} 0\nc\n", .{ mx, my }) catch "";
        if (cmd.len > 0) writeUnixStreamPacket(sock_path, cmd) catch {};
    }

    pub fn handleDataChannelJson(self: *InputSink, allocator: std.mem.Allocator, raw_json: []const u8) !InputEvent {
        if (std.json.parseFromSlice(std.json.Value, allocator, raw_json, .{})) |parsed| {
            defer parsed.deinit();
            if (parsed.value == .object) {
                const root = parsed.value.object;
                if (root.get("type") orelse root.get("event_type") orelse root.get("message_type")) |tv| {
                    if (tv == .string) {
                        if (std.mem.eql(u8, tv.string, "mouseMove")) {
                            const dx = if (root.get("x")) |xv| (parseI32Clamped(xv) orelse 0) else 0;
                            const dy = if (root.get("y")) |yv| (parseI32Clamped(yv) orelse 0) else 0;
                            const is_abs = if (root.get("abs")) |av| parseDownValue(av) else false;
                            if (dx < 0 and dy < 0 and is_abs) {
                                self.mouse_x = -1;
                                self.mouse_y = -1;
                                self.mouse_dirty = true;
                                return .{
                                    .touch = .{
                                        .action = TOUCH_ACTION_MOVE,
                                        .pointer_id = 0,
                                        .x = 0,
                                        .y = 0,
                                        .pressure = 0,
                                    },
                                };
                            }
                            if (is_abs) {
                                self.mouse_x = std.math.clamp(dx, 0, 1079);
                                self.mouse_y = std.math.clamp(dy, 0, 2409);
                            } else {
                                if (self.mouse_x < 0) self.mouse_x = 540;
                                if (self.mouse_y < 0) self.mouse_y = 1205;
                                self.mouse_x = std.math.clamp(self.mouse_x + dx * 2, 0, 1079);
                                self.mouse_y = std.math.clamp(self.mouse_y + dy * 2, 0, 2409);
                            }
                            self.mouse_dirty = true;
                            const mx: u16 = @intCast(self.mouse_x);
                            const my: u16 = @intCast(self.mouse_y);
                            return .{
                                .touch = .{
                                    .action = TOUCH_ACTION_MOVE,
                                    .pointer_id = 0,
                                    .x = mx,
                                    .y = my,
                                    .pressure = if (self.mouse_down) DEFAULT_PRESSURE else 0,
                                },
                            };
                        } else if (std.mem.eql(u8, tv.string, "mouseButton")) {
                            const is_down = if (root.get("down")) |dv| parseDownValue(dv) else false;
                            const was_down = self.mouse_down;
                            self.mouse_down = is_down;
                            self.pointer_down = is_down;
                            if (self.mouse_x < 0) self.mouse_x = 540;
                            if (self.mouse_y < 0) self.mouse_y = 1205;
                            const mx: u16 = @intCast(std.math.clamp(self.mouse_x, 0, 1079));
                            const my: u16 = @intCast(std.math.clamp(self.mouse_y, 0, 2409));
                            if (is_down != was_down) {
                                self.mouse_dirty = false;
                                if (self.uinput_socket_path) |sock_path| {
                                    if (sock_path.len > 0) {
                                        var cmd_buf: [128]u8 = undefined;
                                        const cmd = if (is_down)
                                            std.fmt.bufPrint(&cmd_buf, "p {d} {d} 1\nd 0 {d} {d} {d}\nc\n", .{ mx, my, mx, my, DEFAULT_PRESSURE }) catch ""
                                        else
                                            std.fmt.bufPrint(&cmd_buf, "p {d} {d} 0\nu 0\nc\n", .{ mx, my }) catch "";
                                        if (cmd.len > 0) writeUnixStreamPacket(sock_path, cmd) catch {};
                                    }
                                }
                            }
                            return .{
                                .touch = .{
                                    .action = if (is_down) TOUCH_ACTION_DOWN else TOUCH_ACTION_UP,
                                    .pointer_id = 0,
                                    .x = mx,
                                    .y = my,
                                    .pressure = if (is_down) DEFAULT_PRESSURE else 0,
                                },
                            };
                        }
                    }
                }
            }
        } else |_| {}

        var ev = try parseInputJson(allocator, raw_json);
        switch (ev) {
            .touch => |*t| {
                if (t.action == TOUCH_ACTION_DOWN) {
                    if (self.pointer_down) {
                        t.action = TOUCH_ACTION_MOVE;
                    } else {
                        self.pointer_down = true;
                    }
                } else if (t.action == TOUCH_ACTION_UP) {
                    self.pointer_down = false;
                }
            },
            .key => {},
        }
        if (self.uinput_socket_path) |sock_path| {
            if (sock_path.len > 0) {
                var cmd_buf: [128]u8 = undefined;
                const cmd = try formatUinputCommand(ev, &cmd_buf);
                writeUnixStreamPacket(sock_path, cmd) catch {};
            }
        }
        return ev;
    }
};

pub fn writeUnixStreamPacket(sock_path: []const u8, payload: []const u8) InputError!void {
    if (is_linux) {
        if (sock_path.len == 0 or sock_path.len >= 108) return error.ConnectFailed;
        const rc = linux.socket(linux.AF.UNIX, linux.SOCK.STREAM | linux.SOCK.CLOEXEC, 0);
        if (linux.errno(rc) != .SUCCESS) return error.SocketFailed;
        const fd: i32 = @intCast(rc);
        defer _ = linux.close(fd);

        var addr: linux.sockaddr.un = .{
            .family = linux.AF.UNIX,
            .path = @splat(0),
        };
        @memcpy(addr.path[0..sock_path.len], sock_path);
        if (linux.errno(linux.connect(fd, @ptrCast(&addr), @sizeOf(linux.sockaddr.un))) != .SUCCESS) {
            return error.ConnectFailed;
        }
        var off: usize = 0;
        while (off < payload.len) {
            const wrc = linux.write(fd, payload[off..].ptr, payload.len - off);
            switch (linux.errno(wrc)) {
                .SUCCESS => {
                    if (wrc == 0) return error.WriteFailed;
                    off += wrc;
                },
                .INTR => continue,
                else => return error.WriteFailed,
            }
        }
    } else {
        if (sock_path.len == 0 or sock_path.len >= 104) return error.ConnectFailed;
        const fd = c.socket(c.AF.UNIX, c.SOCK.STREAM, 0);
        if (fd < 0) return error.SocketFailed;
        defer _ = c.close(fd);

        var addr: c.sockaddr.un = std.mem.zeroes(c.sockaddr.un);
        addr.family = @intCast(c.AF.UNIX);
        addr.len = @sizeOf(c.sockaddr.un);
        const path_bytes: []u8 = @ptrCast(&addr.path);
        @memcpy(path_bytes[0..sock_path.len], sock_path);

        if (c.connect(fd, @ptrCast(&addr), @sizeOf(c.sockaddr.un)) != 0) {
            return error.ConnectFailed;
        }
        var off: usize = 0;
        while (off < payload.len) {
            const wrc = c.write(fd, payload[off..].ptr, payload.len - off);
            if (wrc > 0) {
                off += @intCast(wrc);
            } else {
                return error.WriteFailed;
            }
        }
    }
}

test "parseInputJson and binary/uinput encoding for touch, multi-touch, mouse, and keyboard" {
    const allocator = std.testing.allocator;

    // 1. Touch down
    const ev1 = try parseInputJson(allocator, "{\"type\":\"touch\",\"x\":120,\"y\":340,\"down\":1}");
    switch (ev1) {
        .touch => |t| {
            try std.testing.expectEqual(TOUCH_ACTION_DOWN, t.action);
            try std.testing.expectEqual(@as(u8, 0), t.pointer_id);
            try std.testing.expectEqual(@as(u16, 120), t.x);
            try std.testing.expectEqual(@as(u16, 340), t.y);
            const pkt = encodeTouchPacket(t);
            try std.testing.expectEqual(@as(u8, 0x01), pkt[0]);
            try std.testing.expectEqual(@as(u8, 0), pkt[1]);
            try std.testing.expectEqual(@as(u8, 0), pkt[2]);
            try std.testing.expectEqual(@as(u16, 120), std.mem.readInt(u16, pkt[3..5], .little));
            try std.testing.expectEqual(@as(u16, 340), std.mem.readInt(u16, pkt[5..7], .little));

            var cmd_buf: [64]u8 = undefined;
            const cmd = try formatUinputCommand(ev1, &cmd_buf);
            try std.testing.expectEqualStrings("d 0 120 340 50\nc\n", cmd);
        },
        else => return error.TestUnexpectedResult,
    }

    // 2. Multi-touch up
    const ev2 = try parseInputJson(allocator, "{\"type\":\"multi-touch\",\"id\":2,\"x\":500,\"y\":600,\"down\":0}");
    switch (ev2) {
        .touch => |t| {
            try std.testing.expectEqual(TOUCH_ACTION_UP, t.action);
            try std.testing.expectEqual(@as(u8, 2), t.pointer_id);
            var cmd_buf: [64]u8 = undefined;
            const cmd = try formatUinputCommand(ev2, &cmd_buf);
            try std.testing.expectEqualStrings("u 2\nc\n", cmd);
        },
        else => return error.TestUnexpectedResult,
    }

    // 3. Mouse event
    const ev3 = try parseInputJson(allocator, "{\"type\":\"mouse\",\"x\":45,\"y\":90,\"button\":1,\"down\":true}");
    switch (ev3) {
        .touch => |t| {
            try std.testing.expectEqual(TOUCH_ACTION_DOWN, t.action);
            try std.testing.expectEqual(@as(u16, 45), t.x);
            try std.testing.expectEqual(@as(u16, 90), t.y);
        },
        else => return error.TestUnexpectedResult,
    }

    // 4. Keyboard event
    const ev4 = try parseInputJson(allocator, "{\"type\":\"keyboard\",\"keycode\":4,\"down\":1}");
    switch (ev4) {
        .key => |k| {
            try std.testing.expectEqual(KEY_ACTION_DOWN, k.action);
            try std.testing.expectEqual(@as(u32, 4), k.keycode);
            const pkt = encodeKeyPacket(k);
            try std.testing.expectEqual(@as(u8, 0x02), pkt[0]);
            try std.testing.expectEqual(@as(u8, 0), pkt[1]);
            try std.testing.expectEqual(@as(u32, 4), std.mem.readInt(u32, pkt[2..6], .little));
            try std.testing.expectEqual(@as(u8, 0), pkt[6]);
        },
        else => return error.TestUnexpectedResult,
    }

    var sink = InputSink.init(null);
    const ev5 = try sink.handleDataChannelJson(allocator, "{\"type\":\"key\",\"keycode\":66,\"down\":true}");
    switch (ev5) {
        .key => |k| try std.testing.expectEqual(@as(u32, 66), k.keycode),
        else => return error.TestUnexpectedResult,
    }
}
