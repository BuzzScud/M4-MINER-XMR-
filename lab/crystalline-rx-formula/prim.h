/* prim.h — the Crystalline primitives R is built from.
 *
 * Each function is one operation of the Crystalline Abacus
 * (crystalline-main/math/src/bigint), evaluated at word width so the
 * 256 MiB Argon2 pass takes seconds. xcheck.c runs the real abacus calls on
 * random operands and checks every one of these against them.
 *
 *   ring Z/2^64      add64 sub64 mul64 neg64   abacus_mod_add/_sub/_mul, modulus 2^64
 *   high product     mulh smulh                abacus_mul, then abacus_shift_right(base 2, 64)
 *   base-2 beads     xor64 and64 or64          per bead: (a+b) mod 2, a*b, a+b-ab
 *   bead shift       shl shr                   abacus_shift_left / abacus_shift_right, base 2
 *   clock rotation   rotr rotl                 beads move n places round a 64-position clock
 *   exact division   rcp                       abacus_div(2^(63+bitlen d), d)
 *   GF(2^8)          gmul ginv                 base-2 beads, no carry, reduced by x^8+x^4+x^3+x+1
 *   binary64         fadd fsub fmul fdiv fsqrt exact integer result, rounded to 53 beads
 */
#ifndef PRIM_H
#define PRIM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;
typedef unsigned __int128 u128;
typedef __int128 i128;

/* ---- ring Z/2^64 ---- */
static inline u64 add64(u64 a, u64 b) { return a + b; }
static inline u64 sub64(u64 a, u64 b) { return a - b; }
static inline u64 mul64(u64 a, u64 b) { return a * b; }
static inline u64 neg64(u64 a) { return (u64)0 - a; }

/* ---- high half of the exact product ---- */
static inline u64 mulh(u64 a, u64 b) { return (u64)(((u128)a * b) >> 64); }
static inline u64 smulh(u64 a, u64 b) { return (u64)(((i128)(i64)a * (i64)b) >> 64); }

/* ---- base-2 beads ---- */
static inline u64 xor64(u64 a, u64 b) { return a ^ b; }
static inline u64 and64(u64 a, u64 b) { return a & b; }
static inline u64 or64(u64 a, u64 b) { return a | b; }
static inline u64 shl(u64 a, unsigned n) { return a << n; }
static inline u64 shr(u64 a, unsigned n) { return a >> n; }

/* ---- clock rotation ---- */
static inline u64 rotr(u64 x, unsigned n) { n &= 63; return n ? (x >> n) | (x << (64 - n)) : x; }
static inline u64 rotl(u64 x, unsigned n) { n &= 63; return n ? (x << n) | (x >> (64 - n)) : x; }

static inline u64 sext32(u32 x) { return (u64)(i64)(i32)x; }

/* floor(2^(63 + bitlen d) / d): the largest power-of-two quotient that fits 64 bits */
static inline u64 rcp(u64 d) {
    int bl = 64 - __builtin_clzll(d);
    return (u64)((((u128)1) << (63 + bl)) / d);
}

/* little-endian byte groups (host must be little-endian; crx checks) */
static inline u64 le64(const u8* p) { u64 v; memcpy(&v, p, 8); return v; }
static inline u32 le32(const u8* p) { u32 v; memcpy(&v, p, 4); return v; }
static inline void st64(u8* p, u64 v) { memcpy(p, &v, 8); }
static inline void st32(u8* p, u32 v) { memcpy(p, &v, 4); }

/* ---- GF(2^8): the AES field ---- */
static inline u8 gmul(u8 a, u8 b) {
    u8 p = 0;
    while (b) {
        if (b & 1) p ^= a;
        a = (u8)((a << 1) ^ ((a & 0x80) ? 0x1b : 0));
        b >>= 1;
    }
    return p;
}
static inline u8 ginv(u8 x) { /* x^254 = x^-1 */
    u8 r = 1, e = x;
    for (int k = 254; k; k >>= 1) {
        if (k & 1) r = gmul(r, e);
        e = gmul(e, e);
    }
    return x ? r : 0;
}
static inline u8 rot8(u8 b, int n) { return (u8)((b << n) | (b >> (8 - n))); }

static u8 SB[256], ISB[256], X2[256], X3[256], X9[256], X11[256], X13[256], X14[256];

/* S-box = affine map of the field inverse; no table is typed in */
static void aes_tables(void) {
    for (int x = 0; x < 256; x++) {
        u8 b = ginv((u8)x);
        u8 s = b ^ rot8(b, 1) ^ rot8(b, 2) ^ rot8(b, 3) ^ rot8(b, 4) ^ 0x63;
        SB[x] = s;
        ISB[s] = (u8)x;
        X2[x] = gmul((u8)x, 2);   X3[x] = gmul((u8)x, 3);   X9[x] = gmul((u8)x, 9);
        X11[x] = gmul((u8)x, 11); X13[x] = gmul((u8)x, 13); X14[x] = gmul((u8)x, 14);
    }
}

/* one round, x86 AESENC order: ShiftRows, SubBytes, MixColumns, + key */
static inline void aesenc(u8 s[16], const u8 k[16]) {
    u8 t[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) t[4 * c + r] = SB[s[4 * ((c + r) & 3) + r]];
    for (int c = 0; c < 4; c++) {
        u8 a0 = t[4 * c], a1 = t[4 * c + 1], a2 = t[4 * c + 2], a3 = t[4 * c + 3];
        s[4 * c]     = X2[a0] ^ X3[a1] ^ a2 ^ a3 ^ k[4 * c];
        s[4 * c + 1] = a0 ^ X2[a1] ^ X3[a2] ^ a3 ^ k[4 * c + 1];
        s[4 * c + 2] = a0 ^ a1 ^ X2[a2] ^ X3[a3] ^ k[4 * c + 2];
        s[4 * c + 3] = X3[a0] ^ a1 ^ a2 ^ X2[a3] ^ k[4 * c + 3];
    }
}
/* one round, x86 AESDEC order: InvShiftRows, InvSubBytes, InvMixColumns, + key */
static inline void aesdec(u8 s[16], const u8 k[16]) {
    u8 t[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) t[4 * c + r] = ISB[s[4 * ((c - r) & 3) + r]];
    for (int c = 0; c < 4; c++) {
        u8 a0 = t[4 * c], a1 = t[4 * c + 1], a2 = t[4 * c + 2], a3 = t[4 * c + 3];
        s[4 * c]     = X14[a0] ^ X11[a1] ^ X13[a2] ^ X9[a3] ^ k[4 * c];
        s[4 * c + 1] = X9[a0] ^ X14[a1] ^ X11[a2] ^ X13[a3] ^ k[4 * c + 1];
        s[4 * c + 2] = X13[a0] ^ X9[a1] ^ X14[a2] ^ X11[a3] ^ k[4 * c + 2];
        s[4 * c + 3] = X11[a0] ^ X13[a1] ^ X9[a2] ^ X14[a3] ^ k[4 * c + 3];
    }
}

/* ---- Blake2b (RFC 7693). IV = frac(sqrt(first 8 primes)) * 2^64; xcheck re-derives it ---- */
static const u64 B2IV[8] = {
    0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
    0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull};
static const u8 SIGMA[10][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
    {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
    {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
    {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0}};

#define B2G(a, b, c, d, x, y)                         \
    do {                                              \
        a = add64(add64(a, b), x); d = rotr(d ^ a, 32); \
        c = add64(c, d);           b = rotr(b ^ c, 24); \
        a = add64(add64(a, b), y); d = rotr(d ^ a, 16); \
        c = add64(c, d);           b = rotr(b ^ c, 63); \
    } while (0)

static void b2_compress(u64 h[8], const u8 blk[128], u64 t, int last) {
    u64 m[16], v[16];
    for (int i = 0; i < 16; i++) m[i] = le64(blk + 8 * i);
    for (int i = 0; i < 8; i++) { v[i] = h[i]; v[i + 8] = B2IV[i]; }
    v[12] ^= t;
    if (last) v[14] = ~v[14];
    for (int r = 0; r < 12; r++) {
        const u8* s = SIGMA[r % 10];
        B2G(v[0], v[4], v[8], v[12], m[s[0]], m[s[1]]);
        B2G(v[1], v[5], v[9], v[13], m[s[2]], m[s[3]]);
        B2G(v[2], v[6], v[10], v[14], m[s[4]], m[s[5]]);
        B2G(v[3], v[7], v[11], v[15], m[s[6]], m[s[7]]);
        B2G(v[0], v[5], v[10], v[15], m[s[8]], m[s[9]]);
        B2G(v[1], v[6], v[11], v[12], m[s[10]], m[s[11]]);
        B2G(v[2], v[7], v[8], v[13], m[s[12]], m[s[13]]);
        B2G(v[3], v[4], v[9], v[14], m[s[14]], m[s[15]]);
    }
    for (int i = 0; i < 8; i++) h[i] ^= v[i] ^ v[i + 8];
}

static void blake2b(u8* out, size_t outlen, const u8* in, size_t inlen) {
    u64 h[8];
    u8 blk[128], full[64];
    size_t off = 0;
    memcpy(h, B2IV, sizeof h);
    h[0] ^= 0x01010000ull ^ (u64)outlen;
    while (inlen - off > 128) {
        b2_compress(h, in + off, off + 128, 0);
        off += 128;
    }
    memset(blk, 0, sizeof blk);
    if (inlen - off) memcpy(blk, in + off, inlen - off);
    b2_compress(h, blk, inlen, 1);
    for (int i = 0; i < 8; i++) st64(full + 8 * i, h[i]);
    memcpy(out, full, outlen);
}

/* ---- binary64 from integers: exact result, then one rounding to 53 beads ----
 * RM uses the RandomX/MXCSR code: 0 nearest-even, 1 toward -inf, 2 toward +inf, 3 toward 0 */
static int RM;
static long fp_anomaly; /* inf/nan/overflow ever seen; must stay 0 */

static inline int bitlen128(u128 x) {
    u64 hi = (u64)(x >> 64);
    if (hi) return 128 - __builtin_clzll(hi);
    return x ? 64 - __builtin_clzll((u64)x) : 0;
}

/* value = (M + frac) * 2^E, frac in (0,1) when sticky, else exact */
static u64 fpack(int sign, u128 M, int E, int sticky) {
    if (M == 0) {
        if (sticky) fp_anomaly++;
        return (u64)sign << 63;
    }
    int n = bitlen128(M);
    int lsb = E + n - 53;
    if (lsb < -1074) lsb = -1074;
    int sh = lsb - E;
    u128 q;
    int up = 0;
    if (sh > 0) {
        if (sh > 127) { fp_anomaly++; sh = 127; }
        q = M >> sh;
        u128 rem = M & ((((u128)1) << sh) - 1), half = ((u128)1) << (sh - 1);
        int inexact = rem != 0 || sticky;
        switch (RM) {
        case 0: up = rem > half || (rem == half && (sticky || (q & 1))); break;
        case 1: up = inexact && sign; break;
        case 2: up = inexact && !sign; break;
        default: break;
        }
    } else {
        if (sticky) fp_anomaly++;
        q = M << (-sh);
    }
    q += (u128)up;
    if (q >> 53) { q >>= 1; lsb++; }
    if (q < (((u128)1) << 52)) return ((u64)sign << 63) | (u64)q; /* subnormal */
    int be = lsb + 1075;
    if (be >= 2047) { fp_anomaly++; return ((u64)sign << 63) | 0x7FF0000000000000ull; }
    return ((u64)sign << 63) | ((u64)be << 52) | ((u64)q & 0xFFFFFFFFFFFFFull);
}

typedef struct { int s; u64 m; int e; int cls; } fp_t; /* cls: 0 zero, 1 finite, 2 inf/nan */

static inline fp_t funpack(u64 x) {
    fp_t r;
    int be = (int)((x >> 52) & 0x7FF);
    u64 f = x & 0xFFFFFFFFFFFFFull;
    r.s = (int)(x >> 63);
    if (be == 0x7FF) { r.cls = 2; r.m = f; r.e = 0; fp_anomaly++; return r; }
    if (be == 0) { r.m = f; r.e = -1074; r.cls = f ? 1 : 0; return r; }
    r.m = f | (1ull << 52); r.e = be - 1075; r.cls = 1;
    return r;
}
static inline void fnorm(fp_t* a) { while (!(a->m >> 52)) { a->m <<= 1; a->e--; } }

static u64 fadd(u64 x, u64 y) {
    fp_t a = funpack(x), b = funpack(y);
    if (a.cls == 0 && b.cls == 0) {
        if (a.s == b.s) return (u64)a.s << 63;
        return RM == 1 ? 1ull << 63 : 0;
    }
    if (a.cls == 0) return y;
    if (b.cls == 0) return x;
    if (a.e < b.e) { fp_t t = a; a = b; b = t; }
    int d = a.e - b.e;
    if (d > 70) { /* b sits wholly below a's last bead: it only decides rounding */
        u128 M = (u128)a.m << 3;
        return fpack(a.s, a.s == b.s ? M : M - 1, a.e - 3, 1);
    }
    u128 A = (u128)a.m << d, B = b.m;
    if (a.s == b.s) return fpack(a.s, A + B, b.e, 0);
    if (A > B) return fpack(a.s, A - B, b.e, 0);
    if (B > A) return fpack(b.s, B - A, b.e, 0);
    return RM == 1 ? 1ull << 63 : 0;
}
static inline u64 fsub(u64 x, u64 y) { return fadd(x, y ^ (1ull << 63)); }

static u64 fmul(u64 x, u64 y) {
    fp_t a = funpack(x), b = funpack(y);
    int s = a.s ^ b.s;
    if (a.cls == 0 || b.cls == 0) return (u64)s << 63;
    return fpack(s, (u128)a.m * b.m, a.e + b.e, 0);
}

static u64 fdiv(u64 x, u64 y) {
    fp_t a = funpack(x), b = funpack(y);
    int s = a.s ^ b.s;
    if (b.cls == 0) { fp_anomaly++; return ((u64)s << 63) | 0x7FF0000000000000ull; }
    if (a.cls == 0) return (u64)s << 63;
    fnorm(&a);
    fnorm(&b);
    u128 N = (u128)a.m << 74;
    return fpack(s, N / b.m, a.e - b.e - 74, (N % b.m) != 0);
}

static u128 isqrt128(u128 n, u128* rem) { /* digit-by-digit, base 4 */
    u128 res = 0, bit = ((u128)1) << 126;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) { n -= res + bit; res = (res >> 1) + bit; }
        else res >>= 1;
        bit >>= 2;
    }
    *rem = n;
    return res;
}

static u64 fsqrt(u64 x) {
    fp_t a = funpack(x);
    if (a.cls == 0) return x;
    if (a.s) { fp_anomaly++; return 0x7FF8000000000000ull; }
    fnorm(&a);
    u128 m = a.m, rem;
    int e = a.e;
    if (e & 1) { m <<= 1; e -= 1; }
    u128 s = isqrt128(m << 72, &rem);
    return fpack(0, s, (e - 72) / 2, rem != 0);
}

static u64 fcvt_i32(u32 v) {
    i32 x = (i32)v;
    if (!x) return 0;
    int s = x < 0;
    u64 m = s ? (u64)(-(i64)x) : (u64)x;
    return fpack(s, m, 0, 0);
}

#endif
