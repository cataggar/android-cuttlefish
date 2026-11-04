const std = @import("std");
const testing = std.testing;

extern fn cf_test_jsoncpp() c_int;
extern fn cf_test_gflags() c_int;
extern fn cf_test_libyuv() c_int;
extern fn cf_test_codecs() c_int;
extern fn cf_test_android_base_and_absl() c_int;

test "jsoncpp parsing and serialization" {
    try testing.expectEqual(@as(c_int, 0), cf_test_jsoncpp());
}

test "gflags command line flag registration and parsing" {
    try testing.expectEqual(@as(c_int, 0), cf_test_gflags());
}

test "libyuv pixel format conversion and scaling" {
    try testing.expectEqual(@as(c_int, 0), cf_test_libyuv());
}

test "openh264, opus, and vpx encoder/decoder interfaces" {
    try testing.expectEqual(@as(c_int, 0), cf_test_codecs());
}

test "android-base, absl, fmt, tl-expected, fruit, crypto, libxml, and libwebsockets" {
    try testing.expectEqual(@as(c_int, 0), cf_test_android_base_and_absl());
}
