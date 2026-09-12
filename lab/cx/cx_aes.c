/*
 * cx_aes.c — AES tables from first principles on the crystalline ALU.
 *
 *   GF(2^8) multiply: shift-and-add with the AES polynomial x^8+x^4+x^3+x+1 (0x11b)
 *   S-box:            inverse (a^254) followed by the affine map  s ^ rotl(s,1..4) ^ 0x63
 *   T-tables (enc):   Te0[x] = { 2*s, s, s, 3*s }  little-endian;  Te_k = rotl32(Te0, 8k)
 *   T-tables (dec):   Td0[x] = { 14*si, 9*si, 13*si, 11*si };        Td_k = rotl32(Td0, 8k)
 *
 * Every xor / and / shift / or here is a cx64 call (crystalline beads).
 */
#include "cx_aes.h"
#include "cx64.h"

static uint64_t gf_mul(uint64_t a, uint64_t b) {
    uint64_t r = 0;
    for (int i = 0; i < 8; i++) {
        if (cx_and64(b, 1)) r = cx_xor64(r, a);
        uint64_t hi = cx_and64(a, 0x80);
        a = cx_and64(cx_shl64(a, 1), 0xff);
        if (hi) a = cx_xor64(a, 0x1b);
        b = cx_shr64(b, 1);
    }
    return r;
}
static uint64_t gf_inv(uint64_t a) {           /* a^254 */
    if (a == 0) return 0;
    uint64_t r = 1, p = a; int e = 254;
    while (e) { if (e & 1) r = gf_mul(r, p); p = gf_mul(p, p); e >>= 1; }
    return r;
}
static uint64_t rotl8(uint64_t x, int n) {
    return cx_and64(cx_or64(cx_shl64(x, n), cx_shr64(x, 8 - n)), 0xff);
}
static uint64_t sbox(uint64_t x) {
    uint64_t s = gf_inv(x);
    uint64_t t = cx_xor64(cx_xor64(cx_xor64(cx_xor64(s, rotl8(s, 1)), rotl8(s, 2)), rotl8(s, 3)), rotl8(s, 4));
    return cx_xor64(t, 0x63);
}
static uint64_t rotl32(uint64_t x, int n) {
    return cx_and64(cx_or64(cx_shl64(x, n), cx_shr64(x, 32 - n)), 0xffffffffULL);
}
static uint64_t word(uint64_t b0, uint64_t b1, uint64_t b2, uint64_t b3) {
    return cx_or64(cx_or64(cx_or64(b0, cx_shl64(b1, 8)), cx_shl64(b2, 16)), cx_shl64(b3, 24));
}

int cx_aes_verify(const uint32_t enc[4][256], const uint32_t dec[4][256], uint64_t *ops_used) {
    uint64_t before = cx_total_calls();
    int bad = 0;
    uint64_t S[256], SI[256];
    for (int x = 0; x < 256; x++) S[x] = sbox((uint64_t)x);
    for (int x = 0; x < 256; x++) SI[S[x]] = (uint64_t)x;
    for (int x = 0; x < 256; x++) {
        uint64_t s = S[x], si = SI[x];
        uint64_t te0 = word(gf_mul(s, 2), s, s, gf_mul(s, 3));
        uint64_t td0 = word(gf_mul(si, 14), gf_mul(si, 9), gf_mul(si, 13), gf_mul(si, 11));
        for (int k = 0; k < 4; k++) {
            if ((uint32_t)rotl32(te0, 8 * k) != enc[k][x]) bad++;
            if ((uint32_t)rotl32(td0, 8 * k) != dec[k][x]) bad++;
        }
    }
    if (ops_used) *ops_used = cx_total_calls() - before;
    return bad;
}
