#include "absl/algorithm/container.h"
#include "absl/base/no_destructor.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/numeric/bits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "android-base/cmsg.h"
#include "android-base/endian.h"
#include "android-base/file.h"
#include "android-base/logging.h"
#include "android-base/macros.h"
#include "android-base/mapped_file.h"
#include "android-base/no_destructor.h"
#include "android-base/parsebool.h"
#include "android-base/parsedouble.h"
#include "android-base/parseint.h"
#include "android-base/properties.h"
#include "android-base/result.h"
#include "android-base/scopeguard.h"
#include "android-base/stringprintf.h"
#include "android-base/strings.h"
#include "android-base/threads.h"
#include "android-base/unique_fd.h"
#include "crc32c/crc32c.h"
#include "cuda.h"
#include "cutils/sockets.h"
#include "drm/drm_fourcc.h"
#include "drm_fourcc.h"
#include "ffnvcodec/nvEncodeAPI.h"
#include "fmt/chrono.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "fmt/ostream.h"
#include "fmt/ranges.h"
#include "fruit/fruit.h"
#include "gflags/gflags.h"
#include "jerror.h"
#include "jpeglib.h"
#include "json/json.h"
#include "libwebsockets.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "libyuv.h"
#include "log/log.h"
#include "mkvmuxer/mkvmuxer.h"
#include "mkvmuxer/mkvwriter.h"
#include "nvEncodeAPI.h"
#include "openssl/aes.h"
#include "openssl/base.h"
#include "openssl/base64.h"
#include "openssl/bio.h"
#include "openssl/bn.h"
#include "openssl/crypto.h"
#include "openssl/ec.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/hmac.h"
#include "openssl/pem.h"
#include "openssl/rand.h"
#include "openssl/rsa.h"
#include "openssl/sha.h"
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "opus/opus.h"
#include "opus/opus_multistream.h"
#include "png.h"
#include "srtp2/srtp.h"
#include "tl/expected.hpp"
#include "uuid/uuid.h"
#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vpx_encoder.h"
#include "wels/codec_api.h"
#include "zlib.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

DEFINE_bool(cf_test_bool_flag, false, "Test boolean flag");
DEFINE_int32(cf_test_int32_flag, 42, "Test int32 flag");
DEFINE_string(cf_test_str_flag, "default_val", "Test string flag");

namespace {

struct ITestGreeter {
  virtual ~ITestGreeter() = default;
  virtual int Compute() const = 0;
};

struct TestGreeterImpl : public ITestGreeter {
  INJECT(TestGreeterImpl()) = default;
  int Compute() const override { return 77; }
};

fruit::Component<ITestGreeter> GetTestGreeterComponent() {
  return fruit::createComponent().bind<ITestGreeter, TestGreeterImpl>();
}

}  // namespace

extern "C" {

int cf_test_jsoncpp(void) {
  const char* raw_json = R"({
    // Line comment
    "device_id": "cvd-1",
    "displays": [
      {"width": 720, "height": 1280, "dpi": 320}
    ],
    "webrtc": {
      "enabled": true,
      "port": 8443,
      "scale": 1.5
    }
  })";

  Json::CharReaderBuilder rbuilder;
  std::unique_ptr<Json::CharReader> reader(rbuilder.newCharReader());
  Json::Value root;
  std::string errs;
  if (!reader->parse(raw_json, raw_json + std::strlen(raw_json), &root,
                     &errs)) {
    return 1;
  }
  if (!root.isObject()) return 2;
  if (root["device_id"].asString() != "cvd-1") return 3;
  if (!root["displays"].isArray() || root["displays"].size() != 1) return 4;
  if (root["displays"][0]["width"].asInt() != 720) return 5;
  if (root["displays"][0]["height"].asUInt() != 1280u) return 6;
  if (!root["webrtc"]["enabled"].asBool()) return 7;
  if (root["webrtc"]["port"].asInt64() != 8443) return 8;

  root["extra"] = Json::Value(Json::arrayValue);
  root["extra"].append(10);
  root["extra"].append("hello");

  Json::StreamWriterBuilder wbuilder;
  wbuilder["indentation"] = "";
  std::string serialized = Json::writeString(wbuilder, root);
  if (serialized.empty()) return 9;

  Json::Value roundtrip;
  std::istringstream iss(serialized);
  if (!Json::parseFromStream(rbuilder, iss, &roundtrip, &errs)) return 10;
  if (roundtrip["device_id"].asString() != "cvd-1") return 11;
  if (roundtrip["extra"][1].asString() != "hello") return 12;

  return 0;
}

int cf_test_gflags(void) {
  if (FLAGS_cf_test_bool_flag != false) return 1;
  if (FLAGS_cf_test_int32_flag != 42) return 2;
  if (FLAGS_cf_test_str_flag != "default_val") return 3;

  char arg0[] = "test_prog";
  char arg1[] = "--cf_test_bool_flag";
  char arg2[] = "--cf_test_int32_flag=1234";
  char arg3[] = "--cf_test_str_flag=custom_webrtc";
  char arg4[] = "positional";
  char* argv_arr[] = {arg0, arg1, arg2, arg3, arg4, nullptr};
  int argc = 5;
  char** argv = argv_arr;

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (!FLAGS_cf_test_bool_flag) return 4;
  if (FLAGS_cf_test_int32_flag != 1234) return 5;
  if (FLAGS_cf_test_str_flag != "custom_webrtc") return 6;
  if (argc != 2 || std::strcmp(argv[1], "positional") != 0) return 7;

  gflags::CommandLineFlagInfo info =
      gflags::GetCommandLineFlagInfoOrDie("cf_test_int32_flag");
  if (info.is_default || info.current_value != "1234") return 8;

  gflags::SetCommandLineOption("cf_test_int32_flag", "999");
  if (FLAGS_cf_test_int32_flag != 999) return 9;

  return 0;
}

int cf_test_libyuv(void) {
  const int w = 16;
  const int h = 16;
  std::vector<uint8_t> abgr(w * h * 4, 0);
  for (int i = 0; i < w * h; ++i) {
    abgr[i * 4 + 0] = 200;  // R
    abgr[i * 4 + 1] = 100;  // G
    abgr[i * 4 + 2] = 50;   // B
    abgr[i * 4 + 3] = 255;  // A
  }

  std::vector<uint8_t> y(w * h, 0);
  std::vector<uint8_t> u((w / 2) * (h / 2), 0);
  std::vector<uint8_t> v((w / 2) * (h / 2), 0);

  if (libyuv::ABGRToI420(abgr.data(), w * 4, y.data(), w, u.data(), w / 2,
                         v.data(), w / 2, w, h) != 0) {
    return 1;
  }
  if (y[0] == 0) return 2;

  std::vector<uint8_t> dst_argb(w * h * 4, 0);
  if (libyuv::I420ToARGB(y.data(), w, u.data(), w / 2, v.data(), w / 2,
                         dst_argb.data(), w * 4, w, h) != 0) {
    return 3;
  }
  if (dst_argb[3] != 255) return 4;

  std::vector<uint8_t> scaled_y((w / 2) * (h / 2), 0);
  std::vector<uint8_t> scaled_u((w / 4) * (h / 4), 0);
  std::vector<uint8_t> scaled_v((w / 4) * (h / 4), 0);
  if (libyuv::I420Scale(y.data(), w, u.data(), w / 2, v.data(), w / 2, w, h,
                        scaled_y.data(), w / 2, scaled_u.data(), w / 4,
                        scaled_v.data(), w / 4, w / 2, h / 2,
                        libyuv::kFilterBilinear) != 0) {
    return 5;
  }
  if (scaled_y[0] != y[0]) return 6;

  return 0;
}

int cf_test_codecs(void) {
  // 1. OpenH264 test
  ISVCEncoder* h264_enc = nullptr;
  if (WelsCreateSVCEncoder(&h264_enc) != 0 || !h264_enc) return 1;
  SEncParamExt enc_param{};
  h264_enc->GetDefaultParams(&enc_param);
  enc_param.iPicWidth = 64;
  enc_param.iPicHeight = 64;
  if (h264_enc->InitializeExt(&enc_param) != cmResultSuccess) return 2;

  std::vector<uint8_t> yuv(64 * 64 * 3 / 2, 128);
  SSourcePicture pic{};
  pic.iColorFormat = videoFormatI420;
  pic.iPicWidth = 64;
  pic.iPicHeight = 64;
  pic.iStride[0] = 64;
  pic.iStride[1] = 32;
  pic.iStride[2] = 32;
  pic.pData[0] = yuv.data();
  pic.pData[1] = yuv.data() + 64 * 64;
  pic.pData[2] = yuv.data() + 64 * 64 + 32 * 32;

  SFrameBSInfo bs_info{};
  if (h264_enc->EncodeFrame(&pic, &bs_info) != cmResultSuccess) return 3;
  if (bs_info.iFrameSizeInBytes <= 0 || bs_info.iLayerNum < 1) return 4;

  ISVCDecoder* h264_dec = nullptr;
  if (WelsCreateDecoder(&h264_dec) != 0 || !h264_dec) return 5;
  SDecodingParam dec_param{};
  if (h264_dec->Initialize(&dec_param) != cmResultSuccess) return 6;
  unsigned char* dst_planes[3] = {nullptr, nullptr, nullptr};
  SBufferInfo dst_info{};
  if (h264_dec->DecodeFrameNoDelay(bs_info.sLayerInfo[0].pBsBuf,
                                   bs_info.iFrameSizeInBytes, dst_planes,
                                   &dst_info) != dsErrorFree) {
    return 7;
  }
  if (dst_info.iBufferStatus != 1 || !dst_planes[0]) return 8;
  h264_dec->Uninitialize();
  WelsDestroyDecoder(h264_dec);
  h264_enc->Uninitialize();
  WelsDestroySVCEncoder(h264_enc);

  // 2. Opus test
  int opus_err = 0;
  OpusEncoder* opus_enc =
      opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &opus_err);
  if (!opus_enc || opus_err != OPUS_OK) return 9;
  opus_encoder_ctl(opus_enc, OPUS_SET_BITRATE(64000));
  std::vector<opus_int16> pcm_in(960 * 2, 1234);
  unsigned char opus_pkt[256];
  opus_int32 pkt_len =
      opus_encode(opus_enc, pcm_in.data(), 960, opus_pkt, sizeof(opus_pkt));
  if (pkt_len <= 0) return 10;

  OpusDecoder* opus_dec = opus_decoder_create(48000, 2, &opus_err);
  if (!opus_dec || opus_err != OPUS_OK) return 11;
  std::vector<opus_int16> pcm_out(960 * 2, 0);
  int decoded_samples =
      opus_decode(opus_dec, opus_pkt, pkt_len, pcm_out.data(), 960, 0);
  if (decoded_samples != 960 || pcm_out[0] != 1234) return 12;
  opus_decoder_destroy(opus_dec);
  opus_encoder_destroy(opus_enc);

  // 3. VPX test
  vpx_codec_enc_cfg_t vpx_cfg{};
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &vpx_cfg, 0) !=
      VPX_CODEC_OK) {
    return 13;
  }
  vpx_cfg.g_w = 64;
  vpx_cfg.g_h = 64;
  vpx_codec_ctx_t vpx_enc{};
  if (vpx_codec_enc_init(&vpx_enc, vpx_codec_vp8_cx(), &vpx_cfg, 0) !=
      VPX_CODEC_OK) {
    return 14;
  }
  vpx_image_t* raw_img = vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, 64, 64, 1);
  if (!raw_img) return 15;
  if (vpx_codec_encode(&vpx_enc, raw_img, 0, 1, 0, VPX_DL_REALTIME) !=
      VPX_CODEC_OK) {
    return 16;
  }
  vpx_codec_iter_t iter = nullptr;
  const vpx_codec_cx_pkt_t* cx_pkt = vpx_codec_get_cx_data(&vpx_enc, &iter);
  if (!cx_pkt || cx_pkt->kind != VPX_CODEC_CX_FRAME_PKT) return 17;

  vpx_codec_ctx_t vpx_dec{};
  if (vpx_codec_dec_init(&vpx_dec, vpx_codec_vp8_dx(), nullptr, 0) !=
      VPX_CODEC_OK) {
    return 18;
  }
  if (vpx_codec_decode(&vpx_dec,
                       static_cast<const uint8_t*>(cx_pkt->data.frame.buf),
                       static_cast<unsigned int>(cx_pkt->data.frame.sz),
                       nullptr, 0) != VPX_CODEC_OK) {
    return 19;
  }
  vpx_codec_iter_t dec_iter = nullptr;
  vpx_image_t* dec_img = vpx_codec_get_frame(&vpx_dec, &dec_iter);
  if (!dec_img || dec_img->d_w != 64 || dec_img->d_h != 64) return 20;

  vpx_img_free(raw_img);
  vpx_codec_destroy(&vpx_dec);
  vpx_codec_destroy(&vpx_enc);
  return 0;
}

int cf_test_android_base_and_absl(void) {
  // 1. tl::expected
  tl::expected<int, std::string> ok_val = 42;
  if (!ok_val.has_value() || *ok_val != 42) return 1;
  tl::expected<int, std::string> err_val = tl::make_unexpected("boom");
  if (err_val.has_value() || err_val.error() != "boom") return 2;

  // 2. fmt
  std::string formatted =
      fmt::format("Hello {} {}!", "Cuttlefish", 2026);
  if (formatted != "Hello Cuttlefish 2026!") return 3;
  std::vector<int> nums = {1, 2, 3};
  if (fmt::format("{}", fmt::join(nums, "-")) != "1-2-3") return 4;

  // 3. absl strings & cleanup
  std::vector<std::string> parts = absl::StrSplit("a,b,c", ',');
  if (parts.size() != 3 || absl::StrJoin(parts, ":") != "a:b:c") return 5;
  if (!absl::StartsWith("webrtc_stream", "webrtc")) return 6;
  bool cleaned = false;
  {
    auto cleanup = absl::MakeCleanup([&]() { cleaned = true; });
  }
  if (!cleaned) return 7;

  // 4. android-base strings, parseint, file, unique_fd
  auto joined = android::base::Join(std::vector<std::string>{"x", "y"}, "/");
  if (joined != "x/y") return 8;
  int parsed_int = 0;
  if (!android::base::ParseInt("12345", &parsed_int) || parsed_int != 12345) {
    return 9;
  }

  int sv[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 10;
  android::base::unique_fd fd0(sv[0]);
  android::base::unique_fd fd1(sv[1]);
  if (!android::base::WriteStringToFd("ping", fd0)) return 11;
  fd0.reset();
  std::string recv_str;
  if (!android::base::ReadFdToString(fd1, &recv_str) || recv_str != "ping") {
    return 12;
  }

  // 5. fruit DI
  fruit::Injector<ITestGreeter> injector(GetTestGreeterComponent);
  ITestGreeter* greeter = injector.get<ITestGreeter*>();
  if (!greeter || greeter->Compute() != 77) return 13;

  // 6. crypto & crc32c & libxml
  uint8_t sha256_out[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>("abc"), 3, sha256_out);
  if (sha256_out[0] != 0xba || sha256_out[1] != 0x78) return 14;

  uint32_t crc = crc32c_value(reinterpret_cast<const uint8_t*>("123456789"), 9);
  if (crc != 0xE3069283u) return 15;

  const char* xml = "<config><flag name=\"cpus\">4</flag></config>";
  xmlDocPtr doc = xmlReadMemory(xml, static_cast<int>(std::strlen(xml)),
                                "noname.xml", nullptr, 0);
  if (!doc) return 16;
  xmlNodePtr root = xmlDocGetRootElement(doc);
  if (!root || !root->children) return 17;
  xmlChar* prop = xmlGetProp(root->children, BAD_CAST "name");
  if (!prop || std::strcmp(reinterpret_cast<char*>(prop), "cpus") != 0) {
    return 18;
  }
  xmlFree(prop);
  xmlFreeDoc(doc);

  // 7. libwebsockets_slim loopback server test
  android::base::TemporaryFile tf;
  android::base::WriteStringToFd("<html>ok</html>", tf.fd);
  std::string dir = android::base::Dirname(tf.path);
  std::string base = android::base::Basename(tf.path);

  lws_context_creation_info ctx_info{};
  ctx_info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
  lws_context* lws_ctx = lws_create_context(&ctx_info);
  if (!lws_ctx) return 19;

  lws_http_mount mount{};
  mount.mountpoint = "/";
  mount.origin = dir.c_str();
  mount.def = base.c_str();
  mount.origin_protocol = LWSMPRO_FILE;
  mount.mountpoint_len = 1;

  lws_context_creation_info vh_info{};
  vh_info.port = 0;
  vh_info.iface = "127.0.0.1";
  vh_info.mounts = &mount;
  lws_vhost* vh = lws_create_vhost(lws_ctx, &vh_info);
  if (!vh) return 20;
  int port = lws_get_vhost_listen_port(vh);
  if (port <= 0) return 21;
  lws_context_destroy(lws_ctx);

  return 0;
}

}  // extern "C"
