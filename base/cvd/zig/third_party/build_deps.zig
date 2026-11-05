const std = @import("std");

pub const ThirdPartyDeps = struct {
    lib: *std.Build.Step.Compile,
    include_path: std.Build.LazyPath,
    unit_tests: *std.Build.Step.Compile,
};

fn detectBasePrefix(b: *std.Build) []const u8 {
    b.build_root.handle.access("base/cvd/zig/third_party/test_deps.zig", .{}) catch return "";
    return "base/cvd/";
}

pub fn addThirdPartyDeps(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) ThirdPartyDeps {
    const is_self_root = std.mem.endsWith(u8, b.root.root_dir.path orelse "", "zig/third_party") or
        std.mem.endsWith(u8, b.root.sub_path, "zig/third_party");
    if (is_self_root) {
        return addThirdPartyDepsAt(b, target, optimize, true);
    }
    return addThirdPartyDepsWithPrefix(b, target, optimize, detectBasePrefix(b));
}

pub fn addThirdPartyDepsAt(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    is_self_root: bool,
) ThirdPartyDeps {
    if (is_self_root) {
        return addThirdPartyDepsFromDir(b, target, optimize, "", true);
    }
    return addThirdPartyDepsWithPrefix(b, target, optimize, detectBasePrefix(b));
}

pub fn addThirdPartyDepsWithPrefix(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    base_prefix: []const u8,
) ThirdPartyDeps {
    const dir_prefix = b.fmt("{s}zig/third_party/", .{base_prefix});
    return addThirdPartyDepsFromDir(b, target, optimize, dir_prefix, false);
}

fn addThirdPartyDepsFromDir(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    dir_prefix: []const u8,
    is_self_root: bool,
) ThirdPartyDeps {
    const include_path = b.path(if (is_self_root) "include" else b.fmt("{s}include", .{dir_prefix}));

    const cpp_flags: []const []const u8 = &.{
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
    };

    const lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });
    lib_mod.addIncludePath(include_path);
    lib_mod.addCSourceFiles(.{
        .files = &.{
            b.fmt("{s}src/jsoncpp.cpp", .{dir_prefix}),
            b.fmt("{s}src/libwebsockets_slim.cpp", .{dir_prefix}),
            b.fmt("{s}src/absl_and_base.cpp", .{dir_prefix}),
            b.fmt("{s}src/codecs_and_media.cpp", .{dir_prefix}),
            b.fmt("{s}src/crypto_and_utils.cpp", .{dir_prefix}),
        },
        .flags = cpp_flags,
    });

    const lib = b.addLibrary(.{
        .linkage = .static,
        .name = "cf_third_party",
        .root_module = lib_mod,
    });

    const test_mod = b.createModule(.{
        .root_source_file = b.path(b.fmt("{s}test_deps.zig", .{dir_prefix})),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });
    test_mod.addIncludePath(include_path);
    test_mod.addCSourceFiles(.{
        .files = &.{
            b.fmt("{s}src/test_deps_bridge.cpp", .{dir_prefix}),
        },
        .flags = cpp_flags,
    });
    test_mod.linkLibrary(lib);

    const unit_tests = b.addTest(.{
        .name = "cf_third_party_tests",
        .root_module = test_mod,
    });

    return .{
        .lib = lib,
        .include_path = include_path,
        .unit_tests = unit_tests,
    };
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const deps = addThirdPartyDepsAt(b, target, optimize, true);
    b.installArtifact(deps.lib);

    const run_tests = b.addRunArtifact(deps.unit_tests);
    const test_step = b.step("test", "Run libcf_third_party unit tests");
    test_step.dependOn(&run_tests.step);
}
