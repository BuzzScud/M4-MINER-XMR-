#!/usr/bin/env python3
"""
apply_hooks.py — build RandomX-cx/ from the untouched reference RandomX/src.

Every arithmetic site in the reference is rewritten to the cx_* ALU so that the
crystalline abacus computes (and, in CX_STRICT mode, *supplies*) every 64-bit,
32-bit and IEEE-double operation on the way to a RandomX hash:

  Blake2b (input hash, program chain, final hash, Argon2 H', Blake2Generator)
  Argon2d cache fill (fBlaMka, G rounds, block XORs, index_alpha, segment offsets)
  SuperscalarHash (dataset items, reciprocals, mix-block addressing)
  AES (AesGenerator1R/4R, AesHash1R, v2 F/E mixing) — soft path, table lookups
       indexed and combined with cx ops; hardware AES is compiled out
  VM: all 30 opcodes, address masks, scratchpad mixing, dataset XOR, CFROUND

Each replacement asserts its needle count so a silent miss is impossible.
"""
import os, re, shutil, sys

LAB   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC   = os.path.join(LAB, "RandomX", "src")
DST   = os.path.join(LAB, os.environ.get("CX_DST", "RandomX-cx"), "src")   # CX_DST=<dir name> to generate elsewhere
errors = []

def rep(path, old, new, count=1, regex=False, atleast=False):
    p = os.path.join(DST, path)
    s = open(p).read()
    n = len(re.findall(old, s)) if regex else s.count(old)
    ok = (n >= count) if atleast else (n == count)
    if not ok:
        errors.append(f"{path}: expected {count}{'+' if atleast else ''} of {old!r}, found {n}")
        return
    s = re.sub(old, new, s) if regex else s.replace(old, new)
    open(p, "w").write(s)

def insert_after(path, anchor, text):
    p = os.path.join(DST, path); s = open(p).read()
    if s.count(anchor) != 1: errors.append(f"{path}: anchor {anchor!r} count {s.count(anchor)}"); return
    open(p, "w").write(s.replace(anchor, anchor + text))

if os.path.exists(os.path.dirname(DST)): shutil.rmtree(os.path.dirname(DST))
shutil.copytree(SRC, DST)

# --------------------------------------------------------------- bytecode_machine.hpp
f = "bytecode_machine.hpp"
insert_after(f, '#include "program.hpp"\n', '#include "cx_hooks.hpp"\n')
rep(f, "*ibc.idst += (*ibc.isrc << ibc.shift) + ibc.imm;",
       "*ibc.idst = cx_add64(*ibc.idst, cx_add64(cx_shl64(*ibc.isrc, ibc.shift), ibc.imm));")
rep(f, "*ibc.idst += load64(getScratchpadAddress(ibc, scratchpad));",
       "*ibc.idst = cx_add64(*ibc.idst, load64(getScratchpadAddress(ibc, scratchpad)));")
rep(f, "*ibc.idst -= *ibc.isrc;", "*ibc.idst = cx_sub64(*ibc.idst, *ibc.isrc);")
rep(f, "*ibc.idst -= load64(getScratchpadAddress(ibc, scratchpad));",
       "*ibc.idst = cx_sub64(*ibc.idst, load64(getScratchpadAddress(ibc, scratchpad)));")
rep(f, "*ibc.idst *= *ibc.isrc;", "*ibc.idst = cx_mul64(*ibc.idst, *ibc.isrc);")
rep(f, "*ibc.idst *= load64(getScratchpadAddress(ibc, scratchpad));",
       "*ibc.idst = cx_mul64(*ibc.idst, load64(getScratchpadAddress(ibc, scratchpad)));")
rep(f, "*ibc.idst = mulh(*ibc.idst, *ibc.isrc);", "*ibc.idst = cx_mulh64(*ibc.idst, *ibc.isrc);")
rep(f, "*ibc.idst = mulh(*ibc.idst, load64(getScratchpadAddress(ibc, scratchpad)));",
       "*ibc.idst = cx_mulh64(*ibc.idst, load64(getScratchpadAddress(ibc, scratchpad)));")
rep(f, "*ibc.idst = smulh(unsigned64ToSigned2sCompl(*ibc.idst), unsigned64ToSigned2sCompl(*ibc.isrc));",
       "*ibc.idst = cx_smulh64(unsigned64ToSigned2sCompl(*ibc.idst), unsigned64ToSigned2sCompl(*ibc.isrc));")
rep(f, "*ibc.idst = smulh(unsigned64ToSigned2sCompl(*ibc.idst), unsigned64ToSigned2sCompl(load64(getScratchpadAddress(ibc, scratchpad))));",
       "*ibc.idst = cx_smulh64(unsigned64ToSigned2sCompl(*ibc.idst), unsigned64ToSigned2sCompl(load64(getScratchpadAddress(ibc, scratchpad))));")
rep(f, "*ibc.idst = ~(*ibc.idst) + 1; //two's complement negative", "*ibc.idst = cx_neg64(*ibc.idst);")
rep(f, "*ibc.idst ^= *ibc.isrc;", "*ibc.idst = cx_xor64(*ibc.idst, *ibc.isrc);")
rep(f, "*ibc.idst ^= load64(getScratchpadAddress(ibc, scratchpad));",
       "*ibc.idst = cx_xor64(*ibc.idst, load64(getScratchpadAddress(ibc, scratchpad)));")
rep(f, "*ibc.idst = rotr(*ibc.idst, *ibc.isrc & 63);", "*ibc.idst = cx_ror64(*ibc.idst, (unsigned)cx_and64(*ibc.isrc, 63));")
rep(f, "*ibc.idst = rotl(*ibc.idst, *ibc.isrc & 63);", "*ibc.idst = cx_rol64(*ibc.idst, (unsigned)cx_and64(*ibc.isrc, 63));")
rep(f, "*ibc.fdst = rx_add_vec_f128(*ibc.fdst, *ibc.fsrc);", "*ibc.fdst = cxv_fop(*ibc.fdst, *ibc.fsrc, 0);")
rep(f, "*ibc.fdst = rx_add_vec_f128(*ibc.fdst, fsrc);",      "*ibc.fdst = cxv_fop(*ibc.fdst, fsrc, 0);")
rep(f, "*ibc.fdst = rx_sub_vec_f128(*ibc.fdst, *ibc.fsrc);", "*ibc.fdst = cxv_fop(*ibc.fdst, *ibc.fsrc, 1);")
rep(f, "*ibc.fdst = rx_sub_vec_f128(*ibc.fdst, fsrc);",      "*ibc.fdst = cxv_fop(*ibc.fdst, fsrc, 1);")
rep(f, "*ibc.fdst = rx_mul_vec_f128(*ibc.fdst, *ibc.fsrc);", "*ibc.fdst = cxv_fop(*ibc.fdst, *ibc.fsrc, 2);")
rep(f, "*ibc.fdst = rx_div_vec_f128(*ibc.fdst, fsrc);",      "*ibc.fdst = cxv_fop(*ibc.fdst, fsrc, 3);")
rep(f, "*ibc.fdst = rx_sqrt_vec_f128(*ibc.fdst);",           "*ibc.fdst = cxv_fsqrt(*ibc.fdst);")
rep(f, "*ibc.fdst = rx_xor_vec_f128(*ibc.fdst, mask);",      "*ibc.fdst = cxv_bitop(*ibc.fdst, mask, 2);")
rep(f, "rx_cvt_packed_int_vec_f128(getScratchpadAddress(ibc, scratchpad))",
       "cxv_cvt_i32pair(getScratchpadAddress(ibc, scratchpad))", count=3)
rep(f, "*ibc.idst += ibc.imm;\n\t\t\tif ((*ibc.idst & ibc.memMask) == 0) {",
       "*ibc.idst = cx_add64(*ibc.idst, ibc.imm);\n\t\t\tif (cx_and64(*ibc.idst, ibc.memMask) == 0) {")
rep(f, "uint64_t isrc = rotr(*ibc.isrc, ibc.imm);", "uint64_t isrc = cx_ror64(*ibc.isrc, (unsigned)ibc.imm);")
rep(f, "((isrc & 60) == 0)", "(cx_and64(isrc, 60) == 0)")
rep(f, "rx_set_rounding_mode(isrc % 4);", "{ int m_ = (int)cx_and64(isrc, 3); cx_set_rmode(m_); rx_set_rounding_mode(m_); }")
rep(f, "store64(scratchpad + ((*ibc.idst + ibc.imm) & ibc.memMask), *ibc.isrc);",
       "store64(scratchpad + cx_and64(cx_add64(*ibc.idst, ibc.imm), ibc.memMask), *ibc.isrc);")
rep(f, "x = rx_and_vec_f128(x, xmantissaMask);\n\t\t\tx = rx_or_vec_f128(x, xexponentMask);",
       "x = cxv_bitop(x, xmantissaMask, 0);\n\t\t\tx = cxv_bitop(x, xexponentMask, 1);")
rep(f, "uint32_t addr = (*ibc.isrc + ibc.imm) & ibc.memMask;",
       "uint32_t addr = (uint32_t)cx_and64(cx_add64(*ibc.isrc, ibc.imm), ibc.memMask);")

# --------------------------------------------------------------- bytecode_machine.cpp
f = "bytecode_machine.cpp"
rep(f, "ibc.imm = randomx_reciprocal(divisor);", "ibc.imm = cx_rcp64(divisor);")
rep(f, "if (!isZeroOrPowerOf2(divisor)) {", "if (!cx_is_zero_or_pow2(divisor)) {")

# --------------------------------------------------------------- vm_interpreted.cpp
f = "vm_interpreted.cpp"
insert_after(f, '#include "vm_interpreted.hpp"\n', '#include "cx_hooks.hpp"\n')
rep(f, "uint64_t spMix = nreg.r[config.readReg0] ^ nreg.r[config.readReg1];",
       "uint64_t spMix = cx_xor64(nreg.r[config.readReg0], nreg.r[config.readReg1]);")
rep(f, "spAddr0 ^= spMix;\n\t\t\tspAddr0 &= ScratchpadL3Mask64;",
       "spAddr0 = (uint32_t)cx_and64(cx_xor64(spAddr0, spMix), ScratchpadL3Mask64);")
rep(f, "spAddr1 ^= spMix >> 32;\n\t\t\tspAddr1 &= ScratchpadL3Mask64;",
       "spAddr1 = (uint32_t)cx_and64(cx_xor64(spAddr1, cx_shr64(spMix, 32)), ScratchpadL3Mask64);")
rep(f, "nreg.r[i] ^= load64(scratchpad + spAddr0 + 8 * i);",
       "nreg.r[i] = cx_xor64(nreg.r[i], load64(scratchpad + spAddr0 + 8 * i));")
rep(f, "nreg.f[i] = rx_cvt_packed_int_vec_f128(scratchpad + spAddr1 + 8 * i);",
       "nreg.f[i] = cxv_cvt_i32pair(scratchpad + spAddr1 + 8 * i);")
rep(f, "maskRegisterExponentMantissa(config, rx_cvt_packed_int_vec_f128(scratchpad + spAddr1 + 8 * (RegisterCountFlt + i)))",
       "maskRegisterExponentMantissa(config, cxv_cvt_i32pair(scratchpad + spAddr1 + 8 * (RegisterCountFlt + i)))")
rep(f, "const uint64_t readPtr = datasetOffset + (mem.ma & CacheLineAlignMask);",
       "const uint64_t readPtr = cx_add64(datasetOffset, cx_and64(mem.ma, CacheLineAlignMask));")
rep(f, "mp ^= nreg.r[config.readReg2] ^ nreg.r[config.readReg3];",
       "mp = (uint32_t)cx_xor64(mp, cx_xor64(nreg.r[config.readReg2], nreg.r[config.readReg3]));")
rep(f, "nreg.f[i] = rx_xor_vec_f128(nreg.f[i], nreg.e[i]);", "nreg.f[i] = cxv_bitop(nreg.f[i], nreg.e[i], 2);")
rep(f, "r[i] ^= datasetLine[i];", "r[i] = cx_xor64(r[i], datasetLine[i]);")

# --------------------------------------------------------------- vm_interpreted_light.cpp
f = "vm_interpreted_light.cpp"
insert_after(f, '#include "dataset.hpp"\n', '#include "cx64.h"\n')
rep(f, "uint32_t itemNumber = address / CacheLineSize;", "uint32_t itemNumber = (uint32_t)cx_shr64(address, 6); /* CacheLineSize = 64 */")
rep(f, "r[q] ^= rl[q];", "r[q] = cx_xor64(r[q], rl[q]);")

# --------------------------------------------------------------- dataset.cpp
f = "dataset.cpp"
insert_after(f, '#include "superscalar.hpp"\n', '#include "cx64.h"\n#include <vector>\n#include <cstring>\n')
rep(f, "return memory + (registerValue & mask) * CacheLineSize;",
       "return memory + cx_mul64(cx_and64(registerValue, mask), CacheLineSize);")
rep(f, "rl[0] = (itemNumber + 1) * superscalarMul0;", "rl[0] = cx_mul64(cx_add64(itemNumber, 1), superscalarMul0);")
for k in range(1, 8):
    rep(f, f"rl[{k}] = rl[0] ^ superscalarAdd{k};", f"rl[{k}] = cx_xor64(rl[0], superscalarAdd{k});")
rep(f, "rl[q] ^= load64_native(mixBlock + 8 * q);", "rl[q] = cx_xor64(rl[q], load64_native(mixBlock + 8 * q));")
rep(f, "auto rcp = randomx_reciprocal(instr.getImm32());", "auto rcp = cx_rcp64(instr.getImm32());")
# memoize Argon2 cache per key (the reference tests re-init the same key ~20 times)
rep(f, "void initCache(randomx_cache* cache, const void* key, size_t keySize) {",
       "static void initCacheImpl(randomx_cache* cache, const void* key, size_t keySize) {")
MEMO = '''
	void initCache(randomx_cache* cache, const void* key, size_t keySize) {
		/* cx: memoize per key — every crystalline-verified Argon2 fill is done once */
		struct Saved { bool used; std::vector<uint8_t> key; std::vector<uint8_t> mem; SuperscalarProgramList programs; std::vector<uint64_t> rcp; };
		static Saved saved[4];
		for (auto& s : saved)
			if (s.used && s.key.size() == keySize && memcmp(s.key.data(), key, keySize) == 0) {
				memcpy(cache->memory, s.mem.data(), CacheSize);
				cache->programs = s.programs; cache->reciprocalCache = s.rcp;
				return;
			}
		initCacheImpl(cache, key, keySize);
		for (auto& s : saved)
			if (!s.used) {
				s.used = true;
				s.key.assign((const uint8_t*)key, (const uint8_t*)key + keySize);
				s.mem.assign(cache->memory, cache->memory + CacheSize);
				s.programs = cache->programs; s.rcp = cache->reciprocalCache;
				break;
			}
	}

'''
insert_after(f, "\tvoid initCacheCompile(", "")   # existence check only
p = os.path.join(DST, f); s = open(p).read(); s = s.replace("\tvoid initCacheCompile(", MEMO + "\tvoid initCacheCompile(", 1); open(p, "w").write(s)

# --------------------------------------------------------------- superscalar.cpp
f = "superscalar.cpp"
insert_after(f, '#include "superscalar.hpp"\n', '#include "cx64.h"\n')
rep(f, "r[instr.dst] -= r[instr.src];", "r[instr.dst] = cx_sub64(r[instr.dst], r[instr.src]);")
rep(f, "r[instr.dst] ^= r[instr.src];", "r[instr.dst] = cx_xor64(r[instr.dst], r[instr.src]);")
rep(f, "r[instr.dst] += r[instr.src] << instr.getModShift();", "r[instr.dst] = cx_add64(r[instr.dst], cx_shl64(r[instr.src], instr.getModShift()));")
rep(f, "r[instr.dst] *= r[instr.src];", "r[instr.dst] = cx_mul64(r[instr.dst], r[instr.src]);")
rep(f, "r[instr.dst] = rotr(r[instr.dst], instr.getImm32());", "r[instr.dst] = cx_ror64(r[instr.dst], instr.getImm32());")
rep(f, "r[instr.dst] += signExtend2sCompl(instr.getImm32());", "r[instr.dst] = cx_add64(r[instr.dst], signExtend2sCompl(instr.getImm32()));")
rep(f, "r[instr.dst] ^= signExtend2sCompl(instr.getImm32());", "r[instr.dst] = cx_xor64(r[instr.dst], signExtend2sCompl(instr.getImm32()));")
rep(f, "r[instr.dst] = mulh(r[instr.dst], r[instr.src]);", "r[instr.dst] = cx_mulh64(r[instr.dst], r[instr.src]);")
rep(f, "r[instr.dst] = smulh(r[instr.dst], r[instr.src]);", "r[instr.dst] = cx_smulh64(r[instr.dst], r[instr.src]);")
rep(f, "r[instr.dst] *= (*reciprocals)[instr.getImm32()];", "r[instr.dst] = cx_mul64(r[instr.dst], (*reciprocals)[instr.getImm32()]);")
rep(f, "r[instr.dst] *= randomx_reciprocal(instr.getImm32());", "r[instr.dst] = cx_mul64(r[instr.dst], cx_rcp64(instr.getImm32()));")
rep(f, "} while (isZeroOrPowerOf2(imm32_));", "} while (cx_is_zero_or_pow2(imm32_));")

# --------------------------------------------------------------- blake2b.c
f = "blake2/blake2b.c"
insert_after(f, '#include "blake2-impl.h"\n', '#include "cx64.h"\n')
rep(f, r"a = a \+ b \+ (m\[blake2b_sigma\[r\]\[2 \* i \+ [01]\]\]);", r"a = cx_add64(cx_add64(a, b), \1);", count=2, regex=True)
rep(f, r"(\w) = rotr64\((\w) \^ (\w), (\d+)\);", r"\1 = cx_ror64(cx_xor64(\2, \3), \4);", count=4, regex=True)
rep(f, "c = c + d;", "c = cx_add64(c, d);", count=2)
for k, t in ((4, "S->t[0]"), (5, "S->t[1]"), (6, "S->f[0]"), (7, "S->f[1]")):
    rep(f, f"v[{8+k}] = blake2b_IV[{k}] ^ {t};", f"v[{8+k}] = cx_xor64(blake2b_IV[{k}], {t});")
rep(f, "S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];", "S->h[i] = cx_xor64(cx_xor64(S->h[i], v[i]), v[i + 8]);")
rep(f, "S->t[0] += inc;\n\tS->t[1] += (S->t[0] < inc);",
       "S->t[0] = cx_add64(S->t[0], inc);\n\tS->t[1] = cx_add64(S->t[1], (cx_cmp64(S->t[0], inc) < 0));")
rep(f, r"S->h\[i\] \^= load64\(", r"S->h[i] = cx_xor64(S->h[i], load64(", count=1, regex=True, atleast=True)
# the previous regex leaves an unbalanced paren: close it
p = os.path.join(DST, f); s = open(p).read()
s = re.sub(r"S->h\[i\] = cx_xor64\(S->h\[i\], load64\(([^;]*)\);", r"S->h[i] = cx_xor64(S->h[i], load64(\1));", s)
open(p, "w").write(s)

# --------------------------------------------------------------- blamka-round-ref.h / argon2
f = "blake2/blamka-round-ref.h"
insert_after(f, '#include "blake2-impl.h"\n', '#include "cx64.h"\n')
rep(f, "const uint64_t xy = (x & m) * (y & m);\n\treturn x + y + 2 * xy;",
       "const uint64_t xy = cx_mul32x32((uint32_t)cx_and64(x, m), (uint32_t)cx_and64(y, m));\n\treturn cx_add64(cx_add64(x, y), cx_shl64(xy, 1));")
rep(f, r"(\w) = rotr64\((\w) \^ (\w), (\d+)\);", r"\1 = cx_ror64(cx_xor64(\2, \3), \4);", count=4, regex=True)

f = "argon2_core.c"
insert_after(f, '#include "argon2_core.h"\n', '#include "cx64.h"\n')
rep(f, "relative_position = relative_position * relative_position >> 32;",
       "relative_position = cx_shr64(cx_mul64(relative_position, relative_position), 32);")
rep(f, "relative_position = reference_area_size - 1 -\n\t\t(reference_area_size * relative_position >> 32);",
       "relative_position = cx_sub64(cx_sub64(reference_area_size, 1),\n\t\tcx_shr64(cx_mul64(reference_area_size, relative_position), 32));")
rep(f, "absolute_position = (start_position + relative_position) %\n\t\tinstance->lane_length; /* absolute position */",
       "absolute_position = (uint32_t)cx_mod64(cx_add64(start_position, relative_position),\n\t\tinstance->lane_length); /* absolute position */")

f = "argon2_ref.c"
insert_after(f, '#include "argon2_core.h"\n', '#include "cx64.h"\n')
rep(f, "dst->v[i] ^= src->v[i];", "dst->v[i] = cx_xor64(dst->v[i], src->v[i]);")
rep(f, "ref_lane = ((pseudo_rand >> 32)) % instance->lanes;", "ref_lane = cx_mod64(cx_shr64(pseudo_rand, 32), instance->lanes);")
rep(f, "ref_index = randomx_argon2_index_alpha(instance, &position, pseudo_rand & 0xFFFFFFFF,",
       "ref_index = randomx_argon2_index_alpha(instance, &position, (uint32_t)cx_and64(pseudo_rand, 0xFFFFFFFF),")
rep(f, "instance->memory + instance->lane_length * ref_lane + ref_index;",
       "instance->memory + cx_add64(cx_mul64(instance->lane_length, ref_lane), ref_index);")
rep(f, "if (curr_offset % instance->lane_length == 1) {", "if (cx_mod64(curr_offset, instance->lane_length) == 1) {")
rep(f, "if (0 == curr_offset % instance->lane_length) {", "if (0 == cx_mod64(curr_offset, instance->lane_length)) {")
# Argon2 sampling: optional CX_ARGON_EVERY
rep(f, "static void fill_block(const block *prev_block, const block *ref_block,\n\tblock *next_block, int with_xor) {\n\tblock blockR, block_tmp;\n\tunsigned i;\n",
       "static void fill_block(const block *prev_block, const block *ref_block,\n\tblock *next_block, int with_xor) {\n\tblock blockR, block_tmp;\n\tunsigned i;\n\tint cx_saved_mode = cx_get_mode();\n\tif (cx_argon_skip()) cx_set_mode(CX_OFF);\n")
rep(f, "\tcopy_block(next_block, &block_tmp);\n\txor_block(next_block, &blockR);\n}",
       "\tcopy_block(next_block, &block_tmp);\n\txor_block(next_block, &blockR);\n\tcx_set_mode(cx_saved_mode);\n}")

# --------------------------------------------------------------- soft AES
f = "soft_aes.h"
rep(f, "return soft ? soft_aesenc(in, key) : rx_aesenc_vec_i128(in, key);", "return soft_aesenc(in, key); /* cx: hardware AES compiled out */")
rep(f, "return soft ? soft_aesdec(in, key) : rx_aesdec_vec_i128(in, key);", "return soft_aesdec(in, key); /* cx: hardware AES compiled out */")
f = "soft_aes.cpp"
insert_after(f, '#include "soft_aes.h"\n', '#include "cx_hooks.hpp"\n')
for T in ("lutEnc", "lutDec"):
    rep(f, r"\((%s0\[[^\]]*\]) \^ (%s1\[[^\]]*\]) \^ (%s2\[[^\]]*\]) \^ (%s3\[[^\]]*\])\)" % (T, T, T, T),
           r"(int)cx_xor64(cx_xor64(cx_xor64(\1, \2), \3), \4)", count=4, regex=True)
    rep(f, r"%s0\[(\w+) & 0xff\]" % T,            r"%s0[cx_and64(\1, 0xff)]" % T, count=4, regex=True)
    rep(f, r"%s1\[\((\w+) >> 8\) & 0xff\]" % T,   r"%s1[cx_and64(cx_shr64(\1, 8), 0xff)]" % T, count=4, regex=True)
    rep(f, r"%s2\[\((\w+) >> 16\) & 0xff\]" % T,  r"%s2[cx_and64(cx_shr64(\1, 16), 0xff)]" % T, count=4, regex=True)
    rep(f, r"%s3\[(\w+) >> 24\]" % T,             r"%s3[cx_shr64(\1, 24)]" % T, count=4, regex=True)
rep(f, "return rx_xor_vec_i128(out, key);", "return cxv_xor_i128(out, key);", count=2)

# --------------------------------------------------------------- virtual_machine.cpp
f = "virtual_machine.cpp"
insert_after(f, '#include "virtual_machine.hpp"\n', '#include "cx64.h"\n')
rep(f, "mem.ma = program.getEntropy(8) & randomx::CacheLineAlignMask;", "mem.ma = (randomx::addr_t)cx_and64(program.getEntropy(8), randomx::CacheLineAlignMask);")
rep(f, "config.readReg0 = 0 + (addressRegisters & 1);", "config.readReg0 = 0 + (int)cx_and64(addressRegisters, 1);")
rep(f, "config.readReg1 = 2 + (addressRegisters & 1);", "config.readReg1 = 2 + (int)cx_and64(addressRegisters, 1);")
rep(f, "config.readReg2 = 4 + (addressRegisters & 1);", "config.readReg2 = 4 + (int)cx_and64(addressRegisters, 1);")
rep(f, "config.readReg3 = 6 + (addressRegisters & 1);", "config.readReg3 = 6 + (int)cx_and64(addressRegisters, 1);")
rep(f, "addressRegisters >>= 1;", "addressRegisters = cx_shr64(addressRegisters, 1);", count=3)
rep(f, "datasetOffset = (program.getEntropy(13) % (randomx::DatasetExtraItems + 1)) * randomx::CacheLineSize;",
       "datasetOffset = cx_mul64(cx_mod64(program.getEntropy(13), randomx::DatasetExtraItems + 1), randomx::CacheLineSize);")
rep(f, "rx_reset_float_state();", "rx_reset_float_state(); cx_set_rmode(0);", count=1, atleast=True)
# float constant helpers (wherever they live)
for fn in ("virtual_machine.cpp", "common.hpp"):
    p = os.path.join(DST, fn); s = open(p).read()
    if "getSmallPositiveFloatBits(uint64_t entropy)" in s:
        s2 = s
        s2 = s2.replace("auto exponent = entropy >> 59;", "auto exponent = cx_shr64(entropy, 59);")
        s2 = s2.replace("auto mantissa = entropy & mantissaMask;", "auto mantissa = cx_and64(entropy, mantissaMask);")
        s2 = s2.replace("exponent += exponentBias;", "exponent = cx_add64(exponent, exponentBias);")
        s2 = s2.replace("exponent &= exponentMask;", "exponent = cx_and64(exponent, exponentMask);")
        s2 = s2.replace("exponent <<= mantissaSize;", "exponent = cx_shl64(exponent, mantissaSize);")
        s2 = s2.replace("return exponent | mantissa;", "return cx_or64(exponent, mantissa);")
        s2 = s2.replace("exponent |= (entropy >> (64 - staticExponentBits)) << dynamicExponentBits;",
                        "exponent = cx_or64(exponent, cx_shl64(cx_shr64(entropy, 64 - staticExponentBits), dynamicExponentBits));")
        s2 = s2.replace("return (entropy & mask22bit) | getStaticExponent(entropy);",
                        "return cx_or64(cx_and64(entropy, mask22bit), getStaticExponent(entropy));")
        if s2 == s: errors.append(f"{fn}: float helper bodies not matched")
        if fn == "common.hpp" and '#include "cx64.h"' not in s2:
            s2 = s2.replace("#include <cstdint>", "#include <cstdint>\n#include \"cx64.h\"", 1)
        open(p, "w").write(s2)
        break
else:
    errors.append("getSmallPositiveFloatBits not found in virtual_machine.cpp or common.hpp")

# --------------------------------------------------------------- dump instrumentation (lab page vectors)
f = "randomx.cpp"
insert_after(f, '#include "cpu.hpp"\n', '#include "cx64.h"\n#include "cx_dump.h"\n')
rep(f, "\t\tmachine->initScratchpad(&tempHash);\n\t\tmachine->resetRoundingMode();\n\t\tfor (int chain = 0; chain < RANDOMX_PROGRAM_COUNT - 1; ++chain) {\n\t\t\tmachine->run(&tempHash);\n\t\t\tblakeResult = blake2b(tempHash, sizeof(tempHash), machine->getRegisterFile(), sizeof(randomx::RegisterFile), nullptr, 0);\n\t\t\tassert(blakeResult == 0);\n\t\t}\n\t\tmachine->run(&tempHash);\n\t\tmachine->getFinalResult(output, RANDOMX_HASH_SIZE);\n",
       "\t\tif (cx_dump_on()) { cx_dump_begin(\"input_hash\"); cx_dump_hex(\"blake2b512_of_input\", tempHash, 64); cx_dump_end(); }\n"
       "\t\tmachine->initScratchpad(&tempHash);\n"
       "\t\tif (cx_dump_on()) { int sm_ = cx_get_mode(); cx_set_mode(CX_OFF); alignas(16) uint8_t fp_[32]; blake2b(fp_, 32, machine->getScratchpad(), randomx::ScratchpadSize, nullptr, 0); cx_set_mode(sm_);\n"
       "\t\t\tcx_dump_begin(\"scratchpad\"); cx_dump_int(\"bytes\", randomx::ScratchpadSize); cx_dump_hex(\"first64\", machine->getScratchpad(), 64); cx_dump_hex(\"blake2b256_fingerprint\", fp_, 32); cx_dump_end(); }\n"
       "\t\tmachine->resetRoundingMode();\n\t\tfor (int chain = 0; chain < RANDOMX_PROGRAM_COUNT - 1; ++chain) {\n\t\t\tmachine->run(&tempHash);\n"
       "\t\t\tif (cx_dump_on()) { cx_dump_begin(\"program_end\"); cx_dump_int(\"program\", chain); cx_dump_hex(\"register_file\", machine->getRegisterFile(), sizeof(randomx::RegisterFile)); cx_dump_end(); }\n"
       "\t\t\tblakeResult = blake2b(tempHash, sizeof(tempHash), machine->getRegisterFile(), sizeof(randomx::RegisterFile), nullptr, 0);\n\t\t\tassert(blakeResult == 0);\n"
       "\t\t\tif (cx_dump_on()) { cx_dump_begin(\"chain\"); cx_dump_int(\"program\", chain); cx_dump_hex(\"blake2b512_of_register_file\", tempHash, 64); cx_dump_end(); }\n"
       "\t\t}\n\t\tmachine->run(&tempHash);\n"
       "\t\tif (cx_dump_on()) { cx_dump_begin(\"program_end\"); cx_dump_int(\"program\", RANDOMX_PROGRAM_COUNT - 1); cx_dump_hex(\"register_file\", machine->getRegisterFile(), sizeof(randomx::RegisterFile)); cx_dump_end(); }\n"
       "\t\tmachine->getFinalResult(output, RANDOMX_HASH_SIZE);\n"
       "\t\tif (cx_dump_on()) { const randomx::RegisterFile* rf_ = machine->getRegisterFile(); cx_dump_begin(\"final\"); cx_dump_hex(\"A_aeshash1r_of_scratchpad\", &rf_->a, 64); cx_dump_hex(\"register_file_r_f_e_A\", rf_, sizeof(randomx::RegisterFile)); cx_dump_hex(\"R_blake2b256\", output, RANDOMX_HASH_SIZE); cx_dump_end(); }\n")

f = "vm_interpreted.cpp"
insert_after(f, '#include "cx_hooks.hpp"\n', '#include "cx_dump.h"\n')
rep(f, "\t\tcompileProgram(program, bytecode, nreg, randomx_vm::vmFlags);\n",
       "\t\tcompileProgram(program, bytecode, nreg, randomx_vm::vmFlags);\n"
       "\t\tif (cx_dump_on()) { uint64_t ent_[16]; for (int i_ = 0; i_ < 16; ++i_) ent_[i_] = program.getEntropy(i_);\n"
       "\t\t\tcx_dump_begin(\"program\"); cx_dump_u64s(\"entropy\", ent_, 16); cx_dump_vec128s(\"a\", nreg.a, 4);\n"
       "\t\t\tcx_dump_u64(\"ma\", mem.ma); cx_dump_u64(\"mx\", mem.mx); cx_dump_int(\"readReg0\", config.readReg0); cx_dump_int(\"readReg1\", config.readReg1); cx_dump_int(\"readReg2\", config.readReg2); cx_dump_int(\"readReg3\", config.readReg3);\n"
       "\t\t\tcx_dump_u64(\"datasetOffset\", datasetOffset); cx_dump_u64s(\"eMask\", config.eMask, 2); cx_dump_int(\"size\", Program::getSize(randomx_vm::vmFlags)); cx_dump_hex(\"program_bytes_first64\", &program, 64); cx_dump_end(); }\n"
       "\t\tuint64_t cx_rpre_[8];\n")
rep(f, "\t\t\tuint64_t spMix = cx_xor64(nreg.r[config.readReg0], nreg.r[config.readReg1]);\n",
       "\t\t\tuint32_t cx_mx_in_ = mem.mx, cx_ma_in_ = mem.ma;\n"
       "\t\t\tif (cx_dump_want_iter(ic)) { memcpy(cx_rpre_, nreg.r, 64); }\n"
       "\t\t\tuint64_t spMix = cx_xor64(nreg.r[config.readReg0], nreg.r[config.readReg1]);\n")
rep(f, "\t\t\texecuteBytecode(bytecode, scratchpad, config, randomx_vm::getFlags());\n",
       "\t\t\tif (cx_dump_want_iter(ic)) { cx_dump_begin(\"iter\"); cx_dump_set_active(1); cx_dump_int(\"ic\", ic); cx_dump_u64s(\"r_before_step1\", cx_rpre_, 8); cx_dump_u64(\"mx_in\", cx_mx_in_); cx_dump_u64(\"ma_in\", cx_ma_in_); cx_dump_u64(\"spMix\", spMix); cx_dump_u64(\"spAddr0\", spAddr0); cx_dump_u64(\"spAddr1\", spAddr1);\n"
       "\t\t\t\tcx_dump_hex(\"scratchpad_at_spAddr0\", scratchpad + spAddr0, 64); cx_dump_hex(\"scratchpad_at_spAddr1\", scratchpad + spAddr1, 64);\n"
       "\t\t\t\tcx_dump_u64s(\"r_after_step2\", nreg.r, 8); cx_dump_vec128s(\"f_after_step3\", nreg.f, 4); cx_dump_vec128s(\"e_after_step3\", nreg.e, 4); }\n"
       "\t\t\texecuteBytecode(bytecode, scratchpad, config, randomx_vm::getFlags());\n"
       "\t\t\tif (cx_dump_want_iter(ic)) { cx_dump_int(\"ops_after_step4\", (long long)cx_total_calls()); cx_dump_u64s(\"r_after_step4\", nreg.r, 8); cx_dump_vec128s(\"f_after_step4\", nreg.f, 4); cx_dump_vec128s(\"e_after_step4\", nreg.e, 4); }\n")
rep(f, "\t\t\tstd::swap(mem.mx, mem.ma);\n",
       "\t\t\tif (cx_dump_want_iter(ic)) { cx_dump_u64(\"readPtr_step7\", readPtr); cx_dump_u64s(\"r_after_step7\", nreg.r, 8); cx_dump_u64(\"mx_before_swap\", mem.mx); cx_dump_u64(\"ma_before_swap\", mem.ma); }\n"
       "\t\t\tstd::swap(mem.mx, mem.ma);\n")
rep(f, "\t\t\tspAddr0 = 0;\n\t\t\tspAddr1 = 0;\n",
       "\t\t\tif (cx_dump_want_iter(ic)) { cx_dump_vec128s(\"f_after_step10\", nreg.f, 4); cx_dump_set_active(0); cx_dump_end(); }\n"
       "\t\t\tspAddr0 = 0;\n\t\t\tspAddr1 = 0;\n")
insert_after(f, '#include "cx_dump.h"\n', '#include <cstring>\n')
# fast-mode dataset read: record the line while an iter event is open
rep(f, "\t\tuint64_t* datasetLine = (uint64_t*)(mem.memory + address);\n",
       "\t\tuint64_t* datasetLine = (uint64_t*)(mem.memory + address);\n\t\tif (cx_dump_active()) cx_dump_u64s(\"dataset_line_step7\", datasetLine, 8);\n")

f = "vm_interpreted_light.cpp"
insert_after(f, '#include "cx64.h"\n', '#include "cx_dump.h"\n')
rep(f, "\t\tinitDatasetItem(cachePtr, (uint8_t*)rl, itemNumber);\n",
       "\t\tinitDatasetItem(cachePtr, (uint8_t*)rl, itemNumber);\n\t\tif (cx_dump_active()) { cx_dump_u64(\"dataset_item_step7\", itemNumber); cx_dump_u64s(\"dataset_line_step7\", rl, 8); }\n")

f = "dataset.cpp"
insert_after(f, '#include "cx64.h"\n', '#include "cx_dump.h"\n#include "blake2/blake2.h"\n')
rep(f, "\t\tfor (unsigned i = 0; i < RANDOMX_CACHE_ACCESSES; ++i) {\n\t\t\tmixBlock = getMixBlock(registerValue, cache->memory);\n",
       "\t\tint dump_ = cx_dump_want_item();\n"
       "\t\tif (dump_) { cx_dump_begin(\"dataset_item\"); cx_dump_u64(\"item\", itemNumber); cx_dump_u64s(\"rl_init\", rl, 8); }\n"
       "\t\tfor (unsigned i = 0; i < RANDOMX_CACHE_ACCESSES; ++i) {\n\t\t\tmixBlock = getMixBlock(registerValue, cache->memory);\n"
       "\t\t\tif (dump_) { char k_[32]; snprintf(k_, sizeof k_, \"mix_block_offset_%u\", i); cx_dump_u64(k_, (uint64_t)(mixBlock - cache->memory)); }\n")
rep(f, "\t\t\tregisterValue = rl[prog.getAddressRegister()];\n\t\t}\n",
       "\t\t\tregisterValue = rl[prog.getAddressRegister()];\n"
       "\t\t\tif (dump_) { char k_[32]; snprintf(k_, sizeof k_, \"rl_after_prog_%u\", i); cx_dump_u64s(k_, rl, 8); }\n"
       "\t\t}\n\t\tif (dump_) cx_dump_end();\n")
# initial Argon2 blocks (H'(H0 || 0 || 0) and H'(H0 || 1 || 0)) before the three fill passes overwrite them
rep(f, "\t\trandomx_argon2_initialize(&instance, &context);\n\n\t\trandomx_argon2_fill_memory_blocks(&instance);\n",
       "\t\trandomx_argon2_initialize(&instance, &context);\n"
       "\t\tif (cx_dump_on()) { cx_dump_begin(\"argon2_first_blocks\"); cx_dump_hex(\"block0_initial_first64\", cache->memory, 64); cx_dump_hex(\"block1_initial_first64\", cache->memory + 1024, 64); cx_dump_end(); }\n"
       "\n\t\trandomx_argon2_fill_memory_blocks(&instance);\n")
# Argon2 cache summary at the end of initCacheImpl (just before the memoizing wrapper we inserted)
rep(f, MEMO, "\t\t/* cx dump: cache summary */\n" + MEMO)   # anchor check
p = os.path.join(DST, f); s = open(p).read()
s = s.replace("\t\t/* cx dump: cache summary */\n" + MEMO,
   MEMO.replace("\t\tinitCacheImpl(cache, key, keySize);\n",
     "\t\tinitCacheImpl(cache, key, keySize);\n"
     "\t\tif (cx_dump_on()) { int sm_ = cx_get_mode(); cx_set_mode(CX_OFF); alignas(16) uint8_t fp_[32]; blake2b(fp_, 32, cache->memory, CacheSize, nullptr, 0); cx_set_mode(sm_);\n"
     "\t\t\tcx_dump_begin(\"argon2_cache\"); cx_dump_int(\"bytes\", CacheSize); cx_dump_hex(\"block0_first64\", cache->memory, 64); cx_dump_hex(\"blake2b256_fingerprint\", fp_, 32);\n"
     "\t\t\tcx_dump_int(\"blocks_checked\", (long long)cx_argon_blocks_checked()); cx_dump_int(\"blocks_total\", (long long)cx_argon_blocks_total());\n"
     "\t\t\tfor (int i_ = 0; i_ < RANDOMX_CACHE_ACCESSES; ++i_) { char k_[40]; snprintf(k_, sizeof k_, \"superscalar_prog_%d_size\", i_); cx_dump_int(k_, cache->programs[i_].getSize()); }\n"
     "\t\t\tcx_dump_end(); }\n"), 1)
if "#include <cstdio>" not in s: s = s.replace('#include "cx_dump.h"\n', '#include "cx_dump.h"\n#include <cstdio>\n', 1)
open(p, "w").write(s)

# --------------------------------------------------------------- Makefile for RandomX-cx
MK = r'''# RandomX-cx: reference RandomX with every arithmetic site routed through cx64 (crystalline abacus).
CX    = ../cx
CRYS  = ../crystalline-math
CXX   = c++
CC    = cc
# NB: no -I$(CRYS)/include here — crystalline ships its own include/math.h which shadows libc's.
CXXFLAGS = -O2 -std=c++11 -Isrc -I$(CX) -DRANDOMX_CX -Wno-deprecated-declarations
CFLAGS   = -O2 -std=c11 -Isrc -I$(CX) -DRANDOMX_CX

SRCS_CPP = src/aes_hash.cpp src/bytecode_machine.cpp src/cpu.cpp src/dataset.cpp src/soft_aes.cpp \
           src/vm_interpreted.cpp src/allocator.cpp src/assembly_generator_x86.cpp src/instruction.cpp \
           src/randomx.cpp src/superscalar.cpp src/vm_compiled.cpp src/vm_interpreted_light.cpp \
           src/blake2_generator.cpp src/instructions_portable.cpp src/virtual_machine.cpp \
           src/vm_compiled_light.cpp src/jit_compiler_a64.cpp
SRCS_C   = src/argon2_ref.c src/argon2_ssse3.c src/argon2_avx2.c src/virtual_memory.c src/argon2_core.c \
           src/reciprocal.c src/blake2/blake2b.c
SRCS_S   = src/jit_compiler_a64_static.S

OBJS = $(SRCS_CPP:.cpp=.o) $(SRCS_C:.c=.o) $(SRCS_S:.S=.o)

# every object depends on the hook headers (a stale bytecode_machine.o cost an hour once)
$(OBJS): $(CX)/cx_hooks.hpp $(CX)/cx64.h

all: librandomx_cx.a cx-tests

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) -c $< -o $@

librandomx_cx.a: $(OBJS)
	ar rcs $@ $(OBJS)

$(CX)/cx64.o: $(CX)/cx64.c $(CX)/cx64.h
	$(MAKE) -C $(CX) cx64.o

$(CX)/cx_aes.o: $(CX)/cx_aes.c $(CX)/cx64.h
	$(MAKE) -C $(CX) cx_aes.o

$(CX)/cx_dump.o: $(CX)/cx_dump.c $(CX)/cx_dump.h $(CX)/cx64.h
	$(MAKE) -C $(CX) cx_dump.o

CXOBJS = $(CX)/cx64.o $(CX)/cx_aes.o $(CX)/cx_dump.o

all: cx-dump

cx-tests: src/tests/tests.cpp cx_main.cpp librandomx_cx.a $(CXOBJS) $(CRYS)/libcrystalline.a
	$(CXX) $(CXXFLAGS) src/tests/tests.cpp cx_main.cpp librandomx_cx.a $(CXOBJS) $(CRYS)/libcrystalline.a -o cx-tests

cx-dump: $(CX)/cx_dump_main.cpp librandomx_cx.a $(CXOBJS) $(CRYS)/libcrystalline.a
	$(CXX) $(CXXFLAGS) $(CX)/cx_dump_main.cpp librandomx_cx.a $(CXOBJS) $(CRYS)/libcrystalline.a -o cx-dump

cx-tests-pure: src/tests/tests.cpp cx_main.cpp librandomx_cx.a $(CXOBJS) $(CRYS)/libcrystalline_pure.a
	$(CXX) $(CXXFLAGS) src/tests/tests.cpp cx_main.cpp librandomx_cx.a $(CXOBJS) $(CRYS)/libcrystalline_pure.a -o cx-tests-pure

clean:
	rm -f $(OBJS) librandomx_cx.a cx-tests cx-tests-pure
.PHONY: all clean
'''
open(os.path.join(os.path.dirname(DST), "Makefile"), "w").write(MK)

CXMAIN = r'''// cx_main.cpp — process-level setup/report for the hooked test binary.
#include "cx64.h"
#include "cx_aes.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" const uint32_t randomx_aes_lut_enc[4][256];
extern "C" const uint32_t randomx_aes_lut_dec[4][256];
struct CxSetup {
	CxSetup() {
		cx_init();
		const char* m = getenv("CX_MODE");
		cx_set_mode(!m ? CX_STRICT : !strcmp(m, "off") ? CX_OFF : !strcmp(m, "check") ? CX_CHECK : CX_STRICT);
		fprintf(stderr, "cx: mode=%s argon_every=%s\n", m ? m : "strict", getenv("CX_ARGON_EVERY") ? getenv("CX_ARGON_EVERY") : "1");
		uint64_t ops = 0; int bad = cx_aes_verify(randomx_aes_lut_enc, randomx_aes_lut_dec, &ops);
		fprintf(stderr, "cx: AES S-box + 8 T-tables recomputed from GF(2^8) on crystalline bits: %d mismatching entries (%llu ops)\n", bad, (unsigned long long)ops);
	}
	~CxSetup() {
		cx_report();
		fprintf(stderr, "  argon2 blocks verified %llu / %llu\n",
			(unsigned long long)cx_argon_blocks_checked(), (unsigned long long)cx_argon_blocks_total());
		fprintf(stderr, "  TOTAL crystalline-computed ops %llu, mismatches %llu; ops served natively in bypass %llu\n",
			(unsigned long long)cx_total_calls(), (unsigned long long)cx_total_mismatches(), (unsigned long long)cx_total_native());
	}
} cx_setup_instance;
'''
open(os.path.join(os.path.dirname(DST), "cx_main.cpp"), "w").write(CXMAIN)

if errors:
    print("HOOK ERRORS:"); [print("  " + e) for e in errors]; sys.exit(1)
print("hooks applied cleanly ->", os.path.dirname(DST))
