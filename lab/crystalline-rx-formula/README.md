# crystalline-rx-formula

RandomX v1, `R = F(K, H)`, written only in Crystalline Abacus primitives:
the ring Z/2^64, base-2 beads, clock rotation, GF(2^8) and exactly rounded
binary64. No hash library, no hardware AES, no FPU.

Derived 2026-09-12 from the RandomX spec as recalled, without reading
tevador's code or the lab.

Result for tevador's tests.cpp vector 1a:

```
K = "test key 000"
H = "This is a test"
R = 639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f   MATCH
```

It also matches the lab's PROG 0 / IC 0 machine state (A0–A3, F0–F3, E0–E3,
MA, MX, SPA0, SPA1: 16 of 16).

## The formula

```
S₁, SP = Fill1R( B512(H) )                 SP = 2 MiB scratchpad, S₁ = generator's final state
C      = Argon2d( K, "RandomX\x03", 3 passes, 256 MiB )
P₁…P₈  = Superscalar( B2Gen(K) )           D(i) = dataset line i, built from C by P₁…P₈ on demand
Sₖ₊₁   = B512( r‖f‖e‖a after VM(Fill4R(Sₖ), SP, D) )        k = 1…7
R      = B256( r‖f‖e‖Hash1R(SP) after VM(Fill4R(S₈), SP, D) )
```

VM = 2048 iterations of 256 instructions. Integer registers start at 0 for
each program; the scratchpad and the rounding mode carry over.

| In R | Crystalline form |
|---|---|
| add, sub, mul | `abacus_mod_add` / `_sub` / `_mul`, modulus 2^64 |
| high 64 bits of a·b | `abacus_mul`, then shift right 64 beads (base 2) |
| xor, and, or | base-2 beads: (a+b) mod 2, a·b, a+b−ab |
| rotate | beads move n places round a 64-position clock |
| IMUL_RCP constant | `abacus_div(2^(63+bitlen d), d)` |
| AES | GF(2^8), no carries, reduced by x⁸+x⁴+x³+x+1; S-box = x^254 + affine map |
| float add/sub/mul/div/sqrt | exact integer result, one rounding to 53 beads in the current mode |

The S-box is computed from the field; the AES keys and hash state are Blake2b
of their name strings ("RandomX AesGenerator1R keys", ...).

## Files

| File | What |
|---|---|
| `prim.h` | the primitives, each one abacus operation at word width |
| `crx.c` | the formula, with self-tests and the screen checkpoint |
| `fptest.c` | QA: integer-built binary64 vs the FPU, all 4 rounding modes |
| `xcheck.c` | each primitive vs the real crystalline library |
| `probe.c` | small repro of the library's big-number behaviour |

## Run

```
cc -O2 -o crx crx.c
./crx "test key 000" "This is a test" 639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f

cc -O0 -frounding-math -o fptest fptest.c && ./fptest
```

`xcheck` and `probe` link against crystalline-main's math library
(`CM` = path to crystalline-main):

```
mkdir -p cobj
for f in $CM/math/src/{bigint,core,geometry,prime}/*.c; do
  cc -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -I$CM/math/include -c "$f" -o cobj/$(basename "$f" .c).o
done
cc -O2 -std=c11 -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -I$CM/math/include xcheck.c cobj/*.o -lm -o xcheck
./xcheck 50        # slow: every check runs the abacus in its own process
```

## Crystalline library bugs found

All appear once a value needs more than 8 beads:

- `abacus_pow_uint64` (`abacus_gcd.c:604–682`) copies beads into
  `result->beads` without growing it; `abacus_new` allocates 8.
- 2^64 − 1 built in base 60 (`abacus_mul`, then `abacus_sub`) reads back as 1,
  so every mod-2^64 result is wrong.
- Base 2, shift then mod 2^64: `0xffffffff << 14` gives `0x3fc000`, and
  `1 << 42` crashes.

Only the first is root-caused. R was therefore computed with word-level
versions of the operations, not by calling the library.

## Speed

About 0.3 s per hash (light mode, software AES and float), hundreds of times
slower than a tuned miner. It is a reference for testing, not a speed-up.
