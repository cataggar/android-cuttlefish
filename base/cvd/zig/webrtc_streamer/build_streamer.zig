const std = @import("std");

pub const StreamerTargets = struct {
    exe: *std.Build.Step.Compile,
    webrtc_exe: *std.Build.Step.Compile,
    lib: *std.Build.Step.Compile,
    unit_tests: *std.Build.Step.Compile,
};

fn detectBasePrefix(b: *std.Build) []const u8 {
    b.build_root.handle.access("base/cvd/zig/webrtc_streamer/main.zig", .{}) catch return "";
    return "base/cvd/";
}

pub fn addStreamer(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) StreamerTargets {
    const root_path = b.build_root.path orelse "";
    const is_self_root = std.mem.endsWith(u8, root_path, "zig/webrtc_streamer");
    if (is_self_root) {
        return addStreamerAt(b, target, optimize, true);
    }
    return addStreamerWithPrefix(b, target, optimize, detectBasePrefix(b));
}

pub fn addStreamerAt(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    is_self_root: bool,
) StreamerTargets {
    if (is_self_root) {
        return addStreamerFromDir(b, target, optimize, "");
    }
    return addStreamerWithPrefix(b, target, optimize, detectBasePrefix(b));
}

pub fn addStreamerWithPrefix(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    base_prefix: []const u8,
) StreamerTargets {
    const dir_prefix = b.fmt("{s}zig/webrtc_streamer/", .{base_prefix});
    return addStreamerFromDir(b, target, optimize, dir_prefix);
}

fn addStreamerFromDir(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    dir_prefix: []const u8,
) StreamerTargets {
    const link_libc = (target.result.os.tag == .macos);
    const main_path = b.path(b.fmt("{s}main.zig", .{dir_prefix}));

    const exe_mod = b.createModule(.{
        .root_source_file = main_path,
        .target = target,
        .optimize = optimize,
        .link_libc = link_libc,
    });
    const exe = b.addExecutable(.{
        .name = "webrtc_streamer",
        .root_module = exe_mod,
    });

    const webrtc_mod = b.createModule(.{
        .root_source_file = main_path,
        .target = target,
        .optimize = optimize,
        .link_libc = link_libc,
    });
    const webrtc_exe = b.addExecutable(.{
        .name = "webrtc",
        .root_module = webrtc_mod,
    });

    const lib_mod = b.createModule(.{
        .root_source_file = main_path,
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const lib = b.addLibrary(.{
        .linkage = .static,
        .name = "webrtc_streamer_shim",
        .root_module = lib_mod,
    });

    const test_mod = b.createModule(.{
        .root_source_file = main_path,
        .target = target,
        .optimize = optimize,
        .link_libc = link_libc,
    });
    const unit_tests = b.addTest(.{
        .name = "webrtc_streamer_tests",
        .root_module = test_mod,
    });

    return .{
        .exe = exe,
        .webrtc_exe = webrtc_exe,
        .lib = lib,
        .unit_tests = unit_tests,
    };
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const streamer = addStreamerAt(b, target, optimize, true);
    b.installArtifact(streamer.exe);
    b.installArtifact(streamer.lib);

    const run_tests = b.addRunArtifact(streamer.unit_tests);
    const test_step = b.step("test", "Run pure-Zig webrtc_streamer unit tests");
    test_step.dependOn(&run_tests.step);
}
