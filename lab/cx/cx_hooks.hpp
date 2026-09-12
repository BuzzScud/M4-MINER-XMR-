/*
 * cx_hooks.hpp — C++ glue between the reference RandomX vector types and the
 * cx64 scalar ALU. Included by the hooked copies of bytecode_machine.hpp,
 * vm_interpreted.cpp and soft_aes.cpp (see apply_hooks.py).
 *
 * rx_vec_f128 is two doubles (lane 0 = low address), rx_vec_i128 is 16 bytes;
 * both are moved through memcpy so this works for NEON and SSE builds alike.
 */
#pragma once
#include "cx64.h"
#include <cstring>
#include <cstdint>
/* rounding mode comes from the FPU itself (rx_get_rounding_mode), not from a
 * CFROUND-tracked shadow: the reference tests set it directly. */

#include <cstdio>
#include <cstdlib>
static inline int cxv_debug() { static int d = -1; if (d < 0) d = getenv("CX_DEBUG") ? 1 : 0; return d; }
static inline rx_vec_f128 cxv_fop(rx_vec_f128 a, rx_vec_f128 b, int op) {
	double x[2], y[2], r[2];
	memcpy(x, &a, 16); memcpy(y, &b, 16);
	if (cxv_debug()) { uint64_t ux[2], uy[2]; memcpy(ux, x, 16); memcpy(uy, y, 16);
		fprintf(stderr, "[cxv_fop op=%d rm=%u mode=%d] %016llx %016llx | %016llx %016llx\n", op, rx_get_rounding_mode(), cx_get_mode(),
			(unsigned long long)ux[0], (unsigned long long)uy[0], (unsigned long long)ux[1], (unsigned long long)uy[1]); }
	for (int i = 0; i < 2; ++i)
		r[i] = op == 0 ? cx_fadd(x[i], y[i], (int)rx_get_rounding_mode())
		      : op == 1 ? cx_fsub(x[i], y[i], (int)rx_get_rounding_mode())
		      : op == 2 ? cx_fmul(x[i], y[i], (int)rx_get_rounding_mode())
		      :           cx_fdiv(x[i], y[i], (int)rx_get_rounding_mode());
	rx_vec_f128 v; memcpy(&v, r, 16); return v;
}
static inline rx_vec_f128 cxv_fsqrt(rx_vec_f128 a) {
	double x[2], r[2]; memcpy(x, &a, 16);
	r[0] = cx_fsqrt(x[0], (int)rx_get_rounding_mode()); r[1] = cx_fsqrt(x[1], (int)rx_get_rounding_mode());
	rx_vec_f128 v; memcpy(&v, r, 16); return v;
}
/* op: 0 and, 1 or, 2 xor — on the raw 64-bit lane patterns */
static inline rx_vec_f128 cxv_bitop(rx_vec_f128 a, rx_vec_f128 b, int op) {
	uint64_t x[2], y[2], r[2]; memcpy(x, &a, 16); memcpy(y, &b, 16);
	for (int i = 0; i < 2; ++i)
		r[i] = op == 0 ? cx_and64(x[i], y[i]) : op == 1 ? cx_or64(x[i], y[i]) : cx_xor64(x[i], y[i]);
	rx_vec_f128 v; memcpy(&v, r, 16); return v;
}
/* two little-endian int32 at addr -> two doubles (exact) */
static inline rx_vec_f128 cxv_cvt_i32pair(const void* addr) {
	int32_t s[2]; memcpy(s, addr, 8);
	double r[2] = { cx_i32_to_f64(s[0]), cx_i32_to_f64(s[1]) };
	rx_vec_f128 v; memcpy(&v, r, 16); return v;
}
static inline rx_vec_i128 cxv_xor_i128(rx_vec_i128 a, rx_vec_i128 b) {
	uint64_t x[2], y[2], r[2]; memcpy(x, &a, 16); memcpy(y, &b, 16);
	r[0] = cx_xor64(x[0], y[0]); r[1] = cx_xor64(x[1], y[1]);
	rx_vec_i128 v; memcpy(&v, r, 16); return v;
}
