/*
 * cx64 — the crystalline abacus as a 64-bit ALU, under test.
 *
 * Every RandomX primitive op (integer and IEEE double) is computed TWICE:
 *   - natively (the reference behaviour), and
 *   - on the CrystallineAbacus (the engine under test).
 * The two are compared per call. Counters and the first mismatches are kept.
 *
 * Modes (cx_set_mode):
 *   CX_OFF    — native only, no crystalline (baseline / speed).
 *   CX_CHECK  — both, return the native result, record mismatches.
 *   CX_STRICT — both, return the CRYSTALLINE result. A crystalline defect
 *               then propagates into the hash and the official test vector
 *               fails. This is the mode that proves the engine.
 */
#ifndef CX64_H
#define CX64_H
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { CX_OFF = 0, CX_CHECK = 1, CX_STRICT = 2 };

void     cx_init(void);
void     cx_set_mode(int mode);
int      cx_get_mode(void);

/* ---- integer, mod 2^64 --------------------------------------------------- */
uint64_t cx_add64 (uint64_t a, uint64_t b);
uint64_t cx_sub64 (uint64_t a, uint64_t b);
uint64_t cx_mul64 (uint64_t a, uint64_t b);          /* low 64 of a*b   */
uint64_t cx_mulh64(uint64_t a, uint64_t b);          /* high 64, unsigned */
int64_t  cx_smulh64(int64_t a, int64_t b);           /* high 64, signed   */
uint64_t cx_neg64 (uint64_t a);                      /* 2^64 - a          */
uint64_t cx_xor64 (uint64_t a, uint64_t b);
uint64_t cx_and64 (uint64_t a, uint64_t b);
uint64_t cx_or64  (uint64_t a, uint64_t b);
uint64_t cx_shl64 (uint64_t a, unsigned n);          /* n in 0..63 */
uint64_t cx_shr64 (uint64_t a, unsigned n);
uint64_t cx_ror64 (uint64_t a, unsigned n);
uint64_t cx_rol64 (uint64_t a, unsigned n);
int      cx_cmp64 (uint64_t a, uint64_t b);          /* -1, 0, 1 */
uint64_t cx_rcp64 (uint32_t divisor);                /* randomx_reciprocal */
uint32_t cx_add32 (uint32_t a, uint32_t b);
uint32_t cx_mul32 (uint32_t a, uint32_t b);
uint64_t cx_mul32x32(uint32_t a, uint32_t b);        /* exact 64-bit product */
uint64_t cx_mod64 (uint64_t a, uint64_t b);          /* a % b, b != 0 (long division on beads) */
int      cx_is_zero_or_pow2(uint64_t x);             /* (x & (x-1)) == 0 */

/* RandomX rounding-mode tracking: the hooked VM records CFROUND here and
 * also sets the FPU so the native half of every float op agrees. */
extern int cx_rmode;
void     cx_set_rmode(int m);

/* Argon2 sampling: CX_ARGON_EVERY=N verifies every Nth fill_block (default 1 = all).
 * Returns nonzero when the current block should be skipped (run native). */
int      cx_argon_skip(void);
uint64_t cx_argon_blocks_checked(void);
uint64_t cx_argon_blocks_total(void);

/* ---- IEEE-754 binary64 with explicit rounding mode ----------------------- */
/* mode: 0 = nearest-even, 1 = toward -inf, 2 = toward +inf, 3 = toward zero
 * (RandomX CFROUND encoding). */
double   cx_fadd(double a, double b, int mode);
double   cx_fsub(double a, double b, int mode);
double   cx_fmul(double a, double b, int mode);
double   cx_fdiv(double a, double b, int mode);
double   cx_fsqrt(double a, int mode);
double   cx_i32_to_f64(int32_t v);                    /* exact */

/* ---- bookkeeping --------------------------------------------------------- */
typedef struct {
    const char *name;
    uint64_t    calls;        /* computed on the abacus (CX_CHECK / CX_STRICT) */
    uint64_t    mismatches;
    uint64_t    native;       /* served natively while the harness was in CX_OFF */
} cx_counter_t;

#define CX_NCOUNTERS 32
const cx_counter_t *cx_counters(size_t *n);
uint64_t cx_total_calls(void);       /* crystalline-computed ops only */
uint64_t cx_total_native(void);      /* ops served natively in CX_OFF */
uint64_t cx_total_mismatches(void);
void     cx_report(void);            /* print counters + first mismatches */
void     cx_reset_counters(void);

#ifdef __cplusplus
}
#endif
#endif
