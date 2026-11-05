const std = @import("std");
const build_deps = @import("zig/third_party/build_deps.zig");
const build_libwebrtc = @import("zig/libwebrtc/build_libwebrtc.zig");
const build_operator = @import("zig/webrtc_operator/build_operator.zig");
const build_streamer = @import("zig/webrtc_streamer/build_streamer.zig");
const build_host = @import("zig/host_stubs/build_host.zig");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // 1. Third-party C/C++ support library
    const third_party = build_deps.addThirdPartyDepsWithPrefix(b, target, optimize, "");
    b.installArtifact(third_party.lib);

    // 2. Slimmed libwebrtc + Zig wrapper module
    const libwebrtc = build_libwebrtc.addLibWebRtcWithPrefix(b, target, optimize, "");
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
    const operator = build_operator.addOperatorWithPrefix(b, target, optimize, "");
    b.installArtifact(operator.exe);

    // 4. Pure-Zig webrtc_streamer standalone streamer + C ABI shim
    const streamer = build_streamer.addStreamerWithPrefix(b, target, optimize, "");
    b.installArtifact(streamer.exe);

    // 5. Cuttlefish webrtc (webRTC) host binary
    // On Linux: C++ Cuttlefish host binary linked with webrtc_streamer_shim.
    // On non-Linux (e.g. macOS): pure-Zig webrtc_streamer installed as webrtc/webRTC.
    const webrtc_exe = if (target.result.os.tag == .linux)
        build_host.addWebRtcHostBinaryWithPrefix(
            b,
            target,
            optimize,
            libwebrtc.lib,
            libwebrtc.include_path,
            third_party.lib,
            third_party.include_path,
            streamer.lib,
            "",
        )
    else
        streamer.webrtc_exe;

    b.installArtifact(webrtc_exe);
    const install_camel = b.addInstallArtifact(webrtc_exe, .{
        .dest_sub_path = "webRTC",
    });
    b.getInstallStep().dependOn(&install_camel.step);

    // 6. Install operator web assets directory
    const install_assets = b.addInstallDirectory(.{
        .source_dir = b.path("cuttlefish/host/frontend/webrtc_operator/assets"),
        .install_dir = .prefix,
        .install_subdir = "share/cuttlefish-webrtc/assets",
    });
    b.getInstallStep().dependOn(&install_assets.step);

    // 7. Test suite step (`zig build test`)
    const test_step = b.step(
        "test",
        "Run all WebRTC Zig wrapper, third-party, webrtc_operator, and webrtc_streamer tests",
    );
    const run_libwebrtc_tests = b.addRunArtifact(libwebrtc.unit_tests);
    const run_operator_tests = b.addRunArtifact(operator.unit_tests);
    const run_streamer_tests = b.addRunArtifact(streamer.unit_tests);
    const run_third_party_tests = b.addRunArtifact(third_party.unit_tests);
    test_step.dependOn(&run_libwebrtc_tests.step);
    test_step.dependOn(&run_operator_tests.step);
    test_step.dependOn(&run_streamer_tests.step);
    test_step.dependOn(&run_third_party_tests.step);
}

