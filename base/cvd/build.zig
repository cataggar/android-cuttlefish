const std = @import("std");
const build_deps = @import("zig/third_party/build_deps.zig");
const build_libwebrtc = @import("zig/libwebrtc/build_libwebrtc.zig");
const build_operator = @import("zig/webrtc_operator/build_operator.zig");
const build_host = @import("zig/host_stubs/build_host.zig");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // 1. Third-party C/C++ support library
    const third_party = build_deps.addThirdPartyDeps(b, target, optimize);
    b.installArtifact(third_party.lib);

    // 2. Slimmed libwebrtc + Zig wrapper module
    const libwebrtc = build_libwebrtc.addLibWebRtc(b, target, optimize);
    b.installArtifact(libwebrtc.lib);
    const install_webrtc_zig_lib = b.addInstallArtifact(libwebrtc.lib, .{
        .dest_sub_path = "libwebrtc_zig.a",
    });
    b.getInstallStep().dependOn(&install_webrtc_zig_lib.step);
    _ = b.addModule("webrtc", .{
        .root_source_file = b.path("zig/libwebrtc/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    // 3. Pure-Zig webrtc_operator signaling & web UI server
    const operator = build_operator.addOperator(b, target, optimize);
    b.installArtifact(operator.exe);

    // 4. Slimmed Cuttlefish webrtc (webRTC) host binary
    const webrtc_exe = build_host.addWebRtcHostBinary(
        b,
        target,
        optimize,
        libwebrtc.lib,
        libwebrtc.include_path,
        third_party.lib,
        third_party.include_path,
    );
    b.installArtifact(webrtc_exe);
    const install_camel = b.addInstallArtifact(webrtc_exe, .{
        .dest_sub_path = "webRTC",
    });
    b.getInstallStep().dependOn(&install_camel.step);

    // 5. Test suite step (`zig build test`)
    const test_step = b.step("test", "Run all WebRTC Zig wrapper, third-party, and webrtc_operator tests");
    const run_libwebrtc_tests = b.addRunArtifact(libwebrtc.unit_tests);
    const run_operator_tests = b.addRunArtifact(operator.unit_tests);
    const run_third_party_tests = b.addRunArtifact(third_party.unit_tests);
    test_step.dependOn(&run_libwebrtc_tests.step);
    test_step.dependOn(&run_operator_tests.step);
    test_step.dependOn(&run_third_party_tests.step);
}

