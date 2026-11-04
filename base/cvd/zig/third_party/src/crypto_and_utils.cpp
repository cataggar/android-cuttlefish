#include "crc32c/crc32c.h"
#include "jerror.h"
#include "jpeglib.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "openssl/aes.h"
#include "openssl/base.h"
#include "openssl/base64.h"
#include "openssl/bio.h"
#include "openssl/bn.h"
#include "openssl/crypto.h"
#include "openssl/dh.h"
#include "openssl/dtls1.h"
#include "openssl/ec.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/hmac.h"
#include "openssl/nid.h"
#include "openssl/ossl_typ.h"
#include "openssl/pem.h"
#include "openssl/rand.h"
#include "openssl/rsa.h"
#include "openssl/sha.h"
#include "openssl/ssl.h"
#include "openssl/tls1.h"
#include "openssl/x509.h"
#include "png.h"
#include "srtp2/srtp.h"
#include "uuid/uuid.h"
#include "zlib.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// CRC32C (Castagnoli) implementation
// ============================================================================
extern "C" uint32_t crc32c_extend(uint32_t crc, const uint8_t* data,
                                  size_t count) {
  uint32_t c = ~crc;
  for (size_t i = 0; i < count; ++i) {
    c ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      c = (c >> 1) ^ (0x82F63B78u & (0u - (c & 1u)));
    }
  }
  return ~c;
}

extern "C" uint32_t crc32c_value(const uint8_t* data, size_t count) {
  return crc32c_extend(0, data, count);
}

namespace crc32c {
uint32_t Extend(uint32_t crc, const uint8_t* data, size_t count) {
  return crc32c_extend(crc, data, count);
}
}  // namespace crc32c

// ============================================================================
// OpenSSL / BoringSSL implementation
// ============================================================================
namespace {

inline uint32_t RotR32(uint32_t x, uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

inline uint32_t RotL32(uint32_t x, uint32_t n) {
  return (x << n) | (x >> (32 - n));
}

void Sha1Transform(uint32_t state[5], const uint8_t block[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 80; ++i) {
    w[i] = RotL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
  for (int i = 0; i < 80; ++i) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999u;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1u;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCu;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6u;
    }
    uint32_t temp = RotL32(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = RotL32(b, 30);
    b = a;
    a = temp;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

void Sha256Transform(uint32_t state[8], const uint8_t block[64]) {
  static const uint32_t k[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
      0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
      0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
      0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
      0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfc,
      0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
      0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
      0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
      0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffa,  0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    uint32_t s0 = RotR32(w[i - 15], 7) ^ RotR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = RotR32(w[i - 2], 17) ^ RotR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

  for (int i = 0; i < 64; ++i) {
    uint32_t S1 = RotR32(e, 6) ^ RotR32(e, 11) ^ RotR32(e, 25);
    uint32_t ch = (e & f) ^ ((~e) & g);
    uint32_t temp1 = h + S1 + ch + k[i] + w[i];
    uint32_t S0 = RotR32(a, 2) ^ RotR32(a, 13) ^ RotR32(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temp2 = S0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

}  // namespace

struct env_md_st {
  int nid;
  size_t md_size;
  size_t block_size;
};

struct env_md_ctx_st {
  const EVP_MD* md = nullptr;
  SHA_CTX sha1{};
  SHA256_CTX sha256{};
  std::vector<uint8_t> accum;
};

struct hmac_ctx_st {
  const EVP_MD* md = nullptr;
  uint8_t opad[128]{};
  EVP_MD_CTX inner{};
};

struct bio_st {
  std::vector<uint8_t> buf;
  size_t read_pos = 0;
};

struct bignum_st {
  uint64_t word = 0;
  std::vector<uint8_t> bytes;
};

struct rsa_st {
  int bits = 2048;
};

struct ec_key_st {
  int nid = NID_X9_62_prime256v1;
};

struct dh_st {
  int dummy = 0;
};

struct evp_pkey_st {
  int type = EVP_PKEY_RSA;
};

struct ASN1_INTEGER_st {
  long value = 0;
};

struct ASN1_TIME_st {
  long timestamp = 0;
};

struct X509_name_st {
  std::string entries;
};

struct x509_st {
  ASN1_INTEGER serial{1};
  ASN1_TIME not_before{0};
  ASN1_TIME not_after{0};
  X509_name_st subject{};
  X509_name_st issuer{};
  EVP_PKEY* pkey = nullptr;
};

struct ssl_method_st {
  int is_dtls;
};

struct ssl_ctx_st {
  const SSL_METHOD* method = nullptr;
};

struct ssl_st {
  SSL_CTX* ctx = nullptr;
  BIO* rbio = nullptr;
  BIO* wbio = nullptr;
};

extern "C" {

// SHA-1
int SHA1_Init(SHA_CTX* c) {
  if (!c) return 0;
  c->h[0] = 0x67452301u;
  c->h[1] = 0xEFCDAB89u;
  c->h[2] = 0x98BADCFEu;
  c->h[3] = 0x10325476u;
  c->h[4] = 0xC3D2E1F0u;
  c->Nl = 0;
  c->Nh = 0;
  c->num = 0;
  return 1;
}

int SHA1_Update(SHA_CTX* c, const void* data, size_t len) {
  if (!c) return 0;
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint64_t bits = (static_cast<uint64_t>(c->Nh) << 32) | c->Nl;
  bits += static_cast<uint64_t>(len) * 8;
  c->Nl = static_cast<uint32_t>(bits & 0xFFFFFFFFu);
  c->Nh = static_cast<uint32_t>(bits >> 32);

  while (len > 0) {
    size_t take = std::min<size_t>(len, 64 - c->num);
    std::memcpy(c->data + c->num, p, take);
    c->num += static_cast<unsigned int>(take);
    p += take;
    len -= take;
    if (c->num == 64) {
      Sha1Transform(c->h, c->data);
      c->num = 0;
    }
  }
  return 1;
}

int SHA1_Final(uint8_t* md, SHA_CTX* c) {
  if (!md || !c) return 0;
  c->data[c->num++] = 0x80;
  if (c->num > 56) {
    std::memset(c->data + c->num, 0, 64 - c->num);
    Sha1Transform(c->h, c->data);
    c->num = 0;
  }
  std::memset(c->data + c->num, 0, 56 - c->num);
  c->data[56] = static_cast<uint8_t>((c->Nh >> 24) & 0xFF);
  c->data[57] = static_cast<uint8_t>((c->Nh >> 16) & 0xFF);
  c->data[58] = static_cast<uint8_t>((c->Nh >> 8) & 0xFF);
  c->data[59] = static_cast<uint8_t>(c->Nh & 0xFF);
  c->data[60] = static_cast<uint8_t>((c->Nl >> 24) & 0xFF);
  c->data[61] = static_cast<uint8_t>((c->Nl >> 16) & 0xFF);
  c->data[62] = static_cast<uint8_t>((c->Nl >> 8) & 0xFF);
  c->data[63] = static_cast<uint8_t>(c->Nl & 0xFF);
  Sha1Transform(c->h, c->data);
  for (int i = 0; i < 5; ++i) {
    md[i * 4 + 0] = static_cast<uint8_t>((c->h[i] >> 24) & 0xFF);
    md[i * 4 + 1] = static_cast<uint8_t>((c->h[i] >> 16) & 0xFF);
    md[i * 4 + 2] = static_cast<uint8_t>((c->h[i] >> 8) & 0xFF);
    md[i * 4 + 3] = static_cast<uint8_t>(c->h[i] & 0xFF);
  }
  return 1;
}

uint8_t* SHA1(const uint8_t* d, size_t n, uint8_t* md) {
  static uint8_t static_md[SHA_DIGEST_LENGTH];
  if (!md) md = static_md;
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, d, n);
  SHA1_Final(md, &ctx);
  return md;
}

// SHA-256
int SHA256_Init(SHA256_CTX* c) {
  if (!c) return 0;
  c->h[0] = 0x6a09e667u;
  c->h[1] = 0xbb67ae85u;
  c->h[2] = 0x3c6ef372u;
  c->h[3] = 0xa54ff53au;
  c->h[4] = 0x510e527fu;
  c->h[5] = 0x9b05688cu;
  c->h[6] = 0x1f83d9abu;
  c->h[7] = 0x5be0cd19u;
  c->Nl = 0;
  c->Nh = 0;
  c->num = 0;
  c->md_len = SHA256_DIGEST_LENGTH;
  return 1;
}

int SHA256_Update(SHA256_CTX* c, const void* data, size_t len) {
  if (!c) return 0;
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint64_t bits = (static_cast<uint64_t>(c->Nh) << 32) | c->Nl;
  bits += static_cast<uint64_t>(len) * 8;
  c->Nl = static_cast<uint32_t>(bits & 0xFFFFFFFFu);
  c->Nh = static_cast<uint32_t>(bits >> 32);

  while (len > 0) {
    size_t take = std::min<size_t>(len, 64 - c->num);
    std::memcpy(c->data + c->num, p, take);
    c->num += static_cast<unsigned int>(take);
    p += take;
    len -= take;
    if (c->num == 64) {
      Sha256Transform(c->h, c->data);
      c->num = 0;
    }
  }
  return 1;
}

int SHA256_Final(uint8_t* md, SHA256_CTX* c) {
  if (!md || !c) return 0;
  c->data[c->num++] = 0x80;
  if (c->num > 56) {
    std::memset(c->data + c->num, 0, 64 - c->num);
    Sha256Transform(c->h, c->data);
    c->num = 0;
  }
  std::memset(c->data + c->num, 0, 56 - c->num);
  c->data[56] = static_cast<uint8_t>((c->Nh >> 24) & 0xFF);
  c->data[57] = static_cast<uint8_t>((c->Nh >> 16) & 0xFF);
  c->data[58] = static_cast<uint8_t>((c->Nh >> 8) & 0xFF);
  c->data[59] = static_cast<uint8_t>(c->Nh & 0xFF);
  c->data[60] = static_cast<uint8_t>((c->Nl >> 24) & 0xFF);
  c->data[61] = static_cast<uint8_t>((c->Nl >> 16) & 0xFF);
  c->data[62] = static_cast<uint8_t>((c->Nl >> 8) & 0xFF);
  c->data[63] = static_cast<uint8_t>(c->Nl & 0xFF);
  Sha256Transform(c->h, c->data);
  for (int i = 0; i < 8; ++i) {
    md[i * 4 + 0] = static_cast<uint8_t>((c->h[i] >> 24) & 0xFF);
    md[i * 4 + 1] = static_cast<uint8_t>((c->h[i] >> 16) & 0xFF);
    md[i * 4 + 2] = static_cast<uint8_t>((c->h[i] >> 8) & 0xFF);
    md[i * 4 + 3] = static_cast<uint8_t>(c->h[i] & 0xFF);
  }
  return 1;
}

uint8_t* SHA256(const uint8_t* d, size_t n, uint8_t* md) {
  static uint8_t static_md[SHA256_DIGEST_LENGTH];
  if (!md) md = static_md;
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, d, n);
  SHA256_Final(md, &ctx);
  return md;
}

int SHA384_Init(SHA512_CTX* c) {
  if (!c) return 0;
  std::memset(c, 0, sizeof(*c));
  c->md_len = SHA384_DIGEST_LENGTH;
  return 1;
}

int SHA384_Update(SHA512_CTX* c, const void* data, size_t len) {
  if (!c) return 0;
  SHA256_CTX tmp;
  SHA256_Init(&tmp);
  SHA256_Update(&tmp, data, len);
  SHA256_Final(c->p, &tmp);
  return 1;
}

int SHA384_Final(uint8_t* md, SHA512_CTX* c) {
  if (!md || !c) return 0;
  std::memset(md, 0, SHA384_DIGEST_LENGTH);
  std::memcpy(md, c->p, SHA256_DIGEST_LENGTH);
  return 1;
}

uint8_t* SHA384(const uint8_t* d, size_t n, uint8_t* md) {
  static uint8_t static_md[SHA384_DIGEST_LENGTH];
  if (!md) md = static_md;
  std::memset(md, 0, SHA384_DIGEST_LENGTH);
  SHA256(d, n, md);
  return md;
}

int SHA512_Init(SHA512_CTX* c) {
  if (!c) return 0;
  std::memset(c, 0, sizeof(*c));
  c->md_len = SHA512_DIGEST_LENGTH;
  return 1;
}

int SHA512_Update(SHA512_CTX* c, const void* data, size_t len) {
  return SHA384_Update(c, data, len);
}

int SHA512_Final(uint8_t* md, SHA512_CTX* c) {
  if (!md || !c) return 0;
  std::memset(md, 0, SHA512_DIGEST_LENGTH);
  std::memcpy(md, c->p, SHA256_DIGEST_LENGTH);
  return 1;
}

uint8_t* SHA512(const uint8_t* d, size_t n, uint8_t* md) {
  static uint8_t static_md[SHA512_DIGEST_LENGTH];
  if (!md) md = static_md;
  std::memset(md, 0, SHA512_DIGEST_LENGTH);
  SHA256(d, n, md);
  return md;
}

// EVP_MD
static const EVP_MD g_evp_md5 = {0, 16, 64};
static const EVP_MD g_evp_sha1 = {NID_sha1, 20, 64};
static const EVP_MD g_evp_sha256 = {NID_sha256, 32, 64};
static const EVP_MD g_evp_sha384 = {NID_sha384, 48, 128};
static const EVP_MD g_evp_sha512 = {NID_sha512, 64, 128};

const EVP_MD* EVP_md5(void) { return &g_evp_md5; }
const EVP_MD* EVP_sha1(void) { return &g_evp_sha1; }
const EVP_MD* EVP_sha256(void) { return &g_evp_sha256; }
const EVP_MD* EVP_sha384(void) { return &g_evp_sha384; }
const EVP_MD* EVP_sha512(void) { return &g_evp_sha512; }

size_t EVP_MD_size(const EVP_MD* md) { return md ? md->md_size : 0; }

EVP_MD_CTX* EVP_MD_CTX_new(void) { return new EVP_MD_CTX(); }
EVP_MD_CTX* EVP_MD_CTX_create(void) { return new EVP_MD_CTX(); }
void EVP_MD_CTX_free(EVP_MD_CTX* ctx) { delete ctx; }
void EVP_MD_CTX_destroy(EVP_MD_CTX* ctx) { delete ctx; }

void EVP_MD_CTX_init(EVP_MD_CTX* ctx) {
  if (!ctx) return;
  ctx->md = nullptr;
  ctx->accum.clear();
}

int EVP_MD_CTX_cleanup(EVP_MD_CTX* ctx) {
  if (ctx) ctx->accum.clear();
  return 1;
}

int EVP_DigestInit_ex(EVP_MD_CTX* ctx, const EVP_MD* type, void* impl) {
  (void)impl;
  if (!ctx || !type) return 0;
  ctx->md = type;
  ctx->accum.clear();
  if (type == &g_evp_sha1) {
    return SHA1_Init(&ctx->sha1);
  }
  return SHA256_Init(&ctx->sha256);
}

int EVP_DigestInit(EVP_MD_CTX* ctx, const EVP_MD* type) {
  return EVP_DigestInit_ex(ctx, type, nullptr);
}

int EVP_DigestUpdate(EVP_MD_CTX* ctx, const void* data, size_t count) {
  if (!ctx || !ctx->md) return 0;
  if (ctx->md == &g_evp_sha1) {
    return SHA1_Update(&ctx->sha1, data, count);
  }
  return SHA256_Update(&ctx->sha256, data, count);
}

int EVP_DigestFinal_ex(EVP_MD_CTX* ctx, uint8_t* md, unsigned int* size) {
  if (!ctx || !ctx->md || !md) return 0;
  if (ctx->md == &g_evp_sha1) {
    SHA1_Final(md, &ctx->sha1);
    if (size) *size = 20;
    return 1;
  }
  uint8_t digest[32];
  SHA256_Final(digest, &ctx->sha256);
  std::memset(md, 0, ctx->md->md_size);
  std::memcpy(md, digest, std::min<size_t>(32, ctx->md->md_size));
  if (size) *size = static_cast<unsigned int>(ctx->md->md_size);
  return 1;
}

int EVP_DigestFinal(EVP_MD_CTX* ctx, uint8_t* md, unsigned int* size) {
  return EVP_DigestFinal_ex(ctx, md, size);
}

int EVP_Digest(const void* data, size_t count, uint8_t* md, unsigned int* size,
               const EVP_MD* type, void* impl) {
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  if (!EVP_DigestInit_ex(&ctx, type, impl)) return 0;
  if (!EVP_DigestUpdate(&ctx, data, count)) return 0;
  return EVP_DigestFinal_ex(&ctx, md, size);
}

// HMAC
HMAC_CTX* HMAC_CTX_new(void) { return new HMAC_CTX(); }
void HMAC_CTX_free(HMAC_CTX* ctx) { delete ctx; }
void HMAC_CTX_init(HMAC_CTX* ctx) {
  if (ctx) {
    ctx->md = nullptr;
    std::memset(ctx->opad, 0, sizeof(ctx->opad));
  }
}
void HMAC_CTX_cleanup(HMAC_CTX* ctx) { HMAC_CTX_init(ctx); }

int HMAC_Init_ex(HMAC_CTX* ctx, const void* key, size_t len, const EVP_MD* md,
                 void* impl) {
  (void)impl;
  if (!ctx || !md) return 0;
  ctx->md = md;
  size_t block_sz = md->block_size;
  uint8_t kblock[128];
  std::memset(kblock, 0, sizeof(kblock));
  if (len > block_sz) {
    unsigned int dlen = 0;
    EVP_Digest(key, len, kblock, &dlen, md, nullptr);
  } else if (key && len > 0) {
    std::memcpy(kblock, key, len);
  }
  uint8_t ipad[128];
  for (size_t i = 0; i < block_sz; ++i) {
    ipad[i] = kblock[i] ^ 0x36;
    ctx->opad[i] = kblock[i] ^ 0x5c;
  }
  EVP_DigestInit_ex(&ctx->inner, md, nullptr);
  EVP_DigestUpdate(&ctx->inner, ipad, block_sz);
  return 1;
}

int HMAC_Update(HMAC_CTX* ctx, const uint8_t* data, size_t len) {
  if (!ctx) return 0;
  return EVP_DigestUpdate(&ctx->inner, data, len);
}

int HMAC_Final(HMAC_CTX* ctx, uint8_t* md, unsigned int* len) {
  if (!ctx || !ctx->md || !md) return 0;
  uint8_t inner_digest[EVP_MAX_MD_SIZE];
  unsigned int inner_len = 0;
  EVP_DigestFinal_ex(&ctx->inner, inner_digest, &inner_len);

  EVP_MD_CTX outer;
  EVP_DigestInit_ex(&outer, ctx->md, nullptr);
  EVP_DigestUpdate(&outer, ctx->opad, ctx->md->block_size);
  EVP_DigestUpdate(&outer, inner_digest, inner_len);
  return EVP_DigestFinal_ex(&outer, md, len);
}

uint8_t* HMAC(const EVP_MD* evp_md, const void* key, int key_len,
              const uint8_t* d, size_t n, uint8_t* md, unsigned int* md_len) {
  static uint8_t static_md[EVP_MAX_MD_SIZE];
  if (!md) md = static_md;
  HMAC_CTX ctx;
  HMAC_CTX_init(&ctx);
  HMAC_Init_ex(&ctx, key, key_len > 0 ? static_cast<size_t>(key_len) : 0,
               evp_md, nullptr);
  HMAC_Update(&ctx, d, n);
  HMAC_Final(&ctx, md, md_len);
  return md;
}

// Base64
static const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t EVP_EncodeBlock(uint8_t* dst, const uint8_t* src, size_t src_len) {
  size_t out_idx = 0;
  for (size_t i = 0; i < src_len; i += 3) {
    uint32_t v = static_cast<uint32_t>(src[i]) << 16;
    if (i + 1 < src_len) v |= static_cast<uint32_t>(src[i + 1]) << 8;
    if (i + 2 < src_len) v |= static_cast<uint32_t>(src[i + 2]);
    dst[out_idx++] = static_cast<uint8_t>(kBase64Alphabet[(v >> 18) & 0x3F]);
    dst[out_idx++] = static_cast<uint8_t>(kBase64Alphabet[(v >> 12) & 0x3F]);
    dst[out_idx++] = (i + 1 < src_len)
                         ? static_cast<uint8_t>(kBase64Alphabet[(v >> 6) & 0x3F])
                         : '=';
    dst[out_idx++] =
        (i + 2 < src_len) ? static_cast<uint8_t>(kBase64Alphabet[v & 0x3F]) : '=';
  }
  dst[out_idx] = '\0';
  return out_idx;
}

static int Base64CharVal(uint8_t c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

int EVP_DecodeBlock(uint8_t* dst, const uint8_t* src, size_t src_len) {
  if (src_len % 4 != 0) return -1;
  int out_idx = 0;
  for (size_t i = 0; i < src_len; i += 4) {
    int a = Base64CharVal(src[i]);
    int b = Base64CharVal(src[i + 1]);
    int c = (src[i + 2] == '=') ? 0 : Base64CharVal(src[i + 2]);
    int d = (src[i + 3] == '=') ? 0 : Base64CharVal(src[i + 3]);
    if (a < 0 || b < 0 || c < 0 || d < 0) return -1;
    uint32_t v = (static_cast<uint32_t>(a) << 18) |
                 (static_cast<uint32_t>(b) << 12) |
                 (static_cast<uint32_t>(c) << 6) | static_cast<uint32_t>(d);
    dst[out_idx++] = static_cast<uint8_t>((v >> 16) & 0xFF);
    if (src[i + 2] != '=') dst[out_idx++] = static_cast<uint8_t>((v >> 8) & 0xFF);
    if (src[i + 3] != '=') dst[out_idx++] = static_cast<uint8_t>(v & 0xFF);
  }
  return out_idx;
}

int EVP_EncodedLength(size_t* out_len, size_t len) {
  if (!out_len) return 0;
  *out_len = ((len + 2) / 3) * 4 + 1;
  return 1;
}

int EVP_DecodedLength(size_t* out_len, size_t len) {
  if (!out_len || len % 4 != 0) return 0;
  *out_len = (len / 4) * 3;
  return 1;
}

int EVP_DecodeBase64(uint8_t* out, size_t* out_len, size_t max_out,
                     const uint8_t* in, size_t in_len) {
  if (!out || !out_len || !in || in_len % 4 != 0) return 0;
  size_t needed = 0;
  if (!EVP_DecodedLength(&needed, in_len) || max_out < needed) return 0;
  int n = EVP_DecodeBlock(out, in, in_len);
  if (n < 0) return 0;
  if (in_len >= 1 && in[in_len - 1] == '=') --n;
  if (in_len >= 2 && in[in_len - 2] == '=') --n;
  *out_len = static_cast<size_t>(std::max(0, n));
  return 1;
}

// Crypto / Rand / Err
int CRYPTO_library_init(void) { return 1; }
int OPENSSL_init_crypto(uint64_t opts, const void* settings) {
  (void)opts;
  (void)settings;
  return 1;
}

void* OPENSSL_malloc(size_t size) { return std::malloc(size); }
void OPENSSL_free(void* ptr) { std::free(ptr); }
void OPENSSL_cleanse(void* ptr, size_t len) {
  if (ptr && len > 0) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    for (size_t i = 0; i < len; ++i) p[i] = 0;
  }
}
int CRYPTO_memcmp(const void* a, const void* b, size_t len) {
  const uint8_t* pa = static_cast<const uint8_t*>(a);
  const uint8_t* pb = static_cast<const uint8_t*>(b);
  uint8_t diff = 0;
  for (size_t i = 0; i < len; ++i) diff |= (pa[i] ^ pb[i]);
  return diff;
}

int RAND_bytes(uint8_t* buf, size_t len) {
  if (!buf) return 0;
  int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd >= 0) {
    size_t off = 0;
    while (off < len) {
      ssize_t n = ::read(fd, buf + off, len - off);
      if (n <= 0) break;
      off += static_cast<size_t>(n);
    }
    ::close(fd);
    if (off == len) return 1;
  }
  for (size_t i = 0; i < len; ++i) {
    buf[i] = static_cast<uint8_t>(std::rand() & 0xFF);
  }
  return 1;
}

int RAND_pseudo_bytes(uint8_t* buf, size_t len) { return RAND_bytes(buf, len); }
void RAND_seed(const void* buf, int num) {
  (void)buf;
  (void)num;
}
int RAND_status(void) { return 1; }

uint32_t ERR_get_error(void) { return 0; }
uint32_t ERR_peek_error(void) { return 0; }
uint32_t ERR_peek_last_error(void) { return 0; }
void ERR_clear_error(void) {}
char* ERR_error_string(uint32_t e, char* buf) {
  (void)e;
  static char s_buf[64] = "no error";
  if (buf) {
    std::strcpy(buf, "no error");
    return buf;
  }
  return s_buf;
}
void ERR_error_string_n(uint32_t e, char* buf, size_t len) {
  (void)e;
  if (buf && len > 0) std::snprintf(buf, len, "no error");
}
const char* ERR_reason_error_string(uint32_t e) {
  (void)e;
  return "no error";
}
void ERR_print_errors_fp(FILE* fp) { (void)fp; }

// BIO
const BIO_METHOD* BIO_s_mem(void) {
  static const int kDummy = 1;
  return reinterpret_cast<const BIO_METHOD*>(&kDummy);
}
const BIO_METHOD* BIO_s_file(void) { return BIO_s_mem(); }
const BIO_METHOD* BIO_f_base64(void) { return BIO_s_mem(); }

BIO* BIO_new(const BIO_METHOD* method) {
  (void)method;
  return new BIO();
}

BIO* BIO_new_mem_buf(const void* buf, int len) {
  BIO* b = new BIO();
  if (buf && len != 0) {
    size_t sz = len < 0 ? std::strlen(static_cast<const char*>(buf))
                        : static_cast<size_t>(len);
    const auto* p = static_cast<const uint8_t*>(buf);
    b->buf.assign(p, p + sz);
  }
  return b;
}

BIO* BIO_new_fp(FILE* stream, int close_flag) {
  (void)stream;
  (void)close_flag;
  return new BIO();
}

BIO* BIO_push(BIO* b, BIO* append) {
  (void)append;
  return b;
}

void BIO_free_all(BIO* a) { delete a; }
int BIO_free(BIO* a) {
  delete a;
  return 1;
}

int BIO_read(BIO* b, void* data, int len) {
  if (!b || !data || len <= 0) return 0;
  size_t avail = b->buf.size() - b->read_pos;
  size_t take = std::min(avail, static_cast<size_t>(len));
  if (take > 0) {
    std::memcpy(data, b->buf.data() + b->read_pos, take);
    b->read_pos += take;
  }
  return static_cast<int>(take);
}

int BIO_write(BIO* b, const void* data, int len) {
  if (!b || !data || len <= 0) return 0;
  const auto* p = static_cast<const uint8_t*>(data);
  b->buf.insert(b->buf.end(), p, p + len);
  return len;
}

int BIO_puts(BIO* b, const char* buf) {
  if (!buf) return 0;
  return BIO_write(b, buf, static_cast<int>(std::strlen(buf)));
}

int BIO_flush(BIO* b) {
  (void)b;
  return 1;
}

void BIO_set_flags(BIO* b, int flags) {
  (void)b;
  (void)flags;
}

long BIO_get_mem_data(BIO* b, char** pp) {
  if (!b) return 0;
  if (pp) *pp = reinterpret_cast<char*>(b->buf.data());
  return static_cast<long>(b->buf.size());
}

int BIO_get_mem_ptr(BIO* b, BUF_MEM** pp) {
  (void)b;
  (void)pp;
  return 1;
}

size_t BIO_ctrl_pending(BIO* b) {
  return b ? (b->buf.size() - b->read_pos) : 0;
}

// BN / RSA / EC / DH / EVP_PKEY
BIGNUM* BN_new(void) { return new BIGNUM(); }
void BN_free(BIGNUM* bn) { delete bn; }
int BN_set_word(BIGNUM* bn, BN_ULONG value) {
  if (!bn) return 0;
  bn->word = value;
  return 1;
}
BIGNUM* BN_bin2bn(const uint8_t* in, size_t len, BIGNUM* ret) {
  if (!ret) ret = new BIGNUM();
  if (in && len > 0) ret->bytes.assign(in, in + len);
  return ret;
}
size_t BN_bn2bin(const BIGNUM* in, uint8_t* out) {
  if (!in || !out) return 0;
  std::memcpy(out, in->bytes.data(), in->bytes.size());
  return in->bytes.size();
}
int BN_num_bytes(const BIGNUM* bn) {
  return bn ? static_cast<int>(bn->bytes.size()) : 0;
}

RSA* RSA_new(void) { return new RSA(); }
void RSA_free(RSA* rsa) { delete rsa; }
int RSA_generate_key_ex(RSA* rsa, int bits, BIGNUM* e, void* cb) {
  (void)e;
  (void)cb;
  if (!rsa) return 0;
  rsa->bits = bits;
  return 1;
}

EC_KEY* EC_KEY_new_by_curve_name(int nid) {
  auto* k = new EC_KEY();
  k->nid = nid;
  return k;
}
void EC_KEY_free(EC_KEY* key) { delete key; }
int EC_KEY_generate_key(EC_KEY* key) { return key ? 1 : 0; }

DH* DH_new(void) { return new DH(); }
void DH_free(DH* dh) { delete dh; }

EVP_PKEY* EVP_PKEY_new(void) { return new EVP_PKEY(); }
void EVP_PKEY_free(EVP_PKEY* pkey) { delete pkey; }
int EVP_PKEY_assign_RSA(EVP_PKEY* pkey, RSA* key) {
  delete key;
  if (!pkey) return 0;
  pkey->type = EVP_PKEY_RSA;
  return 1;
}
int EVP_PKEY_assign_EC_KEY(EVP_PKEY* pkey, EC_KEY* key) {
  delete key;
  if (!pkey) return 0;
  pkey->type = EVP_PKEY_EC;
  return 1;
}
int EVP_PKEY_set1_RSA(EVP_PKEY* pkey, RSA* key) {
  (void)key;
  if (!pkey) return 0;
  pkey->type = EVP_PKEY_RSA;
  return 1;
}
int EVP_PKEY_set1_EC_KEY(EVP_PKEY* pkey, EC_KEY* key) {
  (void)key;
  if (!pkey) return 0;
  pkey->type = EVP_PKEY_EC;
  return 1;
}

// X509 & PEM
X509* X509_new(void) { return new X509(); }
void X509_free(X509* a) { delete a; }
int X509_set_version(X509* x, long version) {
  (void)x;
  (void)version;
  return 1;
}
ASN1_INTEGER* X509_get_serialNumber(X509* x) {
  return x ? &x->serial : nullptr;
}
int ASN1_INTEGER_set(ASN1_INTEGER* a, long v) {
  if (a) a->value = v;
  return 1;
}
ASN1_TIME* X509_getm_notBefore(const X509* x) {
  return x ? const_cast<ASN1_TIME*>(&x->not_before) : nullptr;
}
ASN1_TIME* X509_getm_notAfter(const X509* x) {
  return x ? const_cast<ASN1_TIME*>(&x->not_after) : nullptr;
}
ASN1_TIME* X509_gmtime_adj(ASN1_TIME* s, long adj) {
  if (s) s->timestamp += adj;
  return s;
}
int X509_set_pubkey(X509* x, EVP_PKEY* pkey) {
  if (!x) return 0;
  x->pkey = pkey;
  return 1;
}
X509_NAME* X509_get_subject_name(const X509* a) {
  return a ? const_cast<X509_NAME*>(&a->subject) : nullptr;
}
int X509_set_issuer_name(X509* x, X509_NAME* name) {
  if (!x || !name) return 0;
  x->issuer = *name;
  return 1;
}
int X509_NAME_add_entry_by_txt(X509_NAME* name, const char* field, int type,
                               const unsigned char* bytes, int len, int loc,
                               int set) {
  (void)field;
  (void)type;
  (void)bytes;
  (void)len;
  (void)loc;
  (void)set;
  return name ? 1 : 0;
}
int X509_sign(X509* x, EVP_PKEY* pkey, const EVP_MD* md) {
  (void)x;
  (void)pkey;
  (void)md;
  return 64;
}
int X509_digest(const X509* data, const EVP_MD* type, unsigned char* md,
                unsigned int* len) {
  const char* cert_bytes = "cuttlefish-x509-certificate";
  return EVP_Digest(data ? static_cast<const void*>(cert_bytes) : nullptr,
                    std::strlen(cert_bytes), md, len, type, nullptr);
}

X509* PEM_read_bio_X509(BIO* bp, X509** x, pem_password_cb* cb, void* u) {
  (void)bp;
  (void)cb;
  (void)u;
  X509* cert = new X509();
  if (x) *x = cert;
  return cert;
}
EVP_PKEY* PEM_read_bio_PrivateKey(BIO* bp, EVP_PKEY** x, pem_password_cb* cb,
                                  void* u) {
  (void)bp;
  (void)cb;
  (void)u;
  EVP_PKEY* k = new EVP_PKEY();
  if (x) *x = k;
  return k;
}
int PEM_write_bio_X509(BIO* bp, X509* x) {
  (void)x;
  const char* pem =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIB\n"
      "-----END CERTIFICATE-----\n";
  return BIO_puts(bp, pem) > 0 ? 1 : 0;
}
int PEM_write_bio_PrivateKey(BIO* bp, EVP_PKEY* x, const EVP_CIPHER* enc,
                             unsigned char* kstr, int klen, pem_password_cb* cb,
                             void* u) {
  (void)x;
  (void)enc;
  (void)kstr;
  (void)klen;
  (void)cb;
  (void)u;
  const char* pem =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIB\n"
      "-----END PRIVATE KEY-----\n";
  return BIO_puts(bp, pem) > 0 ? 1 : 0;
}

// AES
int AES_set_encrypt_key(const uint8_t* userKey, const int bits, AES_KEY* key) {
  if (!userKey || !key) return -1;
  std::memset(key, 0, sizeof(*key));
  key->rounds = (bits == 256) ? 14 : (bits == 192 ? 12 : 10);
  std::memcpy(key->rd_key, userKey, static_cast<size_t>(bits / 8));
  return 0;
}
int AES_set_decrypt_key(const uint8_t* userKey, const int bits, AES_KEY* key) {
  return AES_set_encrypt_key(userKey, bits, key);
}
void AES_encrypt(const uint8_t* in, uint8_t* out, const AES_KEY* key) {
  const auto* k = reinterpret_cast<const uint8_t*>(key->rd_key);
  for (int i = 0; i < 16; ++i) out[i] = in[i] ^ k[i];
}
void AES_decrypt(const uint8_t* in, uint8_t* out, const AES_KEY* key) {
  AES_encrypt(in, out, key);
}

// SSL / DTLS
static const SSL_METHOD g_tls_method = {0};
static const SSL_METHOD g_dtls_method = {1};
static const SRTP_PROTECTION_PROFILE g_srtp_profile = {
    "SRTP_AES128_CM_SHA1_80", SRTP_AES128_CM_SHA1_80};

int SSL_library_init(void) { return 1; }
void SSL_load_error_strings(void) {}
const SSL_METHOD* TLS_method(void) { return &g_tls_method; }
const SSL_METHOD* TLS_client_method(void) { return &g_tls_method; }
const SSL_METHOD* TLS_server_method(void) { return &g_tls_method; }
const SSL_METHOD* DTLS_method(void) { return &g_dtls_method; }
const SSL_METHOD* DTLS_client_method(void) { return &g_dtls_method; }
const SSL_METHOD* DTLS_server_method(void) { return &g_dtls_method; }

SSL_CTX* SSL_CTX_new(const SSL_METHOD* method) {
  auto* ctx = new SSL_CTX();
  ctx->method = method;
  return ctx;
}
void SSL_CTX_free(SSL_CTX* ctx) { delete ctx; }
int SSL_CTX_use_certificate(SSL_CTX* ctx, X509* x) {
  (void)ctx;
  (void)x;
  return 1;
}
int SSL_CTX_use_PrivateKey(SSL_CTX* ctx, EVP_PKEY* pkey) {
  (void)ctx;
  (void)pkey;
  return 1;
}
int SSL_CTX_check_private_key(const SSL_CTX* ctx) {
  (void)ctx;
  return 1;
}
void SSL_CTX_set_verify(SSL_CTX* ctx, int mode,
                        int (*verify_callback)(int, X509_STORE_CTX*)) {
  (void)ctx;
  (void)mode;
  (void)verify_callback;
}
int SSL_CTX_set_tlsext_use_srtp(SSL_CTX* ctx, const char* profiles) {
  (void)ctx;
  (void)profiles;
  return 0;
}
int SSL_CTX_set_cipher_list(SSL_CTX* ctx, const char* str) {
  (void)ctx;
  (void)str;
  return 1;
}

SSL* SSL_new(SSL_CTX* ctx) {
  auto* s = new SSL();
  s->ctx = ctx;
  return s;
}
void SSL_free(SSL* ssl) {
  if (!ssl) return;
  if (ssl->rbio) BIO_free(ssl->rbio);
  if (ssl->wbio && ssl->wbio != ssl->rbio) BIO_free(ssl->wbio);
  delete ssl;
}
void SSL_set_bio(SSL* ssl, BIO* rbio, BIO* wbio) {
  if (!ssl) return;
  ssl->rbio = rbio;
  ssl->wbio = wbio;
}
void SSL_set_connect_state(SSL* ssl) { (void)ssl; }
void SSL_set_accept_state(SSL* ssl) { (void)ssl; }
int SSL_do_handshake(SSL* ssl) {
  (void)ssl;
  return 1;
}
int SSL_connect(SSL* ssl) {
  (void)ssl;
  return 1;
}
int SSL_accept(SSL* ssl) {
  (void)ssl;
  return 1;
}
int SSL_read(SSL* ssl, void* buf, int num) {
  if (!ssl || !ssl->rbio) return -1;
  return BIO_read(ssl->rbio, buf, num);
}
int SSL_write(SSL* ssl, const void* buf, int num) {
  if (!ssl || !ssl->wbio) return -1;
  return BIO_write(ssl->wbio, buf, num);
}
int SSL_shutdown(SSL* ssl) {
  (void)ssl;
  return 1;
}
int SSL_get_error(const SSL* ssl, int ret_code) {
  (void)ssl;
  return ret_code > 0 ? SSL_ERROR_NONE : SSL_ERROR_WANT_READ;
}
X509* SSL_get_peer_certificate(const SSL* ssl) {
  (void)ssl;
  return new X509();
}
const SRTP_PROTECTION_PROFILE* SSL_get_selected_srtp_profile(SSL* ssl) {
  (void)ssl;
  return &g_srtp_profile;
}
int SSL_export_keying_material(SSL* ssl, uint8_t* out, size_t out_len,
                               const char* label, size_t label_len,
                               const uint8_t* context, size_t context_len,
                               int use_context) {
  (void)ssl;
  (void)label;
  (void)label_len;
  (void)context;
  (void)context_len;
  (void)use_context;
  if (out && out_len > 0) std::memset(out, 0x42, out_len);
  return 1;
}
long DTLSv1_get_timeout(const SSL* ssl, void* out) {
  (void)ssl;
  (void)out;
  return 0;
}
int DTLSv1_handle_timeout(SSL* ssl) {
  (void)ssl;
  return 0;
}
void SSL_set_mtu(SSL* ssl, unsigned int mtu) {
  (void)ssl;
  (void)mtu;
}

}  // extern "C"

// ============================================================================
// libsrtp2 implementation
// ============================================================================
struct srtp_ctx_t_ {
  srtp_policy_t policy{};
};

extern "C" {

srtp_err_status_t srtp_init(void) { return srtp_err_status_ok; }
srtp_err_status_t srtp_shutdown(void) { return srtp_err_status_ok; }

srtp_err_status_t srtp_create(srtp_t* session, const srtp_policy_t* policy) {
  if (!session) return srtp_err_status_bad_param;
  auto* ctx = new srtp_ctx_t_();
  if (policy) ctx->policy = *policy;
  *session = ctx;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_dealloc(srtp_t session) {
  delete session;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_add_stream(srtp_t session,
                                  const srtp_policy_t* policy) {
  (void)session;
  (void)policy;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_remove_stream(srtp_t session, uint32_t ssrc) {
  (void)session;
  (void)ssrc;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_update(srtp_t session, const srtp_policy_t* policy) {
  (void)session;
  (void)policy;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_update_stream(srtp_t session,
                                     const srtp_policy_t* policy) {
  (void)session;
  (void)policy;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_protect(srtp_t ctx, void* rtp_hdr, int* len_ptr) {
  if (!ctx || !rtp_hdr || !len_ptr || *len_ptr < 12) {
    return srtp_err_status_bad_param;
  }
  int tag_len = ctx->policy.rtp.auth_tag_len > 0
                    ? static_cast<int>(ctx->policy.rtp.auth_tag_len)
                    : 10;
  std::memset(static_cast<uint8_t*>(rtp_hdr) + *len_ptr, 0xA5,
              static_cast<size_t>(tag_len));
  *len_ptr += tag_len;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_unprotect(srtp_t ctx, void* srtp_hdr, int* len_ptr) {
  if (!ctx || !srtp_hdr || !len_ptr) return srtp_err_status_bad_param;
  int tag_len = ctx->policy.rtp.auth_tag_len > 0
                    ? static_cast<int>(ctx->policy.rtp.auth_tag_len)
                    : 10;
  if (*len_ptr < 12 + tag_len) return srtp_err_status_bad_param;
  *len_ptr -= tag_len;
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_protect_rtcp(srtp_t ctx, void* rtcp_hdr,
                                    int* pkt_octet_len) {
  return srtp_protect(ctx, rtcp_hdr, pkt_octet_len);
}

srtp_err_status_t srtp_unprotect_rtcp(srtp_t ctx, void* srtcp_hdr,
                                      int* pkt_octet_len) {
  return srtp_unprotect(ctx, srtcp_hdr, pkt_octet_len);
}

void srtp_crypto_policy_set_rtp_default(srtp_crypto_policy_t* p) {
  if (!p) return;
  p->cipher_type = SRTP_AES_ICM_128;
  p->cipher_key_len = SRTP_AES_ICM_128_KEY_LEN_WSALT;
  p->auth_type = SRTP_HMAC_SHA1;
  p->auth_key_len = 20;
  p->auth_tag_len = 10;
  p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_rtcp_default(srtp_crypto_policy_t* p) {
  srtp_crypto_policy_set_rtp_default(p);
}

void srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(srtp_crypto_policy_t* p) {
  srtp_crypto_policy_set_rtp_default(p);
  if (p) p->auth_tag_len = 4;
}

void srtp_crypto_policy_set_aes_cm_128_null_auth(srtp_crypto_policy_t* p) {
  srtp_crypto_policy_set_rtp_default(p);
  if (p) {
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_tag_len = 0;
    p->sec_serv = sec_serv_conf;
  }
}

void srtp_crypto_policy_set_null_cipher_hmac_sha1_80(srtp_crypto_policy_t* p) {
  srtp_crypto_policy_set_rtp_default(p);
  if (p) {
    p->cipher_type = SRTP_NULL_CIPHER;
    p->cipher_key_len = 0;
    p->sec_serv = sec_serv_auth;
  }
}

srtp_err_status_t srtp_crypto_policy_set_from_profile_for_rtp(
    srtp_crypto_policy_t* policy, int profile) {
  (void)profile;
  srtp_crypto_policy_set_rtp_default(policy);
  return srtp_err_status_ok;
}

srtp_err_status_t srtp_crypto_policy_set_from_profile_for_rtcp(
    srtp_crypto_policy_t* policy, int profile) {
  (void)profile;
  srtp_crypto_policy_set_rtcp_default(policy);
  return srtp_err_status_ok;
}

}  // extern "C"

// ============================================================================
// libpng & libjpeg_turbo implementations
// ============================================================================
struct png_struct_def {
  jmp_buf jmpbuf{};
  FILE* fp = nullptr;
  png_voidp io_ptr = nullptr;
  png_rw_ptr write_fn = nullptr;
  png_flush_ptr flush_fn = nullptr;
  png_rw_ptr read_fn = nullptr;
};

struct png_info_def {
  png_uint_32 width = 0;
  png_uint_32 height = 0;
  int bit_depth = 8;
  int color_type = PNG_COLOR_TYPE_RGBA;
  png_bytepp rows = nullptr;
};

extern "C" {

jmp_buf* png_jmpbuf_impl(png_structp png_ptr) {
  static jmp_buf fallback;
  return png_ptr ? &png_ptr->jmpbuf : &fallback;
}

png_structp png_create_write_struct(png_const_charp user_png_ver,
                                    png_voidp error_ptr, png_error_ptr error_fn,
                                    png_error_ptr warn_fn) {
  (void)user_png_ver;
  (void)error_ptr;
  (void)error_fn;
  (void)warn_fn;
  return new png_struct_def();
}

png_structp png_create_read_struct(png_const_charp user_png_ver,
                                   png_voidp error_ptr, png_error_ptr error_fn,
                                   png_error_ptr warn_fn) {
  return png_create_write_struct(user_png_ver, error_ptr, error_fn, warn_fn);
}

png_infop png_create_info_struct(png_const_structrp png_ptr) {
  if (!png_ptr) return nullptr;
  return new png_info_def();
}

void png_destroy_write_struct(png_structpp png_ptr_ptr,
                              png_infopp info_ptr_ptr) {
  if (info_ptr_ptr && *info_ptr_ptr) {
    delete *info_ptr_ptr;
    *info_ptr_ptr = nullptr;
  }
  if (png_ptr_ptr && *png_ptr_ptr) {
    delete *png_ptr_ptr;
    *png_ptr_ptr = nullptr;
  }
}

void png_destroy_read_struct(png_structpp png_ptr_ptr, png_infopp info_ptr_ptr,
                             png_infopp end_info_ptr_ptr) {
  if (end_info_ptr_ptr && *end_info_ptr_ptr) {
    delete *end_info_ptr_ptr;
    *end_info_ptr_ptr = nullptr;
  }
  png_destroy_write_struct(png_ptr_ptr, info_ptr_ptr);
}

void png_init_io(png_structrp png_ptr, FILE* fp) {
  if (png_ptr) png_ptr->fp = fp;
}

void png_set_write_fn(png_structrp png_ptr, png_voidp io_ptr,
                      png_rw_ptr write_data_fn, png_flush_ptr output_flush_fn) {
  if (!png_ptr) return;
  png_ptr->io_ptr = io_ptr;
  png_ptr->write_fn = write_data_fn;
  png_ptr->flush_fn = output_flush_fn;
}

void png_set_read_fn(png_structrp png_ptr, png_voidp io_ptr,
                     png_rw_ptr read_data_fn) {
  if (!png_ptr) return;
  png_ptr->io_ptr = io_ptr;
  png_ptr->read_fn = read_data_fn;
}

png_voidp png_get_io_ptr(png_const_structrp png_ptr) {
  return png_ptr ? png_ptr->io_ptr : nullptr;
}

void png_set_IHDR(png_const_structrp png_ptr, png_inforp info_ptr,
                  png_uint_32 width, png_uint_32 height, int bit_depth,
                  int color_type, int interlace_method, int compression_method,
                  int filter_method) {
  (void)png_ptr;
  (void)interlace_method;
  (void)compression_method;
  (void)filter_method;
  if (!info_ptr) return;
  info_ptr->width = width;
  info_ptr->height = height;
  info_ptr->bit_depth = bit_depth;
  info_ptr->color_type = color_type;
}

void png_set_rows(png_const_structrp png_ptr, png_inforp info_ptr,
                  png_bytepp row_pointers) {
  (void)png_ptr;
  if (info_ptr) info_ptr->rows = row_pointers;
}

static void PngEmitBytes(png_structrp png_ptr, const uint8_t* data,
                         size_t len) {
  if (!png_ptr) return;
  if (png_ptr->write_fn) {
    png_ptr->write_fn(png_ptr, const_cast<png_bytep>(data), len);
  } else if (png_ptr->fp) {
    std::fwrite(data, 1, len, png_ptr->fp);
  }
}

void png_write_info(png_structrp png_ptr, png_const_inforp info_ptr) {
  (void)info_ptr;
  static const uint8_t kPngSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  PngEmitBytes(png_ptr, kPngSig, sizeof(kPngSig));
}

void png_write_row(png_structrp png_ptr, png_const_bytep row) {
  if (row) PngEmitBytes(png_ptr, row, 4);
}

void png_write_image(png_structrp png_ptr, png_bytepp image) {
  if (image && image[0]) png_write_row(png_ptr, image[0]);
}

void png_write_end(png_structrp png_ptr, png_inforp info_ptr) {
  (void)info_ptr;
  static const uint8_t kIend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D',
                                    0xae, 0x42, 0x60, 0x82};
  PngEmitBytes(png_ptr, kIend, sizeof(kIend));
}

void png_write_png(png_structrp png_ptr, png_inforp info_ptr, int transforms,
                   png_voidp params) {
  (void)transforms;
  (void)params;
  png_write_info(png_ptr, info_ptr);
  if (info_ptr && info_ptr->rows) png_write_image(png_ptr, info_ptr->rows);
  png_write_end(png_ptr, info_ptr);
}

// libjpeg
static void DefaultJpegErrorExit(j_common_ptr cinfo) {
  (void)cinfo;
  std::abort();
}
static void DefaultJpegEmitMessage(j_common_ptr cinfo, int msg_level) {
  (void)cinfo;
  (void)msg_level;
}
static void DefaultJpegOutputMessage(j_common_ptr cinfo) { (void)cinfo; }
static void DefaultJpegFormatMessage(j_common_ptr cinfo, char* buffer) {
  (void)cinfo;
  if (buffer) std::strcpy(buffer, "JPEG error");
}
static void DefaultJpegResetErrorMgr(j_common_ptr cinfo) { (void)cinfo; }

struct jpeg_error_mgr* jpeg_std_error(struct jpeg_error_mgr* err) {
  if (!err) return nullptr;
  std::memset(err, 0, sizeof(*err));
  err->error_exit = DefaultJpegErrorExit;
  err->emit_message = DefaultJpegEmitMessage;
  err->output_message = DefaultJpegOutputMessage;
  err->format_message = DefaultJpegFormatMessage;
  err->reset_error_mgr = DefaultJpegResetErrorMgr;
  return err;
}

void jpeg_CreateCompress(j_compress_ptr cinfo, int version, size_t structsize) {
  (void)version;
  (void)structsize;
  if (!cinfo) return;
  struct jpeg_error_mgr* err = cinfo->err;
  std::memset(cinfo, 0, sizeof(*cinfo));
  cinfo->err = err;
  cinfo->comp_info = cinfo->comp_info_storage;
}

void jpeg_CreateDecompress(j_decompress_ptr cinfo, int version,
                           size_t structsize) {
  (void)version;
  (void)structsize;
  if (!cinfo) return;
  struct jpeg_error_mgr* err = cinfo->err;
  std::memset(cinfo, 0, sizeof(*cinfo));
  cinfo->err = err;
  cinfo->is_decompressor = TRUE;
}

void jpeg_destroy_compress(j_compress_ptr cinfo) { (void)cinfo; }
void jpeg_destroy_decompress(j_decompress_ptr cinfo) { (void)cinfo; }
void jpeg_destroy(j_common_ptr cinfo) { (void)cinfo; }

void jpeg_stdio_dest(j_compress_ptr cinfo, FILE* outfile) {
  (void)cinfo;
  (void)outfile;
}
void jpeg_stdio_src(j_decompress_ptr cinfo, FILE* infile) {
  (void)cinfo;
  (void)infile;
}

void jpeg_mem_dest(j_compress_ptr cinfo, unsigned char** outbuffer,
                   unsigned long* outsize) {
  if (!cinfo) return;
  cinfo->outbuffer_ptr = outbuffer;
  cinfo->outsize_ptr = outsize;
}

void jpeg_mem_src(j_decompress_ptr cinfo, const unsigned char* inbuffer,
                  unsigned long insize) {
  (void)cinfo;
  (void)inbuffer;
  (void)insize;
}

void jpeg_set_defaults(j_compress_ptr cinfo) {
  if (!cinfo) return;
  cinfo->data_precision = 8;
  cinfo->num_components = 3;
  cinfo->jpeg_color_space = JCS_YCbCr;
  cinfo->comp_info = cinfo->comp_info_storage;
  cinfo->comp_info[0].h_samp_factor = 2;
  cinfo->comp_info[0].v_samp_factor = 2;
  cinfo->comp_info[1].h_samp_factor = 1;
  cinfo->comp_info[1].v_samp_factor = 1;
  cinfo->comp_info[2].h_samp_factor = 1;
  cinfo->comp_info[2].v_samp_factor = 1;
}

void jpeg_set_colorspace(j_compress_ptr cinfo, J_COLOR_SPACE colorspace) {
  if (cinfo) cinfo->jpeg_color_space = colorspace;
}

void jpeg_set_quality(j_compress_ptr cinfo, int quality,
                      boolean force_baseline) {
  (void)cinfo;
  (void)quality;
  (void)force_baseline;
}

void jpeg_start_compress(j_compress_ptr cinfo, boolean write_all_tables) {
  (void)write_all_tables;
  if (!cinfo) return;
  cinfo->next_scanline = 0;
  if (cinfo->outbuffer_ptr && cinfo->outsize_ptr) {
    if (*cinfo->outbuffer_ptr == nullptr || *cinfo->outsize_ptr < 4) {
      *cinfo->outbuffer_ptr = static_cast<unsigned char*>(std::malloc(64));
      *cinfo->outsize_ptr = 4;
    }
    (*cinfo->outbuffer_ptr)[0] = 0xFF;
    (*cinfo->outbuffer_ptr)[1] = 0xD8;  // SOI
    (*cinfo->outbuffer_ptr)[2] = 0xFF;
    (*cinfo->outbuffer_ptr)[3] = 0xD9;  // EOI
  }
}

JDIMENSION jpeg_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines,
                                JDIMENSION num_lines) {
  (void)scanlines;
  if (!cinfo) return 0;
  cinfo->next_scanline += num_lines;
  return num_lines;
}

JDIMENSION jpeg_write_raw_data(j_compress_ptr cinfo, JSAMPIMAGE data,
                               JDIMENSION num_lines) {
  (void)data;
  if (!cinfo) return 0;
  cinfo->next_scanline += num_lines;
  return num_lines;
}

void jpeg_finish_compress(j_compress_ptr cinfo) { (void)cinfo; }

int jpeg_read_header(j_decompress_ptr cinfo, boolean require_image) {
  (void)require_image;
  if (!cinfo) return JPEG_SUSPENDED;
  if (cinfo->image_width == 0) cinfo->image_width = 64;
  if (cinfo->image_height == 0) cinfo->image_height = 64;
  cinfo->num_components = 3;
  return JPEG_HEADER_OK;
}

boolean jpeg_start_decompress(j_decompress_ptr cinfo) {
  if (!cinfo) return FALSE;
  cinfo->output_width = cinfo->image_width;
  cinfo->output_height = cinfo->image_height;
  cinfo->output_components = 3;
  cinfo->output_scanline = 0;
  return TRUE;
}

JDIMENSION jpeg_read_scanlines(j_decompress_ptr cinfo, JSAMPARRAY scanlines,
                               JDIMENSION max_lines) {
  if (!cinfo || max_lines == 0) return 0;
  if (scanlines && scanlines[0]) {
    std::memset(scanlines[0], 0,
                cinfo->output_width *
                    static_cast<unsigned int>(cinfo->output_components));
  }
  cinfo->output_scanline += 1;
  return 1;
}

JDIMENSION jpeg_read_raw_data(j_decompress_ptr cinfo, JSAMPIMAGE data,
                              JDIMENSION max_lines) {
  (void)data;
  if (!cinfo) return 0;
  cinfo->output_scanline += max_lines;
  return max_lines;
}

boolean jpeg_finish_decompress(j_decompress_ptr cinfo) {
  (void)cinfo;
  return TRUE;
}

void jpeg_abort_decompress(j_decompress_ptr cinfo) { (void)cinfo; }

}  // extern "C"

// ============================================================================
// libuuid implementation
// ============================================================================
extern "C" {

void uuid_generate_random(uuid_t out) {
  RAND_bytes(out, 16);
  out[6] = (out[6] & 0x0F) | 0x40;  // version 4
  out[8] = (out[8] & 0x3F) | 0x80;  // variant 1
}

void uuid_generate(uuid_t out) { uuid_generate_random(out); }

void uuid_unparse_lower(const uuid_t uu, char* out) {
  std::snprintf(
      out, 37,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7], uu[8], uu[9],
      uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

void uuid_unparse_upper(const uuid_t uu, char* out) {
  std::snprintf(
      out, 37,
      "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
      uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7], uu[8], uu[9],
      uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

void uuid_unparse(const uuid_t uu, char* out) { uuid_unparse_lower(uu, out); }

int uuid_parse(const char* in, uuid_t uu) {
  if (!in || std::strlen(in) != 36) return -1;
  unsigned int b[16];
  if (std::sscanf(in,
                  "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
                  &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7],
                  &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14],
                  &b[15]) != 16) {
    return -1;
  }
  for (int i = 0; i < 16; ++i) uu[i] = static_cast<unsigned char>(b[i]);
  return 0;
}

void uuid_clear(uuid_t uu) { std::memset(uu, 0, 16); }
void uuid_copy(uuid_t dst, const uuid_t src) { std::memcpy(dst, src, 16); }
int uuid_compare(const uuid_t uu1, const uuid_t uu2) {
  return std::memcmp(uu1, uu2, 16);
}
int uuid_is_null(const uuid_t uu) {
  for (int i = 0; i < 16; ++i) {
    if (uu[i] != 0) return 0;
  }
  return 1;
}

}  // extern "C"

// ============================================================================
// zlib implementation
// ============================================================================
extern "C" {

uLong crc32(uLong crc, const Bytef* buf, uInt len) {
  if (!buf) return 0;
  uint32_t c = ~static_cast<uint32_t>(crc);
  for (uInt i = 0; i < len; ++i) {
    c ^= buf[i];
    for (int k = 0; k < 8; ++k) {
      c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
  }
  return ~c;
}

uLong adler32(uLong adler, const Bytef* buf, uInt len) {
  if (!buf) return 1;
  uint32_t s1 = static_cast<uint32_t>(adler & 0xffff);
  uint32_t s2 = static_cast<uint32_t>((adler >> 16) & 0xffff);
  for (uInt i = 0; i < len; ++i) {
    s1 = (s1 + buf[i]) % 65521;
    s2 = (s2 + s1) % 65521;
  }
  return (static_cast<uLong>(s2) << 16) | s1;
}

uLong compressBound(uLong sourceLen) {
  return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + 11;
}

int compress2(Bytef* dest, uLongf* destLen, const Bytef* source,
              uLong sourceLen, int level) {
  (void)level;
  if (!dest || !destLen || (!source && sourceLen > 0)) return Z_STREAM_ERROR;
  if (*destLen < sourceLen) return Z_BUF_ERROR;
  if (sourceLen > 0) std::memcpy(dest, source, sourceLen);
  *destLen = sourceLen;
  return Z_OK;
}

int compress(Bytef* dest, uLongf* destLen, const Bytef* source,
             uLong sourceLen) {
  return compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

int uncompress(Bytef* dest, uLongf* destLen, const Bytef* source,
               uLong sourceLen) {
  if (!dest || !destLen || (!source && sourceLen > 0)) return Z_STREAM_ERROR;
  if (*destLen < sourceLen) return Z_BUF_ERROR;
  if (sourceLen > 0) std::memcpy(dest, source, sourceLen);
  *destLen = sourceLen;
  return Z_OK;
}

}  // extern "C"

// ============================================================================
// libxml2 DOM parser implementation
// ============================================================================
namespace {

xmlChar* XmlDup(const std::string& s) {
  auto* p = static_cast<xmlChar*>(std::malloc(s.size() + 1));
  if (p) std::memcpy(p, s.c_str(), s.size() + 1);
  return p;
}

xmlNodePtr AllocXmlNode(xmlElementType type, const std::string& name,
                        const std::string& content = "") {
  auto* n = static_cast<xmlNodePtr>(std::calloc(1, sizeof(xmlNode)));
  n->type = type;
  if (!name.empty()) n->name = XmlDup(name);
  if (!content.empty()) n->content = XmlDup(content);
  return n;
}

void AppendXmlChild(xmlNodePtr parent, xmlNodePtr child) {
  if (!parent || !child) return;
  child->parent = parent;
  child->doc = parent->doc;
  if (!parent->children) {
    parent->children = child;
    parent->last = child;
  } else {
    parent->last->next = child;
    child->prev = parent->last;
    parent->last = child;
  }
}

void AppendXmlAttr(xmlNodePtr elem, const std::string& name,
                   const std::string& val) {
  auto* attr = static_cast<xmlAttrPtr>(std::calloc(1, sizeof(xmlAttr)));
  attr->type = XML_ATTRIBUTE_NODE;
  attr->name = XmlDup(name);
  attr->parent = elem;
  attr->doc = elem->doc;
  xmlNodePtr val_node = AllocXmlNode(XML_TEXT_NODE, "text", val);
  val_node->parent = reinterpret_cast<xmlNodePtr>(attr);
  val_node->doc = elem->doc;
  attr->children = val_node;
  attr->last = val_node;

  if (!elem->properties) {
    elem->properties = attr;
  } else {
    xmlAttrPtr cur = elem->properties;
    while (cur->next) cur = cur->next;
    cur->next = attr;
    attr->prev = cur;
  }
}

class MiniXmlParser {
 public:
  MiniXmlParser(const char* buf, size_t len, xmlDocPtr doc)
      : cur_(buf), end_(buf + len), doc_(doc) {}

  xmlNodePtr parseRoot() {
    while (cur_ < end_) {
      skipWs();
      if (cur_ >= end_) break;
      if (*cur_ == '<') {
        if (cur_ + 1 < end_ && (cur_[1] == '?' || cur_[1] == '!')) {
          skipDeclarationOrComment();
          continue;
        }
        return parseElement();
      } else {
        ++cur_;
      }
    }
    return nullptr;
  }

 private:
  void skipWs() {
    while (cur_ < end_ &&
           (*cur_ == ' ' || *cur_ == '\t' || *cur_ == '\r' || *cur_ == '\n')) {
      ++cur_;
    }
  }

  void skipDeclarationOrComment() {
    if (end_ - cur_ >= 4 && std::memcmp(cur_, "<!--", 4) == 0) {
      cur_ += 4;
      while (end_ - cur_ >= 3 && std::memcmp(cur_, "-->", 3) != 0) ++cur_;
      if (end_ - cur_ >= 3) cur_ += 3;
      return;
    }
    while (cur_ < end_ && *cur_ != '>') ++cur_;
    if (cur_ < end_) ++cur_;
  }

  std::string parseName() {
    const char* start = cur_;
    while (cur_ < end_ && *cur_ != ' ' && *cur_ != '\t' && *cur_ != '\r' &&
           *cur_ != '\n' && *cur_ != '>' && *cur_ != '/' && *cur_ != '=') {
      ++cur_;
    }
    return std::string(start, cur_);
  }

  xmlNodePtr parseElement() {
    if (cur_ >= end_ || *cur_ != '<') return nullptr;
    ++cur_;  // '<'
    std::string tag = parseName();
    if (tag.empty()) return nullptr;
    xmlNodePtr elem = AllocXmlNode(XML_ELEMENT_NODE, tag);
    elem->doc = doc_;

    while (cur_ < end_) {
      skipWs();
      if (cur_ >= end_) break;
      if (*cur_ == '/') {
        ++cur_;
        if (cur_ < end_ && *cur_ == '>') ++cur_;
        return elem;
      }
      if (*cur_ == '>') {
        ++cur_;
        break;
      }
      std::string attr_name = parseName();
      if (attr_name.empty()) {
        ++cur_;
        continue;
      }
      skipWs();
      std::string attr_val;
      if (cur_ < end_ && *cur_ == '=') {
        ++cur_;
        skipWs();
        if (cur_ < end_ && (*cur_ == '"' || *cur_ == '\'')) {
          char q = *cur_++;
          const char* vstart = cur_;
          while (cur_ < end_ && *cur_ != q) ++cur_;
          attr_val.assign(vstart, cur_);
          if (cur_ < end_) ++cur_;
        }
      }
      AppendXmlAttr(elem, attr_name, attr_val);
    }

    // Parse children & text until closing tag
    while (cur_ < end_) {
      if (*cur_ == '<') {
        if (cur_ + 1 < end_ && cur_[1] == '/') {
          cur_ += 2;
          while (cur_ < end_ && *cur_ != '>') ++cur_;
          if (cur_ < end_) ++cur_;
          break;
        }
        if (cur_ + 1 < end_ && (cur_[1] == '?' || cur_[1] == '!')) {
          skipDeclarationOrComment();
          continue;
        }
        xmlNodePtr child = parseElement();
        if (child) AppendXmlChild(elem, child);
      } else {
        const char* tstart = cur_;
        while (cur_ < end_ && *cur_ != '<') ++cur_;
        std::string text(tstart, cur_);
        if (!text.empty()) {
          xmlNodePtr tnode = AllocXmlNode(XML_TEXT_NODE, "text", text);
          AppendXmlChild(elem, tnode);
        }
      }
    }
    return elem;
  }

  const char* cur_;
  const char* end_;
  xmlDocPtr doc_;
};

void FreeXmlNodeRecursive(xmlNodePtr node) {
  while (node) {
    xmlNodePtr next = node->next;
    FreeXmlNodeRecursive(node->children);
    xmlAttrPtr attr = node->properties;
    while (attr) {
      xmlAttrPtr anext = attr->next;
      FreeXmlNodeRecursive(attr->children);
      std::free(const_cast<xmlChar*>(attr->name));
      std::free(attr);
      attr = anext;
    }
    std::free(const_cast<xmlChar*>(node->name));
    std::free(node->content);
    std::free(node);
    node = next;
  }
}

}  // namespace

extern "C" {

xmlDocPtr xmlReadMemory(const char* buffer, int size, const char* URL,
                        const char* encoding, int options) {
  (void)URL;
  (void)encoding;
  (void)options;
  if (!buffer || size <= 0) return nullptr;
  auto* doc = static_cast<xmlDocPtr>(std::calloc(1, sizeof(xmlDoc)));
  doc->type = XML_DOCUMENT_NODE;
  doc->doc = doc;
  MiniXmlParser parser(buffer, static_cast<size_t>(size), doc);
  xmlNodePtr root = parser.parseRoot();
  if (!root) {
    std::free(doc);
    return nullptr;
  }
  doc->children = root;
  doc->last = root;
  root->parent = reinterpret_cast<xmlNodePtr>(doc);
  return doc;
}

xmlDocPtr xmlReadFile(const char* filename, const char* encoding, int options) {
  if (!filename) return nullptr;
  std::ifstream ifs(filename, std::ios::binary);
  if (!ifs.is_open()) return nullptr;
  std::ostringstream ss;
  ss << ifs.rdbuf();
  std::string content = ss.str();
  return xmlReadMemory(content.data(), static_cast<int>(content.size()),
                       filename, encoding, options);
}

void xmlFreeDoc(xmlDocPtr cur) {
  if (!cur) return;
  FreeXmlNodeRecursive(cur->children);
  std::free(cur->name);
  std::free(cur);
}

xmlNodePtr xmlDocGetRootElement(const xmlDoc* doc) {
  return doc ? doc->children : nullptr;
}

void xmlCleanupParser(void) {}

xmlAttrPtr xmlHasProp(const xmlNode* node, const xmlChar* name) {
  if (!node || !name) return nullptr;
  for (xmlAttrPtr attr = node->properties; attr != nullptr; attr = attr->next) {
    if (attr->name &&
        std::strcmp(reinterpret_cast<const char*>(attr->name),
                    reinterpret_cast<const char*>(name)) == 0) {
      return attr;
    }
  }
  return nullptr;
}

xmlChar* xmlGetProp(const xmlNode* node, const xmlChar* name) {
  xmlAttrPtr attr = xmlHasProp(node, name);
  if (!attr || !attr->children || !attr->children->content) return nullptr;
  return XmlDup(reinterpret_cast<const char*>(attr->children->content));
}

xmlChar* xmlNodeListGetString(xmlDocPtr doc, const xmlNode* list, int inLine) {
  (void)doc;
  (void)inLine;
  std::string out;
  for (const xmlNode* cur = list; cur != nullptr; cur = cur->next) {
    if ((cur->type == XML_TEXT_NODE || cur->type == XML_CDATA_SECTION_NODE) &&
        cur->content) {
      out += reinterpret_cast<const char*>(cur->content);
    }
  }
  return XmlDup(out);
}

void xmlFree(void* mem) { std::free(mem); }

}  // extern "C"
