const std = @import("std");
const builtin = @import("builtin");
const is_linux = builtin.os.tag == .linux;
const linux = if (is_linux) std.os.linux else struct {};
const c = if (!is_linux) std.c else struct {};

pub const DEFAULT_HWC_SOCK_PATH = "/ipc/hwcomposer.sock";
pub const EXPECTED_MAGIC: i32 = 0x03141592;
pub const EXPECTED_FORMAT: i32 = 1; // HAL_PIXEL_FORMAT_RGBA_8888
pub const RGBX_FORMAT: i32 = 2; // HAL_PIXEL_FORMAT_RGBX_8888
pub const BGRA_FORMAT: i32 = 5; // HAL_PIXEL_FORMAT_BGRA_8888
pub const STOCK_HANDLE_WIRE_SIZE: usize = 48;

pub const PrivateHandle = extern struct {
    version: i32,
    num_fds: i32,
    num_ints: i32,
    data_fd: i32,
    magic: i32,
    flags: i32,
    size: i32,
    offset: i32,
    base: u64,
    pid: i32,
    width: i32,
    height: i32,
    format: i32,
    stride: i32,
    _pad: i32 = 0,

    pub fn validate(self: PrivateHandle, mapped_size: ?usize) !void {
        if (self.magic != EXPECTED_MAGIC) return error.InvalidMagic;
        if (self.format != EXPECTED_FORMAT and self.format != RGBX_FORMAT and self.format != BGRA_FORMAT) {
            return error.UnsupportedFormat;
        }
        if (self.width <= 0 or self.height <= 0) return error.InvalidDimensions;
        if (self.stride < self.width) return error.InvalidStride;
        if (self.size <= 0 or self.offset < 0) return error.InvalidSize;

        const row_bytes = std.math.mul(usize, @intCast(self.stride), 4) catch return error.InvalidSize;
        const required_bytes = std.math.mul(usize, row_bytes, @intCast(self.height)) catch return error.InvalidSize;
        const offset_bytes: usize = @intCast(self.offset);
        const total_required = std.math.add(usize, offset_bytes, required_bytes) catch return error.InvalidSize;
        const declared_size: usize = @intCast(self.size);
        if (declared_size < total_required) return error.BufferTooSmall;
        if (mapped_size) |actual| {
            if (actual < declared_size) return error.BufferTooSmall;
        }
    }
};

comptime {
    std.debug.assert(@sizeOf(PrivateHandle) == 64);
}

pub fn parsePrivateHandle(bytes: []const u8) !PrivateHandle {
    if (bytes.len == @sizeOf(PrivateHandle)) {
        var handle: PrivateHandle = undefined;
        @memcpy(std.mem.asBytes(&handle), bytes);
        try handle.validate(null);
        return handle;
    }
    if (bytes.len == STOCK_HANDLE_WIRE_SIZE) {
        const readI32 = struct {
            fn f(buf: []const u8, off: usize) i32 {
                return std.mem.readInt(i32, buf[off..][0..4], .little);
            }
        }.f;
        const handle = PrivateHandle{
            .version = readI32(bytes, 0),
            .num_fds = readI32(bytes, 4),
            .num_ints = readI32(bytes, 8),
            .data_fd = -1,
            .magic = readI32(bytes, 12),
            .flags = readI32(bytes, 16),
            .size = readI32(bytes, 20),
            .offset = readI32(bytes, 24),
            .base = std.mem.readInt(u64, bytes[28..][0..8], .little),
            .pid = 0,
            .width = readI32(bytes, 36),
            .height = readI32(bytes, 40),
            .format = readI32(bytes, 44),
            .stride = readI32(bytes, 36),
            ._pad = 0,
        };
        try handle.validate(null);
        return handle;
    }
    return error.ShortControlPacket;
}

pub const decodePrivateHandle = parsePrivateHandle;

pub fn encodeStockPrivateHandle48(
    magic: i32,
    flags: i32,
    size: i32,
    offset: i32,
    base: u64,
    width: i32,
    height: i32,
    format: i32,
) [STOCK_HANDLE_WIRE_SIZE]u8 {
    var raw: [STOCK_HANDLE_WIRE_SIZE]u8 = @splat(0);
    std.mem.writeInt(i32, raw[0..4], 12, .little);
    std.mem.writeInt(i32, raw[4..8], 1, .little);
    std.mem.writeInt(i32, raw[8..12], 9, .little);
    std.mem.writeInt(i32, raw[12..16], magic, .little);
    std.mem.writeInt(i32, raw[16..20], flags, .little);
    std.mem.writeInt(i32, raw[20..24], size, .little);
    std.mem.writeInt(i32, raw[24..28], offset, .little);
    std.mem.writeInt(u64, raw[28..36], base, .little);
    std.mem.writeInt(i32, raw[36..40], width, .little);
    std.mem.writeInt(i32, raw[40..44], height, .little);
    std.mem.writeInt(i32, raw[44..48], format, .little);
    return raw;
}

pub fn i420ByteSize(width: u32, height: u32) usize {
    const y_size = @as(usize, width) * @as(usize, height);
    const cw = (@as(usize, width) + 1) / 2;
    const ch = (@as(usize, height) + 1) / 2;
    return y_size + 2 * cw * ch;
}

fn clampU8(val: i32) u8 {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return @intCast(val);
}

/// Converts packed RGBA/RGBX pixels into planar I420 (Y plane followed by U and V planes).
pub fn rgbaToI420(
    rgba: []const u8,
    width: u32,
    height: u32,
    stride_pixels: u32,
    out_i420: []u8,
) void {
    if (width == 0 or height == 0) return;
    const eff_stride: usize = @intCast(if (stride_pixels >= width) stride_pixels else width);
    const w: usize = @intCast(width);
    const h: usize = @intCast(height);
    const y_size = w * h;
    const cw = (w + 1) / 2;
    const ch = (h + 1) / 2;
    const uv_size = cw * ch;
    if (out_i420.len < y_size + 2 * uv_size) return;

    const y_plane = out_i420[0..y_size];
    const u_plane = out_i420[y_size .. y_size + uv_size];
    const v_plane = out_i420[y_size + uv_size .. y_size + 2 * uv_size];

    var y: usize = 0;
    while (y < h) : (y += 1) {
        var x: usize = 0;
        while (x < w) : (x += 1) {
            const px_off = (y * eff_stride + x) * 4;
            const r: i32 = if (px_off < rgba.len) rgba[px_off] else 0;
            const g: i32 = if (px_off + 1 < rgba.len) rgba[px_off + 1] else 0;
            const b: i32 = if (px_off + 2 < rgba.len) rgba[px_off + 2] else 0;

            const luma = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            y_plane[y * w + x] = clampU8(luma);
        }
    }

    var cy: usize = 0;
    while (cy < ch) : (cy += 1) {
        const y0 = cy * 2;
        const y1 = @min(y0 + 1, h - 1);
        var cx: usize = 0;
        while (cx < cw) : (cx += 1) {
            const x0 = cx * 2;
            const x1 = @min(x0 + 1, w - 1);

            const coords = [4][2]usize{
                .{ x0, y0 },
                .{ x1, y0 },
                .{ x0, y1 },
                .{ x1, y1 },
            };
            var r_sum: i32 = 0;
            var g_sum: i32 = 0;
            var b_sum: i32 = 0;
            for (coords) |pt| {
                const off = (pt[1] * eff_stride + pt[0]) * 4;
                r_sum += if (off < rgba.len) rgba[off] else 0;
                g_sum += if (off + 1 < rgba.len) rgba[off + 1] else 0;
                b_sum += if (off + 2 < rgba.len) rgba[off + 2] else 0;
            }
            const r = @divTrunc(r_sum + 2, 4);
            const g = @divTrunc(g_sum + 2, 4);
            const b = @divTrunc(b_sum + 2, 4);

            const u_val = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
            const v_val = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
            u_plane[cy * cw + cx] = clampU8(u_val);
            v_plane[cy * cw + cx] = clampU8(v_val);
        }
    }
}

/// Generates a deterministic animated I420 test pattern when no guest frame socket is connected.
pub fn generateTestPatternI420(
    width: u32,
    height: u32,
    frame_idx: u32,
    out_i420: []u8,
) void {
    if (width == 0 or height == 0) return;
    const w: usize = @intCast(width);
    const h: usize = @intCast(height);
    const y_size = w * h;
    const cw = (w + 1) / 2;
    const ch = (h + 1) / 2;
    const uv_size = cw * ch;
    if (out_i420.len < y_size + 2 * uv_size) return;

    const y_plane = out_i420[0..y_size];
    const u_plane = out_i420[y_size .. y_size + uv_size];
    const v_plane = out_i420[y_size + uv_size .. y_size + 2 * uv_size];

    const shift: usize = @as(usize, frame_idx) * 4;
    const bar_y = (@as(usize, frame_idx) * 3) % h;

    var y: usize = 0;
    while (y < h) : (y += 1) {
        const is_scanbar = (y >= bar_y and y < bar_y + 4);
        var x: usize = 0;
        while (x < w) : (x += 1) {
            if (is_scanbar) {
                y_plane[y * w + x] = 235;
            } else {
                const band = ((x + shift) * 8) / @max(w, 1);
                const base_luma: u8 = @intCast(16 + ((band * 28 + (y % 32)) % 200));
                y_plane[y * w + x] = base_luma;
            }
        }
    }

    var cy: usize = 0;
    while (cy < ch) : (cy += 1) {
        var cx: usize = 0;
        while (cx < cw) : (cx += 1) {
            const u_val: u8 = @intCast((cx * 7 + cy * 3 + @as(usize, frame_idx) * 5) & 0xff);
            const v_val: u8 = @intCast((255 - ((cx * 5 + cy * 11 + @as(usize, frame_idx) * 9) & 0xff)));
            u_plane[cy * cw + cx] = u_val;
            v_plane[cy * cw + cx] = v_val;
        }
    }
}

pub const FrameSource = struct {
    width: u32,
    height: u32,
    frame_idx: u32 = 0,
    hwcomposer_socket_path: ?[]const u8 = null,
    vsock_socket_path: []const u8 = "",

    pub fn init(
        width: u32,
        height: u32,
        hwcomposer_socket_path: ?[]const u8,
        vsock_socket_path: []const u8,
    ) FrameSource {
        return .{
            .width = width,
            .height = height,
            .hwcomposer_socket_path = hwcomposer_socket_path,
            .vsock_socket_path = vsock_socket_path,
        };
    }

    pub fn nextFrameI420(self: *FrameSource, out_i420: []u8) void {
        generateTestPatternI420(self.width, self.height, self.frame_idx, out_i420);
        self.frame_idx +%= 1;
    }
};

test "PrivateHandle 64-byte and stock 48-byte gralloc wire parsing" {
    const h64 = PrivateHandle{
        .version = 12,
        .num_fds = 1,
        .num_ints = 12,
        .data_fd = -1,
        .magic = EXPECTED_MAGIC,
        .flags = 4,
        .size = 720 * 1280 * 4,
        .offset = 0,
        .base = 0,
        .pid = 42,
        .width = 720,
        .height = 1280,
        .format = EXPECTED_FORMAT,
        .stride = 720,
        ._pad = 0,
    };
    const parsed64 = try parsePrivateHandle(std.mem.asBytes(&h64));
    try std.testing.expectEqual(@as(i32, 720), parsed64.width);
    try std.testing.expectEqual(@as(i32, 1280), parsed64.height);
    try std.testing.expectEqual(@as(i32, 720), parsed64.stride);
    try std.testing.expectEqual(EXPECTED_MAGIC, parsed64.magic);

    const raw48 = encodeStockPrivateHandle48(
        EXPECTED_MAGIC,
        0x04,
        1080 * 1920 * 4,
        0,
        0,
        1080,
        1920,
        EXPECTED_FORMAT,
    );
    const parsed48 = try parsePrivateHandle(&raw48);
    try std.testing.expectEqual(@as(i32, 1080), parsed48.width);
    try std.testing.expectEqual(@as(i32, 1920), parsed48.height);
    try std.testing.expectEqual(@as(i32, 1080), parsed48.stride);
    try std.testing.expectEqual(EXPECTED_MAGIC, parsed48.magic);

    const bad48 = encodeStockPrivateHandle48(0x1234, 0, 1024, 0, 0, 16, 16, EXPECTED_FORMAT);
    try std.testing.expectError(error.InvalidMagic, parsePrivateHandle(&bad48));
}

test "rgbaToI420 converts primary colors to expected BT.601 luma and chroma" {
    const w: u32 = 4;
    const h: u32 = 4;
    var rgba: [w * h * 4]u8 = undefined;
    // Fill with pure red (R=255, G=0, B=0, A=255)
    var i: usize = 0;
    while (i < w * h) : (i += 1) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 0;
        rgba[i * 4 + 2] = 0;
        rgba[i * 4 + 3] = 255;
    }

    var i420_buf: [i420ByteSize(w, h)]u8 = undefined;
    rgbaToI420(&rgba, w, h, w, &i420_buf);

    // Pure red BT.601: Y = ((66*255+128)>>8)+16 = 82, U = 90, V = 240
    try std.testing.expectEqual(@as(u8, 82), i420_buf[0]);
    try std.testing.expectEqual(@as(u8, 90), i420_buf[w * h]);
    try std.testing.expectEqual(@as(u8, 240), i420_buf[w * h + 4]);
}

test "generateTestPatternI420 is deterministic and animates across frames" {
    const w: u32 = 16;
    const h: u32 = 16;
    var f0_a: [i420ByteSize(w, h)]u8 = undefined;
    var f0_b: [i420ByteSize(w, h)]u8 = undefined;
    var f1: [i420ByteSize(w, h)]u8 = undefined;

    generateTestPatternI420(w, h, 0, &f0_a);
    generateTestPatternI420(w, h, 0, &f0_b);
    generateTestPatternI420(w, h, 1, &f1);

    try std.testing.expectEqualSlices(u8, &f0_a, &f0_b);
    try std.testing.expect(!std.mem.eql(u8, &f0_a, &f1));
}
