/*
 * cx_selftest — randomized differential test of every cx64 primitive against
 * native arithmetic. Runs in CX_CHECK mode (native result returned, crystalline
 * result compared). Exit status 1 on any mismatch.
 *
 *   ./cx_selftest [iterations-per-op]      default 20000
 */
#include "cx64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#include <math.h>
#include <time.h>

static uint64_t s[2] = { 0x9E3779B97F4A7C15ULL, 0xD1B54A32D192ED03ULL };
static uint64_t rnd(void) {            /* xorshift128+ */
    uint64_t x = s[0], y = s[1]; s[0] = y; x ^= x << 23; s[1] = x ^ y ^ (x >> 17) ^ (y >> 26); return s[1] + y;
}
static const uint64_t edges[] = {
    0, 1, 2, 3, 0x7FFFFFFFULL, 0x80000000ULL, 0xFFFFFFFFULL, 0x100000000ULL,
    0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL,
    0xFFFFFFFFFFFFFFFEULL, 0xDEADBEEFCAFEBABEULL, 0x0123456789ABCDEFULL,
    0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL, 0x00FF00FF00FF00FFULL, 0x0000FFFF0000FFFFULL,
    0x8000000080000000ULL, 0xFFFF0000FFFF0000ULL
};
#define NE (sizeof edges / sizeof edges[0])
static uint64_t pick(int i) {                /* first passes: edge cases, then random */
    if (i < (int)(NE * NE)) return edges[i % NE];
    switch (rnd() & 7) {
        case 0: return rnd() & 0xFF;
        case 1: return rnd() & 0xFFFFFFFFULL;
        case 2: return rnd() | 0x8000000000000000ULL;
        case 3: return 1ULL << (rnd() & 63);
        default: return rnd();
    }
}
static uint64_t pick2(int i) { if (i < (int)(NE * NE)) return edges[i / NE]; return pick(i); }

static double d2(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
/* RandomX-like doubles: group F values (converted int32 pairs, then sums/products),
 * group E values (exponent 0x300..0x30F, random mantissa), plus generic finite values. */
static double rdouble(int i) {
    switch ((rnd() + i) % 6) {
        case 0: return (double)(int32_t)(uint32_t)rnd();
        case 1: { uint64_t m = rnd() & ((1ULL<<52)-1); uint64_t e = 0x300 + (rnd() & 15); return d2((rnd()&1)<<63 | e<<52 | m); }
        case 2: { uint64_t m = rnd() & ((1ULL<<52)-1); uint64_t e = 1 + rnd() % 2046; return d2((rnd()&1)<<63 | e<<52 | m); }
        case 3: { uint64_t m = rnd() & ((1ULL<<52)-1); uint64_t e = 1023 + (rnd() % 200) - 100; return d2((rnd()&1)<<63 | e<<52 | m); }
        case 4: { uint64_t e = 1 + rnd() % 2046; return d2((rnd()&1)<<63 | e<<52); }   /* powers of two */
        default: { uint64_t m = rnd() & ((1ULL<<52)-1); return d2((rnd()&1)<<63 | m); } /* subnormals */
    }
}
static int isfin(double x) { return __builtin_isfinite(x); }   /* crystalline's math.h shadows libc's */

int main(int argc, char **argv) {
    int N = argc > 1 ? atoi(argv[1]) : 20000;
    cx_init();
    cx_set_mode(CX_CHECK);
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < N; i++) {
        uint64_t a = pick(i), b = pick2(i); unsigned n = (unsigned)(rnd() & 63);
        cx_add64(a, b); cx_sub64(a, b); cx_mul64(a, b); cx_mulh64(a, b);
        cx_smulh64((int64_t)a, (int64_t)b); cx_neg64(a);
        cx_xor64(a, b); cx_and64(a, b); cx_or64(a, b);
        cx_shl64(a, n); cx_shr64(a, n); cx_ror64(a, n); cx_rol64(a, n);
        cx_cmp64(a, b); cx_add32((uint32_t)a, (uint32_t)b); cx_mul32((uint32_t)a, (uint32_t)b);
        cx_mul32x32((uint32_t)a, (uint32_t)b);
        uint32_t dv = (uint32_t)a; if (dv == 0 || (dv & (dv - 1)) == 0) dv = 0x9E3779B9u | 1;
        if (i % 8 == 0) cx_rcp64(dv);
    }
    for (int i = 0; i < N; i++) {
        double a = rdouble(i), b = rdouble(i + 1); int rm = (int)(rnd() & 3);
        if (!isfin(a) || !isfin(b)) continue;
        cx_fadd(a, b, rm); cx_fsub(a, b, rm); cx_fmul(a, b, rm);
        if (b != 0.0) cx_fdiv(a, b, rm);
        cx_fsqrt(__builtin_fabs(a), rm);
        cx_i32_to_f64((int32_t)(uint32_t)rnd());
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double dt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    cx_report();
    fprintf(stderr, "  %llu calls in %.1fs = %.0f ops/s\n", (unsigned long long)cx_total_calls(), dt, cx_total_calls() / dt);
    if (cx_total_mismatches()) { fprintf(stderr, "SELFTEST FAILED\n"); return 1; }
    fprintf(stderr, "SELFTEST PASSED\n");
    return 0;
}
