/* crx.c — R = F(K, H), written only in the Crystalline primitives of prim.h.
 *
 *   cc -O2 -o crx crx.c
 *   ./crx "test key 000" "This is a test" 639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f
 *
 * No hash library, no hardware AES, no FPU: Blake2b, AES, Argon2d and IEEE
 * binary64 are all spelled out below from ring / bead / field arithmetic.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "prim.h"

/* ---- parameters (the screen pins the ones marked) ---- */
#define ARGON_BLOCKS   262144u               /* 1 KiB blocks: 256 MiB cache */
#define ARGON_PASSES   3u
#define ARGON_SALT     "RandomX\x03"
#define CACHE_ACCESSES 8
#define SS_LATENCY     170
#define SS_MAX         (3 * SS_LATENCY + 2)
#define CYCLE_MAP      (SS_LATENCY + 4)
#define CACHE_ITEMS    (ARGON_BLOCKS * 16u)  /* 64-byte lines */
#define EXTRA_ITEMS    (33554368u / 64u)
#define SP_BYTES       2097152u              /* screen: SPA = M & 0x1FFFC0 */
#define L1_MASK        0x3FF8u
#define L2_MASK        0x3FFF8u
#define L3_MASK        0x1FFFF8u
#define L3_LINE_MASK   0x1FFFC0u
#define LINE_ALIGN     0x7FFFFFC0u
#define PROG_SIZE      256
#define PROG_BYTES     (128 + 8 * PROG_SIZE)
#define PROG_ITERS     2048                  /* screen: IC 0 (2047 left) */
#define PROG_COUNT     8
#define JUMP_OFFSET    8
#define STORE_L3_COND  14

static u64* cache;
static u8* sp;

static double now(void) { return (double)clock() / CLOCKS_PER_SEC; }

static void hex(const char* label, const u8* p, size_t n) {
    printf("%s", label);
    for (size_t i = 0; i < n; i++) printf("%02x", p[i]);
    printf("\n");
}
static int unhex(const char* s, u8* out) {
    size_t n = strlen(s);
    for (size_t i = 0; i + 1 < n; i += 2) {
        unsigned v;
        if (sscanf(s + i, "%2x", &v) != 1) return -1;
        out[i / 2] = (u8)v;
    }
    return (int)(n / 2);
}

/* ======================= AES generator constants ======================= */
/* Derived: Blake2b of their name strings. The literals below are only my
 * recollection, printed next to the derivation as a cross-check. */
static u8 K1R[4][16], K4R[8][16], H1S[4][16], H1X[2][16];

static void derive_aes_constants(void) {
    const char* s;
    s = "RandomX AesGenerator1R keys";     blake2b(&K1R[0][0], 64, (const u8*)s, strlen(s));
    s = "RandomX AesGenerator4R keys 0-3"; blake2b(&K4R[0][0], 64, (const u8*)s, strlen(s));
    s = "RandomX AesGenerator4R keys 4-7"; blake2b(&K4R[4][0], 64, (const u8*)s, strlen(s));
    s = "RandomX AesHash1R state";         blake2b(&H1S[0][0], 64, (const u8*)s, strlen(s));
    s = "RandomX AesHash1R xkeys";         blake2b(&H1X[0][0], 32, (const u8*)s, strlen(s));
}

static const u32 MEM_K1R[4][4] = {
    {0xb4f44917, 0xdbb5552b, 0x62716609, 0x6daca553}, {0x0da1dc4e, 0x1725d378, 0x846a710d, 0x6d7caf07},
    {0x3e20e345, 0xf4c0794f, 0x9f947ec6, 0x3f1262f1}, {0x49169154, 0x16314c88, 0xb1ba317c, 0x6aef8135}};
static const u32 MEM_K4R[8][4] = {
    {0x99e5d23f, 0x2f546d2b, 0xd1833ddb, 0x6421aadd}, {0xa5dfcde5, 0x06f79d53, 0xb6913f55, 0xb20e3450},
    {0x171c02bf, 0x0aa4679f, 0x515e7baf, 0x5c3ed904}, {0xd8ded291, 0xcd673785, 0xe78f5d08, 0x85623763},
    {0x229effb4, 0x3d518b6d, 0xe3d6a7a6, 0xb5826f73}, {0xb272b7d2, 0xe9024d4e, 0x9c10b3d9, 0xc7566bf3},
    {0xf63befa7, 0x2ba9660a, 0xf765a38b, 0xf273c9e7}, {0xc0b0762d, 0x0c06d1fd, 0x915839de, 0x7a7cd609}};
static const u32 MEM_H1S[4][4] = {
    {0xd7983aad, 0xcc82db47, 0x9fa856de, 0x92b52c0d}, {0xace78057, 0xf59e125a, 0x15c7b798, 0x338d996e},
    {0xe8a07ce4, 0x5079506b, 0xae62c7d0, 0x6a770017}, {0x7e994948, 0x79a10005, 0x07ad828d, 0x630a240c}};
static const u32 MEM_H1X[2][4] = {
    {0x06890201, 0x90dc56bf, 0x8b24949f, 0xf6fa8389}, {0xed18f99b, 0xee1043c6, 0x51f4e03c, 0x61b263d1}};

static int same_lanes(const u8 k[16], const u32 l[4]) { /* lanes listed high to low */
    u8 b[16];
    st32(b, l[3]); st32(b + 4, l[2]); st32(b + 8, l[1]); st32(b + 12, l[0]);
    return memcmp(b, k, 16) == 0;
}
static int aes_constants_match_memory(void) {
    int ok = 1;
    for (int i = 0; i < 4; i++) ok &= same_lanes(K1R[i], MEM_K1R[i]);
    for (int i = 0; i < 8; i++) ok &= same_lanes(K4R[i], MEM_K4R[i]);
    for (int i = 0; i < 4; i++) ok &= same_lanes(H1S[i], MEM_H1S[i]);
    for (int i = 0; i < 2; i++) ok &= same_lanes(H1X[i], MEM_H1X[i]);
    return ok;
}

/* ======================= self tests of the primitives ======================= */
static void invmix(const u8 k[16], u8 o[16]) {
    for (int c = 0; c < 4; c++) {
        u8 a0 = k[4 * c], a1 = k[4 * c + 1], a2 = k[4 * c + 2], a3 = k[4 * c + 3];
        o[4 * c]     = X14[a0] ^ X11[a1] ^ X13[a2] ^ X9[a3];
        o[4 * c + 1] = X9[a0] ^ X14[a1] ^ X11[a2] ^ X13[a3];
        o[4 * c + 2] = X13[a0] ^ X9[a1] ^ X14[a2] ^ X11[a3];
        o[4 * c + 3] = X11[a0] ^ X13[a1] ^ X9[a2] ^ X14[a3];
    }
}

static int selftest(void) {
    u8 out[64], want[64], w[176], st[16], pt[16], t[16];
    int ok = 1;

    unhex("ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
          "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923", want);
    blake2b(out, 64, (const u8*)"abc", 3);
    printf("  Blake2b-512(\"abc\") = RFC 7693 vector ........ %s\n", memcmp(out, want, 64) ? "FAIL" : "ok");
    ok &= !memcmp(out, want, 64);

    printf("  S-box from GF(2^8) inverse: S(00)=%02x S(01)=%02x S(53)=%02x .. %s\n", SB[0], SB[1], SB[0x53],
           (SB[0] == 0x63 && SB[1] == 0x7c && SB[0x53] == 0xed) ? "ok" : "FAIL");
    ok &= SB[0] == 0x63 && SB[1] == 0x7c && SB[0x53] == 0xed;

    /* AES-128 (FIPS-197 C.1) from the same round and S-box; key schedule's Rcon = powers of x */
    unhex("000102030405060708090a0b0c0d0e0f", w);
    u8 rc = 1;
    for (int i = 4; i < 44; i++) {
        u8 tw[4];
        memcpy(tw, w + 4 * (i - 1), 4);
        if (i % 4 == 0) {
            u8 t0 = tw[0];
            tw[0] = SB[tw[1]] ^ rc; tw[1] = SB[tw[2]]; tw[2] = SB[tw[3]]; tw[3] = SB[t0];
            rc = gmul(rc, 2);
        }
        for (int j = 0; j < 4; j++) w[4 * i + j] = w[4 * (i - 4) + j] ^ tw[j];
    }
    unhex("00112233445566778899aabbccddeeff", pt);
    for (int j = 0; j < 16; j++) st[j] = pt[j] ^ w[j];
    for (int r = 1; r < 10; r++) aesenc(st, w + 16 * r);
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) t[4 * c + r] = SB[st[4 * ((c + r) & 3) + r]];
    for (int j = 0; j < 16; j++) st[j] = t[j] ^ w[160 + j];
    unhex("69c4e0d86a7b0430d8cdb78070b4c55a", want);
    printf("  AES-128 encrypt = FIPS-197 C.1 .............. %s\n", memcmp(st, want, 16) ? "FAIL" : "ok");
    ok &= !memcmp(st, want, 16);

    for (int j = 0; j < 16; j++) st[j] ^= w[160 + j];
    for (int r = 9; r >= 1; r--) { u8 dk[16]; invmix(w + 16 * r, dk); aesdec(st, dk); }
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) t[4 * c + r] = ISB[st[4 * ((c - r) & 3) + r]];
    for (int j = 0; j < 16; j++) st[j] = t[j] ^ w[j];
    printf("  AES-128 decrypt via AESDEC rounds ........... %s\n", memcmp(st, pt, 16) ? "FAIL" : "ok");
    ok &= !memcmp(st, pt, 16);
    return ok;
}

/* ======================= AES generators and hash ======================= */
static void fill_1r(u8 state[64], u8* out, size_t n) {
    u8 s[4][16];
    memcpy(s, state, 64);
    for (size_t o = 0; o < n; o += 64) {
        aesdec(s[0], K1R[0]); aesenc(s[1], K1R[1]); aesdec(s[2], K1R[2]); aesenc(s[3], K1R[3]);
        memcpy(out + o, s, 64);
    }
    memcpy(state, s, 64);
}
static void fill_4r(const u8 state[64], u8* out, size_t n) {
    u8 s[4][16];
    memcpy(s, state, 64);
    for (size_t o = 0; o < n; o += 64) {
        for (int k = 0; k < 4; k++) {
            aesdec(s[0], K4R[k]); aesenc(s[1], K4R[k]); aesdec(s[2], K4R[k + 4]); aesenc(s[3], K4R[k + 4]);
        }
        memcpy(out + o, s, 64);
    }
}
static void hash_1r(const u8* in, size_t n, u8 out[64]) {
    u8 s[4][16];
    memcpy(s, H1S, 64);
    for (size_t o = 0; o < n; o += 64) {
        aesenc(s[0], in + o); aesdec(s[1], in + o + 16); aesenc(s[2], in + o + 32); aesdec(s[3], in + o + 48);
    }
    for (int x = 0; x < 2; x++) {
        aesenc(s[0], H1X[x]); aesdec(s[1], H1X[x]); aesenc(s[2], H1X[x]); aesdec(s[3], H1X[x]);
    }
    memcpy(out, s, 64);
}

/* ======================= Argon2d cache ======================= */
static int IXR[8][16], IXC[8][16];

static void argon_index_tables(void) {
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 16; j++) IXR[i][j] = 16 * i + j;
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) { IXC[i][2 * j] = 2 * i + 16 * j; IXC[i][2 * j + 1] = 2 * i + 16 * j + 1; }
}

/* BlaMka: a + b + 2*lo32(a)*lo32(b), all in Z/2^64 */
static inline u64 blamka(u64 a, u64 b) {
    return add64(add64(a, b), mul64(2, mul64(a & 0xFFFFFFFFull, b & 0xFFFFFFFFull)));
}
#define GB(a, b, c, d)                                   \
    do {                                                 \
        a = blamka(a, b); d = rotr(d ^ a, 32);           \
        c = blamka(c, d); b = rotr(b ^ c, 24);           \
        a = blamka(a, b); d = rotr(d ^ a, 16);           \
        c = blamka(c, d); b = rotr(b ^ c, 63);           \
    } while (0)

static inline void argonP(u64* R, const int* x) {
    GB(R[x[0]], R[x[4]], R[x[8]], R[x[12]]);
    GB(R[x[1]], R[x[5]], R[x[9]], R[x[13]]);
    GB(R[x[2]], R[x[6]], R[x[10]], R[x[14]]);
    GB(R[x[3]], R[x[7]], R[x[11]], R[x[15]]);
    GB(R[x[0]], R[x[5]], R[x[10]], R[x[15]]);
    GB(R[x[1]], R[x[6]], R[x[11]], R[x[12]]);
    GB(R[x[2]], R[x[7]], R[x[8]], R[x[13]]);
    GB(R[x[3]], R[x[4]], R[x[9]], R[x[14]]);
}

static void fill_block(const u64* prev, const u64* ref, u64* next, int with_xor) {
    u64 R[128], T[128];
    for (int i = 0; i < 128; i++) {
        R[i] = ref[i] ^ prev[i];
        T[i] = with_xor ? R[i] ^ next[i] : R[i];
    }
    for (int i = 0; i < 8; i++) argonP(R, IXR[i]);
    for (int i = 0; i < 8; i++) argonP(R, IXC[i]);
    for (int i = 0; i < 128; i++) next[i] = T[i] ^ R[i];
}

static void blake2b_long(u8* out, u32 outlen, const u8* in, size_t inlen) {
    u8 buf[4 + 128], v[64];
    st32(buf, outlen);
    memcpy(buf + 4, in, inlen);
    if (outlen <= 64) { blake2b(out, outlen, buf, 4 + inlen); return; }
    blake2b(v, 64, buf, 4 + inlen);
    memcpy(out, v, 32);
    u32 pos = 32, left = outlen - 32;
    while (left > 64) {
        blake2b(v, 64, v, 64);
        memcpy(out + pos, v, 32);
        pos += 32;
        left -= 32;
    }
    blake2b(v, left, v, 64);
    memcpy(out + pos, v, left);
}

static void argon2d_cache(const u8* K, size_t klen) {
    u8 pre[512], h0[72], blk[1024];
    size_t n = 0;
    st32(pre + n, 1); n += 4;              /* lanes */
    st32(pre + n, 0); n += 4;              /* tag length: the tag is never produced */
    st32(pre + n, ARGON_BLOCKS); n += 4;   /* memory, KiB */
    st32(pre + n, ARGON_PASSES); n += 4;
    st32(pre + n, 0x13); n += 4;           /* version */
    st32(pre + n, 0); n += 4;              /* type d */
    st32(pre + n, (u32)klen); n += 4; memcpy(pre + n, K, klen); n += klen;
    st32(pre + n, 8); n += 4; memcpy(pre + n, ARGON_SALT, 8); n += 8;
    st32(pre + n, 0); n += 4;              /* secret */
    st32(pre + n, 0); n += 4;              /* associated data */
    blake2b(h0, 64, pre, n);
    for (u32 b = 0; b < 2; b++) {
        st32(h0 + 64, b);
        st32(h0 + 68, 0);
        blake2b_long(blk, 1024, h0, 72);
        for (int i = 0; i < 128; i++) cache[b * 128 + i] = le64(blk + 8 * i);
    }
    const u32 lane = ARGON_BLOCKS, seg = lane / 4;
    for (u32 pass = 0; pass < ARGON_PASSES; pass++)
        for (u32 slice = 0; slice < 4; slice++)
            for (u32 idx = (pass == 0 && slice == 0) ? 2 : 0; idx < seg; idx++) {
                u32 cur = slice * seg + idx;
                u32 prev = cur ? cur - 1 : lane - 1;
                u64 j1 = (u32)cache[(size_t)prev * 128];
                u32 area = pass == 0 ? (slice == 0 ? idx - 1 : slice * seg + idx - 1) : lane - seg + idx - 1;
                u64 rel = (j1 * j1) >> 32;
                rel = (u64)(area - 1) - (((u64)area * rel) >> 32);
                u32 start = pass == 0 ? 0 : (slice == 3 ? 0 : (slice + 1) * seg);
                u32 ref = (u32)((start + rel) % lane);
                fill_block(cache + (size_t)prev * 128, cache + (size_t)ref * 128, cache + (size_t)cur * 128, pass != 0);
            }
}

/* ======================= Blake2 generator ======================= */
typedef struct { u8 data[64]; int idx; } B2Gen;

static void b2gen_init(B2Gen* g, const u8* seed, size_t n, u32 nonce) {
    memset(g->data, 0, 64);
    memcpy(g->data, seed, n > 60 ? 60 : n);
    st32(g->data + 60, nonce);
    g->idx = 64;
}
static void b2gen_need(B2Gen* g, int k) {
    if (g->idx + k > 64) { blake2b(g->data, 64, g->data, 64); g->idx = 0; }
}
static u8 b2_byte(B2Gen* g) { b2gen_need(g, 1); return g->data[g->idx++]; }
static u32 b2_u32(B2Gen* g) { b2gen_need(g, 4); u32 v = le32(g->data + g->idx); g->idx += 4; return v; }

/* ======================= superscalar programs ======================= */
enum { S_ISUB_R, S_IXOR_R, S_IADD_RS, S_IMUL_R, S_IROR_C, S_IADD_C7, S_IXOR_C7, S_IADD_C8, S_IXOR_C8,
       S_IADD_C9, S_IXOR_C9, S_IMULH_R, S_ISMULH_R, S_IMUL_RCP, S_INVALID = -1 };
enum { P0 = 1, P1 = 2, P5 = 4, P01 = P0 | P1, P05 = P0 | P5, P015 = P0 | P1 | P5 };

typedef struct { int size, latency, uop1, uop2, dependent; } MacroOp;
static const MacroOp M_ADD_RI = {7, 1, P015, 0, 0}, M_XOR_RI = {7, 1, P015, 0, 0},
                     M_SUB_RR = {3, 1, P015, 0, 0}, M_XOR_RR = {3, 1, P015, 0, 0},
                     M_LEA_SIB = {4, 1, P01, 0, 0}, M_IMUL_RR = {4, 3, P1, 0, 0},
                     M_ROR_RI = {4, 1, P05, 0, 0}, M_MOV_RR = {3, 0, 0, 0, 0},
                     M_MUL_R = {3, 4, P1, P5, 0}, M_IMUL_R = {3, 4, P1, P5, 0},
                     M_MOV_RI64 = {10, 1, P015, 0, 0}, M_IMUL_RR_DEP = {4, 3, P1, 0, 1};

typedef struct { int type, nops; const MacroOp* op[3]; int resultOp, dstOp, srcOp; } SInfo;
static const SInfo SI[14] = {
    {S_ISUB_R, 1, {&M_SUB_RR}, 0, 0, 0},     {S_IXOR_R, 1, {&M_XOR_RR}, 0, 0, 0},
    {S_IADD_RS, 1, {&M_LEA_SIB}, 0, 0, 0},   {S_IMUL_R, 1, {&M_IMUL_RR}, 0, 0, 0},
    {S_IROR_C, 1, {&M_ROR_RI}, 0, 0, -1},    {S_IADD_C7, 1, {&M_ADD_RI}, 0, 0, -1},
    {S_IXOR_C7, 1, {&M_XOR_RI}, 0, 0, -1},   {S_IADD_C8, 1, {&M_ADD_RI}, 0, 0, -1},
    {S_IXOR_C8, 1, {&M_XOR_RI}, 0, 0, -1},   {S_IADD_C9, 1, {&M_ADD_RI}, 0, 0, -1},
    {S_IXOR_C9, 1, {&M_XOR_RI}, 0, 0, -1},
    {S_IMULH_R, 3, {&M_MOV_RR, &M_MUL_R, &M_MOV_RR}, 1, 0, 1},
    {S_ISMULH_R, 3, {&M_MOV_RR, &M_IMUL_R, &M_MOV_RR}, 1, 0, 1},
    {S_IMUL_RCP, 2, {&M_MOV_RI64, &M_IMUL_RR_DEP}, 1, 1, -1},
};
static const SInfo SI_NOP = {S_INVALID, 0, {0}, 0, 0, 0};

typedef struct { int index, n, size[4]; } DBuf;
static const DBuf D484 = {0, 3, {4, 8, 4}}, D7333 = {1, 4, {7, 3, 3, 3}}, D3733 = {2, 4, {3, 7, 3, 3}},
                  D493 = {3, 3, {4, 9, 3}}, D4444 = {4, 4, {4, 4, 4, 4}}, D3310 = {5, 3, {3, 3, 10}};
static const DBuf* DRANDOM[4] = {&D484, &D7333, &D3733, &D493};

typedef struct { const SInfo* info; int src, dst, mod, group, canReuse, parIsSrc; u32 imm; i32 groupPar; } SCand;
typedef struct { int latency, lastGroup; i32 lastPar; } RegInfo;
typedef struct { u8 op, dst, src, mod; u32 imm; u64 rcp; } SIns;
typedef struct { SIns ins[SS_MAX]; int size, addrReg; } SSProg;
static SSProg SSP[CACHE_ACCESSES];

static void cand_create(SCand* c, const SInfo* info, B2Gen* g) {
    c->info = info;
    c->src = c->dst = -1;
    c->canReuse = c->parIsSrc = 0;
    switch (info->type) {
    case S_ISUB_R:  c->mod = 0; c->imm = 0; c->group = S_IADD_RS; c->parIsSrc = 1; break;
    case S_IXOR_R:  c->mod = 0; c->imm = 0; c->group = S_IXOR_R; c->parIsSrc = 1; break;
    case S_IADD_RS: c->mod = b2_byte(g); c->imm = 0; c->group = S_IADD_RS; c->parIsSrc = 1; break;
    case S_IMUL_R:  c->mod = 0; c->imm = 0; c->group = S_IMUL_R; c->parIsSrc = 1; break;
    case S_IROR_C:
        c->mod = 0;
        do { c->imm = b2_byte(g) & 63; } while (c->imm == 0);
        c->group = S_IROR_C; c->groupPar = -1;
        break;
    case S_IADD_C7: case S_IADD_C8: case S_IADD_C9:
        c->mod = 0; c->imm = b2_u32(g); c->group = S_IADD_C7; c->groupPar = -1; break;
    case S_IXOR_C7: case S_IXOR_C8: case S_IXOR_C9:
        c->mod = 0; c->imm = b2_u32(g); c->group = S_IXOR_C7; c->groupPar = -1; break;
    case S_IMULH_R:
        c->canReuse = 1; c->mod = 0; c->imm = 0; c->group = S_IMULH_R; c->groupPar = (i32)b2_u32(g); break;
    case S_ISMULH_R:
        c->canReuse = 1; c->mod = 0; c->imm = 0; c->group = S_ISMULH_R; c->groupPar = (i32)b2_u32(g); break;
    case S_IMUL_RCP:
        c->mod = 0;
        do { c->imm = b2_u32(g); } while ((c->imm & (c->imm - 1)) == 0);
        c->group = S_IMUL_RCP; c->groupPar = -1;
        break;
    }
}

static void cand_for_slot(SCand* c, B2Gen* g, int slot, int fetchType, int isLast) {
    static const int s3[2] = {S_ISUB_R, S_IXOR_R}, s3L[4] = {S_ISUB_R, S_IXOR_R, S_IMULH_R, S_ISMULH_R},
                     s4[2] = {S_IROR_C, S_IADD_RS}, s7[2] = {S_IXOR_C7, S_IADD_C7},
                     s8[2] = {S_IXOR_C8, S_IADD_C8}, s9[2] = {S_IXOR_C9, S_IADD_C9};
    switch (slot) {
    case 3:
        if (isLast) cand_create(c, &SI[s3L[b2_byte(g) & 3]], g);
        else cand_create(c, &SI[s3[b2_byte(g) & 1]], g);
        break;
    case 4:
        if (fetchType == 4 && !isLast) cand_create(c, &SI[S_IMUL_R], g);
        else cand_create(c, &SI[s4[b2_byte(g) & 1]], g);
        break;
    case 7: cand_create(c, &SI[s7[b2_byte(g) & 1]], g); break;
    case 8: cand_create(c, &SI[s8[b2_byte(g) & 1]], g); break;
    case 9: cand_create(c, &SI[s9[b2_byte(g) & 1]], g); break;
    case 10: cand_create(c, &SI[S_IMUL_RCP], g); break;
    }
}

static int select_reg(const int* avail, int n, B2Gen* g, int* reg) {
    if (n == 0) return 0;
    *reg = avail[n > 1 ? (int)(b2_u32(g) % (u32)n) : 0];
    return 1;
}
static int select_dst(SCand* c, int cycle, int allowChainedMul, const RegInfo* regs, B2Gen* g) {
    int avail[8], n = 0;
    for (int i = 0; i < 8; i++)
        if (regs[i].latency <= cycle && (c->canReuse || i != c->src) &&
            (allowChainedMul || c->group != S_IMUL_R || regs[i].lastGroup != S_IMUL_R) &&
            (regs[i].lastGroup != c->group || regs[i].lastPar != c->groupPar) &&
            (c->info->type != S_IADD_RS || i != 5))
            avail[n++] = i;
    return select_reg(avail, n, g, &c->dst);
}
static int select_src(SCand* c, int cycle, const RegInfo* regs, B2Gen* g) {
    int avail[8], n = 0;
    for (int i = 0; i < 8; i++)
        if (regs[i].latency <= cycle) avail[n++] = i;
    if (n == 2 && c->info->type == S_IADD_RS && (avail[0] == 5 || avail[1] == 5)) {
        c->groupPar = c->src = 5;
        return 1;
    }
    if (select_reg(avail, n, g, &c->src)) {
        if (c->parIsSrc) c->groupPar = c->src;
        return 1;
    }
    return 0;
}

static int sched_uop(int uop, int busy[][3], int cycle, int commit) {
    for (; cycle < CYCLE_MAP; ++cycle) {
        if ((uop & P5) && !busy[cycle][2]) { if (commit) busy[cycle][2] = uop; return cycle; }
        if ((uop & P0) && !busy[cycle][0]) { if (commit) busy[cycle][0] = uop; return cycle; }
        if ((uop & P1) && !busy[cycle][1]) { if (commit) busy[cycle][1] = uop; return cycle; }
    }
    return -1;
}
static int sched_mop(const MacroOp* m, int busy[][3], int cycle, int depCycle, int commit) {
    if (m->dependent && depCycle > cycle) cycle = depCycle;
    if (!m->uop1) return cycle; /* eliminated */
    if (!m->uop2) return sched_uop(m->uop1, busy, cycle, commit);
    for (; cycle < CYCLE_MAP; ++cycle) {
        int c1 = sched_uop(m->uop1, busy, cycle, 0), c2 = sched_uop(m->uop2, busy, cycle, 0);
        if (c1 >= 0 && c1 == c2) {
            if (commit) { sched_uop(m->uop1, busy, c1, 1); sched_uop(m->uop2, busy, c2, 1); }
            return c1;
        }
    }
    return -1;
}

static const DBuf* fetch_next(int type, int cycle, int mulCount, B2Gen* g) {
    if (type == S_IMULH_R || type == S_ISMULH_R) return &D3310;
    if (mulCount < cycle + 1) return &D4444;
    if (type == S_IMUL_RCP) return (b2_byte(g) & 1) ? &D484 : &D493;
    return DRANDOM[b2_byte(g) & 3];
}

static int is_mul(int t) { return t == S_IMUL_R || t == S_IMULH_R || t == S_ISMULH_R || t == S_IMUL_RCP; }

static void ss_generate(SSProg* p, B2Gen* g) {
    int busy[CYCLE_MAP][3];
    RegInfo regs[8];
    SCand cur;
    memset(busy, 0, sizeof busy);
    memset(&cur, 0, sizeof cur);
    for (int i = 0; i < 8; i++) { regs[i].latency = 0; regs[i].lastGroup = S_INVALID; regs[i].lastPar = -1; }
    cur.info = &SI_NOP;
    int mopIndex = 0, cycle = 0, depCycle = 0, saturated = 0, size = 0, mulCount = 0, throwAway = 0;

    for (int decodeCycle = 0; decodeCycle < SS_LATENCY && !saturated && size < SS_MAX; ++decodeCycle) {
        const DBuf* db = fetch_next(cur.info->type, decodeCycle, mulCount, g);
        int bi = 0;
        while (bi < db->n) {
            int topCycle = cycle;
            if (mopIndex >= cur.info->nops) {
                if (saturated || size >= SS_MAX) break;
                cand_for_slot(&cur, g, db->size[bi], db->index, db->n == bi + 1);
                mopIndex = 0;
            }
            const MacroOp* mop = cur.info->op[mopIndex];
            int sc = sched_mop(mop, busy, cycle, depCycle, 0);
            if (sc < 0) { saturated = 1; break; }

            if (mopIndex == cur.info->srcOp) {
                int fw;
                for (fw = 0; fw < 4 && !select_src(&cur, sc, regs, g); ++fw) { ++sc; ++cycle; }
                if (fw == 4) {
                    if (throwAway < 256) { throwAway++; mopIndex = cur.info->nops; continue; }
                    cur.info = &SI_NOP;
                    break;
                }
            }
            if (mopIndex == cur.info->dstOp) {
                int fw;
                for (fw = 0; fw < 4 && !select_dst(&cur, sc, throwAway > 0, regs, g); ++fw) { ++sc; ++cycle; }
                if (fw == 4) {
                    if (throwAway < 256) { throwAway++; mopIndex = cur.info->nops; continue; }
                    cur.info = &SI_NOP;
                    break;
                }
            }
            throwAway = 0;

            sc = sched_mop(mop, busy, sc, sc, 1);
            if (sc < 0) { saturated = 1; break; }
            depCycle = sc + mop->latency;
            if (mopIndex == cur.info->resultOp) {
                regs[cur.dst].latency = depCycle;
                regs[cur.dst].lastGroup = cur.group;
                regs[cur.dst].lastPar = cur.groupPar;
            }
            bi++;
            mopIndex++;
            if (sc >= SS_LATENCY) saturated = 1;
            cycle = topCycle;
            if (mopIndex >= cur.info->nops) {
                SIns* in = &p->ins[size++];
                in->op = (u8)cur.info->type;
                in->mod = (u8)cur.mod;
                in->imm = cur.imm;
                in->dst = (u8)cur.dst;
                in->src = (u8)(cur.src < 0 ? cur.dst : cur.src);
                in->rcp = in->op == S_IMUL_RCP ? rcp(cur.imm) : 0;
                mulCount += is_mul(cur.info->type);
            }
        }
        ++cycle;
    }

    /* address register = the one with the longest dependency chain */
    int asic[8] = {0}, best = 0;
    for (int i = 0; i < size; i++) {
        const SIns* in = &p->ins[i];
        int ld = asic[in->dst] + 1, ls = in->dst != in->src ? asic[in->src] + 1 : 0;
        asic[in->dst] = ld > ls ? ld : ls;
    }
    p->addrReg = 0;
    for (int i = 0; i < 8; i++)
        if (asic[i] > best) { best = asic[i]; p->addrReg = i; }
    p->size = size;
}

static void ss_exec(u64 r[8], const SSProg* p) {
    for (int j = 0; j < p->size; j++) {
        const SIns* in = &p->ins[j];
        u64* d = &r[in->dst];
        u64 s = r[in->src];
        switch (in->op) {
        case S_ISUB_R: *d = sub64(*d, s); break;
        case S_IXOR_R: *d = xor64(*d, s); break;
        case S_IADD_RS: *d = add64(*d, shl(s, (in->mod >> 2) & 3)); break;
        case S_IMUL_R: *d = mul64(*d, s); break;
        case S_IROR_C: *d = rotr(*d, in->imm); break;
        case S_IADD_C7: case S_IADD_C8: case S_IADD_C9: *d = add64(*d, sext32(in->imm)); break;
        case S_IXOR_C7: case S_IXOR_C8: case S_IXOR_C9: *d = xor64(*d, sext32(in->imm)); break;
        case S_IMULH_R: *d = mulh(*d, s); break;
        case S_ISMULH_R: *d = smulh(*d, s); break;
        case S_IMUL_RCP: *d = mul64(*d, in->rcp); break;
        }
    }
}

static const u64 SS_MUL0 = 6364136223846793005ull;
static const u64 SS_ADD[8] = {0, 9298411001130361340ull, 12065312585734608966ull, 9306329213124626780ull,
                              5281919268842080866ull, 10536153434571861004ull, 3398623926847679864ull,
                              9549104520008361294ull};

static void dataset_item(u64 out[8], u64 item) {
    u64 rl[8], regVal = item;
    rl[0] = mul64(add64(item, 1), SS_MUL0);
    for (int q = 1; q < 8; q++) rl[q] = xor64(rl[0], SS_ADD[q]);
    for (int i = 0; i < CACHE_ACCESSES; i++) {
        const u64* mix = cache + (regVal & (CACHE_ITEMS - 1)) * 8;
        ss_exec(rl, &SSP[i]);
        for (int q = 0; q < 8; q++) rl[q] = xor64(rl[q], mix[q]);
        regVal = rl[SSP[i].addrReg];
    }
    memcpy(out, rl, 64);
}

/* ======================= the VM ======================= */
enum { I_IADD_RS, I_IADD_M, I_ISUB_R, I_ISUB_M, I_IMUL_R, I_IMUL_M, I_IMULH_R, I_IMULH_M, I_ISMULH_R,
       I_ISMULH_M, I_IMUL_RCP, I_INEG_R, I_IXOR_R, I_IXOR_M, I_IROR_R, I_IROL_R, I_ISWAP_R, I_FSWAP_R,
       I_FADD_R, I_FADD_M, I_FSUB_R, I_FSUB_M, I_FSCAL_R, I_FMUL_R, I_FDIV_M, I_FSQRT_R, I_CBRANCH,
       I_CFROUND, I_ISTORE, I_NOP, I_COUNT };
static const int FREQ[I_COUNT] = {16, 7, 16, 7, 16, 4, 4, 1, 4, 1, 8, 2, 15, 5, 8,
                                  2, 4, 4, 16, 5, 16, 5, 6, 32, 4, 6, 25, 1, 16, 0};
static int OPC[256];

static int opcode_table(void) {
    int k = 0;
    for (int t = 0; t < I_COUNT; t++)
        for (int i = 0; i < FREQ[t] && k < 256; i++) OPC[k++] = t;
    return k;
}

typedef struct { int type, dst, src, useImm, shift, target; u64 imm, mask; } BC;
typedef struct { u64 lo, hi; } F128;
typedef struct {
    u64 r[8];
    F128 f[4], e[4], a[4];
    u32 ma, mx;
    int rr0, rr1, rr2, rr3;
    u64 dsOffset, eMask[2];
} VM;

static u64 small_pos_float(u64 ent) {
    u64 ex = ((ent >> 59) + 1023) & 0x7FF;
    return (ex << 52) | (ent & 0xFFFFFFFFFFFFFull);
}
static u64 float_mask(u64 ent) {
    return (ent & ((1ull << 22) - 1)) | ((0x300ull | ((ent >> 60) << 4)) << 52);
}
static inline F128 cvt_pair(const u8* p) {
    F128 v;
    v.lo = fcvt_i32(le32(p));
    v.hi = fcvt_i32(le32(p + 4));
    return v;
}
static inline F128 emask(const VM* m, F128 v) {
    const u64 dm = (1ull << 56) - 1;
    v.lo = (v.lo & dm) | m->eMask[0];
    v.hi = (v.hi & dm) | m->eMask[1];
    return v;
}

static void vm_init(VM* m, const u8* prog) {
    u64 ent[16];
    for (int i = 0; i < 16; i++) ent[i] = le64(prog + 8 * i);
    for (int i = 0; i < 4; i++) { m->a[i].lo = small_pos_float(ent[2 * i]); m->a[i].hi = small_pos_float(ent[2 * i + 1]); }
    m->ma = (u32)(ent[8] & LINE_ALIGN);
    m->mx = (u32)ent[10];
    m->rr0 = 0 + (int)(ent[12] & 1);
    m->rr1 = 2 + (int)((ent[12] >> 1) & 1);
    m->rr2 = 4 + (int)((ent[12] >> 2) & 1);
    m->rr3 = 6 + (int)((ent[12] >> 3) & 1);
    m->dsOffset = (ent[13] % (EXTRA_ITEMS + 1)) * 64;
    m->eMask[0] = float_mask(ent[14]);
    m->eMask[1] = float_mask(ent[15]);
}

static void vm_compile(const u8* code, BC* bc) {
    int use[8];
    for (int i = 0; i < 8; i++) use[i] = -1;
    for (int i = 0; i < PROG_SIZE; i++) {
        const u8* ins = code + 8 * i;
        BC* b = &bc[i];
        int dst = ins[1] % 8, src = ins[2] % 8, mod = ins[3];
        u32 imm = le32(ins + 4);
        int mem = mod % 4, shift = (mod >> 2) % 4, cond = mod >> 4;
        memset(b, 0, sizeof *b);
        b->type = OPC[ins[0]];
        b->dst = dst;
        b->src = src;
        switch (b->type) {
        case I_IADD_RS: b->shift = shift; b->imm = dst == 5 ? sext32(imm) : 0; use[dst] = i; break;
        case I_IADD_M: case I_ISUB_M: case I_IMUL_M: case I_IMULH_M: case I_ISMULH_M: case I_IXOR_M:
            b->imm = sext32(imm);
            if (src != dst) b->mask = mem ? L1_MASK : L2_MASK;
            else { b->useImm = 1; b->mask = L3_MASK; }
            use[dst] = i;
            break;
        case I_ISUB_R: case I_IMUL_R: case I_IXOR_R:
            if (src == dst) { b->useImm = 1; b->imm = sext32(imm); }
            use[dst] = i;
            break;
        case I_IMULH_R: case I_ISMULH_R: case I_INEG_R: use[dst] = i; break;
        case I_IMUL_RCP:
            if (imm & (imm - 1)) { b->imm = rcp(imm); use[dst] = i; }
            else b->type = I_NOP;
            break;
        case I_IROR_R: case I_IROL_R:
            if (src == dst) { b->useImm = 1; b->imm = imm; }
            use[dst] = i;
            break;
        case I_ISWAP_R:
            if (src != dst) { use[dst] = i; use[src] = i; }
            else b->type = I_NOP;
            break;
        case I_FSWAP_R: break;
        case I_FADD_R: case I_FSUB_R: case I_FMUL_R: b->dst = dst % 4; b->src = src % 4; break;
        case I_FADD_M: case I_FSUB_M: case I_FDIV_M:
            b->dst = dst % 4; b->mask = mem ? L1_MASK : L2_MASK; b->imm = sext32(imm); break;
        case I_FSCAL_R: case I_FSQRT_R: b->dst = dst % 4; break;
        case I_CBRANCH: {
            int sh = cond + JUMP_OFFSET;
            u64 im = sext32(imm) | (1ull << sh);
            im &= ~(1ull << (sh - 1));
            b->imm = im;
            b->mask = 0xFFull << sh;
            b->target = use[dst];
            for (int j = 0; j < 8; j++) use[j] = i;
        } break;
        case I_CFROUND: b->imm = imm & 63; break;
        case I_ISTORE:
            b->imm = sext32(imm);
            b->mask = cond < STORE_L3_COND ? (mem ? L1_MASK : L2_MASK) : L3_MASK;
            break;
        }
    }
}

static inline u64 sp_addr(const VM* m, const BC* b) { return add64(b->useImm ? 0 : m->r[b->src], b->imm) & b->mask; }

static void vm_program(VM* m, const BC* bc) {
    for (int pc = 0; pc < PROG_SIZE; pc++) {
        const BC* b = &bc[pc];
        u64* d = &m->r[b->dst];
        switch (b->type) {
        case I_IADD_RS: *d = add64(*d, add64(shl(m->r[b->src], (unsigned)b->shift), b->imm)); break;
        case I_IADD_M: *d = add64(*d, le64(sp + sp_addr(m, b))); break;
        case I_ISUB_R: *d = sub64(*d, b->useImm ? b->imm : m->r[b->src]); break;
        case I_ISUB_M: *d = sub64(*d, le64(sp + sp_addr(m, b))); break;
        case I_IMUL_R: *d = mul64(*d, b->useImm ? b->imm : m->r[b->src]); break;
        case I_IMUL_M: *d = mul64(*d, le64(sp + sp_addr(m, b))); break;
        case I_IMULH_R: *d = mulh(*d, m->r[b->src]); break;
        case I_IMULH_M: *d = mulh(*d, le64(sp + sp_addr(m, b))); break;
        case I_ISMULH_R: *d = smulh(*d, m->r[b->src]); break;
        case I_ISMULH_M: *d = smulh(*d, le64(sp + sp_addr(m, b))); break;
        case I_IMUL_RCP: *d = mul64(*d, b->imm); break;
        case I_INEG_R: *d = neg64(*d); break;
        case I_IXOR_R: *d = xor64(*d, b->useImm ? b->imm : m->r[b->src]); break;
        case I_IXOR_M: *d = xor64(*d, le64(sp + sp_addr(m, b))); break;
        case I_IROR_R: *d = rotr(*d, (unsigned)((b->useImm ? b->imm : m->r[b->src]) & 63)); break;
        case I_IROL_R: *d = rotl(*d, (unsigned)((b->useImm ? b->imm : m->r[b->src]) & 63)); break;
        case I_ISWAP_R: { u64 t = *d; *d = m->r[b->src]; m->r[b->src] = t; } break;
        case I_FSWAP_R: {
            F128* x = b->dst < 4 ? &m->f[b->dst] : &m->e[b->dst - 4];
            u64 t = x->lo; x->lo = x->hi; x->hi = t;
        } break;
        case I_FADD_R:
            m->f[b->dst].lo = fadd(m->f[b->dst].lo, m->a[b->src].lo);
            m->f[b->dst].hi = fadd(m->f[b->dst].hi, m->a[b->src].hi);
            break;
        case I_FADD_M: {
            F128 v = cvt_pair(sp + sp_addr(m, b));
            m->f[b->dst].lo = fadd(m->f[b->dst].lo, v.lo);
            m->f[b->dst].hi = fadd(m->f[b->dst].hi, v.hi);
        } break;
        case I_FSUB_R:
            m->f[b->dst].lo = fsub(m->f[b->dst].lo, m->a[b->src].lo);
            m->f[b->dst].hi = fsub(m->f[b->dst].hi, m->a[b->src].hi);
            break;
        case I_FSUB_M: {
            F128 v = cvt_pair(sp + sp_addr(m, b));
            m->f[b->dst].lo = fsub(m->f[b->dst].lo, v.lo);
            m->f[b->dst].hi = fsub(m->f[b->dst].hi, v.hi);
        } break;
        case I_FSCAL_R:
            m->f[b->dst].lo ^= 0x80F0000000000000ull;
            m->f[b->dst].hi ^= 0x80F0000000000000ull;
            break;
        case I_FMUL_R:
            m->e[b->dst].lo = fmul(m->e[b->dst].lo, m->a[b->src].lo);
            m->e[b->dst].hi = fmul(m->e[b->dst].hi, m->a[b->src].hi);
            break;
        case I_FDIV_M: {
            F128 v = emask(m, cvt_pair(sp + sp_addr(m, b)));
            m->e[b->dst].lo = fdiv(m->e[b->dst].lo, v.lo);
            m->e[b->dst].hi = fdiv(m->e[b->dst].hi, v.hi);
        } break;
        case I_FSQRT_R:
            m->e[b->dst].lo = fsqrt(m->e[b->dst].lo);
            m->e[b->dst].hi = fsqrt(m->e[b->dst].hi);
            break;
        case I_CBRANCH:
            *d = add64(*d, b->imm);
            if ((*d & b->mask) == 0) pc = b->target;
            break;
        case I_CFROUND: RM = (int)(rotr(m->r[b->src], (unsigned)b->imm) % 4); break;
        case I_ISTORE: st64(sp + (add64(*d, b->imm) & b->mask), m->r[b->src]); break;
        default: break;
        }
    }
}

/* what the screenshot shows for PROG 0, IC 0 */
static const struct { const char* name; u64 lo, hi; } SCREEN[] = {
    {"A0", 0x418e4a297ebfc304ull, 0x4019c856c26708a9ull}, {"A1", 0x40cd8725df13238aull, 0x41e807a5dc7740b5ull},
    {"A2", 0x4176971a789beed7ull, 0x417112c274f91d68ull}, {"A3", 0x414e441747df76c6ull, 0x40bd229eeedd8e98ull},
    {"F0", 0x41bffca1bb000000ull, 0xc1b3ba1e01000000ull}, {"F1", 0x41ca28b047800000ull, 0xc1d7467d7a800000ull},
    {"F2", 0x41b84fec7e000000ull, 0x41d8c7bef6c00000ull}, {"F3", 0x41d2ab39ca000000ull, 0x41c2c01946000000ull},
    {"E0", 0x3c90fb0e9c1e145full, 0x3ac61cba5211d432ull}, {"E1", 0x3cc8bbbf3f9e145full, 0x3add66c15e51d432ull},
    {"E2", 0x3cb547dc751e145full, 0x3ad14272b091d432ull}, {"E3", 0x3cdde0fd9d9e145full, 0x3ad2222e7f51d432ull},
};

static int screen_check(const VM* m, u32 sp0, u32 sp1) {
    const F128* regs[12] = {&m->a[0], &m->a[1], &m->a[2], &m->a[3], &m->f[0], &m->f[1],
                            &m->f[2], &m->f[3], &m->e[0], &m->e[1], &m->e[2], &m->e[3]};
    int good = 0, total = 0;
    for (int i = 0; i < 12; i++) {
        int ok = regs[i]->lo == SCREEN[i].lo && regs[i]->hi == SCREEN[i].hi;
        printf("    %s %016llx %016llx  %s\n", SCREEN[i].name, (unsigned long long)regs[i]->lo,
               (unsigned long long)regs[i]->hi, ok ? "= screen" : "!= screen");
        good += ok; total++;
    }
    struct { const char* n; u32 got, want; } s[4] = {
        {"MA  ", m->ma, 0x738ddb40}, {"MX  ", m->mx, 0x8a8a6230}, {"SPA0", sp0, 0x000a6200}, {"SPA1", sp1, 0x000ddb40}};
    for (int i = 0; i < 4; i++) {
        printf("    %s %08x  %s\n", s[i].n, s[i].got, s[i].got == s[i].want ? "= screen" : "!= screen");
        good += s[i].got == s[i].want; total++;
    }
    printf("    screen checkpoint: %d/%d\n", good, total);
    return good == total;
}

static void vm_execute(VM* m, const u8* prog, int progIdx) {
    BC bc[PROG_SIZE];
    vm_compile(prog + 128, bc);
    for (int i = 0; i < 8; i++) m->r[i] = 0;
    u32 sp0 = m->mx, sp1 = m->ma;
    for (int ic = 0; ic < PROG_ITERS; ic++) {
        u64 mix = xor64(m->r[m->rr0], m->r[m->rr1]);
        sp0 = (sp0 ^ (u32)mix) & L3_LINE_MASK;
        sp1 = (sp1 ^ (u32)(mix >> 32)) & L3_LINE_MASK;
        for (int i = 0; i < 8; i++) m->r[i] = xor64(m->r[i], le64(sp + sp0 + 8 * i));
        for (int i = 0; i < 4; i++) m->f[i] = cvt_pair(sp + sp1 + 8 * i);
        for (int i = 0; i < 4; i++) m->e[i] = emask(m, cvt_pair(sp + sp1 + 8 * (4 + i)));
        if (progIdx == 0 && ic == 0) screen_check(m, sp0, sp1);

        vm_program(m, bc);

        m->mx ^= (u32)xor64(m->r[m->rr2], m->r[m->rr3]);
        m->mx &= LINE_ALIGN;
        u64 item[8];
        dataset_item(item, (m->dsOffset + m->ma) / 64);
        for (int i = 0; i < 8; i++) m->r[i] = xor64(m->r[i], item[i]);
        u32 t = m->mx; m->mx = m->ma; m->ma = t;
        for (int i = 0; i < 8; i++) st64(sp + sp1 + 8 * i, m->r[i]);
        for (int i = 0; i < 4; i++) { m->f[i].lo ^= m->e[i].lo; m->f[i].hi ^= m->e[i].hi; }
        for (int i = 0; i < 4; i++) { st64(sp + sp0 + 16 * i, m->f[i].lo); st64(sp + sp0 + 16 * i + 8, m->f[i].hi); }
        sp0 = 0;
        sp1 = 0;
    }
}

static void regfile(const VM* m, u8 out[256]) {
    for (int i = 0; i < 8; i++) st64(out + 8 * i, m->r[i]);
    for (int i = 0; i < 4; i++) { st64(out + 64 + 16 * i, m->f[i].lo); st64(out + 72 + 16 * i, m->f[i].hi); }
    for (int i = 0; i < 4; i++) { st64(out + 128 + 16 * i, m->e[i].lo); st64(out + 136 + 16 * i, m->e[i].hi); }
    for (int i = 0; i < 4; i++) { st64(out + 192 + 16 * i, m->a[i].lo); st64(out + 200 + 16 * i, m->a[i].hi); }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s K H [expected R hex]\n", argv[0]);
        return 2;
    }
    const u8* K = (const u8*)argv[1];
    const u8* H = (const u8*)argv[2];
    size_t klen = strlen(argv[1]), hlen = strlen(argv[2]);
    u64 one = 1;
    if (*(u8*)&one != 1 || klen > 400) { fprintf(stderr, "needs a little-endian host and |K| <= 400\n"); return 2; }

    aes_tables();
    argon_index_tables();
    if (opcode_table() != 256) { fprintf(stderr, "opcode frequencies do not sum to 256\n"); return 1; }

    printf("self tests\n");
    if (!selftest()) return 1;
    derive_aes_constants();
    printf("  AES keys = Blake2b(name strings); agree with my recollection: %s\n",
           aes_constants_match_memory() ? "yes" : "NO (using the derived ones)");

    double t0 = now(), t;
    u8 S[64], prog[PROG_BYTES], rf[256], a64[64], R[32];

    blake2b(S, 64, H, hlen);
    hex("\nS  = B512(H)             ", S, 64);
    sp = malloc(SP_BYTES);
    cache = malloc((size_t)ARGON_BLOCKS * 1024);
    if (!sp || !cache) { fprintf(stderr, "out of memory\n"); return 1; }
    fill_1r(S, sp, SP_BYTES);

    t = now();
    argon2d_cache(K, klen);
    printf("C  = Argon2d(K) 256 MiB    block0 word0 = %016llx   (%.1fs)\n", (unsigned long long)cache[0], now() - t);

    B2Gen g;
    b2gen_init(&g, K, klen, 0);
    printf("P1..P8 = Superscalar(B2Gen(K)): sizes");
    for (int i = 0; i < CACHE_ACCESSES; i++) {
        ss_generate(&SSP[i], &g);
        printf(" %d/r%d", SSP[i].size, SSP[i].addrReg);
    }
    printf("   (size/address register)\n");

    VM m;
    memset(&m, 0, sizeof m);
    RM = 0;
    t = now();
    for (int p = 0; p < PROG_COUNT; p++) {
        fill_4r(S, prog, PROG_BYTES);
        vm_init(&m, prog);
        if (p == 0) printf("\nprogram 0, iteration 0 vs the screenshot:\n");
        vm_execute(&m, prog, p);
        if (p < PROG_COUNT - 1) {
            regfile(&m, rf);
            blake2b(S, 64, rf, 256);
        }
    }
    printf("\nVM: 8 programs x 2048 iterations x 256 instructions   (%.1fs)\n", now() - t);

    hash_1r(sp, SP_BYTES, a64);
    for (int i = 0; i < 4; i++) { m.a[i].lo = le64(a64 + 16 * i); m.a[i].hi = le64(a64 + 16 * i + 8); }
    regfile(&m, rf);
    blake2b(R, 32, rf, 256);

    hex("\nR  = B256(r|f|e|Hash1R(SP)) = ", R, 32);
    printf("float anomalies (inf/nan/overflow): %ld   total %.1fs\n", fp_anomaly, now() - t0);
    if (argc > 3) {
        u8 want[32];
        int ok = unhex(argv[3], want) == 32 && !memcmp(want, R, 32);
        printf("official R                     = %s\n%s\n", argv[3], ok ? "MATCH" : "MISMATCH");
        return ok ? 0 : 1;
    }
    return 0;
}
