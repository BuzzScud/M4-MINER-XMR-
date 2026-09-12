/*
 * cx64.c — crystalline abacus as a 64-bit / IEEE-754 ALU, under test.
 * See cx64.h for the contract.
 *
 * Representation choices (each one is a documented consequence of a
 * crystalline behaviour found during the RandomX differential):
 *   - Integers live in base 65536 (4 beads per uint64). Digits are read by
 *     weight_exponent, never by array index, because crystalline strips zero
 *     beads (holes) and leaves max_exponent stale after mul / shift.
 *   - Bitwise ops (xor/and/or) are digit-wise on the base-2 abacus built with
 *     abacus_from_uint64(x, 2) (exact). abacus_convert_base() is NOT used:
 *     it is lossy (drops low bits). The library has no bitwise op of its own;
 *     digit-wise (a+b) mod 2 on beads is the GF(2) extension of its digits.
 *   - Rotates are bead re-indexing (weight_exponent -> (w+n) mod 64) — the
 *     "rotate positions on the lattice" the abacus header describes.
 *   - Shifts use abacus_shift_left / abacus_shift_right; shift_right keeps the
 *     shifted-out digits as fractional beads, so integer digits are read back
 *     with weight_exponent >= 0 only (that is the truncation).
 *   - Division and square root are long division / digit-by-digit root built
 *     from abacus_compare / abacus_sub / abacus_shift_* on base-2 beads.
 *     abacus_div and abacus_sqrt themselves are NOT used: abacus_div overflows
 *     the stack and abacus_sqrt errors on inputs above 2^64 (see PATCHES.md).
 *   - Doubles are (sign, integer mantissa abacus, C exponent). Fractional
 *     beads are never multiplied (abacus_mul on fractions is wrong).
 *     Rounding is decided by inspecting the mantissa's beads.
 */
#include "cx64.h"
#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#include <math.h>
#include <stdarg.h>

#define BASE 65536u

/* ------------------------------------------------------------------------- */
/* bookkeeping                                                                */
/* ------------------------------------------------------------------------- */
enum {
    C_ADD64, C_SUB64, C_MUL64, C_MULH64, C_SMULH64, C_NEG64, C_XOR64, C_AND64,
    C_OR64, C_SHL64, C_SHR64, C_ROR64, C_ROL64, C_CMP64, C_RCP64, C_ADD32,
    C_MUL32, C_MUL32X32, C_FADD, C_FSUB, C_FMUL, C_FDIV, C_FSQRT, C_I2F,
    C_MOD64,
    C_LAST
};
static cx_counter_t counters[CX_NCOUNTERS] = {
    {"add64",0,0,0},{"sub64",0,0,0},{"mul64",0,0,0},{"mulh64",0,0,0},{"smulh64",0,0,0},
    {"neg64",0,0,0},{"xor64",0,0,0},{"and64",0,0,0},{"or64",0,0,0},{"shl64",0,0,0},
    {"shr64",0,0,0},{"ror64",0,0,0},{"rol64",0,0,0},{"cmp64",0,0,0},{"rcp64",0,0,0},
    {"add32",0,0,0},{"mul32",0,0,0},{"mul32x32",0,0,0},{"fadd",0,0,0},{"fsub",0,0,0},
    {"fmul",0,0,0},{"fdiv",0,0,0},{"fsqrt",0,0,0},{"i32_to_f64",0,0,0},{"mod64",0,0,0}
};
static int mode = CX_STRICT;
#define MAXLOG 24
static char mismatch_log[MAXLOG][200];
static int  nlog = 0;

static void note(int c, int ok, const char *fmt, ...) {
    counters[c].calls++;
    if (ok) return;
    counters[c].mismatches++;
    if (nlog < MAXLOG) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(mismatch_log[nlog], sizeof mismatch_log[0], fmt, ap);
        va_end(ap);
        nlog++;
    }
}

void cx_set_mode(int m) { mode = m; }
int  cx_get_mode(void)  { return mode; }
const cx_counter_t *cx_counters(size_t *n) { if (n) *n = C_LAST; return counters; }
uint64_t cx_total_calls(void) { uint64_t t=0; for (int i=0;i<C_LAST;i++) t+=counters[i].calls; return t; }
uint64_t cx_total_mismatches(void) { uint64_t t=0; for (int i=0;i<C_LAST;i++) t+=counters[i].mismatches; return t; }
void cx_reset_counters(void) { for (int i=0;i<C_LAST;i++) counters[i].calls=counters[i].mismatches=counters[i].native=0; nlog=0; }
uint64_t cx_total_native(void) { uint64_t t=0; for (int i=0;i<C_LAST;i++) t+=counters[i].native; return t; }
void cx_report(void) {
    fprintf(stderr, "cx64 report (mode %s)\n", mode==CX_OFF?"off":mode==CX_CHECK?"check":"strict");
    for (int i=0;i<C_LAST;i++) if (counters[i].calls || counters[i].native)
        fprintf(stderr, "  %-11s crystalline %12llu  mismatches %llu  native(bypass) %llu\n", counters[i].name,
                (unsigned long long)counters[i].calls, (unsigned long long)counters[i].mismatches, (unsigned long long)counters[i].native);
    for (int i=0;i<nlog;i++) fprintf(stderr, "  ! %s\n", mismatch_log[i]);
}

/* ------------------------------------------------------------------------- */
/* abacus helpers                                                             */
/* ------------------------------------------------------------------------- */
static CrystallineAbacus *ZERO, *ONE, *TWO, *MAXU, *P63, *POW2[260]; /* base 65536 */
static CrystallineAbacus *ONE2;                                     /* base 2 */

static CrystallineAbacus *U(uint64_t v) { return abacus_from_uint64(v, BASE); }
static CrystallineAbacus *U2(uint64_t v) { return abacus_from_uint64(v, 2); }
static CrystallineAbacus *N(void) { return abacus_new(BASE); }
static CrystallineAbacus *N2(void) { return abacus_new(2); }

/* read digits with weight_exponent in [lo, hi) as an integer (base 65536) */
static uint64_t D(const CrystallineAbacus *x, int lo, int hi) {
    uint64_t v = 0;
    for (size_t i = 0; i < x->num_beads; i++) {
        int32_t w = x->beads[i].weight_exponent;
        if (w >= lo && w < hi) v |= (uint64_t)x->beads[i].value << (16 * (w - lo));
    }
    return v;
}
/* read base-2 beads with weight in [lo, hi) as an integer */
static uint64_t D2(const CrystallineAbacus *x, int lo, int hi) {
    uint64_t v = 0;
    for (size_t i = 0; i < x->num_beads; i++) {
        int32_t w = x->beads[i].weight_exponent;
        if (w >= lo && w < hi && (x->beads[i].value & 1)) v |= 1ULL << (w - lo);
    }
    return v;
}
static void F(CrystallineAbacus *a) { if (a) abacus_free(a); }

void cx_init(void) {
    static int done = 0; if (done) return; done = 1;
    ZERO = U(0); ONE = U(1); TWO = U(2); MAXU = U(0xFFFFFFFFFFFFFFFFULL); P63 = U(1ULL << 63);
    ONE2 = U2(1);
    POW2[0] = U(1);
    for (int k = 1; k < 260; k++) { POW2[k] = N(); abacus_mul(POW2[k], POW2[k-1], TWO); }
}

/* ------------------------------------------------------------------------- */
/* integer ops                                                                */
/* ------------------------------------------------------------------------- */
#define RET(c, cxv, nv, fmt, ...) do { int ok_ = ((cxv) == (nv)); \
    note(c, ok_, "%s " fmt " -> cx %016llx native %016llx", counters[c].name, __VA_ARGS__, \
         (unsigned long long)(cxv), (unsigned long long)(nv)); \
    return mode == CX_STRICT ? (cxv) : (nv); } while (0)

uint64_t cx_add64(uint64_t a, uint64_t b) {
    uint64_t nv = a + b;
    if (mode == CX_OFF) { counters[C_ADD64].native++; return nv; }
    CrystallineAbacus *A = U(a), *B = U(b), *R = N();
    abacus_add(R, A, B);
    uint64_t cv = D(R, 0, 4);
    F(A); F(B); F(R);
    RET(C_ADD64, cv, nv, "%016llx %016llx", (unsigned long long)a, (unsigned long long)b);
}

uint64_t cx_sub64(uint64_t a, uint64_t b) {
    uint64_t nv = a - b;
    if (mode == CX_OFF) { counters[C_SUB64].native++; return nv; }
    CrystallineAbacus *A = U(a), *B = U(b), *R = N();
    uint64_t cv;
    if (abacus_compare(A, B) >= 0) {
        abacus_sub(R, A, B);
        cv = D(R, 0, 4);
    } else {
        /* a < b: 2^64 - (b - a) = (MAXU - (b - a)) + 1, everything below 2^64 */
        CrystallineAbacus *T = N(), *T2 = N();
        abacus_sub(R, B, A);
        abacus_sub(T, MAXU, R);
        abacus_add(T2, T, ONE);
        cv = D(T2, 0, 4);
        F(T); F(T2);
    }
    F(A); F(B); F(R);
    RET(C_SUB64, cv, nv, "%016llx %016llx", (unsigned long long)a, (unsigned long long)b);
}

static void mul128(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi) {
    CrystallineAbacus *A = U(a), *B = U(b), *R = N();
    abacus_mul(R, A, B);
    *lo = D(R, 0, 4); *hi = D(R, 4, 8);
    F(A); F(B); F(R);
}
uint64_t cx_mul64(uint64_t a, uint64_t b) {
    uint64_t nv = a * b;
    if (mode == CX_OFF) { counters[C_MUL64].native++; return nv; }
    uint64_t lo, hi; mul128(a, b, &lo, &hi);
    RET(C_MUL64, lo, nv, "%016llx %016llx", (unsigned long long)a, (unsigned long long)b);
}
uint64_t cx_mulh64(uint64_t a, uint64_t b) {
    uint64_t nv = (uint64_t)(((__uint128_t)a * b) >> 64);
    if (mode == CX_OFF) { counters[C_MULH64].native++; return nv; }
    uint64_t lo, hi; mul128(a, b, &lo, &hi);
    RET(C_MULH64, hi, nv, "%016llx %016llx", (unsigned long long)a, (unsigned long long)b);
}
int64_t cx_smulh64(int64_t a, int64_t b) {
    int64_t nv = (int64_t)(((__int128)a * b) >> 64);
    if (mode == CX_OFF) { counters[C_SMULH64].native++; return nv; }
    /* smulh(a,b) = mulh(ua,ub) - (a<0 ? ub : 0) - (b<0 ? ua : 0)   (mod 2^64) */
    uint64_t ua = (uint64_t)a, ub = (uint64_t)b, lo, hi;
    mul128(ua, ub, &lo, &hi);
    CrystallineAbacus *A = U(ua), *B = U(ub);
    int aneg = abacus_compare(A, P63) >= 0, bneg = abacus_compare(B, P63) >= 0;
    F(A); F(B);
    int saved = mode; mode = CX_STRICT;   /* inner ops are part of this op */
    uint64_t cv = hi;
    if (aneg) cv = cx_sub64(cv, ub);
    if (bneg) cv = cx_sub64(cv, ua);
    mode = saved;
    counters[C_SUB64].calls -= (aneg + bneg);
    RET(C_SMULH64, (int64_t)cv, nv, "%016llx %016llx", (unsigned long long)ua, (unsigned long long)ub);
}
uint64_t cx_neg64(uint64_t a) {
    uint64_t nv = (uint64_t)0 - a;
    if (mode == CX_OFF) { counters[C_NEG64].native++; return nv; }
    int saved = mode; mode = CX_STRICT;
    uint64_t cv = cx_sub64(0, a);
    mode = saved; counters[C_SUB64].calls--;
    RET(C_NEG64, cv, nv, "%016llx", (unsigned long long)a);
}

/* digit-wise ops on base-2 beads */
static uint64_t bitop(uint64_t a, uint64_t b, int op) {
    CrystallineAbacus *A = U2(a), *B = U2(b);
    uint8_t da[64] = {0}, db[64] = {0};
    for (size_t i = 0; i < A->num_beads; i++) { int32_t w = A->beads[i].weight_exponent; if (w >= 0 && w < 64) da[w] = A->beads[i].value & 1; }
    for (size_t i = 0; i < B->num_beads; i++) { int32_t w = B->beads[i].weight_exponent; if (w >= 0 && w < 64) db[w] = B->beads[i].value & 1; }
    uint64_t v = 0;
    for (int w = 0; w < 64; w++) {
        unsigned d = op == 0 ? (da[w] + db[w]) % 2       /* xor: GF(2) add */
                   : op == 1 ? (da[w] * db[w])           /* and: GF(2) mul */
                   :           (da[w] + db[w] + da[w]*db[w]) % 2; /* or */
        if (d) v |= 1ULL << w;
    }
    F(A); F(B);
    return v;
}
uint64_t cx_xor64(uint64_t a, uint64_t b) {
    uint64_t nv = a ^ b; if (mode == CX_OFF) { counters[C_XOR64].native++; return nv; }
    uint64_t cv = bitop(a, b, 0);
    RET(C_XOR64, cv, nv, "%016llx %016llx", (unsigned long long)a, (unsigned long long)b);
}
uint64_t cx_and64(uint64_t a, uint64_t b) {
    uint64_t nv = a & b; if (mode == CX_OFF) { counters[C_AND64].native++; return nv; }
    uint64_t cv = bitop(a, b, 1);
    RET(C_AND64, cv, nv, "%016llx %016llx", (unsigned long long)a, (unsigned long long)b);
}
uint64_t cx_or64(uint64_t a, uint64_t b) {
    uint64_t nv = a | b; if (mode == CX_OFF) { counters[C_OR64].native++; return nv; }
    uint64_t cv = bitop(a, b, 2);
    RET(C_OR64, cv, nv, "%016llx %016llx", (unsigned long long)a, (unsigned long long)b);
}
uint64_t cx_shl64(uint64_t a, unsigned n) {
    n &= 63; uint64_t nv = a << n; if (mode == CX_OFF) { counters[C_SHL64].native++; return nv; }
    CrystallineAbacus *A = U2(a), *R = N2();
    abacus_shift_left(R, A, n);
    uint64_t cv = D2(R, 0, 64);            /* beads at weight >= 64 fall off */
    F(A); F(R);
    RET(C_SHL64, cv, nv, "%016llx <<%u", (unsigned long long)a, n);
}
uint64_t cx_shr64(uint64_t a, unsigned n) {
    n &= 63; uint64_t nv = a >> n; if (mode == CX_OFF) { counters[C_SHR64].native++; return nv; }
    CrystallineAbacus *A = U2(a), *R = N2();
    abacus_shift_right(R, A, n);
    uint64_t cv = D2(R, 0, 64);            /* fractional beads (w < 0) are the truncation */
    F(A); F(R);
    RET(C_SHR64, cv, nv, "%016llx >>%u", (unsigned long long)a, n);
}
static uint64_t rot(uint64_t a, unsigned n, int left) {
    CrystallineAbacus *A = U2(a);
    uint64_t v = 0;
    for (size_t i = 0; i < A->num_beads; i++) {
        int32_t w = A->beads[i].weight_exponent;
        if (w < 0 || w >= 64 || !(A->beads[i].value & 1)) continue;
        int nw = left ? (w + (int)n) % 64 : (w - (int)n + 64) % 64;
        v |= 1ULL << nw;
    }
    F(A);
    return v;
}
uint64_t cx_ror64(uint64_t a, unsigned n) {
    n &= 63; uint64_t nv = n ? (a >> n) | (a << (64 - n)) : a;
    if (mode == CX_OFF) { counters[C_ROR64].native++; return nv; }
    uint64_t cv = rot(a, n, 0);
    RET(C_ROR64, cv, nv, "%016llx ror %u", (unsigned long long)a, n);
}
uint64_t cx_rol64(uint64_t a, unsigned n) {
    n &= 63; uint64_t nv = n ? (a << n) | (a >> (64 - n)) : a;
    if (mode == CX_OFF) { counters[C_ROL64].native++; return nv; }
    uint64_t cv = rot(a, n, 1);
    RET(C_ROL64, cv, nv, "%016llx rol %u", (unsigned long long)a, n);
}
int cx_cmp64(uint64_t a, uint64_t b) {
    int nv = a < b ? -1 : a > b ? 1 : 0;
    if (mode == CX_OFF) { counters[C_CMP64].native++; return nv; }
    CrystallineAbacus *A = U(a), *B = U(b);
    int cv = abacus_compare(A, B);
    F(A); F(B);
    note(C_CMP64, cv == nv, "cmp64 %016llx %016llx -> cx %d native %d", (unsigned long long)a, (unsigned long long)b, cv, nv);
    return mode == CX_STRICT ? cv : nv;
}
uint32_t cx_add32(uint32_t a, uint32_t b) {
    uint32_t nv = a + b; if (mode == CX_OFF) { counters[C_ADD32].native++; return nv; }
    CrystallineAbacus *A = U(a), *B = U(b), *R = N();
    abacus_add(R, A, B);
    uint32_t cv = (uint32_t)D(R, 0, 2);
    F(A); F(B); F(R);
    RET(C_ADD32, cv, nv, "%08x %08x", a, b);
}
uint32_t cx_mul32(uint32_t a, uint32_t b) {
    uint32_t nv = a * b; if (mode == CX_OFF) { counters[C_MUL32].native++; return nv; }
    CrystallineAbacus *A = U(a), *B = U(b), *R = N();
    abacus_mul(R, A, B);
    uint32_t cv = (uint32_t)D(R, 0, 2);
    F(A); F(B); F(R);
    RET(C_MUL32, cv, nv, "%08x %08x", a, b);
}
uint64_t cx_mul32x32(uint32_t a, uint32_t b) {
    uint64_t nv = (uint64_t)a * b; if (mode == CX_OFF) { counters[C_MUL32X32].native++; return nv; }
    CrystallineAbacus *A = U(a), *B = U(b), *R = N();
    abacus_mul(R, A, B);
    uint64_t cv = D(R, 0, 4);
    F(A); F(B); F(R);
    RET(C_MUL32X32, cv, nv, "%08x %08x", a, b);
}

/* ------------------------------------------------------------------------- */
/* big unsigned integers on base-2 beads: long division and isqrt            */
/* built only from abacus_compare / abacus_sub / abacus_shift_* / abacus_add */
/* ------------------------------------------------------------------------- */
static int bitlen2(const CrystallineAbacus *x) {
    int32_t mx = -1;
    for (size_t i = 0; i < x->num_beads; i++)
        if ((x->beads[i].value & 1) && x->beads[i].weight_exponent > mx) mx = x->beads[i].weight_exponent;
    return mx + 1;
}
/* integer part of a base-2 abacus, as a fresh abacus (drops fractional beads) */
static CrystallineAbacus *intpart2(const CrystallineAbacus *x) {
    CrystallineAbacus *r = N2();
    abacus_truncate(r, x, 0);
    return r;
}
/* q = n / d, r = n % d  (all base-2 abaci, n,d > 0). Restoring division. */
static void divmod2(const CrystallineAbacus *n, const CrystallineAbacus *d, CrystallineAbacus **q_out, CrystallineAbacus **r_out) {
    int nb = bitlen2(n);
    CrystallineAbacus *rem = U2(0), *q = U2(0);
    for (int i = nb - 1; i >= 0; i--) {
        /* rem = rem*2 + bit_i(n); q = q*2 */
        CrystallineAbacus *t = N2(); abacus_shift_left(t, rem, 1); F(rem); rem = t;
        int bit = (int)D2(n, i, i + 1);
        if (bit) { t = N2(); abacus_add(t, rem, ONE2); F(rem); rem = t; }
        t = N2(); abacus_shift_left(t, q, 1); F(q); q = t;
        if (abacus_compare(rem, d) >= 0) {
            t = N2(); abacus_sub(t, rem, d); F(rem); rem = t;
            t = N2(); abacus_add(t, q, ONE2); F(q); q = t;
        }
    }
    *q_out = q; *r_out = rem;
}
/* s = floor(sqrt(n)), r = n - s*s.  Digit-by-digit binary method. */
static void isqrt2(const CrystallineAbacus *n, CrystallineAbacus **s_out, CrystallineAbacus **r_out) {
    int nb = bitlen2(n);
    int top = (nb - 1) & ~1;                 /* highest even bit position */
    CrystallineAbacus *rem = U2(0), *res = U2(0), *t, *u;
    for (int i = top; i >= 0; i -= 2) {
        /* rem = rem*4 + next two bits */
        t = N2(); abacus_shift_left(t, rem, 2); F(rem); rem = t;
        uint64_t two = D2(n, i, i + 2);
        if (two) { CrystallineAbacus *b = U2(two); t = N2(); abacus_add(t, rem, b); F(rem); F(b); rem = t; }
        /* candidate = res*4 + 1 ; if rem >= candidate: rem -= candidate, res = res*2 + 1 else res = res*2 */
        CrystallineAbacus *cand = N2(); abacus_shift_left(cand, res, 2);
        u = N2(); abacus_add(u, cand, ONE2); F(cand); cand = u;
        t = N2(); abacus_shift_left(t, res, 1); F(res); res = t;
        if (abacus_compare(rem, cand) >= 0) {
            t = N2(); abacus_sub(t, rem, cand); F(rem); rem = t;
            t = N2(); abacus_add(t, res, ONE2); F(res); res = t;
        }
        F(cand);
    }
    *s_out = res; *r_out = rem;
}

uint64_t cx_rcp64(uint32_t divisor) {
    /* reference: q = 2^63 / d, r = 2^63 % d, shift = 64 - clz(d); (q << shift) + ((r << shift) / d) */
    uint64_t nv;
    {
        const uint64_t p63 = 1ULL << 63, q = p63 / divisor, r = p63 % divisor;
        const uint32_t shift = 64 - __builtin_clzll(divisor);
        nv = (q << shift) + ((r << shift) / divisor);
    }
    if (mode == CX_OFF) { counters[C_RCP64].native++; return nv; }
    CrystallineAbacus *P = U2(1ULL << 63), *Dv = U2(divisor), *q, *r;
    divmod2(P, Dv, &q, &r);
    int shift = bitlen2(Dv);
    CrystallineAbacus *qs = N2(), *rs = N2(), *q2, *r2, *sum = N2();
    abacus_shift_left(qs, q, shift);
    abacus_shift_left(rs, r, shift);
    divmod2(rs, Dv, &q2, &r2);
    abacus_add(sum, qs, q2);
    uint64_t cv = D2(sum, 0, 64);
    F(P); F(Dv); F(q); F(r); F(qs); F(rs); F(q2); F(r2); F(sum);
    RET(C_RCP64, cv, nv, "%08x", divisor);
}

uint64_t cx_mod64(uint64_t a, uint64_t b) {
    uint64_t nv = a % b;
    if (mode == CX_OFF) { counters[C_MOD64].native++; return nv; }
    CrystallineAbacus *A = U2(a), *B = U2(b), *q, *r;
    uint64_t cv;
    if (abacus_is_zero(A)) cv = 0;
    else { divmod2(A, B, &q, &r); cv = D2(r, 0, 64); F(q); F(r); }
    F(A); F(B);
    RET(C_MOD64, cv, nv, "%016llx %% %016llx", (unsigned long long)a, (unsigned long long)b);
}
int cx_is_zero_or_pow2(uint64_t x) {
    int saved = mode; if (mode == CX_OFF) return (x & (x - 1)) == 0;
    mode = CX_STRICT;
    uint64_t v = cx_and64(x, cx_sub64(x, 1));
    mode = saved;
    return v == 0;
}

int cx_rmode = 0;
void cx_set_rmode(int m) { cx_rmode = m & 3; }

static uint64_t argon_total = 0, argon_checked = 0; static int argon_every = -1;
int cx_argon_skip(void) {
    if (argon_every < 0) { const char *e = getenv("CX_ARGON_EVERY"); argon_every = e ? atoi(e) : 1; if (argon_every < 1) argon_every = 1; }
    argon_total++;
    if ((argon_total - 1) % (uint64_t)argon_every) return 1;
    argon_checked++; return 0;
}
uint64_t cx_argon_blocks_checked(void) { return argon_checked; }
uint64_t cx_argon_blocks_total(void) { return argon_total; }

/* ------------------------------------------------------------------------- */
/* IEEE-754 binary64                                                          */
/* ------------------------------------------------------------------------- */
typedef struct { int sign; int zero; int special; CrystallineAbacus *M; int E; } cxf;
/* value = (-1)^sign * M * 2^E ; M is a base-2 integer abacus */

static uint64_t d2u(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double   u2d(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }

static cxf decomp(double d) {
    cxf f; memset(&f, 0, sizeof f);
    uint64_t u = d2u(d);
    f.sign = (int)(u >> 63);
    int e = (int)((u >> 52) & 0x7FF);
    uint64_t frac = u & ((1ULL << 52) - 1);
    if (e == 0x7FF) { f.special = 1; return f; }
    if (e == 0) {
        if (frac == 0) { f.zero = 1; return f; }
        /* subnormal: normalise to a 53-bit mantissa so div/sqrt keep enough quotient bits */
        CrystallineAbacus *raw = U2(frac); int nb = bitlen2(raw);
        f.M = N2(); abacus_shift_left(f.M, raw, 53 - nb); F(raw);
        f.E = -1074 - (53 - nb); return f;
    }
    f.M = U2(frac | (1ULL << 52)); f.E = e - 1075;
    return f;
}
static void cxf_free(cxf *f) { if (f->M) { F(f->M); f->M = NULL; } }

/* round (sign, M, E) to a double under mode. M base-2 abacus, may be any width. */
static double compose(int sign, CrystallineAbacus *M, int E, int rmode) {
    int n = bitlen2(M);
    if (n == 0) return u2d((uint64_t)sign << 63);
    int Et = E + n - 53;
    if (Et < -1074) Et = -1074;
    int k = Et - E;                          /* bits to drop (k>0) or add (k<0) */
    uint64_t m; int guard = 0, sticky = 0;
    if (k > 0) {
        m = D2(M, k, k + 53);
        guard = (int)D2(M, k - 1, k);
        for (int i = 0; i < k - 1 && !sticky; i++) sticky |= (int)D2(M, i, i + 1);
    } else {
        m = D2(M, 0, n) << (-k);
    }
    int inc = 0;
    switch (rmode & 3) {
        case 0: inc = guard && (sticky || (m & 1)); break;   /* nearest even */
        case 1: inc = sign && (guard || sticky); break;       /* toward -inf  */
        case 2: inc = !sign && (guard || sticky); break;      /* toward +inf  */
        case 3: inc = 0; break;                               /* toward zero  */
    }
    if (inc) {
        int saved = mode; mode = CX_STRICT; m = cx_add64(m, 1); mode = saved; counters[C_ADD64].calls--;
    }
    if (m == (1ULL << 53)) { m >>= 1; Et++; }
    int biased;
    if (m >= (1ULL << 52)) biased = Et + 1075; else biased = 0;   /* subnormal or zero */
    if (biased >= 0x7FF) {
        /* overflow: RN and the "away" direction give inf; RZ and the "toward" direction give max finite */
        int to_inf = (rmode & 3) == 0 || ((rmode & 3) == 2 && !sign) || ((rmode & 3) == 1 && sign);
        uint64_t mag = to_inf ? (0x7FFULL << 52) : 0x7FEFFFFFFFFFFFFFULL;
        return u2d(((uint64_t)sign << 63) | mag);
    }
    if (biased < 0) biased = 0;
    uint64_t bits = ((uint64_t)sign << 63) | ((uint64_t)biased << 52) | (m & ((1ULL << 52) - 1));
    return u2d(bits);
}

static double native_f(int op, double a, double b, int rmode) {
    static const int fe[4] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
    int old = fegetround(); fesetround(fe[rmode & 3]);
    volatile double r;
    switch (op) { case 0: r = a + b; break; case 1: r = a - b; break; case 2: r = a * b; break;
                  case 3: r = a / b; break; default: r = __builtin_sqrt(a); }
    /* __builtin_sqrt: crystalline's include/math.h shadows libc <math.h> on the -I path */
    fesetround(old);
    return r;
}

static double fadd_impl(double a, double b, int rmode, int sub) {
    cxf x = decomp(a), y = decomp(sub ? -b : b);
    double out;
    if (x.special || y.special) { out = native_f(sub, a, b, rmode); goto done; }
    if (x.zero && y.zero) {
        int s = (x.sign && y.sign) || ((rmode & 3) == 1 && (x.sign || y.sign));
        out = u2d((uint64_t)s << 63); goto done;
    }
    if (x.zero) { out = sub ? -b : b; goto done; }
    if (y.zero) { out = a; goto done; }
    if (x.E < y.E) { cxf t = x; x = y; y = t; }          /* x has the larger exponent */
    int d = x.E - y.E;
    CrystallineAbacus *Mx, *M = N2(), *t;
    int E, sign;
    if (d > 130) {
        /* y only contributes a sticky bit: value strictly between representable neighbours */
        Mx = N2(); abacus_shift_left(Mx, x.M, 2);
        if (x.sign == y.sign) abacus_add(M, Mx, ONE2); else abacus_sub(M, Mx, ONE2);
        E = x.E - 2; sign = x.sign;
        F(Mx);
    } else {
        Mx = N2(); abacus_shift_left(Mx, x.M, d);
        E = y.E;
        if (x.sign == y.sign) { abacus_add(M, Mx, y.M); sign = x.sign; }
        else {
            int c = abacus_compare(Mx, y.M);
            if (c == 0) { F(Mx); F(M); out = u2d((uint64_t)((rmode & 3) == 1) << 63); goto done; }
            if (c > 0) { abacus_sub(M, Mx, y.M); sign = x.sign; }
            else       { abacus_sub(M, y.M, Mx); sign = y.sign; }
        }
        F(Mx);
    }
    t = intpart2(M); F(M); M = t;
    out = compose(sign, M, E, rmode);
    F(M);
done:
    cxf_free(&x); cxf_free(&y);
    return out;
}
double cx_fadd(double a, double b, int rmode) {
    double nv = native_f(0, a, b, rmode);
    if (mode == CX_OFF) { counters[C_FADD].native++; return nv; }
    double cv = fadd_impl(a, b, rmode, 0);
    int ok = d2u(cv) == d2u(nv);
    note(C_FADD, ok, "fadd %016llx %016llx rm%d -> cx %016llx native %016llx", (unsigned long long)d2u(a), (unsigned long long)d2u(b), rmode & 3, (unsigned long long)d2u(cv), (unsigned long long)d2u(nv));
    return mode == CX_STRICT ? cv : nv;
}
double cx_fsub(double a, double b, int rmode) {
    double nv = native_f(1, a, b, rmode);
    if (mode == CX_OFF) { counters[C_FSUB].native++; return nv; }
    double cv = fadd_impl(a, b, rmode, 1);
    int ok = d2u(cv) == d2u(nv);
    note(C_FSUB, ok, "fsub %016llx %016llx rm%d -> cx %016llx native %016llx", (unsigned long long)d2u(a), (unsigned long long)d2u(b), rmode & 3, (unsigned long long)d2u(cv), (unsigned long long)d2u(nv));
    return mode == CX_STRICT ? cv : nv;
}
double cx_fmul(double a, double b, int rmode) {
    double nv = native_f(2, a, b, rmode);
    if (mode == CX_OFF) { counters[C_FMUL].native++; return nv; }
    cxf x = decomp(a), y = decomp(b); double cv;
    if (x.special || y.special) cv = nv;
    else if (x.zero || y.zero) cv = u2d((uint64_t)(x.sign ^ y.sign) << 63);
    else {
        CrystallineAbacus *M = N2(); abacus_mul(M, x.M, y.M);
        cv = compose(x.sign ^ y.sign, M, x.E + y.E, rmode); F(M);
    }
    cxf_free(&x); cxf_free(&y);
    int ok = d2u(cv) == d2u(nv);
    note(C_FMUL, ok, "fmul %016llx %016llx rm%d -> cx %016llx native %016llx", (unsigned long long)d2u(a), (unsigned long long)d2u(b), rmode & 3, (unsigned long long)d2u(cv), (unsigned long long)d2u(nv));
    return mode == CX_STRICT ? cv : nv;
}
double cx_fdiv(double a, double b, int rmode) {
    double nv = native_f(3, a, b, rmode);
    if (mode == CX_OFF) { counters[C_FDIV].native++; return nv; }
    cxf x = decomp(a), y = decomp(b); double cv;
    if (x.special || y.special || y.zero) cv = nv;            /* inf/nan paths: not RandomX-reachable */
    else if (x.zero) cv = u2d((uint64_t)(x.sign ^ y.sign) << 63);
    else {
        const int K = 64;
        CrystallineAbacus *Nn = N2(), *q, *r, *M = N2();
        abacus_shift_left(Nn, x.M, K);
        divmod2(Nn, y.M, &q, &r);
        abacus_shift_left(M, q, 1);
        if (!abacus_is_zero(r)) { CrystallineAbacus *t = N2(); abacus_add(t, M, ONE2); F(M); M = t; }
        cv = compose(x.sign ^ y.sign, M, x.E - y.E - K - 1, rmode);
        F(Nn); F(q); F(r); F(M);
    }
    cxf_free(&x); cxf_free(&y);
    int ok = d2u(cv) == d2u(nv);
    note(C_FDIV, ok, "fdiv %016llx %016llx rm%d -> cx %016llx native %016llx", (unsigned long long)d2u(a), (unsigned long long)d2u(b), rmode & 3, (unsigned long long)d2u(cv), (unsigned long long)d2u(nv));
    return mode == CX_STRICT ? cv : nv;
}
double cx_fsqrt(double a, int rmode) {
    double nv = native_f(4, a, 0, rmode);
    if (mode == CX_OFF) { counters[C_FSQRT].native++; return nv; }
    cxf x = decomp(a); double cv;
    if (x.special || x.zero || x.sign) cv = nv;               /* -x: NaN, not RandomX-reachable */
    else {
        int E = x.E; CrystallineAbacus *M0 = N2();
        if (E & 1) { abacus_shift_left(M0, x.M, 1); E--; } else abacus_shift_left(M0, x.M, 0);
        const int K = 32;
        CrystallineAbacus *Nn = N2(), *s, *r, *M = N2();
        abacus_shift_left(Nn, M0, 2 * K);
        isqrt2(Nn, &s, &r);
        abacus_shift_left(M, s, 1);
        if (!abacus_is_zero(r)) { CrystallineAbacus *t = N2(); abacus_add(t, M, ONE2); F(M); M = t; }
        cv = compose(0, M, (E - 2 * K) / 2 - 1, rmode);
        F(M0); F(Nn); F(s); F(r); F(M);
    }
    cxf_free(&x);
    int ok = d2u(cv) == d2u(nv);
    note(C_FSQRT, ok, "fsqrt %016llx rm%d -> cx %016llx native %016llx", (unsigned long long)d2u(a), rmode & 3, (unsigned long long)d2u(cv), (unsigned long long)d2u(nv));
    return mode == CX_STRICT ? cv : nv;
}
double cx_i32_to_f64(int32_t v) {
    double nv = (double)v;
    if (mode == CX_OFF) { counters[C_I2F].native++; return nv; }
    double cv;
    if (v == 0) cv = 0.0;
    else {
        uint64_t mag = v < 0 ? (uint64_t)(-(int64_t)v) : (uint64_t)v;
        CrystallineAbacus *M = U2(mag);
        cv = compose(v < 0, M, 0, 0); F(M);
    }
    int ok = d2u(cv) == d2u(nv);
    note(C_I2F, ok, "i32_to_f64 %d -> cx %016llx native %016llx", v, (unsigned long long)d2u(cv), (unsigned long long)d2u(nv));
    return mode == CX_STRICT ? cv : nv;
}
