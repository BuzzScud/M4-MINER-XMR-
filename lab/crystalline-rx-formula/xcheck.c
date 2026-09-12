/* xcheck.c — every primitive in prim.h against the real Crystalline Abacus
 * (crystalline-main/math/src/bigint) on random and edge operands, plus the
 * Blake2b IV re-derived as frac(sqrt(prime_nth(i))) with abacus_sqrt.
 *
 * Each abacus evaluation runs in its own forked child, so a library crash is
 * counted against that primitive instead of killing the run.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "math/abacus.h"
#include "math/prime.h"
#include "prim.h"

static u64 s_rng = 0x243F6A8885A308D3ull;
static u64 rnd(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 7; s_rng ^= s_rng << 17; return s_rng; }
static const u64 EDGE[8] = {0, 1, 2, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull,
                            0xFFFFFFFFull, 0x100000000ull};

static CrystallineAbacus* A(u64 v, uint32_t base) { return abacus_from_uint64(v, base); }
static CrystallineAbacus* Z(uint32_t base) { return abacus_new(base); }
static u64 V(const CrystallineAbacus* x) {
    u64 v = 0;
    if (abacus_to_uint64(x, &v) != MATH_SUCCESS) v = 0xBAD0BAD0BAD0BAD0ull;
    return v;
}
/* 2^n by multiplication; abacus_pow_uint64 copies beads past result->capacity */
static CrystallineAbacus* pow2(unsigned n, uint32_t base) {
    CrystallineAbacus* r = A(1, base);
    while (n) {
        unsigned k = n > 32 ? 32 : n;
        CrystallineAbacus *f = A(1ull << k, base), *p = Z(base);
        abacus_mul(p, r, f);
        abacus_free(r);
        abacus_free(f);
        r = p;
        n -= k;
    }
    return r;
}

static CrystallineAbacus *M60, *M2;

/* ---- the abacus side of each primitive ---- */
static u64 ab_ring(int op, u64 a, u64 b) {
    CrystallineAbacus *x = A(a, 60), *y = A(b, 60), *r = Z(60);
    if (op == 0) abacus_mod_add(r, x, y, M60);
    else if (op == 1) abacus_mod_sub(r, x, y, M60);
    else abacus_mod_mul(r, x, y, M60);
    return V(r);
}
static u64 ab_mulh(u64 a, u64 b) {
    CrystallineAbacus *p = Z(2), *q = Z(2);
    abacus_mul(p, A(a, 2), A(b, 2));
    abacus_shift_right(q, p, 64);
    return V(q);
}
static u64 ab_smulh(u64 a, u64 b) {
    u64 h = ab_mulh(a, b);
    if (a >> 63) h = ab_ring(1, h, b);
    if (b >> 63) h = ab_ring(1, h, a);
    return h;
}
static u64 ab_shl(u64 a, unsigned n) {
    CrystallineAbacus *s = Z(2), *m = Z(2);
    abacus_shift_left(s, A(a, 2), n);
    abacus_mod(m, s, M2);
    return V(m);
}
static u64 ab_shr(u64 a, unsigned n) {
    CrystallineAbacus* s = Z(2);
    abacus_shift_right(s, A(a, 2), n);
    return V(s);
}
static u64 ab_rotr(u64 a, unsigned n) {
    if (n == 0) return ab_shr(a, 0);
    CrystallineAbacus *x = A(a, 2), *lo = Z(2), *hi = Z(2), *him = Z(2), *r = Z(2);
    abacus_shift_right(lo, x, n);
    abacus_shift_left(hi, x, 64 - n);
    abacus_mod(him, hi, M2);
    abacus_add(r, lo, him);
    return V(r);
}
static u64 ab_bitop(int op, u64 a, u64 b) {
    CrystallineAbacus *x = A(a, 2), *y = A(b, 2), *two = A(2, 2), *acc = A(0, 2);
    for (unsigned i = 0; i < 64; i++) {
        CrystallineAbacus *xs = Z(2), *ys = Z(2), *xb = Z(2), *yb = Z(2), *s = Z(2), *sh = Z(2), *next = Z(2);
        abacus_shift_right(xs, x, i); abacus_mod(xb, xs, two);
        abacus_shift_right(ys, y, i); abacus_mod(yb, ys, two);
        if (op == 0) abacus_mod_add(s, xb, yb, two);
        else if (op == 1) abacus_mul(s, xb, yb);
        else {
            CrystallineAbacus *p = Z(2), *q = Z(2);
            abacus_add(q, xb, yb);
            abacus_mul(p, xb, yb);
            abacus_sub(s, q, p);
        }
        abacus_shift_left(sh, s, i);
        abacus_add(next, acc, sh);
        acc = next;
    }
    return V(acc);
}
static u64 ab_rcp(u64 d) {
    int bl = 64 - __builtin_clzll(d);
    CrystallineAbacus *q = Z(60), *r = Z(60);
    abacus_div(q, r, pow2((unsigned)(63 + bl), 60), A(d, 60));
    return V(q);
}
static u64 ab_gmul(u64 a, u64 b) {
    u64 acc = 0;
    for (unsigned i = 0; i < 8; i++)
        if (ab_shr(b, i) & 1) acc = ab_bitop(0, acc, ab_shl(a, i));
    for (int k = 14; k >= 8; k--)
        if (ab_shr(acc, (unsigned)k) & 1) acc = ab_bitop(0, acc, ab_shl(0x11b, (unsigned)(k - 8)));
    return acc;
}
static u64 ab_iv(u64 p) {
    CrystallineAbacus *ps = Z(2), *s = Z(2), *sm = Z(2);
    abacus_shift_left(ps, A(p, 2), 128);
    abacus_sqrt(s, ps);
    abacus_mod(sm, s, M2);
    return V(sm);
}

enum { F_ADD, F_SUB, F_MUL, F_MULH, F_SMULH, F_SHL, F_SHR, F_ROTR, F_XOR, F_AND, F_OR, F_RCP, F_GMUL, F_IV, NF };
static const char* FNAME[NF] = {"add64", "sub64", "mul64", "mulh", "smulh", "shl", "shr", "rotr",
                                "xor64", "and64", "or64", "rcp", "gmul", "IV"};

static u64 abacus_side(int f, u64 a, u64 b) {
    switch (f) {
    case F_ADD: return ab_ring(0, a, b);
    case F_SUB: return ab_ring(1, a, b);
    case F_MUL: return ab_ring(2, a, b);
    case F_MULH: return ab_mulh(a, b);
    case F_SMULH: return ab_smulh(a, b);
    case F_SHL: return ab_shl(a, (unsigned)b);
    case F_SHR: return ab_shr(a, (unsigned)b);
    case F_ROTR: return ab_rotr(a, (unsigned)b);
    case F_XOR: return ab_bitop(0, a, b);
    case F_AND: return ab_bitop(1, a, b);
    case F_OR: return ab_bitop(2, a, b);
    case F_RCP: return ab_rcp(a);
    case F_GMUL: return ab_gmul(a, b);
    default: return ab_iv(a);
    }
}
static u64 word_side(int f, u64 a, u64 b) {
    switch (f) {
    case F_ADD: return add64(a, b);
    case F_SUB: return sub64(a, b);
    case F_MUL: return mul64(a, b);
    case F_MULH: return mulh(a, b);
    case F_SMULH: return smulh(a, b);
    case F_SHL: return shl(a, (unsigned)b);
    case F_SHR: return shr(a, (unsigned)b);
    case F_ROTR: return rotr(a, (unsigned)b);
    case F_XOR: return xor64(a, b);
    case F_AND: return and64(a, b);
    case F_OR: return or64(a, b);
    case F_RCP: return rcp(a);
    case F_GMUL: return gmul((u8)a, (u8)b);
    default: return 0;
    }
}

/* 0 = returned, 1 = child crashed */
static int isolated(int f, u64 a, u64 b, u64* out) {
    int fd[2];
    if (pipe(fd)) return 1;
    pid_t pid = fork();
    if (pid == 0) {
        close(fd[0]);
        u64 v = abacus_side(f, a, b);
        ssize_t w = write(fd[1], &v, 8);
        _exit(w == 8 ? 0 : 3);
    }
    close(fd[1]);
    ssize_t got = read(fd[0], out, 8);
    close(fd[0]);
    int st;
    waitpid(pid, &st, 0);
    return !(got == 8 && WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

static long n_[NF], wrong[NF], crash[NF];

static void check(int f, u64 a, u64 b, u64 want) {
    u64 got = 0;
    n_[f]++;
    if (isolated(f, a, b, &got)) {
        if (crash[f]++ < 2) printf("  %-6s a=%016llx b=%016llx  abacus CRASHED\n", FNAME[f], (unsigned long long)a, (unsigned long long)b);
    } else if (got != want) {
        if (wrong[f]++ < 3)
            printf("  %-6s a=%016llx b=%016llx  abacus %016llx  word %016llx\n", FNAME[f], (unsigned long long)a,
                   (unsigned long long)b, (unsigned long long)got, (unsigned long long)want);
    }
}

int main(int argc, char** argv) {
    int samples = argc > 1 ? atoi(argv[1]) : 1500;
    setvbuf(stdout, NULL, _IONBF, 0);
    M60 = pow2(64, 60);
    M2 = pow2(64, 2);
    {
        CrystallineAbacus *one = A(1, 60), *m1 = Z(60);
        abacus_sub(m1, M60, one);
        printf("modulus check: 2^64 - 1 built in base 60 reads back as %016llx\n\n", (unsigned long long)V(m1));
    }

    for (int i = 0; i < samples; i++) {
        u64 a, b;
        if (i < 64) { a = EDGE[i / 8]; b = EDGE[i % 8]; }
        else { a = rnd() >> (rnd() % 64); b = rnd() >> (rnd() % 64); }
        unsigned n = (unsigned)(rnd() % 64);
        for (int f = F_ADD; f <= F_SMULH; f++) check(f, a, b, word_side(f, a, b));
        for (int f = F_SHL; f <= F_ROTR; f++) check(f, a, n, word_side(f, a, n));
        if (i < 200)
            for (int f = F_XOR; f <= F_OR; f++) check(f, a, b, word_side(f, a, b));
        u64 d = (rnd() & 0xFFFFFFFFull) | 3;
        check(F_RCP, d, 0, word_side(F_RCP, d, 0));
        if (i < 150) {
            u64 ga = rnd() & 0xFF, gb = rnd() & 0xFF;
            check(F_GMUL, ga, gb, word_side(F_GMUL, ga, gb));
        }
    }

    printf("Blake2b IV = frac(sqrt(prime_nth(i))) * 2^64 via abacus_sqrt:\n");
    for (int i = 1; i <= 8; i++) {
        u64 p = prime_nth((uint64_t)i), iv = 0;
        int crashed = isolated(F_IV, p, 0, &iv);
        n_[F_IV]++;
        if (crashed) crash[F_IV]++;
        else if (iv != B2IV[i - 1]) wrong[F_IV]++;
        printf("  prime_nth(%d) = %2llu  -> %016llx  %s\n", i, (unsigned long long)p, (unsigned long long)iv,
               crashed ? "CRASHED" : iv == B2IV[i - 1] ? "= IV" : "!= IV");
    }

    printf("\nprimitive   checks   agree   wrong   crashed\n");
    for (int f = 0; f < NF; f++)
        printf("  %-7s %7ld %7ld %7ld %7ld\n", FNAME[f], n_[f], n_[f] - wrong[f] - crash[f], wrong[f], crash[f]);
    return 0;
}
