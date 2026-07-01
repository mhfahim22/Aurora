#include "std/crypto.hpp"
#include <cstring>
#include <cstdlib>

/* ═══════════════════════════════════════════════════════════════
   SHA-256 (FIPS 180-4)
   ═══════════════════════════════════════════════════════════════ */
struct Sha256Ctx {
    uint32_t state[8];
    uint64_t count;
    unsigned char buffer[64];
    size_t buflen;
};

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t ror(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t sig0(uint32_t x) { return ror(x, 2) ^ ror(x, 13) ^ ror(x, 22); }
static inline uint32_t sig1(uint32_t x) { return ror(x, 6) ^ ror(x, 11) ^ ror(x, 25); }
static inline uint32_t om0(uint32_t x) { return ror(x, 7) ^ ror(x, 18) ^ (x >> 3); }
static inline uint32_t om1(uint32_t x) { return ror(x, 17) ^ ror(x, 19) ^ (x >> 10); }

static void sha256_transform(Sha256Ctx* ctx) {
    uint32_t W[64], a, b, c, d, e, f, g, h, T1, T2;
    for (int i = 0; i < 16; i++)
        W[i] = ((uint32_t)ctx->buffer[i * 4] << 24) | ((uint32_t)ctx->buffer[i * 4 + 1] << 16) |
               ((uint32_t)ctx->buffer[i * 4 + 2] << 8) | (uint32_t)ctx->buffer[i * 4 + 3];
    for (int i = 16; i < 64; i++)
        W[i] = om1(W[i - 2]) + W[i - 7] + om0(W[i - 15]) + W[i - 16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (int i = 0; i < 64; i++) {
        T1 = h + sig1(e) + ch(e, f, g) + K256[i] + W[i];
        T2 = sig0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(Sha256Ctx* ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->count = 0; ctx->buflen = 0;
}

static void sha256_update(Sha256Ctx* ctx, const unsigned char* data, size_t len) {
    ctx->count += (uint64_t)len * 8;
    while (len > 0) {
        size_t space = 64 - ctx->buflen;
        size_t copy = len < space ? len : space;
        memcpy(ctx->buffer + ctx->buflen, data, copy);
        ctx->buflen += copy; data += copy; len -= copy;
        if (ctx->buflen == 64) {
            sha256_transform(ctx);
            ctx->buflen = 0;
        }
    }
}

static void sha256_final(Sha256Ctx* ctx, unsigned char* out) {
    ctx->buffer[ctx->buflen] = 0x80;
    if (ctx->buflen >= 56) {
        memset(ctx->buffer + ctx->buflen + 1, 0, 64 - ctx->buflen - 1);
        sha256_transform(ctx);
        ctx->buflen = 0;
    }
    memset(ctx->buffer + ctx->buflen + 1, 0, 56 - ctx->buflen - 1);
    for (int i = 0; i < 8; i++)
        ctx->buffer[56 + i] = (unsigned char)(ctx->count >> (56 - i * 8));
    sha256_transform(ctx);
    for (int i = 0; i < 8; i++) {
        out[i * 4]     = (unsigned char)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
}

void aurora_sha256(const unsigned char* data, size_t len, unsigned char* out) {
    Sha256Ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

/* ═══════════════════════════════════════════════════════════════
   HMAC-SHA256 (RFC 2104)
   ═══════════════════════════════════════════════════════════════ */
void aurora_hmac_sha256(const unsigned char* key, size_t key_len,
                        const unsigned char* data, size_t data_len,
                        unsigned char* out) {
    unsigned char k[64];
    Sha256Ctx ctx;
    if (key_len > 64) {
        aurora_sha256(key, key_len, k);
        memset(k + 32, 0, 32);
    } else {
        memcpy(k, key, key_len);
        memset(k + key_len, 0, 64 - key_len);
    }
    for (size_t i = 0; i < 64; i++) k[i] ^= 0x36;
    sha256_init(&ctx);
    sha256_update(&ctx, k, 64);
    sha256_update(&ctx, data, data_len);
    unsigned char inner[32];
    sha256_final(&ctx, inner);
    for (size_t i = 0; i < 64; i++) k[i] ^= (0x36 ^ 0x5c);
    sha256_init(&ctx);
    sha256_update(&ctx, k, 64);
    sha256_update(&ctx, inner, 32);
    sha256_final(&ctx, out);
}

/* ═══════════════════════════════════════════════════════════════
   AES-128/256 (ECB + CBC) — compact implementation
   ═══════════════════════════════════════════════════════════════ */
static const unsigned char sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const unsigned char rsbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static uint32_t aes_rcon(uint32_t i) {
    static const uint8_t RCON[] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};
    if (i >= 10) return 0;
    return (uint32_t)RCON[i] << 24;
}

static void aes_key_expansion(const unsigned char* key, int nk, uint32_t* w) {
    int nr = nk + 6;
    int i = 0;
    while (i < nk) {
        w[i] = ((uint32_t)key[i * 4] << 24) | ((uint32_t)key[i * 4 + 1] << 16) |
               ((uint32_t)key[i * 4 + 2] << 8) | (uint32_t)key[i * 4 + 3];
        i++;
    }
    i = nk;
    while (i < 4 * (nr + 1)) {
        uint32_t temp = w[i - 1];
        if (i % nk == 0) {
            temp = ((uint32_t)sbox[(temp >> 16) & 0xff] << 24) |
                   ((uint32_t)sbox[(temp >> 8) & 0xff] << 16) |
                   ((uint32_t)sbox[temp & 0xff] << 8) |
                   (uint32_t)sbox[temp >> 24];
            temp ^= aes_rcon(i / nk - 1);
        } else if (nk > 6 && (i % nk) == 4) {
            temp = ((uint32_t)sbox[temp >> 24] << 24) |
                   ((uint32_t)sbox[(temp >> 16) & 0xff] << 16) |
                   ((uint32_t)sbox[(temp >> 8) & 0xff] << 8) |
                   ((uint32_t)sbox[temp & 0xff]);
        }
        w[i] = w[i - nk] ^ temp;
        i++;
    }
}

static void aes_add_round_key(unsigned char state[16], const uint32_t* rk, int round) {
    for (int i = 0; i < 4; i++) {
        uint32_t k = rk[round * 4 + i];
        state[i * 4]     ^= (unsigned char)(k >> 24);
        state[i * 4 + 1] ^= (unsigned char)(k >> 16);
        state[i * 4 + 2] ^= (unsigned char)(k >> 8);
        state[i * 4 + 3] ^= (unsigned char)(k);
    }
}

static void aes_sub_bytes(unsigned char state[16]) {
    for (int i = 0; i < 16; i++) state[i] = sbox[state[i]];
}

static void aes_inv_sub_bytes(unsigned char state[16]) {
    for (int i = 0; i < 16; i++) state[i] = rsbox[state[i]];
}

static void aes_shift_rows(unsigned char state[16]) {
    unsigned char t;
    t = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = t;
    t = state[2]; state[2] = state[10]; state[10] = t;
    t = state[6]; state[6] = state[14]; state[14] = t;
    t = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = t;
}

static void aes_inv_shift_rows(unsigned char state[16]) {
    unsigned char t;
    t = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = t;
    t = state[2]; state[2] = state[10]; state[10] = t;
    t = state[6]; state[6] = state[14]; state[14] = t;
    t = state[3]; state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = t;
}

static unsigned char gf_mul(unsigned char a, unsigned char b) {
    unsigned char p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        unsigned char hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

static void aes_mix_columns(unsigned char state[16]) {
    for (int i = 0; i < 4; i++) {
        unsigned char* col = state + i * 4;
        unsigned char a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
        col[0] = gf_mul(2, a0) ^ gf_mul(3, a1) ^ a2 ^ a3;
        col[1] = a0 ^ gf_mul(2, a1) ^ gf_mul(3, a2) ^ a3;
        col[2] = a0 ^ a1 ^ gf_mul(2, a2) ^ gf_mul(3, a3);
        col[3] = gf_mul(3, a0) ^ a1 ^ a2 ^ gf_mul(2, a3);
    }
}

static void aes_inv_mix_columns(unsigned char state[16]) {
    for (int i = 0; i < 4; i++) {
        unsigned char* col = state + i * 4;
        unsigned char a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
        col[0] = gf_mul(14, a0) ^ gf_mul(11, a1) ^ gf_mul(13, a2) ^ gf_mul(9, a3);
        col[1] = gf_mul(9, a0) ^ gf_mul(14, a1) ^ gf_mul(11, a2) ^ gf_mul(13, a3);
        col[2] = gf_mul(13, a0) ^ gf_mul(9, a1) ^ gf_mul(14, a2) ^ gf_mul(11, a3);
        col[3] = gf_mul(11, a0) ^ gf_mul(13, a1) ^ gf_mul(9, a2) ^ gf_mul(14, a3);
    }
}

static void aes_encrypt_block(const unsigned char* in, unsigned char* out, const uint32_t* rk, int nr) {
    unsigned char state[16];
    memcpy(state, in, 16);
    aes_add_round_key(state, rk, 0);
    for (int round = 1; round < nr; round++) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(state, rk, round);
    }
    aes_sub_bytes(state);
    aes_shift_rows(state);
    aes_add_round_key(state, rk, nr);
    memcpy(out, state, 16);
}

static void aes_decrypt_block(const unsigned char* in, unsigned char* out, const uint32_t* rk, int nr) {
    unsigned char state[16];
    memcpy(state, in, 16);
    aes_add_round_key(state, rk, nr);
    for (int round = nr - 1; round > 0; round--) {
        aes_inv_shift_rows(state);
        aes_inv_sub_bytes(state);
        aes_add_round_key(state, rk, round);
        aes_inv_mix_columns(state);
    }
    aes_inv_shift_rows(state);
    aes_inv_sub_bytes(state);
    aes_add_round_key(state, rk, 0);
    memcpy(out, state, 16);
}

static int aes_ecb_encrypt(const unsigned char* key, int nk,
                           const unsigned char* input, size_t len,
                           unsigned char* output) {
    int nr = nk + 6;
    uint32_t rk[4 * (14 + 1)];
    aes_key_expansion(key, nk, rk);
    size_t blocks = len / 16;
    for (size_t i = 0; i < blocks; i++)
        aes_encrypt_block(input + i * 16, output + i * 16, rk, nr);
    return (int)blocks * 16;
}

static int aes_ecb_decrypt(const unsigned char* key, int nk,
                           const unsigned char* input, size_t len,
                           unsigned char* output) {
    int nr = nk + 6;
    uint32_t rk[4 * (14 + 1)];
    aes_key_expansion(key, nk, rk);
    size_t blocks = len / 16;
    for (size_t i = 0; i < blocks; i++)
        aes_decrypt_block(input + i * 16, output + i * 16, rk, nr);
    return (int)blocks * 16;
}

static void aes_xor_block(unsigned char* a, const unsigned char* b) {
    for (int i = 0; i < 16; i++) a[i] ^= b[i];
}

static int aes_cbc_encrypt(const unsigned char* key, int nk,
                           const unsigned char* iv,
                           const unsigned char* input, size_t len,
                           unsigned char* output) {
    int nr = nk + 6;
    uint32_t rk[4 * (14 + 1)];
    aes_key_expansion(key, nk, rk);
    unsigned char block[16];
    memcpy(block, iv, 16);
    size_t blocks = len / 16;
    for (size_t i = 0; i < blocks; i++) {
        aes_xor_block(block, input + i * 16);
        aes_encrypt_block(block, output + i * 16, rk, nr);
        memcpy(block, output + i * 16, 16);
    }
    return (int)blocks * 16;
}

static int aes_cbc_decrypt(const unsigned char* key, int nk,
                           const unsigned char* iv,
                           const unsigned char* input, size_t len,
                           unsigned char* output) {
    int nr = nk + 6;
    uint32_t rk[4 * (14 + 1)];
    aes_key_expansion(key, nk, rk);
    unsigned char block[16], next[16];
    memcpy(block, iv, 16);
    size_t blocks = len / 16;
    for (size_t i = 0; i < blocks; i++) {
        memcpy(next, input + i * 16, 16);
        aes_decrypt_block(input + i * 16, output + i * 16, rk, nr);
        aes_xor_block(output + i * 16, block);
        memcpy(block, next, 16);
    }
    return (int)blocks * 16;
}

int aurora_aes128_ecb_encrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output) {
    return aes_ecb_encrypt(key, 4, input, len, output);
}
int aurora_aes128_ecb_decrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output) {
    return aes_ecb_decrypt(key, 4, input, len, output);
}
int aurora_aes256_ecb_encrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output) {
    return aes_ecb_encrypt(key, 8, input, len, output);
}
int aurora_aes256_ecb_decrypt(const unsigned char* key, const unsigned char* input,
                              size_t len, unsigned char* output) {
    return aes_ecb_decrypt(key, 8, input, len, output);
}
int aurora_aes128_cbc_encrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output) {
    return aes_cbc_encrypt(key, 4, iv, input, len, output);
}
int aurora_aes128_cbc_decrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output) {
    return aes_cbc_decrypt(key, 4, iv, input, len, output);
}
int aurora_aes256_cbc_encrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output) {
    return aes_cbc_encrypt(key, 8, iv, input, len, output);
}
int aurora_aes256_cbc_decrypt(const unsigned char* key, const unsigned char* iv,
                              const unsigned char* input, size_t len,
                              unsigned char* output) {
    return aes_cbc_decrypt(key, 8, iv, input, len, output);
}

/* ═══════════════════════════════════════════════════════════════
   Base64
   ═══════════════════════════════════════════════════════════════ */
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int aurora_base64_encode(const unsigned char* data, size_t len,
                         char* out, size_t out_size) {
    size_t needed = ((len + 2) / 3) * 4 + 1;
    if (out_size < needed) return -1;
    size_t i = 0, j = 0;
    for (; i < len; i += 3) {
        unsigned int a = data[i];
        unsigned int b = i + 1 < len ? data[i + 1] : 0;
        unsigned int c = i + 2 < len ? data[i + 2] : 0;
        out[j++] = b64_table[a >> 2];
        out[j++] = b64_table[((a & 3) << 4) | (b >> 4)];
        out[j++] = i + 1 < len ? b64_table[((b & 0x0f) << 2) | (c >> 6)] : '=';
        out[j++] = i + 2 < len ? b64_table[c & 0x3f] : '=';
    }
    out[j] = '\0';
    return (int)j;
}

int aurora_base64_decode(const char* in, unsigned char* out, size_t out_size) {
    size_t in_len = strlen(in);
    if (in_len % 4) return -1;
    size_t out_len = in_len / 4 * 3;
    if (in[in_len - 1] == '=') out_len--;
    if (in[in_len - 2] == '=') out_len--;
    if (out_size < out_len) return -1;

    static signed char b64_rev[256];
    static int b64_init = 0;
    if (!b64_init) {
        for (int i = 0; i < 256; i++) b64_rev[i] = -1;
        for (int i = 0; b64_table[i]; i++) b64_rev[(unsigned char)b64_table[i]] = i;
        b64_init = 1;
    }
    size_t j = 0;
    for (size_t i = 0; i < in_len; i += 4) {
        unsigned char a[4];
        for (int k = 0; k < 4; k++) a[k] = (unsigned char)b64_rev[(unsigned char)in[i + k]];
        if (j < out_size) out[j++] = (a[0] << 2) | (a[1] >> 4);
        if (j < out_size && in[i + 2] != '=') out[j++] = (a[1] << 4) | (a[2] >> 2);
        if (j < out_size && in[i + 3] != '=') out[j++] = (a[2] << 6) | a[3];
    }
    return (int)j;
}

/* ═══════════════════════════════════════════════════════════════
   Hex encoding
   ═══════════════════════════════════════════════════════════════ */
char* aurora_hex_encode(const unsigned char* data, size_t len) {
    char* out = (char*)malloc(len * 2 + 1);
    if (!out) return nullptr;
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex[(data[i] >> 4) & 0x0f];
        out[i * 2 + 1] = hex[data[i] & 0x0f];
    }
    out[len * 2] = '\0';
    return out;
}
