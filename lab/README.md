# RandomX lab — crystalline as the arithmetic engine under test

Goal: turn `RandomX-formula-lab.html` from a sketch with stubs into a real math check for XMR mining, with the
Crystalline abacus (`~/Downloads/crystalline-main/math`) supplying every arithmetic result on the way to a hash,
checked against tevador's reference RandomX and its official test vectors.

## Layout

| path | what |
|---|---|
| `RandomX/` | tevador's reference, untouched (clone of 2026-09-08, includes RandomX v2). `RandomX/build/randomx-tests` passes 107/107 on this M4. |
| `crystalline-math/` | a COPY of the crystalline math library. `PATCHES.md` lists every change (2 patches) and every worked-around defect. Builds `libcrystalline.a` (as shipped) and `libcrystalline_pure.a` (`-DCX_PURE_GEOMETRIC`: uint64 shortcuts compiled out so the bead algorithms run). |
| `cx/cx64.c,h` | the crystalline ALU: every RandomX primitive (add/sub/mul/mulh/smulh/neg/xor/and/or/shl/shr/ror/rol/cmp/mod/reciprocal, add32/mul32, IEEE add/sub/mul/div/sqrt in all 4 rounding modes, int32→double) computed on the abacus AND natively, compared per call. Modes `off` / `check` / `strict` (strict = the abacus result is the one used). |
| `cx/cx_selftest.c` | randomized differential test of the ALU. `make && ./cx_selftest 20000` → 462,500 ops, 0 mismatches, on both library builds. |
| `cx/cx_aes.c` | AES S-box, inverse S-box and the 8 T-tables recomputed from GF(2^8) on crystalline bit ops and compared with the reference tables (0 / 2048 mismatching). |
| `cx/apply_hooks.py` | copies `RandomX/src` to `RandomX-cx/src` and rewrites every arithmetic site to `cx_*` (asserting each replacement lands): Blake2b, Argon2d, SuperscalarHash, soft AES (hardware AES compiled out), the VM's 30 opcodes, address masks, scratchpad/dataset mixing, CFROUND, entropy→config. Also inserts the trace hooks for the page and a per-key Argon2 memo. |
| `cx/cx_dump.c` + `cx_dump_main.cpp` | `cx-dump -k <key> -i <input> [-v both] -o trace.jsonl`: one hash (or several) through the hooked build, emitting per-step values as newline-delimited JSON. |
| `cx/lab_template.html` + `build_page.py` | the new lab page: `build_page.py -o RandomX-formula-lab.html trace.jsonl` inlines the traces. |
| `RandomX-cx/` | generated. `make` → `librandomx_cx.a`, `cx-tests` (tevador's tests.cpp linked against the hooked library), `cx-dump`. |

## Run it

```bash
cd cx && make && ./cx_selftest 20000 && ./cx_selftest_pure 20000     # ALU vs native
python3 apply_hooks.py && cd ../RandomX-cx && make                   # hooked reference
CX_MODE=off    ./cx-tests                                            # baseline: 105 pass, hooks bypassed
CX_MODE=strict CX_ARGON_EVERY=4096 ./cx-tests                        # every op through crystalline; Argon2 sampled
CX_MODE=strict ./cx-tests                                            # everything, Argon2 fully (hours)
./cx-dump -k "test key 000" -i "This is a test" -v both -o ../traces/1a.jsonl
python3 ../cx/build_page.py -o ../RandomX-formula-lab.html ../traces/*.jsonl
```

Env: `CX_MODE=off|check|strict` (default strict), `CX_ARGON_EVERY=N` (verify every Nth Argon2 block; default 1 = all).

## Results (2026-09-12, Apple M4)

| run | result |
|---|---|
| `cx_selftest 20000` (as-shipped lib) and `cx_selftest_pure 20000` | 462,500 ops each, 0 mismatches |
| AES tables from GF(2^8) on crystalline bits | 0 / 2048 entries differ |
| `CX_MODE=off ./cx-tests` | All 105 tests pass with the hooks compiled in (3 platform-skipped); the suite performs 12.19 billion arithmetic ops |
| `CX_MODE=strict CX_ARGON_EVERY=4096 ./cx-tests` | **All 105 tests pass, 0 mismatches**, 1 h 31 min. Every interpreter hash vector — 1a–1f (v1), 1a–1e (v2), commitment, batch, v1↔v2 switch — was computed on the abacus. In this run only 576 of the 2,359,290 Argon2 blocks were routed through crystalline (the rest ran natively), so ~1.6 billion of the 12.19 billion ops were abacus-computed here. |
| `cx-dump` full run (`CX_ARGON_EVERY=1`), key "test key 000": 1a, 1b, 1c × v1, v2 | all six R = official, 0 mismatches; Argon2: 786,430 / 786,430 fill blocks on the abacus (30 min); per hash 86.6–86.7 M (v1) / 100.8–101.0 M (v2) abacus ops, 271–414 s. Page shows all six with 21/26 steps recomputed in-browser. |

| `cx-dump` full run, key "test key 001": 1d, 1e × v1, v2 | all four R = official, 0 mismatches; per hash 85.8 M (v1) / 100.2 M (v2) abacus ops |
| `cx-dump` full run, 1f (`cx/run_1f.sh`) | R = official (78af2a18…35a8), 0 mismatches, 86.8 M abacus ops. A first attempt hashed a 32-byte key and gave a self-consistent but non-official R with 0 mismatches — the abacus agreed with native, the *input* was wrong: tests.cpp passes `N-1` = 31 bytes of its 32-byte key array (kept as `traces/key1f_wrongkey32.jsonl`). |
| **Final page** | 6 vectors, 11 hashes, all official; 3 Argon2 fills × 786,430 blocks on the abacus; 1,021,488,076 abacus ops inside the hashes; Playwright walk: 21/26 steps recomputed in-browser for every vector/version, 0 disagreements, 0 JS errors. |

Per-hash cost on the abacus is ~86–101 M ops (~5–7 min); the Argon2 cache fill is ~3.5 billion ops (~28–30 min) per key.
Counting note: the trace hook hashes the 2 MiB scratchpad for a fingerprint in bypass mode; that is exactly 22,396,936 hook calls per
hash (16384 Blake2b compressions × 1367 + 8, measured after the counters were split into crystalline/native). Traces written before the
split include it in `ops_this_job`; `build_page.py` subtracts it for those traces.

## What "verified" means here

`cx-tests` in strict mode is tevador's own test suite, but the library it links computes each 64-bit, 32-bit and
double operation on the abacus and USES that result. If any bead were wrong, the hash would differ and the
official vector assertion would fail. The `cx64 report` at exit lists calls and mismatches per op.

Things that are deliberately NOT crystalline (documented in `cx64.c`): bitwise xor/and/or are digit-wise on the
base-2 abacus (the library has no bitwise ops); rotates re-index beads; the harness reads digits by
`weight_exponent` because the library strips zero beads and leaves `max_exponent` stale; division and square root
are long division / digit-by-digit root built from `abacus_compare`/`abacus_sub`/`abacus_shift_*` because
`abacus_div` and `abacus_sqrt` fail above 2^64.

## Findings in crystalline (see `crystalline-math/PATCHES.md`)

1. `compare_magnitude` stopped after the leading digit → `sub`/`div`/`sqrt` cascade (patched, #1).
2. `add`/`sub`/`mul`/`div`/`sqrt` have `uint64_t` fast paths that call the CPU's native arithmetic (`-DCX_PURE_GEOMETRIC` compiles the first three out, #2).
3. `abacus_div` recurses to a stack overflow above 2^64; `abacus_sqrt` errors/zeroes above 2^64.
4. `abacus_convert_base` to base 2 loses low bits; `abacus_mul` on fractional beads is wrong; `shift_right` is fixed-point; zero beads are stripped; `max_exponent` goes stale.
5. `include/math.h` shadows libc's `<math.h>`.
