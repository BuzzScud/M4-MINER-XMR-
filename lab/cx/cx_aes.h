/* cx_aes — recompute the AES S-box, inverse S-box and the 4+4 T-tables that
 * RandomX's soft AES uses, from GF(2^8) arithmetic built on cx64 bit ops, and
 * compare with the tables compiled into the reference. Returns the number of
 * mismatching table entries (0 = the 2048 lookup values are the real AES). */
#ifndef CX_AES_H
#define CX_AES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int cx_aes_verify(const uint32_t enc[4][256], const uint32_t dec[4][256], uint64_t *ops_used);
#ifdef __cplusplus
}
#endif
#endif
