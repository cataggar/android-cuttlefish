const std = @import("std");

pub const OperatorTargets = struct {
    exe: *std.Build.Step.Compile,
    unit_tests: *std.Build.Step.Compile,
};

pub fn addOperator(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) OperatorTargets {
    const exe_mod = b.createModule(.{
        .root_source_file = b.path("zig/webrtc_operator/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "webrtc_operator",
        .root_module = exe_mod,
    });

    const test_mod = b.createModule(.{
        .root_source_file = b.path("zig/webrtc_operator/main.zig"),
        .target = target,
        .optimize = optimize,
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
