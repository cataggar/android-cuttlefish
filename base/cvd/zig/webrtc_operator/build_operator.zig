const std = @import("std");

pub const OperatorTargets = struct {
    exe: *std.Build.Step.Compile,
    unit_tests: *std.Build.Step.Compile,
};

fn detectBasePrefix(b: *std.Build) []const u8 {
    b.build_root.handle.access("base/cvd/zig/webrtc_operator/main.zig", .{}) catch return "";
    return "base/cvd/";
}

pub fn addOperator(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) OperatorTargets {
    return addOperatorWithPrefix(b, target, optimize, detectBasePrefix(b));
}

pub fn addOperatorWithPrefix(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    base_prefix: []const u8,
) OperatorTargets {
    const link_libc = (target.result.os.tag == .macos);
    const main_path = b.path(b.fmt("{s}zig/webrtc_operator/main.zig", .{base_prefix}));

    const exe_mod = b.createModule(.{
        .root_source_file = main_path,
        .target = target,
        .optimize = optimize,
        .link_libc = link_libc,
    });

    const exe = b.addExecutable(.{
        .name = "webrtc_operator",
        .root_module = exe_mod,
    });

    const test_mod = b.createModule(.{
        .root_source_file = main_path,
        .target = target,
        .optimize = optimize,
        .link_libc = link_libc,
    });

    const unit_tests = b.addTest(.{
        .name = "webrtc_operator_tests",
        .root_module = test_mod,
    });

    return .{
        .exe = exe,
        .unit_tests = unit_tests,
    };
}
