const std = @import("std");
pub const slim_sources = @import("slim_sources.zig");

pub const LibWebRtcArtifacts = struct {
    lib: *std.Build.Step.Compile,
    mod: *std.Build.Module,
    unit_tests: *std.Build.Step.Compile,
    include_path: std.Build.LazyPath,
};

pub fn addLibWebRtc(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) LibWebRtcArtifacts {
    const is_self_root = std.mem.endsWith(u8, b.root.root_dir.path orelse "", "zig/libwebrtc") or
        std.mem.endsWith(u8, b.root.sub_path, "zig/libwebrtc");
    return addLibWebRtcAt(b, target, optimize, is_self_root);
}

pub fn addLibWebRtcAt(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    is_self_root: bool,
) LibWebRtcArtifacts {
    const dir_prefix = if (is_self_root) "" else "zig/libwebrtc/";

    const include_path = b.path(if (is_self_root) "include" else "zig/libwebrtc/include");
    const self_path = b.path(if (is_self_root) "." else "zig/libwebrtc");

    const cpp_flags: []const []const u8 = &.{
        "-std=c++20",
        "-fno-exceptions",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-DWEBRTC_POSIX",
        "-DWEBRTC_LINUX",
        "-DWEBRTC_HAVE_SCTP",
        "-DWEBRTC_HAVE_DCSCTP",
        "-DWEBRTC_CODEC_OPUS",
        "-DWEBRTC_USE_BUILTIN_OPUS=1",
        "-DWEBRTC_USE_BUILTIN_ILBC=0",
        "-DWEBRTC_USE_H264",
        "-DWEBRTC_ENABLE_PROTOBUF=0",
        "-DWEBRTC_APM_DEBUG_DUMP=0",
    };

    const c_lib_mod = b.createModule(.{
        .root_source_file = b.path(b.fmt("{s}root.zig", .{dir_prefix})),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });
    c_lib_mod.addIncludePath(include_path);
    c_lib_mod.addIncludePath(self_path);
    c_lib_mod.addCSourceFiles(.{
        .files = &.{
            b.fmt("{s}csrc/slim_webrtc_core.cpp", .{dir_prefix}),
            b.fmt("{s}stubs/audio_processing_stub.cc", .{dir_prefix}),
            b.fmt("{s}stubs/h264_decoder_stub.cc", .{dir_prefix}),
        },
        .flags = cpp_flags,
    });

    const lib = b.addLibrary(.{
        .linkage = .static,
        .name = "webrtc_slim",
        .root_module = c_lib_mod,
    });

    const mod = b.createModule(.{
        .root_source_file = b.path(b.fmt("{s}root.zig", .{dir_prefix})),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addIncludePath(include_path);
    mod.addIncludePath(self_path);

    const test_mod = b.createModule(.{
        .root_source_file = b.path(b.fmt("{s}root.zig", .{dir_prefix})),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    test_mod.addIncludePath(include_path);
    test_mod.addIncludePath(self_path);

    const unit_tests = b.addTest(.{
        .name = "libwebrtc_tests",
        .root_module = test_mod,
    });

    return .{
        .lib = lib,
        .mod = mod,
        .unit_tests = unit_tests,
        .include_path = include_path,
    };
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const artifacts = addLibWebRtcAt(b, target, optimize, true);
    b.installArtifact(artifacts.lib);

    const run_tests = b.addRunArtifact(artifacts.unit_tests);
    const test_step = b.step("test", "Run slimmed libwebrtc and Zig wrapper unit tests");
    test_step.dependOn(&run_tests.step);
}
