/* probe.c — how the abacus calls used by xcheck actually behave */
#include <stdio.h>
#include <stdlib.h>
#include "math/abacus.h"

static void show(const char* what, MathError e, const CrystallineAbacus* a) {
    char* s = a ? abacus_to_string(a) : NULL;
    uint64_t v = 0;
    MathError e2 = a ? abacus_to_uint64(a, &v) : 0;
    printf("%-44s err=%d  str=%s  u64=%llx (err %d)\n", what, (int)e, s ? s : "-", (unsigned long long)v, (int)e2);
    free(s);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    for (uint32_t base = 2; base <= 60; base += 58) {
        printf("--- base %u\n", base);
        CrystallineAbacus *two = abacus_from_uint64(2, base), *r = abacus_new(base);
        show("pow_uint64(2, 64)", abacus_pow_uint64(r, two, 64), r);
        CrystallineAbacus *r2 = abacus_new(base);
        show("pow_uint64(2, 10)", abacus_pow_uint64(r2, two, 10), r2);
        CrystallineAbacus *one = abacus_from_uint64(1, base), *s = abacus_new(base);
        show("shift_left(1, 64) [base^64]", abacus_shift_left(s, one, 64), s);
        CrystallineAbacus *big = abacus_from_uint64(0xFFFFFFFFFFFFFFFFull, base);
        show("from_uint64(2^64-1)", 0, big);
        CrystallineAbacus *p = abacus_new(base);
        show("mul(2^64-1, 2)", abacus_mul(p, big, two), p);
        CrystallineAbacus *m = abacus_new(base);
        show("add(2^64-1, 1)", abacus_add(m, big, one), m);
        CrystallineAbacus *zero = abacus_from_uint64(0, base), *ma = abacus_new(base);
        show("mod_add(0, 1, 2^64-1+1)", abacus_mod_add(ma, zero, one, m), ma);
        CrystallineAbacus *mm = abacus_new(base);
        show("mod(2^64-1 * 2, 2^64)", abacus_mod(mm, p, m), mm);
    }
    return 0;
}
