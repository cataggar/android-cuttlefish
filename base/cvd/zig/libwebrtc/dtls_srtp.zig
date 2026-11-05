const std = @import("std");
const Sha256 = std.crypto.hash.sha2.Sha256;
const HmacSha256 = std.crypto.auth.hmac.sha2.HmacSha256;
const HmacSha1 = std.crypto.auth.hmac.HmacSha1;
const X25519 = std.crypto.dh.X25519;
const EcdsaP256Sha256 = std.crypto.sign.ecdsa.EcdsaP256Sha256;
const Aes128Gcm = std.crypto.aead.aes_gcm.Aes128Gcm;
const Aes128 = std.crypto.core.aes.Aes128;

pub const Error = error{
    InvalidArgument,
    InvalidPacket,
    InvalidRecord,
    InvalidHandshake,
    InvalidCertificate,
    FingerprintMismatch,
    SignatureVerificationFailed,
    UnsupportedProfile,
    KeyDerivationFailed,
    AuthenticationFailed,
    ReplayDetected,
    InvalidState,
    BufferTooSmall,
    OutOfMemory,
};

pub const TransportState = enum {
    new,
    connecting,
    connected,
    closed,
    failed,
};

pub const Config = struct {
    enable_srtp_aes_gcm: bool = true,
    fingerprint_hash: []const u8 = "sha-256",
};

/// RFC 5764 / RFC 7714 DTLS-SRTP protection profile identifiers.
pub const SrtpProfile = enum(u16) {
    aes128_cm_hmac_sha1_80 = 0x0001,
    aead_aes_128_gcm = 0x0007,

    pub fn keyLength(self: SrtpProfile) usize {
        return switch (self) {
            .aes128_cm_hmac_sha1_80 => 16,
            .aead_aes_128_gcm => 16,
        };
    }

    pub fn saltLength(self: SrtpProfile) usize {
        return switch (self) {
            .aes128_cm_hmac_sha1_80 => 14,
            .aead_aes_128_gcm => 12,
        };
    }

    pub fn tagLength(self: SrtpProfile) usize {
        return switch (self) {
            .aes128_cm_hmac_sha1_80 => 10,
            .aead_aes_128_gcm => 16,
        };
    }
};

pub const Role = enum {
    client,
    server,
};

/// RFC 7983 Section 7 multiplexed packet classification.
pub const PacketKind = enum {
    stun,
    dtls,
    rtp_rtcp,
    turn_channel,
    unknown,
};

/// Inspects the leading byte of a UDP datagram per RFC 7983:
/// - `0..3`     -> `.stun`
/// - `20..63`   -> `.dtls`
/// - `64..127`  -> `.turn_channel`
/// - `128..191` -> `.rtp_rtcp`
pub fn classifyPacket(data: []const u8) PacketKind {
    if (data.len == 0) return .unknown;
    return switch (data[0]) {
        0...3 => .stun,
        20...63 => .dtls,
        64...127 => .turn_channel,
        128...191 => .rtp_rtcp,
        else => .unknown,
    };
}

// ============================================================================
// Ephemeral Certificate & SHA-256 Fingerprint
// ============================================================================

/// Fixed-layout self-signed X.509v3 DER certificate size (ECDSA P-256).
pub const der_cert_len: usize = 274;
pub const fingerprint_str_len: usize = 95;

const tbs_prefix = [_]u8{
    // TBSCertificate SEQUENCE (length 188 = 0xBC)
    0x30, 0x81, 0xBC,
    // version [0] EXPLICIT INTEGER (v3 = 2)
    0xA0, 0x03, 0x02, 0x01, 0x02,
    // serialNumber INTEGER (8 bytes follow at offset 10..18 of TBS)
    0x02, 0x08,
};

const tbs_middle = [_]u8{
    // signature AlgorithmIdentifier: ecdsa-with-SHA256 (1.2.840.10045.4.3.2)
    0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02,
    // issuer Name: CN=WebRTC
    0x30, 0x11, 0x31, 0x0F, 0x30, 0x0D, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x06,
    'W', 'e', 'b', 'R', 'T', 'C',
    // validity: 250101000000Z .. 350101000000Z
    0x30, 0x1E,
    0x17, 0x0D, '2', '5', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z',
    0x17, 0x0D, '3', '5', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z',
    // subject Name: CN=WebRTC
    0x30, 0x11, 0x31, 0x0F, 0x30, 0x0D, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x06,
    'W', 'e', 'b', 'R', 'T', 'C',
    // subjectPublicKeyInfo: id-ecPublicKey (1.2.840.10045.2.1) + prime256v1 (1.2.840.10045.3.1.7)
    0x30, 0x59, 0x30, 0x13,
    0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
    0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
    // BIT STRING (66 bytes: 0x00 unused bits + 65-byte uncompressed SEC1 point)
    0x03, 0x42, 0x00,
};

const cert_sig_alg_and_bitstring_hdr = [_]u8{
    // signatureAlgorithm: ecdsa-with-SHA256
    0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02,
    // signatureValue BIT STRING (65 bytes: 0x00 unused bits + 64-byte raw r||s)
    0x03, 0x41, 0x00,
};

pub const Certificate = struct {
    ecdsa_key_pair: EcdsaP256Sha256.KeyPair,
    x25519_key_pair: X25519.KeyPair,
    der_bytes: [der_cert_len]u8,
    sha256_digest: [32]u8,
    fingerprint_str: [fingerprint_str_len]u8,

    pub fn generateDeterministic(seed: [32]u8) Error!Certificate {
        var ecdsa_seed: [32]u8 = undefined;
        var x25519_seed: [32]u8 = undefined;
        var serial_digest: [32]u8 = undefined;

        HmacSha256.create(&ecdsa_seed, "dtls-ecdsa-p256-identity", &seed);
        HmacSha256.create(&x25519_seed, "dtls-x25519-ephemeral", &seed);
        HmacSha256.create(&serial_digest, "dtls-cert-serial", &seed);

        const ecdsa_kp = EcdsaP256Sha256.KeyPair.generateDeterministic(ecdsa_seed) catch
            return Error.InvalidCertificate;
        const x25519_kp = X25519.KeyPair.generateDeterministic(x25519_seed);

        const pub_sec1 = ecdsa_kp.public_key.toUncompressedSec1();

        var der_buf: [der_cert_len]u8 = undefined;
        // Outer Certificate SEQUENCE (length 270 = 0x010E)
        der_buf[0] = 0x30;
        der_buf[1] = 0x82;
        der_buf[2] = 0x01;
        der_buf[3] = 0x0E;

        var pos: usize = 4;
        const tbs_start = pos;
        @memcpy(der_buf[pos .. pos + tbs_prefix.len], &tbs_prefix);
        pos += tbs_prefix.len;

        // 8-byte positive serial number
        @memcpy(der_buf[pos .. pos + 8], serial_digest[0..8]);
        der_buf[pos] = (der_buf[pos] & 0x7F) | 0x01;
        pos += 8;

        @memcpy(der_buf[pos .. pos + tbs_middle.len], &tbs_middle);
        pos += tbs_middle.len;

        @memcpy(der_buf[pos .. pos + 65], &pub_sec1);
        pos += 65;
        const tbs_end = pos;

        const tbs_slice = der_buf[tbs_start..tbs_end];
        const sig = ecdsa_kp.sign(tbs_slice, null) catch return Error.InvalidCertificate;
        const sig_bytes = sig.toBytes();

        @memcpy(der_buf[pos .. pos + cert_sig_alg_and_bitstring_hdr.len], &cert_sig_alg_and_bitstring_hdr);
        pos += cert_sig_alg_and_bitstring_hdr.len;

        @memcpy(der_buf[pos .. pos + 64], &sig_bytes);
        pos += 64;
        std.debug.assert(pos == der_cert_len);

        var digest: [32]u8 = undefined;
        Sha256.hash(&der_buf, &digest, .{});
        const fp_str = formatDigestFingerprint(digest);

        return .{
            .ecdsa_key_pair = ecdsa_kp,
            .x25519_key_pair = x25519_kp,
            .der_bytes = der_buf,
            .sha256_digest = digest,
            .fingerprint_str = fp_str,
        };
    }

    pub fn der(self: *const Certificate) []const u8 {
        return &self.der_bytes;
    }

    pub fn fingerprint(self: *const Certificate) []const u8 {
        return &self.fingerprint_str;
    }
};

/// Formats a 32-byte SHA-256 digest as an uppercase colon-separated 95-char string `"AA:BB:..."`.
pub fn formatDigestFingerprint(digest: [32]u8) [fingerprint_str_len]u8 {
    const hex_chars = "0123456789ABCDEF";
    var out: [fingerprint_str_len]u8 = undefined;
    for (digest, 0..) |b, i| {
        const base = i * 3;
        out[base] = hex_chars[b >> 4];
        out[base + 1] = hex_chars[b & 0x0F];
        if (i < 31) {
            out[base + 2] = ':';
        }
    }
    return out;
}

/// Computes the SHA-256 fingerprint string (`"AA:BB:..."`) for an arbitrary DER certificate buffer.
pub fn computeFingerprint(der_cert: []const u8) [fingerprint_str_len]u8 {
    var digest: [32]u8 = undefined;
    Sha256.hash(der_cert, &digest, .{});
    return formatDigestFingerprint(digest);
}

/// Verifies that `der_cert` hashes to `expected_fingerprint_str` (case-insensitive hex, `:` separated).
pub fn verifyFingerprint(der_cert: []const u8, expected_fingerprint_str: []const u8) bool {
    if (der_cert.len == 0 or expected_fingerprint_str.len != fingerprint_str_len) return false;
    const actual = computeFingerprint(der_cert);
    for (actual, expected_fingerprint_str) |a, e| {
        if (a == ':') {
            if (e != ':') return false;
        } else {
            if (std.ascii.toUpper(e) != a) return false;
        }
    }
    return true;
}

/// Validates our DER X.509 certificate structure and self-signature, returning the peer's ECDSA P-256 public key.
pub fn parseAndVerifyDerCertificate(der_cert: []const u8) Error!EcdsaP256Sha256.PublicKey {
    if (der_cert.len != der_cert_len) return Error.InvalidCertificate;
    if (der_cert[0] != 0x30 or der_cert[1] != 0x82 or der_cert[2] != 0x01 or der_cert[3] != 0x0E) {
        return Error.InvalidCertificate;
    }
    const tbs_start: usize = 4;
    const tbs_end: usize = 4 + 191;
    if (!std.mem.eql(u8, der_cert[tbs_start .. tbs_start + tbs_prefix.len], &tbs_prefix)) {
        return Error.InvalidCertificate;
    }
    const mid_start = tbs_start + tbs_prefix.len + 8;
    if (!std.mem.eql(u8, der_cert[mid_start .. mid_start + tbs_middle.len], &tbs_middle)) {
        return Error.InvalidCertificate;
    }
    const pub_start = mid_start + tbs_middle.len;
    var pub_sec1: [65]u8 = undefined;
    @memcpy(&pub_sec1, der_cert[pub_start .. pub_start + 65]);

    const pub_key = EcdsaP256Sha256.PublicKey.fromSec1(&pub_sec1) catch
        return Error.InvalidCertificate;

    const sig_hdr_start = tbs_end;
    if (!std.mem.eql(
        u8,
        der_cert[sig_hdr_start .. sig_hdr_start + cert_sig_alg_and_bitstring_hdr.len],
        &cert_sig_alg_and_bitstring_hdr,
    )) {
        return Error.InvalidCertificate;
    }
    const sig_start = sig_hdr_start + cert_sig_alg_and_bitstring_hdr.len;
    var sig_bytes: [64]u8 = undefined;
    @memcpy(&sig_bytes, der_cert[sig_start .. sig_start + 64]);

    const sig = EcdsaP256Sha256.Signature.fromBytes(sig_bytes);
    sig.verify(der_cert[tbs_start..tbs_end], pub_key) catch
        return Error.SignatureVerificationFailed;

    return pub_key;
}

// ============================================================================
// TLS 1.2 PRF (RFC 5246 Section 5) & RFC 5764 Key Extractor
// ============================================================================

/// TLS 1.2 P_SHA256 pseudorandom function:
/// `PRF(secret, label, seed) = P_SHA256(secret, label ++ seed)`.
pub fn tls12PrfSha256(
    secret: []const u8,
    label: []const u8,
    seed: []const u8,
    out: []u8,
) void {
    // a_buf holds A(i), initialized to A(0) = label ++ seed
    var a_i: [HmacSha256.mac_length]u8 = undefined;
    {
        var h = HmacSha256.init(secret);
        h.update(label);
        h.update(seed);
        h.final(&a_i);
    }

    var written: usize = 0;
    while (written < out.len) {
        var block: [HmacSha256.mac_length]u8 = undefined;
        {
            var h = HmacSha256.init(secret);
            h.update(&a_i);
            h.update(label);
            h.update(seed);
            h.final(&block);
        }

        const copy_len = @min(out.len - written, block.len);
        @memcpy(out[written .. written + copy_len], block[0..copy_len]);
        written += copy_len;

        if (written < out.len) {
            var next_a: [HmacSha256.mac_length]u8 = undefined;
            HmacSha256.create(&next_a, &a_i, secret);
            a_i = next_a;
        }
    }
}

/// Extracted SRTP master keys and master salts per RFC 5764 Section 4.2.
pub const SrtpKeyMaterial = struct {
    profile: SrtpProfile,
    client_write_key: [16]u8,
    server_write_key: [16]u8,
    client_write_salt: [14]u8,
    server_write_salt: [14]u8,

    pub fn clientSaltSlice(self: *const SrtpKeyMaterial) []const u8 {
        return self.client_write_salt[0..self.profile.saltLength()];
    }

    pub fn serverSaltSlice(self: *const SrtpKeyMaterial) []const u8 {
        return self.server_write_salt[0..self.profile.saltLength()];
    }
};

/// Derives RFC 5764 `EXTRACTOR-dtls_srtp` keying material from a 48-byte TLS master secret
/// and the 32-byte client/server randoms.
pub fn deriveSrtpKeyMaterial(
    profile: SrtpProfile,
    master_secret: [48]u8,
    client_random: [32]u8,
    server_random: [32]u8,
) SrtpKeyMaterial {
    var seed: [64]u8 = undefined;
    @memcpy(seed[0..32], &client_random);
    @memcpy(seed[32..64], &server_random);

    const key_len = profile.keyLength();
    const salt_len = profile.saltLength();
    var raw_mat: [2 * (16 + 14)]u8 = @splat(0);
    const total_len = 2 * (key_len + salt_len);

    tls12PrfSha256(&master_secret, "EXTRACTOR-dtls_srtp", &seed, raw_mat[0..total_len]);

    var result = SrtpKeyMaterial{
        .profile = profile,
        .client_write_key = @splat(0),
        .server_write_key = @splat(0),
        .client_write_salt = @splat(0),
        .server_write_salt = @splat(0),
    };

    var offset: usize = 0;
    @memcpy(&result.client_write_key, raw_mat[offset .. offset + key_len]);
    offset += key_len;
    @memcpy(&result.server_write_key, raw_mat[offset .. offset + key_len]);
    offset += key_len;
    @memcpy(result.client_write_salt[0..salt_len], raw_mat[offset .. offset + salt_len]);
    offset += salt_len;
    @memcpy(result.server_write_salt[0..salt_len], raw_mat[offset .. offset + salt_len]);

    return result;
}

// ============================================================================
// DTLS 1.2 Record Header & Session Handshake State Machine
// ============================================================================

pub const dtls_version_1_2: u16 = 0xFEFD;
pub const use_srtp_extension_type: u16 = 0x000E;
pub const cipher_suite_ecdhe_ecdsa_aes128_gcm_sha256: u16 = 0xC02B;

pub const ContentType = enum(u8) {
    change_cipher_spec = 20,
    alert = 21,
    handshake = 22,
    application_data = 23,
    _,
};

pub const HandshakeType = enum(u8) {
    client_hello = 1,
    server_hello = 2,
    certificate = 11,
    server_key_exchange = 12,
    server_hello_done = 14,
    certificate_verify = 15,
    client_key_exchange = 16,
    finished = 20,
    _,
};

/// 13-byte DTLS 1.2 record header (RFC 6347 Section 4.1).
pub const RecordHeader = struct {
    content_type: ContentType,
    version: u16 = dtls_version_1_2,
    epoch: u16,
    sequence_number: u48,
    length: u16,

    pub const encoded_len: usize = 13;

    pub fn encode(self: RecordHeader, out: *[encoded_len]u8) void {
        out[0] = @intFromEnum(self.content_type);
        std.mem.writeInt(u16, out[1..3], self.version, .big);
        std.mem.writeInt(u16, out[3..5], self.epoch, .big);
        std.mem.writeInt(u48, out[5..11], self.sequence_number, .big);
        std.mem.writeInt(u16, out[11..13], self.length, .big);
    }

    pub fn decode(bytes: []const u8) Error!RecordHeader {
        if (bytes.len < encoded_len) return Error.InvalidRecord;
        const ver = std.mem.readInt(u16, bytes[1..3], .big);
        if (ver != dtls_version_1_2 and ver != 0xFEFF) return Error.InvalidRecord;
        return .{
            .content_type = @enumFromInt(bytes[0]),
            .version = ver,
            .epoch = std.mem.readInt(u16, bytes[3..5], .big),
            .sequence_number = std.mem.readInt(u48, bytes[5..11], .big),
            .length = std.mem.readInt(u16, bytes[11..13], .big),
        };
    }
};

/// 12-byte DTLS 1.2 handshake message header (RFC 6347 Section 4.2.2).
pub const HandshakeHeader = struct {
    msg_type: HandshakeType,
    length: u24,
    message_seq: u16,
    fragment_offset: u24 = 0,
    fragment_length: u24,

    pub const encoded_len: usize = 12;

    pub fn encode(self: HandshakeHeader, out: *[encoded_len]u8) void {
        out[0] = @intFromEnum(self.msg_type);
        std.mem.writeInt(u24, out[1..4], self.length, .big);
        std.mem.writeInt(u16, out[4..6], self.message_seq, .big);
        std.mem.writeInt(u24, out[6..9], self.fragment_offset, .big);
        std.mem.writeInt(u24, out[9..12], self.fragment_length, .big);
    }

    pub fn decode(bytes: []const u8) Error!HandshakeHeader {
        if (bytes.len < encoded_len) return Error.InvalidHandshake;
        return .{
            .msg_type = @enumFromInt(bytes[0]),
            .length = std.mem.readInt(u24, bytes[1..4], .big),
            .message_seq = std.mem.readInt(u16, bytes[4..6], .big),
            .fragment_offset = std.mem.readInt(u24, bytes[6..9], .big),
            .fragment_length = std.mem.readInt(u24, bytes[9..12], .big),
        };
    }
};

pub const DtlsSession = struct {
    role: Role,
    config: Config,
    state: TransportState = .new,
    certificate: Certificate,
    expected_remote_fingerprint: ?[fingerprint_str_len]u8 = null,
    remote_cert_verified: bool = false,

    local_random: [32]u8,
    remote_random: [32]u8 = @splat(0),
    selected_profile: ?SrtpProfile = null,

    write_epoch: u16 = 0,
    write_seq: u48 = 0,
    handshake_tx_seq: u16 = 0,
    app_rx_replay: ReplayWindow = .{},

    transcript_hash: Sha256 = Sha256.init(.{}),
    master_secret: [48]u8 = @splat(0),
    srtp_keys: ?SrtpKeyMaterial = null,

    // DTLS record layer AES-128-GCM keys for epoch >= 1 (SCTP over DTLS)
    client_app_key: [16]u8 = @splat(0),
    server_app_key: [16]u8 = @splat(0),
    client_app_salt: [4]u8 = @splat(0),
    server_app_salt: [4]u8 = @splat(0),

    pub fn init(role: Role, config: Config, seed: [32]u8) Error!DtlsSession {
        const cert = try Certificate.generateDeterministic(seed);
        var rand_bytes: [32]u8 = undefined;
        HmacSha256.create(&rand_bytes, "dtls-hello-random", &seed);
        return .{
            .role = role,
            .config = config,
            .certificate = cert,
            .local_random = rand_bytes,
        };
    }

    pub fn setExpectedRemoteFingerprint(self: *DtlsSession, fp_str: []const u8) Error!void {
        if (fp_str.len != fingerprint_str_len) return Error.InvalidArgument;
        var buf: [fingerprint_str_len]u8 = undefined;
        @memcpy(&buf, fp_str);
        self.expected_remote_fingerprint = buf;
    }

    pub fn localFingerprint(self: *const DtlsSession) []const u8 {
        return self.certificate.fingerprint();
    }

    /// Starts the DTLS client handshake by emitting Flight 1 (`ClientHello` with `use_srtp` extension `0x000e`).
    pub fn startHandshake(self: *DtlsSession, out_buf: []u8) Error![]u8 {
        if (self.role != .client or self.state != .new) return Error.InvalidState;
        self.state = .connecting;

        var body: [128]u8 = undefined;
        var pos: usize = 0;

        // client_version = DTLS 1.2 (0xFEFD)
        std.mem.writeInt(u16, body[pos..][0..2], dtls_version_1_2, .big);
        pos += 2;
        // random (32 bytes)
        @memcpy(body[pos .. pos + 32], &self.local_random);
        pos += 32;
        // session_id length = 0
        body[pos] = 0;
        pos += 1;
        // cookie length = 0
        body[pos] = 0;
        pos += 1;
        // cipher_suites length = 2 (TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256)
        std.mem.writeInt(u16, body[pos..][0..2], 2, .big);
        pos += 2;
        std.mem.writeInt(u16, body[pos..][0..2], cipher_suite_ecdhe_ecdsa_aes128_gcm_sha256, .big);
        pos += 2;
        // compression_methods: 1 byte (null = 0)
        body[pos] = 1;
        body[pos + 1] = 0;
        pos += 2;

        // Extensions: use_srtp (0x000e)
        if (self.config.enable_srtp_aes_gcm) {
            // 2 profiles: aead_aes_128_gcm (0x0007), aes128_cm_hmac_sha1_80 (0x0001)
            // ext_total_len = 2(type) + 2(ext_len=7) + 2(profiles_len=4) + 4(profiles) + 1(mki=0) = 11
            std.mem.writeInt(u16, body[pos..][0..2], 11, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], use_srtp_extension_type, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], 7, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], 4, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], @intFromEnum(SrtpProfile.aead_aes_128_gcm), .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], @intFromEnum(SrtpProfile.aes128_cm_hmac_sha1_80), .big);
            pos += 2;
            body[pos] = 0; // srtp_mki length = 0
            pos += 1;
        } else {
            // 1 profile: aes128_cm_hmac_sha1_80 (0x0001)
            std.mem.writeInt(u16, body[pos..][0..2], 9, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], use_srtp_extension_type, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], 5, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], 2, .big);
            pos += 2;
            std.mem.writeInt(u16, body[pos..][0..2], @intFromEnum(SrtpProfile.aes128_cm_hmac_sha1_80), .big);
            pos += 2;
            body[pos] = 0; // srtp_mki length = 0
            pos += 1;
        }

        var out_pos: usize = 0;
        try self.appendHandshakeRecord(out_buf, &out_pos, .client_hello, body[0..pos]);
        return out_buf[0..out_pos];
    }

    /// Processes an incoming DTLS handshake datagram and writes any response flight to `out_buf`.
    pub fn processHandshakePacket(
        self: *DtlsSession,
        packet: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        if (packet.len < RecordHeader.encoded_len) return Error.InvalidRecord;
        return switch (self.role) {
            .server => self.processServerHandshake(packet, out_buf),
            .client => self.processClientHandshake(packet, out_buf),
        };
    }

    fn processServerHandshake(
        self: *DtlsSession,
        packet: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        if (self.state == .new) {
            self.state = .connecting;
        }
        if (self.state != .connecting) return Error.InvalidState;

        const first_hdr = try RecordHeader.decode(packet);
        if (first_hdr.content_type != .handshake) return Error.InvalidHandshake;
        if (packet.len < RecordHeader.encoded_len + first_hdr.length) return Error.InvalidRecord;
        const first_frag = packet[RecordHeader.encoded_len .. RecordHeader.encoded_len + first_hdr.length];
        const hs_hdr = try HandshakeHeader.decode(first_frag);

        if (hs_hdr.msg_type == .client_hello) {
            return self.handleClientHelloAndBuildServerFlight(first_frag, out_buf);
        } else if (hs_hdr.msg_type == .certificate) {
            return self.handleClientFlight2AndFinishServer(packet, out_buf);
        } else {
            return Error.InvalidHandshake;
        }
    }

    fn handleClientHelloAndBuildServerFlight(
        self: *DtlsSession,
        hs_msg: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        const hs_hdr = try HandshakeHeader.decode(hs_msg);
        const body_len: usize = hs_hdr.length;
        if (hs_msg.len < HandshakeHeader.encoded_len + body_len) return Error.InvalidHandshake;
        const body = hs_msg[HandshakeHeader.encoded_len .. HandshakeHeader.encoded_len + body_len];
        if (body.len < 2 + 32 + 1 + 1 + 2 + 2 + 2) return Error.InvalidHandshake;

        const ver = std.mem.readInt(u16, body[0..2], .big);
        if (ver != dtls_version_1_2) return Error.InvalidHandshake;
        @memcpy(&self.remote_random, body[2..34]);

        var pos: usize = 34;
        const sid_len = body[pos];
        pos += 1 + sid_len;
        if (pos >= body.len) return Error.InvalidHandshake;
        const cookie_len = body[pos];
        pos += 1 + cookie_len;
        if (pos + 2 > body.len) return Error.InvalidHandshake;
        const cs_len = std.mem.readInt(u16, body[pos..][0..2], .big);
        pos += 2 + cs_len;
        if (pos >= body.len) return Error.InvalidHandshake;
        const comp_len = body[pos];
        pos += 1 + comp_len;

        // Parse extensions to find use_srtp (0x000e)
        var chosen_profile: ?SrtpProfile = null;
        if (pos + 2 <= body.len) {
            const ext_total = std.mem.readInt(u16, body[pos..][0..2], .big);
            pos += 2;
            const ext_end = @min(body.len, pos + ext_total);
            while (pos + 4 <= ext_end) {
                const ext_type = std.mem.readInt(u16, body[pos..][0..2], .big);
                const ext_len = std.mem.readInt(u16, body[pos + 2 ..][0..2], .big);
                pos += 4;
                if (pos + ext_len > ext_end) return Error.InvalidHandshake;
                if (ext_type == use_srtp_extension_type) {
                    if (ext_len < 3) return Error.InvalidHandshake;
                    const profiles_len = std.mem.readInt(u16, body[pos..][0..2], .big);
                    if (profiles_len < 2 or (profiles_len % 2) != 0 or 2 + profiles_len > ext_len) {
                        return Error.InvalidHandshake;
                    }
                    var p_idx: usize = 0;
                    while (p_idx < profiles_len) : (p_idx += 2) {
                        const raw_p = std.mem.readInt(u16, body[pos + 2 + p_idx ..][0..2], .big);
                        if (raw_p == @intFromEnum(SrtpProfile.aead_aes_128_gcm) and self.config.enable_srtp_aes_gcm) {
                            chosen_profile = .aead_aes_128_gcm;
                            break;
                        } else if (raw_p == @intFromEnum(SrtpProfile.aes128_cm_hmac_sha1_80) and chosen_profile == null) {
                            chosen_profile = .aes128_cm_hmac_sha1_80;
                        }
                    }
                }
                pos += ext_len;
            }
        }

        const profile = chosen_profile orelse return Error.UnsupportedProfile;
        self.selected_profile = profile;
        self.transcript_hash.update(hs_msg[0 .. HandshakeHeader.encoded_len + body_len]);

        var out_pos: usize = 0;

        // 1. ServerHello
        var sh_body: [64]u8 = undefined;
        var sh_pos: usize = 0;
        std.mem.writeInt(u16, sh_body[sh_pos..][0..2], dtls_version_1_2, .big);
        sh_pos += 2;
        @memcpy(sh_body[sh_pos .. sh_pos + 32], &self.local_random);
        sh_pos += 32;
        sh_body[sh_pos] = 0; // session_id len
        sh_pos += 1;
        std.mem.writeInt(u16, sh_body[sh_pos..][0..2], cipher_suite_ecdhe_ecdsa_aes128_gcm_sha256, .big);
        sh_pos += 2;
        sh_body[sh_pos] = 0; // compression method null
        sh_pos += 1;
        // use_srtp extension (selected profile)
        std.mem.writeInt(u16, sh_body[sh_pos..][0..2], 9, .big);
        sh_pos += 2;
        std.mem.writeInt(u16, sh_body[sh_pos..][0..2], use_srtp_extension_type, .big);
        sh_pos += 2;
        std.mem.writeInt(u16, sh_body[sh_pos..][0..2], 5, .big);
        sh_pos += 2;
        std.mem.writeInt(u16, sh_body[sh_pos..][0..2], 2, .big);
        sh_pos += 2;
        std.mem.writeInt(u16, sh_body[sh_pos..][0..2], @intFromEnum(profile), .big);
        sh_pos += 2;
        sh_body[sh_pos] = 0; // mki len = 0
        sh_pos += 1;

        try self.appendHandshakeRecord(out_buf, &out_pos, .server_hello, sh_body[0..sh_pos]);

        // 2. Certificate (server DER cert)
        try self.appendHandshakeRecord(out_buf, &out_pos, .certificate, self.certificate.der());

        // 3. ServerKeyExchange (ECDHE X25519 params + ECDSA-P256-SHA256 signature)
        var ske_body: [4 + 32 + 64]u8 = undefined;
        ske_body[0] = 3; // named_curve
        std.mem.writeInt(u16, ske_body[1..3], 0x001D, .big); // x25519
        ske_body[3] = 32;
        @memcpy(ske_body[4..36], &self.certificate.x25519_key_pair.public_key);

        var signed_input: [32 + 32 + 36]u8 = undefined;
        @memcpy(signed_input[0..32], &self.remote_random); // client_random
        @memcpy(signed_input[32..64], &self.local_random); // server_random
        @memcpy(signed_input[64..100], ske_body[0..36]);

        const sig = self.certificate.ecdsa_key_pair.sign(&signed_input, null) catch
            return Error.InvalidHandshake;
        const sig_bytes = sig.toBytes();
        @memcpy(ske_body[36..100], &sig_bytes);

        try self.appendHandshakeRecord(out_buf, &out_pos, .server_key_exchange, &ske_body);

        return out_buf[0..out_pos];
    }

    fn processClientHandshake(
        self: *DtlsSession,
        packet: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        if (self.state != .connecting) return Error.InvalidState;

        const first_hdr = try RecordHeader.decode(packet);
        if (first_hdr.content_type == .handshake) {
            return self.handleServerFlightAndBuildClientFlight2(packet, out_buf);
        } else if (first_hdr.content_type == .change_cipher_spec) {
            return self.handleServerFinished(packet, out_buf);
        } else {
            return Error.InvalidHandshake;
        }
    }

    fn handleServerFlightAndBuildClientFlight2(
        self: *DtlsSession,
        packet: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        var offset: usize = 0;
        var got_sh = false;
        var got_cert = false;
        var got_ske = false;
        var peer_ecdsa_pub: ?EcdsaP256Sha256.PublicKey = null;
        var server_x25519_pub: [32]u8 = undefined;

        while (offset < packet.len) {
            const rec_hdr = try RecordHeader.decode(packet[offset..]);
            offset += RecordHeader.encoded_len;
            if (offset + rec_hdr.length > packet.len) return Error.InvalidRecord;
            const frag = packet[offset .. offset + rec_hdr.length];
            offset += rec_hdr.length;

            if (rec_hdr.content_type != .handshake) return Error.InvalidHandshake;
            const hs_hdr = try HandshakeHeader.decode(frag);
            const body_len: usize = hs_hdr.length;
            if (frag.len < HandshakeHeader.encoded_len + body_len) return Error.InvalidHandshake;
            const body = frag[HandshakeHeader.encoded_len .. HandshakeHeader.encoded_len + body_len];

            switch (hs_hdr.msg_type) {
                .server_hello => {
                    if (body.len < 2 + 32 + 1 + 2 + 1 + 9) return Error.InvalidHandshake;
                    @memcpy(&self.remote_random, body[2..34]);
                    // Parse use_srtp extension at body[38..]
                    const ext_total = std.mem.readInt(u16, body[38..40], .big);
                    if (ext_total < 9) return Error.InvalidHandshake;
                    const ext_type = std.mem.readInt(u16, body[40..42], .big);
                    if (ext_type != use_srtp_extension_type) return Error.InvalidHandshake;
                    const raw_prof = std.mem.readInt(u16, body[46..48], .big);
                    self.selected_profile = switch (raw_prof) {
                        @intFromEnum(SrtpProfile.aead_aes_128_gcm) => .aead_aes_128_gcm,
                        @intFromEnum(SrtpProfile.aes128_cm_hmac_sha1_80) => .aes128_cm_hmac_sha1_80,
                        else => return Error.UnsupportedProfile,
                    };
                    got_sh = true;
                },
                .certificate => {
                    if (self.expected_remote_fingerprint) |exp_fp| {
                        if (!verifyFingerprint(body, &exp_fp)) {
                            self.state = .failed;
                            return Error.FingerprintMismatch;
                        }
                    }
                    peer_ecdsa_pub = try parseAndVerifyDerCertificate(body);
                    self.remote_cert_verified = true;
                    got_cert = true;
                },
                .server_key_exchange => {
                    if (!got_sh or !got_cert or body.len != 100) return Error.InvalidHandshake;
                    if (body[0] != 3 or std.mem.readInt(u16, body[1..3], .big) != 0x001D or body[3] != 32) {
                        return Error.InvalidHandshake;
                    }
                    @memcpy(&server_x25519_pub, body[4..36]);
                    var sig_bytes: [64]u8 = undefined;
                    @memcpy(&sig_bytes, body[36..100]);

                    var signed_input: [32 + 32 + 36]u8 = undefined;
                    @memcpy(signed_input[0..32], &self.local_random); // client_random
                    @memcpy(signed_input[32..64], &self.remote_random); // server_random
                    @memcpy(signed_input[64..100], body[0..36]);

                    const sig = EcdsaP256Sha256.Signature.fromBytes(sig_bytes);
                    sig.verify(&signed_input, peer_ecdsa_pub.?) catch {
                        self.state = .failed;
                        return Error.SignatureVerificationFailed;
                    };
                    got_ske = true;
                },
                else => return Error.InvalidHandshake,
            }

            self.transcript_hash.update(frag[0 .. HandshakeHeader.encoded_len + body_len]);
        }

        if (!got_sh or !got_cert or !got_ske) return Error.InvalidHandshake;

        var out_pos: usize = 0;

        // 1. Client Certificate
        try self.appendHandshakeRecord(out_buf, &out_pos, .certificate, self.certificate.der());

        // 2. ClientKeyExchange (32-byte X25519 public key + 64-byte ECDSA signature)
        var cke_body: [32 + 64]u8 = undefined;
        @memcpy(cke_body[0..32], &self.certificate.x25519_key_pair.public_key);

        var cke_signed_input: [32 + 32 + 32]u8 = undefined;
        @memcpy(cke_signed_input[0..32], &self.local_random);
        @memcpy(cke_signed_input[32..64], &self.remote_random);
        @memcpy(cke_signed_input[64..96], cke_body[0..32]);
        const cke_sig = self.certificate.ecdsa_key_pair.sign(&cke_signed_input, null) catch
            return Error.InvalidHandshake;
        const cke_sig_bytes = cke_sig.toBytes();
        @memcpy(cke_body[32..96], &cke_sig_bytes);

        try self.appendHandshakeRecord(out_buf, &out_pos, .client_key_exchange, &cke_body);

        // Compute shared secret, master_secret, DTLS app keys, and EXTRACTOR-dtls_srtp keys
        try self.computeKeys(server_x25519_pub);

        // 3. ChangeCipherSpec (content_type = 20)
        try self.appendRawRecord(out_buf, &out_pos, .change_cipher_spec, &[_]u8{0x01});

        // 4. Finished (12-byte verify_data)
        const verify_data = self.computeVerifyData("client finished");
        try self.appendHandshakeRecord(out_buf, &out_pos, .finished, &verify_data);

        return out_buf[0..out_pos];
    }

    fn handleClientFlight2AndFinishServer(
        self: *DtlsSession,
        packet: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        var offset: usize = 0;
        var got_cert = false;
        var got_cke = false;
        var got_ccs = false;
        var got_fin = false;
        var peer_ecdsa_pub: ?EcdsaP256Sha256.PublicKey = null;
        var client_x25519_pub: [32]u8 = undefined;

        while (offset < packet.len) {
            const rec_hdr = try RecordHeader.decode(packet[offset..]);
            offset += RecordHeader.encoded_len;
            if (offset + rec_hdr.length > packet.len) return Error.InvalidRecord;
            const frag = packet[offset .. offset + rec_hdr.length];
            offset += rec_hdr.length;

            if (rec_hdr.content_type == .change_cipher_spec) {
                if (frag.len != 1 or frag[0] != 0x01) return Error.InvalidHandshake;
                got_ccs = true;
                continue;
            }
            if (rec_hdr.content_type != .handshake) return Error.InvalidHandshake;

            const hs_hdr = try HandshakeHeader.decode(frag);
            const body_len: usize = hs_hdr.length;
            if (frag.len < HandshakeHeader.encoded_len + body_len) return Error.InvalidHandshake;
            const body = frag[HandshakeHeader.encoded_len .. HandshakeHeader.encoded_len + body_len];

            switch (hs_hdr.msg_type) {
                .certificate => {
                    if (self.expected_remote_fingerprint) |exp_fp| {
                        if (!verifyFingerprint(body, &exp_fp)) {
                            self.state = .failed;
                            return Error.FingerprintMismatch;
                        }
                    }
                    peer_ecdsa_pub = try parseAndVerifyDerCertificate(body);
                    self.remote_cert_verified = true;
                    got_cert = true;
                    self.transcript_hash.update(frag[0 .. HandshakeHeader.encoded_len + body_len]);
                },
                .client_key_exchange => {
                    if (!got_cert or body.len != 96) return Error.InvalidHandshake;
                    @memcpy(&client_x25519_pub, body[0..32]);
                    var sig_bytes: [64]u8 = undefined;
                    @memcpy(&sig_bytes, body[32..96]);

                    var signed_input: [32 + 32 + 32]u8 = undefined;
                    @memcpy(signed_input[0..32], &self.remote_random); // client_random
                    @memcpy(signed_input[32..64], &self.local_random); // server_random
                    @memcpy(signed_input[64..96], body[0..32]);

                    const sig = EcdsaP256Sha256.Signature.fromBytes(sig_bytes);
                    sig.verify(&signed_input, peer_ecdsa_pub.?) catch {
                        self.state = .failed;
                        return Error.SignatureVerificationFailed;
                    };
                    got_cke = true;
                    self.transcript_hash.update(frag[0 .. HandshakeHeader.encoded_len + body_len]);
                    try self.computeKeys(client_x25519_pub);
                },
                .finished => {
                    if (!got_cke or !got_ccs or body.len != 12) return Error.InvalidHandshake;
                    const expected_verify = self.computeVerifyData("client finished");
                    if (!std.crypto.timing_safe.eql([12]u8, expected_verify, body[0..12].*)) {
                        self.state = .failed;
                        return Error.AuthenticationFailed;
                    }
                    self.transcript_hash.update(frag[0 .. HandshakeHeader.encoded_len + body_len]);
                    got_fin = true;
                },
                else => return Error.InvalidHandshake,
            }
        }

        if (!got_cert or !got_cke or !got_ccs or !got_fin) return Error.InvalidHandshake;

        var out_pos: usize = 0;
        // Server ChangeCipherSpec
        try self.appendRawRecord(out_buf, &out_pos, .change_cipher_spec, &[_]u8{0x01});
        // Server Finished
        const server_verify = self.computeVerifyData("server finished");
        try self.appendHandshakeRecord(out_buf, &out_pos, .finished, &server_verify);

        self.write_epoch = 1;
        self.write_seq = 0;
        self.state = .connected;
        return out_buf[0..out_pos];
    }

    fn handleServerFinished(
        self: *DtlsSession,
        packet: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        var offset: usize = 0;
        var got_ccs = false;
        var got_fin = false;

        while (offset < packet.len) {
            const rec_hdr = try RecordHeader.decode(packet[offset..]);
            offset += RecordHeader.encoded_len;
            if (offset + rec_hdr.length > packet.len) return Error.InvalidRecord;
            const frag = packet[offset .. offset + rec_hdr.length];
            offset += rec_hdr.length;

            if (rec_hdr.content_type == .change_cipher_spec) {
                if (frag.len != 1 or frag[0] != 0x01) return Error.InvalidHandshake;
                got_ccs = true;
            } else if (rec_hdr.content_type == .handshake) {
                const hs_hdr = try HandshakeHeader.decode(frag);
                const body_len: usize = hs_hdr.length;
                if (hs_hdr.msg_type != .finished or body_len != 12) return Error.InvalidHandshake;
                if (frag.len < HandshakeHeader.encoded_len + body_len) return Error.InvalidHandshake;
                const body = frag[HandshakeHeader.encoded_len .. HandshakeHeader.encoded_len + body_len];
                const expected_verify = self.computeVerifyData("server finished");
                if (!std.crypto.timing_safe.eql([12]u8, expected_verify, body[0..12].*)) {
                    self.state = .failed;
                    return Error.AuthenticationFailed;
                }
                self.transcript_hash.update(frag[0 .. HandshakeHeader.encoded_len + body_len]);
                got_fin = true;
            } else {
                return Error.InvalidHandshake;
            }
        }

        if (!got_ccs or !got_fin) return Error.InvalidHandshake;
        self.write_epoch = 1;
        self.write_seq = 0;
        self.state = .connected;
        return out_buf[0..0];
    }

    fn computeKeys(self: *DtlsSession, peer_x25519_public: [32]u8) Error!void {
        const pre_master = X25519.scalarmult(
            self.certificate.x25519_key_pair.secret_key,
            peer_x25519_public,
        ) catch return Error.KeyDerivationFailed;

        const client_random = if (self.role == .client) self.local_random else self.remote_random;
        const server_random = if (self.role == .server) self.local_random else self.remote_random;

        var random_seed: [64]u8 = undefined;
        @memcpy(random_seed[0..32], &client_random);
        @memcpy(random_seed[32..64], &server_random);

        tls12PrfSha256(&pre_master, "master secret", &random_seed, &self.master_secret);

        // Derive DTLS record-layer keys (RFC 5246 / RFC 5288: server_random ++ client_random)
        var key_seed: [64]u8 = undefined;
        @memcpy(key_seed[0..32], &server_random);
        @memcpy(key_seed[32..64], &client_random);
        var key_block: [16 + 16 + 4 + 4]u8 = undefined;
        tls12PrfSha256(&self.master_secret, "key expansion", &key_seed, &key_block);
        @memcpy(&self.client_app_key, key_block[0..16]);
        @memcpy(&self.server_app_key, key_block[16..32]);
        @memcpy(&self.client_app_salt, key_block[32..36]);
        @memcpy(&self.server_app_salt, key_block[36..40]);

        const profile = self.selected_profile orelse return Error.UnsupportedProfile;
        self.srtp_keys = deriveSrtpKeyMaterial(profile, self.master_secret, client_random, server_random);
    }

    fn computeVerifyData(self: *const DtlsSession, label: []const u8) [12]u8 {
        var h_copy = self.transcript_hash;
        var digest: [32]u8 = undefined;
        h_copy.final(&digest);
        var out: [12]u8 = undefined;
        tls12PrfSha256(&self.master_secret, label, &digest, &out);
        return out;
    }

    fn appendHandshakeRecord(
        self: *DtlsSession,
        out_buf: []u8,
        out_pos: *usize,
        msg_type: HandshakeType,
        body: []const u8,
    ) Error!void {
        const frag_len = HandshakeHeader.encoded_len + body.len;
        const total_len = RecordHeader.encoded_len + frag_len;
        if (out_pos.* + total_len > out_buf.len) return Error.BufferTooSmall;

        const rec_hdr = RecordHeader{
            .content_type = .handshake,
            .epoch = self.write_epoch,
            .sequence_number = self.write_seq,
            .length = @intCast(frag_len),
        };
        self.write_seq += 1;

        const hs_hdr = HandshakeHeader{
            .msg_type = msg_type,
            .length = @intCast(body.len),
            .message_seq = self.handshake_tx_seq,
            .fragment_offset = 0,
            .fragment_length = @intCast(body.len),
        };
        self.handshake_tx_seq += 1;

        var rec_bytes: [RecordHeader.encoded_len]u8 = undefined;
        rec_hdr.encode(&rec_bytes);
        @memcpy(out_buf[out_pos.* .. out_pos.* + RecordHeader.encoded_len], &rec_bytes);
        out_pos.* += RecordHeader.encoded_len;

        const hs_start = out_pos.*;
        var hs_bytes: [HandshakeHeader.encoded_len]u8 = undefined;
        hs_hdr.encode(&hs_bytes);
        @memcpy(out_buf[out_pos.* .. out_pos.* + HandshakeHeader.encoded_len], &hs_bytes);
        out_pos.* += HandshakeHeader.encoded_len;

        @memcpy(out_buf[out_pos.* .. out_pos.* + body.len], body);
        out_pos.* += body.len;

        self.transcript_hash.update(out_buf[hs_start..out_pos.*]);
    }

    fn appendRawRecord(
        self: *DtlsSession,
        out_buf: []u8,
        out_pos: *usize,
        content_type: ContentType,
        payload: []const u8,
    ) Error!void {
        const total_len = RecordHeader.encoded_len + payload.len;
        if (out_pos.* + total_len > out_buf.len) return Error.BufferTooSmall;

        const rec_hdr = RecordHeader{
            .content_type = content_type,
            .epoch = self.write_epoch,
            .sequence_number = self.write_seq,
            .length = @intCast(payload.len),
        };
        self.write_seq += 1;

        var rec_bytes: [RecordHeader.encoded_len]u8 = undefined;
        rec_hdr.encode(&rec_bytes);
        @memcpy(out_buf[out_pos.* .. out_pos.* + RecordHeader.encoded_len], &rec_bytes);
        out_pos.* += RecordHeader.encoded_len;

        @memcpy(out_buf[out_pos.* .. out_pos.* + payload.len], payload);
        out_pos.* += payload.len;
    }

    /// Encrypts an SCTP packet (or other payload) into a DTLS 1.2 `application_data` AEAD record.
    pub fn protectApplicationData(
        self: *DtlsSession,
        plaintext: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        if (self.state != .connected or self.write_epoch == 0) return Error.InvalidState;
        // Fragment = 8-byte explicit nonce + ciphertext + 16-byte GCM tag
        const frag_len = 8 + plaintext.len + Aes128Gcm.tag_length;
        const total_len = RecordHeader.encoded_len + frag_len;
        if (out_buf.len < total_len) return Error.BufferTooSmall;

        const epoch = self.write_epoch;
        const seq = self.write_seq;
        self.write_seq += 1;

        const rec_hdr = RecordHeader{
            .content_type = .application_data,
            .version = dtls_version_1_2,
            .epoch = epoch,
            .sequence_number = seq,
            .length = @intCast(frag_len),
        };
        rec_hdr.encode(out_buf[0..RecordHeader.encoded_len]);

        // Explicit nonce = epoch (2B) || seq (6B)
        var explicit_nonce: [8]u8 = undefined;
        std.mem.writeInt(u16, explicit_nonce[0..2], epoch, .big);
        std.mem.writeInt(u48, explicit_nonce[2..8], seq, .big);
        @memcpy(out_buf[RecordHeader.encoded_len .. RecordHeader.encoded_len + 8], &explicit_nonce);

        const write_key = if (self.role == .client) self.client_app_key else self.server_app_key;
        const write_salt = if (self.role == .client) self.client_app_salt else self.server_app_salt;

        var nonce: [12]u8 = undefined;
        @memcpy(nonce[0..4], &write_salt);
        @memcpy(nonce[4..12], &explicit_nonce);

        var aad: [13]u8 = undefined;
        @memcpy(aad[0..8], &explicit_nonce);
        aad[8] = @intFromEnum(ContentType.application_data);
        std.mem.writeInt(u16, aad[9..11], dtls_version_1_2, .big);
        std.mem.writeInt(u16, aad[11..13], @intCast(plaintext.len), .big);

        const ct_start = RecordHeader.encoded_len + 8;
        const ct_end = ct_start + plaintext.len;
        var tag: [Aes128Gcm.tag_length]u8 = undefined;

        Aes128Gcm.encrypt(
            out_buf[ct_start..ct_end],
            &tag,
            plaintext,
            &aad,
            nonce,
            write_key,
        );
        @memcpy(out_buf[ct_end .. ct_end + Aes128Gcm.tag_length], &tag);
        return out_buf[0..total_len];
    }

    /// Decrypts and authenticates a DTLS 1.2 `application_data` AEAD record carrying SCTP data.
    pub fn unprotectApplicationData(
        self: *DtlsSession,
        record: []const u8,
        out_buf: []u8,
    ) Error![]u8 {
        if (self.state != .connected) return Error.InvalidState;
        const rec_hdr = try RecordHeader.decode(record);
        if (rec_hdr.content_type != .application_data or rec_hdr.epoch == 0) {
            return Error.InvalidRecord;
        }
        if (record.len < RecordHeader.encoded_len + rec_hdr.length) return Error.InvalidRecord;
        if (rec_hdr.length < 8 + Aes128Gcm.tag_length) return Error.InvalidRecord;

        const seq_u64: u64 = rec_hdr.sequence_number;
        if (!self.app_rx_replay.check(seq_u64)) return Error.ReplayDetected;

        const frag = record[RecordHeader.encoded_len .. RecordHeader.encoded_len + rec_hdr.length];
        const explicit_nonce = frag[0..8];
        const pt_len = frag.len - 8 - Aes128Gcm.tag_length;
        if (out_buf.len < pt_len) return Error.BufferTooSmall;

        const ciphertext = frag[8 .. 8 + pt_len];
        var tag: [Aes128Gcm.tag_length]u8 = undefined;
        @memcpy(&tag, frag[8 + pt_len ..][0..Aes128Gcm.tag_length]);

        const read_key = if (self.role == .client) self.server_app_key else self.client_app_key;
        const read_salt = if (self.role == .client) self.server_app_salt else self.client_app_salt;

        var nonce: [12]u8 = undefined;
        @memcpy(nonce[0..4], &read_salt);
        @memcpy(nonce[4..12], explicit_nonce);

        var aad: [13]u8 = undefined;
        std.mem.writeInt(u16, aad[0..2], rec_hdr.epoch, .big);
        std.mem.writeInt(u48, aad[2..8], rec_hdr.sequence_number, .big);
        aad[8] = @intFromEnum(ContentType.application_data);
        std.mem.writeInt(u16, aad[9..11], rec_hdr.version, .big);
        std.mem.writeInt(u16, aad[11..13], @intCast(pt_len), .big);

        Aes128Gcm.decrypt(
            out_buf[0..pt_len],
            ciphertext,
            tag,
            &aad,
            nonce,
            read_key,
        ) catch return Error.AuthenticationFailed;

        self.app_rx_replay.accept(seq_u64);
        return out_buf[0..pt_len];
    }
};

// ============================================================================
// 64-Packet Sliding Replay Window (RFC 3711 Section 3.3.2)
// ============================================================================

pub const ReplayWindow = struct {
    highest_index: u64 = 0,
    bitmap: u64 = 0,
    initialized: bool = false,

    pub fn check(self: *const ReplayWindow, index: u64) bool {
        if (!self.initialized) return true;
        if (index > self.highest_index) return true;
        const delta = self.highest_index - index;
        if (delta >= 64) return false;
        const mask: u64 = @as(u64, 1) << @intCast(delta);
        return (self.bitmap & mask) == 0;
    }

    pub fn accept(self: *ReplayWindow, index: u64) void {
        if (!self.initialized) {
            self.initialized = true;
            self.highest_index = index;
            self.bitmap = 1;
            return;
        }
        if (index > self.highest_index) {
            const shift = index - self.highest_index;
            if (shift >= 64) {
                self.bitmap = 1;
            } else {
                self.bitmap = (self.bitmap << @intCast(shift)) | 1;
            }
            self.highest_index = index;
        } else {
            const delta = self.highest_index - index;
            if (delta < 64) {
                self.bitmap |= (@as(u64, 1) << @intCast(delta));
            }
        }
    }
};

// ============================================================================
// SRTP & SRTCP Protection / Unprotection Context (RFC 3711 & RFC 7714)
// ============================================================================

/// Returns the total byte length of the RTP header (fixed 12B + CSRCs + optional extension).
pub fn parseRtpHeaderLength(packet: []const u8) Error!usize {
    if (packet.len < 12) return Error.InvalidPacket;
    const version = packet[0] >> 6;
    if (version != 2) return Error.InvalidPacket;
    const csrc_count: usize = packet[0] & 0x0F;
    const has_extension = (packet[0] & 0x10) != 0;

    var header_len: usize = 12 + csrc_count * 4;
    if (packet.len < header_len) return Error.InvalidPacket;

    if (has_extension) {
        if (packet.len < header_len + 4) return Error.InvalidPacket;
        const ext_words: usize = std.mem.readInt(u16, packet[header_len + 2 ..][0..2], .big);
        header_len += 4 + ext_words * 4;
        if (packet.len < header_len) return Error.InvalidPacket;
    }
    return header_len;
}

/// Validates the fixed 8-byte RTCP header.
pub fn validateRtcpHeader(packet: []const u8) Error!void {
    if (packet.len < 8) return Error.InvalidPacket;
    const version = packet[0] >> 6;
    if (version != 2) return Error.InvalidPacket;
}

/// RFC 3711 Section 4.3.1 AES-CM Pseudo-Random Function for session key derivation.
fn deriveSessionKeyAesCm(
    master_key: [16]u8,
    master_salt: []const u8,
    label: u8,
    out: []u8,
) void {
    const aes = Aes128.initEnc(master_key);
    var base_iv: [16]u8 = @splat(0);
    const copy_salt = @min(master_salt.len, 14);
    @memcpy(base_iv[0..copy_salt], master_salt[0..copy_salt]);
    // key_id = label << 48 (7 bytes aligned to the right of the 14-byte salt -> index 7)
    base_iv[7] ^= label;

    var written: usize = 0;
    var block_ctr: u16 = 0;
    while (written < out.len) : (block_ctr += 1) {
        var counter_block = base_iv;
        std.mem.writeInt(u16, counter_block[14..16], block_ctr, .big);
        var keystream: [16]u8 = undefined;
        aes.encrypt(&keystream, &counter_block);

        const n = @min(out.len - written, 16);
        @memcpy(out[written .. written + n], keystream[0..n]);
        written += n;
    }
}

/// Encrypts/decrypts payload in-place using AES-128 Counter Mode per RFC 3711 Section 4.1.1.
fn aes128CmCrypt(
    session_key: [16]u8,
    session_salt: [14]u8,
    ssrc: u32,
    packet_index: u48,
    dst: []u8,
    src: []const u8,
) void {
    const aes = Aes128.initEnc(session_key);
    var base_iv: [16]u8 = @splat(0);
    @memcpy(base_iv[0..14], &session_salt);

    // IV = (k_s * 2^16) XOR (SSRC * 2^64) XOR (i * 2^16)
    var ssrc_bytes: [4]u8 = undefined;
    std.mem.writeInt(u32, &ssrc_bytes, ssrc, .big);
    for (0..4) |idx| {
        base_iv[4 + idx] ^= ssrc_bytes[idx];
    }

    var index_bytes: [6]u8 = undefined;
    std.mem.writeInt(u48, &index_bytes, packet_index, .big);
    for (0..6) |idx| {
        base_iv[8 + idx] ^= index_bytes[idx];
    }

    var offset: usize = 0;
    var block_ctr: u16 = 0;
    while (offset < src.len) : (block_ctr += 1) {
        var counter_block = base_iv;
        std.mem.writeInt(u16, counter_block[14..16], block_ctr, .big);
        var keystream: [16]u8 = undefined;
        aes.encrypt(&keystream, &counter_block);

        const n = @min(src.len - offset, 16);
        for (0..n) |i| {
            dst[offset + i] = src[offset + i] ^ keystream[i];
        }
        offset += n;
    }
}

/// Constructs the 12-byte AEAD-AES-128-GCM IV per RFC 7714 Section 8.1 / 9.1.
fn buildGcmIv(salt: [12]u8, ssrc: u32, index: u48) [12]u8 {
    var iv = salt;
    var ssrc_bytes: [4]u8 = undefined;
    std.mem.writeInt(u32, &ssrc_bytes, ssrc, .big);
    for (0..4) |i| {
        iv[2 + i] ^= ssrc_bytes[i];
    }
    var idx_bytes: [6]u8 = undefined;
    std.mem.writeInt(u48, &idx_bytes, index, .big);
    for (0..6) |i| {
        iv[6 + i] ^= idx_bytes[i];
    }
    return iv;
}

pub const SrtpContext = struct {
    profile: SrtpProfile,

    // SRTP session keys (RFC 3711 labels 0x00, 0x01, 0x02)
    rtp_enc_key: [16]u8,
    rtp_auth_key: [20]u8,
    rtp_salt: [14]u8,

    // SRTCP session keys (RFC 3711 labels 0x03, 0x04, 0x05)
    rtcp_enc_key: [16]u8,
    rtcp_auth_key: [20]u8,
    rtcp_salt: [14]u8,

    // Rollover counter (ROC) & sequence tracking
    roc: u32 = 0,
    last_seq: u16 = 0,
    seq_initialized: bool = false,
    srtcp_tx_index: u31 = 0,

    rtp_replay: ReplayWindow = .{},
    rtcp_replay: ReplayWindow = .{},

    pub fn init(
        profile: SrtpProfile,
        master_key: [16]u8,
        master_salt: []const u8,
    ) Error!SrtpContext {
        if (master_salt.len < profile.saltLength()) return Error.InvalidPacket;
        const salt_slice = master_salt[0..profile.saltLength()];

        var ctx = SrtpContext{
            .profile = profile,
            .rtp_enc_key = undefined,
            .rtp_auth_key = undefined,
            .rtp_salt = @splat(0),
            .rtcp_enc_key = undefined,
            .rtcp_auth_key = undefined,
            .rtcp_salt = @splat(0),
        };

        deriveSessionKeyAesCm(master_key, salt_slice, 0x00, &ctx.rtp_enc_key);
        deriveSessionKeyAesCm(master_key, salt_slice, 0x01, &ctx.rtp_auth_key);
        deriveSessionKeyAesCm(master_key, salt_slice, 0x02, ctx.rtp_salt[0..profile.saltLength()]);

        deriveSessionKeyAesCm(master_key, salt_slice, 0x03, &ctx.rtcp_enc_key);
        deriveSessionKeyAesCm(master_key, salt_slice, 0x04, &ctx.rtcp_auth_key);
        deriveSessionKeyAesCm(master_key, salt_slice, 0x05, ctx.rtcp_salt[0..profile.saltLength()]);

        return ctx;
    }

    pub fn initFromKeyMaterial(keys: SrtpKeyMaterial, sender_role: Role) Error!SrtpContext {
        return switch (sender_role) {
            .client => init(keys.profile, keys.client_write_key, keys.clientSaltSlice()),
            .server => init(keys.profile, keys.server_write_key, keys.serverSaltSlice()),
        };
    }

    fn estimatePacketIndex(self: *const SrtpContext, seq: u16) struct { roc: u32, index: u48 } {
        if (!self.seq_initialized) {
            return .{ .roc = self.roc, .index = (@as(u48, self.roc) << 16) | seq };
        }
        var est_roc = self.roc;
        if (self.last_seq < 0x8000) {
            if (seq > self.last_seq and (seq - self.last_seq) > 0x8000) {
                if (est_roc > 0) est_roc -= 1;
            }
        } else {
            if (seq < self.last_seq and (self.last_seq - seq) > 0x8000) {
                est_roc +%= 1;
            }
        }
        return .{ .roc = est_roc, .index = (@as(u48, est_roc) << 16) | seq };
    }

    fn updateSeqAndRoc(self: *SrtpContext, seq: u16, used_roc: u32) void {
        if (!self.seq_initialized) {
            self.seq_initialized = true;
            self.last_seq = seq;
            self.roc = used_roc;
            return;
        }
        if (used_roc > self.roc or (used_roc == self.roc and seq > self.last_seq)) {
            self.roc = used_roc;
            self.last_seq = seq;
        }
    }

    /// Protects an RTP packet in-place into `out_buf`, returning the protected SRTP slice.
    pub fn protectRtp(self: *SrtpContext, rtp_packet: []const u8, out_buf: []u8) Error![]u8 {
        const hdr_len = try parseRtpHeaderLength(rtp_packet);
        const tag_len = self.profile.tagLength();
        const total_len = rtp_packet.len + tag_len;
        if (out_buf.len < total_len) return Error.BufferTooSmall;

        const seq = std.mem.readInt(u16, rtp_packet[2..4], .big);
        const ssrc = std.mem.readInt(u32, rtp_packet[8..12], .big);
        const est = self.estimatePacketIndex(seq);
        self.updateSeqAndRoc(seq, est.roc);

        // Copy cleartext RTP header
        @memcpy(out_buf[0..hdr_len], rtp_packet[0..hdr_len]);
        const payload = rtp_packet[hdr_len..];
        const ct_slice = out_buf[hdr_len .. hdr_len + payload.len];

        switch (self.profile) {
            .aead_aes_128_gcm => {
                const iv = buildGcmIv(self.rtp_salt[0..12].*, ssrc, est.index);
                var tag: [16]u8 = undefined;
                Aes128Gcm.encrypt(
                    ct_slice,
                    &tag,
                    payload,
                    out_buf[0..hdr_len],
                    iv,
                    self.rtp_enc_key,
                );
                @memcpy(out_buf[hdr_len + payload.len .. total_len], &tag);
            },
            .aes128_cm_hmac_sha1_80 => {
                aes128CmCrypt(self.rtp_enc_key, self.rtp_salt, ssrc, est.index, ct_slice, payload);
                var roc_be: [4]u8 = undefined;
                std.mem.writeInt(u32, &roc_be, est.roc, .big);

                var mac: [HmacSha1.mac_length]u8 = undefined;
                var h = HmacSha1.init(&self.rtp_auth_key);
                h.update(out_buf[0 .. hdr_len + payload.len]);
                h.update(&roc_be);
                h.final(&mac);

                @memcpy(out_buf[hdr_len + payload.len .. total_len], mac[0..10]);
            },
        }
        return out_buf[0..total_len];
    }

    /// Authenticates, checks replay window, and decrypts an SRTP packet into `out_buf`.
    pub fn unprotectRtp(self: *SrtpContext, srtp_packet: []const u8, out_buf: []u8) Error![]u8 {
        const hdr_len = try parseRtpHeaderLength(srtp_packet);
        const tag_len = self.profile.tagLength();
        if (srtp_packet.len < hdr_len + tag_len) return Error.InvalidPacket;

        const pt_len = srtp_packet.len - tag_len;
        if (out_buf.len < pt_len) return Error.BufferTooSmall;

        const seq = std.mem.readInt(u16, srtp_packet[2..4], .big);
        const ssrc = std.mem.readInt(u32, srtp_packet[8..12], .big);
        const est = self.estimatePacketIndex(seq);

        if (!self.rtp_replay.check(est.index)) return Error.ReplayDetected;

        const ciphertext = srtp_packet[hdr_len..pt_len];
        const tag_slice = srtp_packet[pt_len..];

        @memcpy(out_buf[0..hdr_len], srtp_packet[0..hdr_len]);
        const pt_payload = out_buf[hdr_len..pt_len];

        switch (self.profile) {
            .aead_aes_128_gcm => {
                const iv = buildGcmIv(self.rtp_salt[0..12].*, ssrc, est.index);
                var tag: [16]u8 = undefined;
                @memcpy(&tag, tag_slice[0..16]);
                Aes128Gcm.decrypt(
                    pt_payload,
                    ciphertext,
                    tag,
                    srtp_packet[0..hdr_len],
                    iv,
                    self.rtp_enc_key,
                ) catch return Error.AuthenticationFailed;
            },
            .aes128_cm_hmac_sha1_80 => {
                var roc_be: [4]u8 = undefined;
                std.mem.writeInt(u32, &roc_be, est.roc, .big);

                var mac: [HmacSha1.mac_length]u8 = undefined;
                var h = HmacSha1.init(&self.rtp_auth_key);
                h.update(srtp_packet[0..pt_len]);
                h.update(&roc_be);
                h.final(&mac);

                if (!std.crypto.timing_safe.eql([10]u8, mac[0..10].*, tag_slice[0..10].*)) {
                    return Error.AuthenticationFailed;
                }
                aes128CmCrypt(self.rtp_enc_key, self.rtp_salt, ssrc, est.index, pt_payload, ciphertext);
            },
        }

        self.rtp_replay.accept(est.index);
        self.updateSeqAndRoc(seq, est.roc);
        return out_buf[0..pt_len];
    }

    /// Protects an RTCP packet into an SRTCP packet (with E-flag + 31-bit SRTCP index + auth tag).
    pub fn protectRtcp(self: *SrtpContext, rtcp_packet: []const u8, out_buf: []u8) Error![]u8 {
        try validateRtcpHeader(rtcp_packet);
        const tag_len = self.profile.tagLength();
        // SRTCP appends tag + 4-byte E|SRTCP_INDEX trailer
        const total_len = rtcp_packet.len + tag_len + 4;
        if (out_buf.len < total_len) return Error.BufferTooSmall;

        const ssrc = std.mem.readInt(u32, rtcp_packet[4..8], .big);
        const srtcp_idx: u32 = self.srtcp_tx_index;
        self.srtcp_tx_index +%= 1;
        const e_and_index: u32 = 0x80000000 | srtcp_idx;

        // First 8 bytes of RTCP header remain unencrypted
        @memcpy(out_buf[0..8], rtcp_packet[0..8]);
        const payload = rtcp_packet[8..];
        const ct_slice = out_buf[8 .. 8 + payload.len];

        switch (self.profile) {
            .aead_aes_128_gcm => {
                // RFC 7714 Section 9: SRTCP layout is Header(8) || Ciphertext || GCM_Tag(16) || E|Index(4)
                // AAD is Header(8) || E|Index(4)
                var aad: [12]u8 = undefined;
                @memcpy(aad[0..8], rtcp_packet[0..8]);
                std.mem.writeInt(u32, aad[8..12], e_and_index, .big);

                const iv = buildGcmIv(self.rtcp_salt[0..12].*, ssrc, @as(u48, srtcp_idx));
                var tag: [16]u8 = undefined;
                Aes128Gcm.encrypt(
                    ct_slice,
                    &tag,
                    payload,
                    &aad,
                    iv,
                    self.rtcp_enc_key,
                );
                const tag_pos = 8 + payload.len;
                @memcpy(out_buf[tag_pos .. tag_pos + 16], &tag);
                std.mem.writeInt(u32, out_buf[tag_pos + 16 .. total_len][0..4], e_and_index, .big);
            },
            .aes128_cm_hmac_sha1_80 => {
                // RFC 3711 Section 3.4: SRTCP layout is Header(8) || Ciphertext || E|Index(4) || Tag(10)
                aes128CmCrypt(self.rtcp_enc_key, self.rtcp_salt, ssrc, @as(u48, srtcp_idx), ct_slice, payload);
                const idx_pos = 8 + payload.len;
                std.mem.writeInt(u32, out_buf[idx_pos .. idx_pos + 4][0..4], e_and_index, .big);

                var mac: [HmacSha1.mac_length]u8 = undefined;
                var h = HmacSha1.init(&self.rtcp_auth_key);
                h.update(out_buf[0 .. idx_pos + 4]);
                h.final(&mac);

                @memcpy(out_buf[idx_pos + 4 .. total_len], mac[0..10]);
            },
        }
        return out_buf[0..total_len];
    }

    /// Authenticates, checks replay protection, and decrypts an SRTCP packet into `out_buf`.
    pub fn unprotectRtcp(self: *SrtpContext, srtcp_packet: []const u8, out_buf: []u8) Error![]u8 {
        try validateRtcpHeader(srtcp_packet);
        const tag_len = self.profile.tagLength();
        if (srtcp_packet.len < 8 + tag_len + 4) return Error.InvalidPacket;

        const pt_len = srtcp_packet.len - tag_len - 4;
        if (out_buf.len < pt_len) return Error.BufferTooSmall;

        const ssrc = std.mem.readInt(u32, srtcp_packet[4..8], .big);
        @memcpy(out_buf[0..8], srtcp_packet[0..8]);
        const pt_payload = out_buf[8..pt_len];
        const payload_len = pt_len - 8;

        switch (self.profile) {
            .aead_aes_128_gcm => {
                const tag_pos = 8 + payload_len;
                const e_and_index = std.mem.readInt(u32, srtcp_packet[tag_pos + 16 ..][0..4], .big);
                if ((e_and_index & 0x80000000) == 0) return Error.InvalidPacket;
                const srtcp_idx: u32 = e_and_index & 0x7FFFFFFF;

                if (!self.rtcp_replay.check(srtcp_idx)) return Error.ReplayDetected;

                var aad: [12]u8 = undefined;
                @memcpy(aad[0..8], srtcp_packet[0..8]);
                std.mem.writeInt(u32, aad[8..12], e_and_index, .big);

                const iv = buildGcmIv(self.rtcp_salt[0..12].*, ssrc, @as(u48, srtcp_idx));
                var tag: [16]u8 = undefined;
                @memcpy(&tag, srtcp_packet[tag_pos .. tag_pos + 16]);

                Aes128Gcm.decrypt(
                    pt_payload,
                    srtcp_packet[8..tag_pos],
                    tag,
                    &aad,
                    iv,
                    self.rtcp_enc_key,
                ) catch return Error.AuthenticationFailed;

                self.rtcp_replay.accept(srtcp_idx);
            },
            .aes128_cm_hmac_sha1_80 => {
                const idx_pos = 8 + payload_len;
                const e_and_index = std.mem.readInt(u32, srtcp_packet[idx_pos .. idx_pos + 4][0..4], .big);
                if ((e_and_index & 0x80000000) == 0) return Error.InvalidPacket;
                const srtcp_idx: u32 = e_and_index & 0x7FFFFFFF;

                if (!self.rtcp_replay.check(srtcp_idx)) return Error.ReplayDetected;

                var mac: [HmacSha1.mac_length]u8 = undefined;
                var h = HmacSha1.init(&self.rtcp_auth_key);
                h.update(srtcp_packet[0 .. idx_pos + 4]);
                h.final(&mac);

                const tag_slice = srtcp_packet[idx_pos + 4 ..];
                if (!std.crypto.timing_safe.eql([10]u8, mac[0..10].*, tag_slice[0..10].*)) {
                    return Error.AuthenticationFailed;
                }

                aes128CmCrypt(
                    self.rtcp_enc_key,
                    self.rtcp_salt,
                    ssrc,
                    @as(u48, srtcp_idx),
                    pt_payload,
                    srtcp_packet[8..idx_pos],
                );
                self.rtcp_replay.accept(srtcp_idx);
            },
        }
        return out_buf[0..pt_len];
    }
};

// ============================================================================
// Unit Tests
// ============================================================================

test "RFC 7983 packet demultiplexer classifyPacket" {
    try std.testing.expectEqual(PacketKind.unknown, classifyPacket(&[_]u8{}));

    // STUN: 0..3
    try std.testing.expectEqual(PacketKind.stun, classifyPacket(&[_]u8{ 0, 1, 0, 0 }));
    try std.testing.expectEqual(PacketKind.stun, classifyPacket(&[_]u8{3}));

    // DTLS: 20..63
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(&[_]u8{20})); // change_cipher_spec
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(&[_]u8{21})); // alert
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(&[_]u8{22})); // handshake
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(&[_]u8{23})); // application_data
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(&[_]u8{63}));

    // TURN ChannelData: 64..127
    try std.testing.expectEqual(PacketKind.turn_channel, classifyPacket(&[_]u8{64}));
    try std.testing.expectEqual(PacketKind.turn_channel, classifyPacket(&[_]u8{127}));

    // RTP / RTCP: 128..191
    try std.testing.expectEqual(PacketKind.rtp_rtcp, classifyPacket(&[_]u8{0x80})); // RTP v2
    try std.testing.expectEqual(PacketKind.rtp_rtcp, classifyPacket(&[_]u8{0x90})); // RTP v2 + ext
    try std.testing.expectEqual(PacketKind.rtp_rtcp, classifyPacket(&[_]u8{191}));

    // Out-of-range leading bytes
    try std.testing.expectEqual(PacketKind.unknown, classifyPacket(&[_]u8{19}));
    try std.testing.expectEqual(PacketKind.unknown, classifyPacket(&[_]u8{192}));
}

test "Ephemeral Certificate DER generation, SHA-256 fingerprint formatting, and verification" {
    const seed: [32]u8 = @splat(0x42);
    const cert = try Certificate.generateDeterministic(seed);

    // Valid DER X.509 SEQUENCE header
    try std.testing.expectEqual(@as(usize, der_cert_len), cert.der().len);
    try std.testing.expectEqual(@as(u8, 0x30), cert.der_bytes[0]);
    try std.testing.expectEqual(@as(u8, 0x82), cert.der_bytes[1]);

    // Self-signature and embedded public key verification
    const extracted_pub = try parseAndVerifyDerCertificate(cert.der());
    try std.testing.expectEqualSlices(
        u8,
        &cert.ecdsa_key_pair.public_key.toUncompressedSec1(),
        &extracted_pub.toUncompressedSec1(),
    );

    // 95-char colon-separated uppercase SHA-256 fingerprint ("AA:BB:...")
    const fp = cert.fingerprint();
    try std.testing.expectEqual(@as(usize, 95), fp.len);
    for (fp, 0..) |ch, idx| {
        if ((idx + 1) % 3 == 0) {
            try std.testing.expectEqual(@as(u8, ':'), ch);
        } else {
            try std.testing.expect(std.ascii.isUpper(ch) or std.ascii.isDigit(ch));
        }
    }

    try std.testing.expect(verifyFingerprint(cert.der(), fp));

    // Lowercase fingerprint should also verify
    var lower_fp: [95]u8 = undefined;
    for (fp, 0..) |c, i| lower_fp[i] = std.ascii.toLower(c);
    try std.testing.expect(verifyFingerprint(cert.der(), &lower_fp));

    // Tampered fingerprint must fail
    var bad_fp = cert.fingerprint_str;
    bad_fp[0] = if (bad_fp[0] == 'A') 'B' else 'A';
    try std.testing.expect(!verifyFingerprint(cert.der(), &bad_fp));
}

test "Full DTLS 1.2 Client <-> Server handshake, EXTRACTOR-dtls_srtp, and SCTP application_data" {
    const client_seed: [32]u8 = @splat(0x11);
    const server_seed: [32]u8 = @splat(0x22);

    var client = try DtlsSession.init(.client, .{ .enable_srtp_aes_gcm = true }, client_seed);
    var server = try DtlsSession.init(.server, .{ .enable_srtp_aes_gcm = true }, server_seed);

    // Pin each peer's expected SDP certificate fingerprint
    try client.setExpectedRemoteFingerprint(server.localFingerprint());
    try server.setExpectedRemoteFingerprint(client.localFingerprint());

    var flight1_buf: [512]u8 = undefined;
    const flight1 = try client.startHandshake(&flight1_buf);
    try std.testing.expectEqual(TransportState.connecting, client.state);
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(flight1));

    var server_flight_buf: [1024]u8 = undefined;
    const server_flight = try server.processHandshakePacket(flight1, &server_flight_buf);
    try std.testing.expectEqual(TransportState.connecting, server.state);
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(server_flight));

    var client_flight2_buf: [1024]u8 = undefined;
    const client_flight2 = try client.processHandshakePacket(server_flight, &client_flight2_buf);
    try std.testing.expect(client.remote_cert_verified);
    try std.testing.expect(client_flight2.len > 0);

    var server_flight2_buf: [512]u8 = undefined;
    const server_flight2 = try server.processHandshakePacket(client_flight2, &server_flight2_buf);
    try std.testing.expect(server.remote_cert_verified);
    try std.testing.expectEqual(TransportState.connected, server.state);

    var final_buf: [128]u8 = undefined;
    const final_out = try client.processHandshakePacket(server_flight2, &final_buf);
    try std.testing.expectEqual(@as(usize, 0), final_out.len);
    try std.testing.expectEqual(TransportState.connected, client.state);

    // Verify both sides negotiated AEAD_AES_128_GCM and derived identical SRTP keys
    try std.testing.expectEqual(SrtpProfile.aead_aes_128_gcm, client.selected_profile.?);
    try std.testing.expectEqual(SrtpProfile.aead_aes_128_gcm, server.selected_profile.?);

    const c_keys = client.srtp_keys.?;
    const s_keys = server.srtp_keys.?;
    try std.testing.expectEqualSlices(u8, &c_keys.client_write_key, &s_keys.client_write_key);
    try std.testing.expectEqualSlices(u8, &c_keys.server_write_key, &s_keys.server_write_key);
    try std.testing.expectEqualSlices(u8, c_keys.clientSaltSlice(), s_keys.clientSaltSlice());
    try std.testing.expectEqualSlices(u8, c_keys.serverSaltSlice(), s_keys.serverSaltSlice());

    // Exchange encrypted DTLS application_data (SCTP over DTLS)
    const sctp_payload = "SCTP_DATA_CHANNEL_OPEN_ACK_PAYLOAD";
    var dtls_app_record: [256]u8 = undefined;
    const encrypted_rec = try client.protectApplicationData(sctp_payload, &dtls_app_record);
    try std.testing.expectEqual(PacketKind.dtls, classifyPacket(encrypted_rec));

    var decrypted_buf: [256]u8 = undefined;
    const decrypted = try server.unprotectApplicationData(encrypted_rec, &decrypted_buf);
    try std.testing.expectEqualStrings(sctp_payload, decrypted);

    // Replay of the exact same DTLS application_data record must be rejected
    try std.testing.expectError(
        Error.ReplayDetected,
        server.unprotectApplicationData(encrypted_rec, &decrypted_buf),
    );
}

test "SRTP and SRTCP protect/unprotect with AEAD_AES_128_GCM (RFC 7714)" {
    const master_key = [_]u8{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    };
    const master_salt = [_]u8{
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB,
    };

    var sender = try SrtpContext.init(.aead_aes_128_gcm, master_key, &master_salt);
    var receiver = try SrtpContext.init(.aead_aes_128_gcm, master_key, &master_salt);

    // Construct RTP packet: V=2, PT=96, seq=100, ts=0x11223344, ssrc=0xDEADBEEF + payload
    var rtp_pkt = [_]u8{
        0x80, 96, 0x00, 100,
        0x11, 0x22, 0x33, 0x44,
        0xDE, 0xAD, 0xBE, 0xEF,
        'o',  'p',  'u',  's',  '-', 'f', 'r', 'a', 'm', 'e',
    };

    var srtp_buf: [128]u8 = undefined;
    const srtp_pkt = try sender.protectRtp(&rtp_pkt, &srtp_buf);
    try std.testing.expectEqual(rtp_pkt.len + 16, srtp_pkt.len);
    try std.testing.expectEqual(PacketKind.rtp_rtcp, classifyPacket(srtp_pkt));

    var out_rtp_buf: [128]u8 = undefined;
    const recovered_rtp = try receiver.unprotectRtp(srtp_pkt, &out_rtp_buf);
    try std.testing.expectEqualSlices(u8, &rtp_pkt, recovered_rtp);

    // Replay rejection
    try std.testing.expectError(Error.ReplayDetected, receiver.unprotectRtp(srtp_pkt, &out_rtp_buf));

    // Tamper detection on next packet (seq = 101)
    rtp_pkt[3] = 101;
    const srtp_pkt2 = try sender.protectRtp(&rtp_pkt, &srtp_buf);
    srtp_buf[14] ^= 0xFF; // flip ciphertext byte
    try std.testing.expectError(Error.AuthenticationFailed, receiver.unprotectRtp(srtp_pkt2, &out_rtp_buf));

    // SRTCP protect / unprotect
    const rtcp_pkt = [_]u8{
        0x80, 200, 0x00, 0x06, // SR header
        0xDE, 0xAD, 0xBE, 0xEF, // SSRC
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // Sender info payload
    };
    var srtcp_buf: [128]u8 = undefined;
    const srtcp_pkt = try sender.protectRtcp(&rtcp_pkt, &srtcp_buf);
    try std.testing.expectEqual(rtcp_pkt.len + 16 + 4, srtcp_pkt.len);

    var out_rtcp_buf: [128]u8 = undefined;
    const recovered_rtcp = try receiver.unprotectRtcp(srtcp_pkt, &out_rtcp_buf);
    try std.testing.expectEqualSlices(u8, &rtcp_pkt, recovered_rtcp);

    // SRTCP replay rejection
    try std.testing.expectError(Error.ReplayDetected, receiver.unprotectRtcp(srtcp_pkt, &out_rtcp_buf));
}

test "SRTP and SRTCP protect/unprotect with AES128_CM_HMAC_SHA1_80 (RFC 3711)" {
    const master_key = [_]u8{
        0xE1, 0xF9, 0x7A, 0x0D, 0x3E, 0x01, 0x8B, 0xE0,
        0xD6, 0x4F, 0xA3, 0x2C, 0x06, 0xDE, 0x41, 0x39,
    };
    const master_salt = [_]u8{
        0x0E, 0xC6, 0x75, 0xAD, 0x49, 0x8A, 0xFE, 0xEB,
        0xB6, 0x96, 0x0B, 0x3A, 0xAB, 0xE6,
    };

    var sender = try SrtpContext.init(.aes128_cm_hmac_sha1_80, master_key, &master_salt);
    var receiver = try SrtpContext.init(.aes128_cm_hmac_sha1_80, master_key, &master_salt);

    var rtp_pkt = [_]u8{
        0x80, 96, 0x01, 0x00,
        0x00, 0x00, 0x10, 0x00,
        0xCA, 0xFE, 0xBA, 0xBE,
        'h',  '2',  '6',  '4',  '-', 'n', 'a', 'l', '-', 'u', 'n', 'i', 't',
    };

    var srtp_buf: [128]u8 = undefined;
    const srtp_pkt = try sender.protectRtp(&rtp_pkt, &srtp_buf);
    try std.testing.expectEqual(rtp_pkt.len + 10, srtp_pkt.len);

    var out_rtp_buf: [128]u8 = undefined;
    const recovered_rtp = try receiver.unprotectRtp(srtp_pkt, &out_rtp_buf);
    try std.testing.expectEqualSlices(u8, &rtp_pkt, recovered_rtp);

    // Replay detection
    try std.testing.expectError(Error.ReplayDetected, receiver.unprotectRtp(srtp_pkt, &out_rtp_buf));

    // Tamper detection on next packet
    rtp_pkt[3] = 0x01;
    const srtp_pkt2 = try sender.protectRtp(&rtp_pkt, &srtp_buf);
    srtp_buf[13] ^= 0x01;
    try std.testing.expectError(Error.AuthenticationFailed, receiver.unprotectRtp(srtp_pkt2, &out_rtp_buf));

    // SRTCP protect / unprotect under AES-CM + HMAC-SHA1-80
    const rtcp_pkt = [_]u8{
        0x81, 201, 0x00, 0x07,
        0xCA, 0xFE, 0xBA, 0xBE,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
    };
    var srtcp_buf: [128]u8 = undefined;
    const srtcp_pkt = try sender.protectRtcp(&rtcp_pkt, &srtcp_buf);
    try std.testing.expectEqual(rtcp_pkt.len + 4 + 10, srtcp_pkt.len);

    var out_rtcp_buf: [128]u8 = undefined;
    const recovered_rtcp = try receiver.unprotectRtcp(srtcp_pkt, &out_rtcp_buf);
    try std.testing.expectEqualSlices(u8, &rtcp_pkt, recovered_rtcp);

    // Tampered SRTCP tag rejection
    const srtcp_pkt2 = try sender.protectRtcp(&rtcp_pkt, &srtcp_buf);
    srtcp_buf[srtcp_pkt2.len - 1] ^= 0x80;
    try std.testing.expectError(Error.AuthenticationFailed, receiver.unprotectRtcp(srtcp_pkt2, &out_rtcp_buf));
}

test "DTLS fingerprint mismatch rejection and SRTP profile fallback negotiation" {
    const client_seed: [32]u8 = @splat(0x55);
    const server_seed: [32]u8 = @splat(0x66);
    const attacker_seed: [32]u8 = @splat(0x77);
    const attacker_cert = try Certificate.generateDeterministic(attacker_seed);

    // 1. Fingerprint mismatch must fail handshake and transition to .failed
    {
        var client = try DtlsSession.init(.client, .{}, client_seed);
        var server = try DtlsSession.init(.server, .{}, server_seed);
        try client.setExpectedRemoteFingerprint(attacker_cert.fingerprint());

        var f1_buf: [512]u8 = undefined;
        const f1 = try client.startHandshake(&f1_buf);
        var sf_buf: [1024]u8 = undefined;
        const sf = try server.processHandshakePacket(f1, &sf_buf);
        var cf2_buf: [1024]u8 = undefined;
        try std.testing.expectError(
            Error.FingerprintMismatch,
            client.processHandshakePacket(sf, &cf2_buf),
        );
        try std.testing.expectEqual(TransportState.failed, client.state);
    }

    // 2. Fallback to aes128_cm_hmac_sha1_80 when enable_srtp_aes_gcm = false
    {
        var client = try DtlsSession.init(.client, .{ .enable_srtp_aes_gcm = false }, client_seed);
        var server = try DtlsSession.init(.server, .{ .enable_srtp_aes_gcm = true }, server_seed);

        var f1_buf: [512]u8 = undefined;
        const f1 = try client.startHandshake(&f1_buf);
        var sf_buf: [1024]u8 = undefined;
        const sf = try server.processHandshakePacket(f1, &sf_buf);
        var cf2_buf: [1024]u8 = undefined;
        const cf2 = try client.processHandshakePacket(sf, &cf2_buf);
        var sf2_buf: [512]u8 = undefined;
        const sf2 = try server.processHandshakePacket(cf2, &sf2_buf);
        var fin_buf: [128]u8 = undefined;
        _ = try client.processHandshakePacket(sf2, &fin_buf);

        try std.testing.expectEqual(SrtpProfile.aes128_cm_hmac_sha1_80, client.selected_profile.?);
        try std.testing.expectEqual(SrtpProfile.aes128_cm_hmac_sha1_80, server.selected_profile.?);

        // Verify SRTP context initialized directly from negotiated DtlsSession SrtpKeyMaterial
        var client_tx = try SrtpContext.initFromKeyMaterial(client.srtp_keys.?, .client);
        var server_rx = try SrtpContext.initFromKeyMaterial(server.srtp_keys.?, .client);

        const rtp = [_]u8{
            0x80, 111, 0x00, 0x01,
            0x00, 0x01, 0x02, 0x03,
            0x12, 0x34, 0x56, 0x78,
            0xAA, 0xBB, 0xCC, 0xDD,
        };
        var srtp_buf: [128]u8 = undefined;
        const protected = try client_tx.protectRtp(&rtp, &srtp_buf);
        var dec_buf: [128]u8 = undefined;
        const unprotected = try server_rx.unprotectRtp(protected, &dec_buf);
        try std.testing.expectEqualSlices(u8, &rtp, unprotected);
    }
}

test "SRTP 64-packet sliding replay window out-of-order acceptance and RTP header extensions" {
    const master_key: [16]u8 = @splat(0x3C);
    const master_salt: [12]u8 = @splat(0x5A);

    var sender = try SrtpContext.init(.aead_aes_128_gcm, master_key, &master_salt);
    var receiver = try SrtpContext.init(.aead_aes_128_gcm, master_key, &master_salt);

    // RTP packet with 1 CSRC (CC=1) and 1-word header extension (X=1):
    // Fixed(12) + CSRC(4) + ExtHeader(4) + ExtData(4) = 24 bytes header
    var rtp_ext = [_]u8{
        0x91, 96, 0x00, 10, // V=2, X=1, CC=1, PT=96, seq=10
        0x00, 0x00, 0x03, 0xE8, // timestamp
        0x11, 0x22, 0x33, 0x44, // SSRC
        0x55, 0x66, 0x77, 0x88, // CSRC[0]
        0xBE, 0xDE, 0x00, 0x01, // Extension profile 0xBEDE, length = 1 word (4 bytes)
        0x10, 0xFF, 0x00, 0x00, // Extension data
        'v',  'p',  '8',  '-',  'k', 'e', 'y',
    };
    try std.testing.expectEqual(@as(usize, 24), try parseRtpHeaderLength(&rtp_ext));

    var pkt10_buf: [128]u8 = undefined;
    const pkt10 = try sender.protectRtp(&rtp_ext, &pkt10_buf);

    rtp_ext[3] = 25; // jump ahead to seq=25
    var pkt25_buf: [128]u8 = undefined;
    const pkt25 = try sender.protectRtp(&rtp_ext, &pkt25_buf);

    rtp_ext[3] = 90; // jump ahead by 65 from seq=25 (so seq=10 is now outside 64-packet window: 90 - 10 = 80 >= 64)
    var pkt90_buf: [128]u8 = undefined;
    const pkt90 = try sender.protectRtp(&rtp_ext, &pkt90_buf);

    var dec_buf: [128]u8 = undefined;
    // Receive seq=25 first, then out-of-order seq=10 (within 64 window: 25 - 10 = 15 < 64)
    _ = try receiver.unprotectRtp(pkt25, &dec_buf);
    const rec10 = try receiver.unprotectRtp(pkt10, &dec_buf);
    try std.testing.expectEqual(@as(u8, 10), rec10[3]);

    // Duplicate of seq=10 must be rejected
    try std.testing.expectError(Error.ReplayDetected, receiver.unprotectRtp(pkt10, &dec_buf));

    // Receive seq=90; now any packet with seq <= 26 is outside the 64-packet window
    _ = try receiver.unprotectRtp(pkt90, &dec_buf);
    try std.testing.expect(!receiver.rtp_replay.check(26));
}
