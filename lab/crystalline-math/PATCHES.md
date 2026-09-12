# Lab patches to the crystalline math copy

This is a COPY of ~/Downloads/crystalline-main/math taken 2026-09-12. The original is untouched.
Every change to library source is listed here; each one is a defect found by the RandomX differential.

| # | File | Defect | Minimal repro |
|---|------|--------|---------------|
| 1 | src/bigint/abacus.c `compare_magnitude` | Digit walk stopped at min(max_exp_a, max_exp_b) = after the leading digit. Two numbers with the same digit count and same leading digit compared EQUAL. Cascades into `abacus_sub` (wrong sign / zero result), `abacus_div` (`find_quotient_digit` binary search -> infinite recursion, stack overflow), `abacus_sqrt` (err 13 / zero). | `A = 0xDEADBEEFCAFEBABE; abacus_compare(A+A, 2^64)` returned 0; `abacus_div(A*B, 2^64)` crashed. |

Known, worked around without patching (documented in cx64.c):
- `abacus_shift_left` / `abacus_mul` leave `max_exponent` stale (min can exceed max). Harness never reads that field.
- `abacus_convert_base(x -> base 2)` is lossy (0xDEADBEEFCAFEBABE -> 0xDEADBEEFCAFEB800). Harness builds base-2 values with `abacus_from_uint64(x, 2)`, which is exact.
- `abacus_shift_right` keeps the shifted-out digits as fractional beads (fixed-point, not integer). Harness reads integer digits directly / uses `abacus_truncate`.
- `abacus_mul` on fractional beads is wrong ((pi/2)^2 -> 1). Harness never multiplies fractional values; floats use integer mantissas + a C-side exponent.
- Zero beads are stripped from results (holes in the bead array). Harness reads beads by `weight_exponent`, never by index.

## Patch #2 — test configuration, not a bug fix
`src/bigint/abacus.c`: `abacus_add`, `abacus_sub`, `abacus_mul` each begin with a shortcut — if both operands
convert to `uint64_t`, the result is computed with the CPU's native `+`, `-`, `*` and converted back. `abacus_div`
and `abacus_sqrt` do the same with native `/`, `%` and a native Newton loop (those two are left as-is; the harness
does not call them). For 64-bit operands the library therefore defers to the machine. `-DCX_PURE_GEOMETRIC`
compiles the three shortcuts out so the bead-level algorithms (digit carry loops, `multiply_by_digit`) are what
gets tested. Two archives are built: `libcrystalline.a` (as shipped) and `libcrystalline_pure.a` (pure).
