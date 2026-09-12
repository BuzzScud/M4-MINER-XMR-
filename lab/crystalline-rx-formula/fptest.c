/* fptest.c — QA only: the integer-built binary64 ops in prim.h against the
 * hardware FPU in all four rounding modes. The formula itself never uses the FPU.
 *   cc -O0 -frounding-math -o fptest fptest.c
 */
#include <stdio.h>
#include <fenv.h>
#include "prim.h"

#pragma STDC FENV_ACCESS ON

static u64 bits(double d) { u64 u; memcpy(&u, &d, 8); return u; }
static double dbl(u64 u) { double d; memcpy(&d, &u, 8); return d; }
static u64 s = 0x9E3779B97F4A7C15ull;
static u64 rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }

/* operands shaped like the VM's: int32 conversions, a-register floats, masked e values, and wide normals */
static u64 operand(int kind) {
    u64 r = rnd();
    switch (kind) {
    case 0: return fcvt_i32((u32)r);
    case 1: return (((r >> 59) + 1023) << 52) | (r & 0xFFFFFFFFFFFFFull);
    case 2: return (r & ((1ull << 56) - 1)) | ((0x300ull | ((rnd() >> 60) << 4)) << 52);
    default: {
        u64 e = 0x200 + (rnd() % 0x400);
        return (r & 0x800FFFFFFFFFFFFFull) | (e << 52);
    }
    }
}

int main(void) {
    const int modes[4] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const char* names[5] = {"add", "sub", "mul", "div", "sqrt"};
    long n[5] = {0}, bad[5] = {0};
    for (int m = 0; m < 4; m++) {
        RM = m;
        fesetround(modes[m]);
        for (int i = 0; i < 1500000; i++) {
            u64 x = operand(i % 4), y = operand((i / 4) % 4);
            if (i % 7 == 0) y ^= 1ull << 63;
            volatile double X = dbl(x), Y = dbl(y);
            double h;
            h = X + Y; n[0]++; bad[0] += bits(h) != fadd(x, y);
            h = X - Y; n[1]++; bad[1] += bits(h) != fsub(x, y);
            h = X * Y; n[2]++; bad[2] += bits(h) != fmul(x, y);
            if (Y != 0) { h = X / Y; n[3]++; bad[3] += bits(h) != fdiv(x, y); }
            volatile double AX = X < 0 ? -X : X;
            h = __builtin_sqrt(AX); n[4]++; bad[4] += bits(h) != fsqrt(bits(AX));
        }
    }
    fesetround(FE_TONEAREST);
    long tb = 0;
    for (int k = 0; k < 5; k++) {
        printf("  %-4s %9ld cases  %ld mismatches\n", names[k], n[k], bad[k]);
        tb += bad[k];
    }
    printf("%s (fp_anomaly=%ld)\n", tb ? "SOFT FLOAT DIFFERS FROM IEEE" : "soft float = IEEE 754 in all 4 modes", fp_anomaly);
    return tb ? 1 : 0;
}
