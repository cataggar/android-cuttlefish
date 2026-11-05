const std = @import("std");

fn detectBasePrefix(b: *std.Build) []const u8 {
    b.build_root.handle.access("base/cvd/zig/host_stubs/webrtc_host_stubs.cpp", .{}) catch return "";
    return "base/cvd/";
}

pub fn addWebRtcHostBinary(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    libwebrtc_lib: *std.Build.Step.Compile,
    libwebrtc_include: std.Build.LazyPath,
    third_party_lib: *std.Build.Step.Compile,
    third_party_include: std.Build.LazyPath,
    streamer_shim_lib: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    return addWebRtcHostBinaryWithPrefix(
        b,
        target,
        optimize,
        libwebrtc_lib,
        libwebrtc_include,
        third_party_lib,
        third_party_include,
        streamer_shim_lib,
        detectBasePrefix(b),
    );
}

pub fn addWebRtcHostBinaryWithPrefix(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    libwebrtc_lib: *std.Build.Step.Compile,
    libwebrtc_include: std.Build.LazyPath,
    third_party_lib: *std.Build.Step.Compile,
    third_party_include: std.Build.LazyPath,
    streamer_shim_lib: *std.Build.Step.Compile,
    base_prefix: []const u8,
) *std.Build.Step.Compile {
    const os_define: []const u8 = if (target.result.os.tag == .macos) "-DWEBRTC_MAC" else "-DWEBRTC_LINUX";
    const cpp_flags: []const []const u8 = &.{
        "-std=c++20",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-Wno-missing-field-initializers",
        "-Wno-deprecated-declarations",
        "-DWEBRTC_POSIX",
        os_define,
        "-DWEBRTC_HAVE_SCTP",
        "-DWEBRTC_HAVE_DCSCTP",
        "-DWEBRTC_CODEC_OPUS",
        "-DWEBRTC_USE_BUILTIN_OPUS=1",
        "-DWEBRTC_USE_BUILTIN_ILBC=0",
        "-DWEBRTC_USE_H264",
    };

    const host_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });

    const cvd_root_path = if (base_prefix.len == 0) "." else std.mem.trimEnd(u8, base_prefix, "/");
    const cvd_root = b.path(cvd_root_path);

    // Include paths: pregenerated_protos first (for generated .pb.h and slim confui override),
    // then third_party, libwebrtc, and base/cvd root.
    host_mod.addIncludePath(b.path(b.fmt("{s}zig/host_stubs/pregenerated_protos", .{base_prefix})));
    host_mod.addIncludePath(third_party_include);
    host_mod.addIncludePath(libwebrtc_include);
    host_mod.addIncludePath(cvd_root);

    host_mod.addCSourceFiles(.{
        .root = cvd_root,
        .files = &.{
            // Core result, ansi_codes, posix, io, files, process, flag_parser
            "cuttlefish/result/error_type.cc",
            "cuttlefish/result/function_name.cc",
            "cuttlefish/result/stack_trace_entry.cc",
            "cuttlefish/ansi_codes/ansi_codes.cc",
            "cuttlefish/ansi_codes/should_color.cc",
            "cuttlefish/ansi_codes/terminal_colors.cc",
            "cuttlefish/posix/readlink.cc",
            "cuttlefish/posix/realpath.cc",
            "cuttlefish/posix/remove.cc",
            "cuttlefish/posix/rename.cc",
            "cuttlefish/posix/stat.cc",
            "cuttlefish/posix/strerror.cc",
            "cuttlefish/posix/symlink.cc",
            "cuttlefish/io/copy.cc",
            "cuttlefish/io/default_visitor.cc",
            "cuttlefish/io/io.cc",
            "cuttlefish/io/length.cc",
            "cuttlefish/io/native_filesystem.cc",
            "cuttlefish/io/read_exact.cc",
            "cuttlefish/io/string.cc",
            "cuttlefish/io/write_exact.cc",
            "cuttlefish/files/are_hard_linked.cc",
            "cuttlefish/files/copy.cc",
            "cuttlefish/files/copy_with_attributes.cc",
            "cuttlefish/files/directory_contents.cc",
            "cuttlefish/files/directory_exists.cc",
            "cuttlefish/files/file_device_id.cc",
            "cuttlefish/files/file_exists.cc",
            "cuttlefish/files/file_is_socket.cc",
            "cuttlefish/files/is_directory_empty.cc",
            "cuttlefish/files/is_symlink.cc",
            "cuttlefish/files/link_or_copy.cc",
            "cuttlefish/files/recursively_remove_directory.cc",
            "cuttlefish/files/sparse_file_sizes.cc",
            "cuttlefish/process/command.cc",
            "cuttlefish/process/envp_to_map.cc",
            "cuttlefish/process/execute.cc",
            "cuttlefish/process/managed_stdio.cc",
            "cuttlefish/process/proc_file_utils.cc",
            "cuttlefish/process/subprocess.cc",
            "cuttlefish/process/subprocess_options.cc",
            "cuttlefish/flag_parser/flag.cpp",
            "cuttlefish/flag_parser/gflags_compat.cpp",
            "cuttlefish/flag_parser/shared_fd_flag.cpp",

            // Common libraries (fs, key_equals_value, transport, utils, confui)
            "cuttlefish/common/libs/fs/epoll.cpp",
            "cuttlefish/common/libs/fs/fd.cc",
            "cuttlefish/common/libs/fs/recv_all.cc",
            "cuttlefish/common/libs/fs/scoped_mmap.cc",
            "cuttlefish/common/libs/fs/shared_buf.cc",
            "cuttlefish/common/libs/fs/shared_fd.cpp",
            "cuttlefish/common/libs/fs/shared_fd_stream.cpp",
            "cuttlefish/common/libs/key_equals_value/key_equals_value.cc",
            "cuttlefish/common/libs/transport/channel.cpp",
            "cuttlefish/common/libs/transport/channel_sharedfd.cpp",
            "cuttlefish/common/libs/utils/base64.cpp",
            "cuttlefish/common/libs/utils/container.cpp",
            "cuttlefish/common/libs/utils/device_type.cpp",
            "cuttlefish/common/libs/utils/environment.cpp",
            "cuttlefish/common/libs/utils/files.cpp",
            "cuttlefish/common/libs/utils/flags_validator.cpp",
            "cuttlefish/common/libs/utils/host_info.cpp",
            "cuttlefish/common/libs/utils/in_sandbox.cpp",
            "cuttlefish/common/libs/utils/inotify.cpp",
            "cuttlefish/common/libs/utils/is_google_corp.cpp",
            "cuttlefish/common/libs/utils/json.cpp",
            "cuttlefish/common/libs/utils/known_paths.cpp",
            "cuttlefish/common/libs/utils/network.cpp",
            "cuttlefish/common/libs/utils/random.cpp",
            "cuttlefish/common/libs/utils/semver.cpp",
            "cuttlefish/common/libs/utils/signals.cpp",
            "cuttlefish/common/libs/utils/socket2socket_proxy.cpp",
            "cuttlefish/common/libs/utils/tee_logging.cpp",
            "cuttlefish/common/libs/utils/unix_sockets.cpp",
            "cuttlefish/common/libs/utils/users.cpp",
            "cuttlefish/common/libs/utils/vsock_connection.cpp",
            "cuttlefish/common/libs/utils/wait_for_file.cc",
            "cuttlefish/common/libs/utils/wait_for_unix_socket.cpp",

            // Host libs (audio, audio_connector, input_connector, screen_connector, config, gpu, feature)
            "cuttlefish/host/libs/audio/channel_layout.cpp",
            "cuttlefish/host/libs/audio_connector/buffers.cpp",
            "cuttlefish/host/libs/audio_connector/commands.cpp",
            "cuttlefish/host/libs/audio_connector/server.cpp",
            "cuttlefish/host/libs/input_connector/event_buffer.cpp",
            "cuttlefish/host/libs/input_connector/input_connection.cpp",
            "cuttlefish/host/libs/input_connector/input_connector.cpp",
            "cuttlefish/host/libs/input_connector/input_devices.cpp",
            "cuttlefish/host/libs/screen_connector/composition_manager.cpp",
            "cuttlefish/host/libs/screen_connector/ring_buffer_manager.cpp",
            "cuttlefish/host/libs/screen_connector/screen_connector_common.cc",
            "cuttlefish/host/libs/config/config_flag.cpp",
            "cuttlefish/host/libs/config/config_instance_derived.cc",
            "cuttlefish/host/libs/config/config_utils.cpp",
            "cuttlefish/host/libs/config/custom_actions.cpp",
            "cuttlefish/host/libs/config/cuttlefish_config.cpp",
            "cuttlefish/host/libs/config/cuttlefish_config_environment.cpp",
            "cuttlefish/host/libs/config/cuttlefish_config_instance.cpp",
            "cuttlefish/host/libs/config/data_image_policy.cc",
            "cuttlefish/host/libs/config/external_network_mode.cc",
            "cuttlefish/host/libs/config/gpu_mode.cc",
            "cuttlefish/host/libs/config/guest_hwui_renderer.cc",
            "cuttlefish/host/libs/config/guest_renderer_preload.cc",
            "cuttlefish/host/libs/config/known_paths.cpp",
            "cuttlefish/host/libs/config/logging.cpp",
            "cuttlefish/host/libs/config/openwrt_args.cpp",
            "cuttlefish/host/libs/config/secure_hals.cpp",
            "cuttlefish/host/libs/config/vmm_mode.cc",
            "cuttlefish/host/libs/feature/feature.cpp",
            "cuttlefish/host/libs/gpu/cuda_context.cpp",
            "cuttlefish/host/libs/gpu/cuda_loader.cpp",
            "cuttlefish/host/libs/gpu/nvenc_capabilities.cpp",
            "cuttlefish/host/libs/gpu/nvenc_loader.cpp",
            "cuttlefish/host/commands/assemble_cvd/flags/flag_base.cc",
            "cuttlefish/host/commands/assemble_cvd/flags/system_image_dir.cc",
            "cuttlefish/host/commands/cvdalloc/interface.cpp",
            "cuttlefish/host/commands/kernel_log_monitor/kernel_log_server.cc",
            "cuttlefish/host/commands/kernel_log_monitor/utils.cc",

            // WebRTC libcommon
            "cuttlefish/host/frontend/webrtc/libcommon/abgr_buffer.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/audio_device.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/av1_nvenc_encoder_provider.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/builtin_decoder_provider.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/builtin_encoder_provider.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/composite_decoder_factory.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/composite_encoder_factory.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/connection_controller.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/decoder_provider_registry.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/encoder_provider_registry.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/nvenc_video_encoder.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/peer_connection_utils.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/utils.cpp",
            "cuttlefish/host/frontend/webrtc/libcommon/vp8only_encoder_factory.cpp",

            // WebRTC libdevice
            "cuttlefish/host/frontend/webrtc/libdevice/audio_track_source_impl.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/camera_streamer.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/client_handler.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/data_channels.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/gamepad.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/keyboard.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/lights_observer.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/recording_manager.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/server_connection.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/streamer.cpp",
            "cuttlefish/host/frontend/webrtc/libdevice/video_track_source_impl.cpp",

            // WebRTC frontend & slim host stubs
            "cuttlefish/host/frontend/webrtc/adb_handler.cpp",
            "cuttlefish/host/frontend/webrtc/audio_channel_matrix.cpp",
            "cuttlefish/host/frontend/webrtc/audio_handler.cpp",
            "cuttlefish/host/frontend/webrtc/audio_mixer.cpp",
            "cuttlefish/host/frontend/webrtc/bluetooth_handler.cpp",
            "cuttlefish/host/frontend/webrtc/client_server.cpp",
            "cuttlefish/host/frontend/webrtc/connection_observer.cpp",
            "cuttlefish/host/frontend/webrtc/cvd_abgr_video_frame_buffer.cpp",
            "cuttlefish/host/frontend/webrtc/display_handler.cpp",
            "cuttlefish/host/frontend/webrtc/kernel_log_events_handler.cpp",
            "cuttlefish/host/frontend/webrtc/main.cpp",
            "cuttlefish/host/frontend/webrtc/sensors_handler.cpp",
            "cuttlefish/host/frontend/webrtc/tuner_audio_source.cpp",
            "cuttlefish/host/frontend/webrtc/webrtc_command_channel.cpp",
            "zig/host_stubs/webrtc_host_stubs.cpp",
            "zig/host_stubs/wayland_connector_slim.cpp",
        },
        .flags = cpp_flags,
    });

    host_mod.linkLibrary(libwebrtc_lib);
    host_mod.linkLibrary(third_party_lib);
    host_mod.linkLibrary(streamer_shim_lib);

    return b.addExecutable(.{
        .name = "webrtc",
        .root_module = host_mod,
    });
}
