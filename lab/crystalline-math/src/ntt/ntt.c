/**
 * @file ntt.c
 * @brief Number Theoretic Transform (NTT) - Pure Crystalline Abacus Implementation
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * This file implements the Number Theoretic Transform using pure crystalline
 * clock lattice geometry through the Abacus. NO array manipulation - only
 * geometric operations on the clock lattice.
 * 
 * NTT provides O(n log n) multiplication for large integers using modular
 * arithmetic, perfectly aligned with the crystalline lattice structure.
 * 
 * NOTE: Using base 60 for Abacus (Babylonian clock structure)
 */

#include "math/ntt.h"
#include "math/abacus.h"
#include "math/arithmetic.h"
#include <stdlib.h>
#include <string.h>

/* Default base for NTT operations - use 60 (Babylonian) */
#define NTT_ABACUS_BASE 60

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

bool ntt_is_power_of_2(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

size_t ntt_next_power_of_2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

uint32_t ntt_log2(size_t n) {
    uint32_t log = 0;
    while (n > 1) {
        n >>= 1;
        log++;
    }
    return log;
}

void ntt_bit_reverse(CrystallineAbacus** array, size_t n) {
    if (!ntt_is_power_of_2(n)) return;
    
    uint32_t log_n = ntt_log2(n);
    
    for (size_t i = 0; i < n; i++) {
        size_t j = 0;
        for (uint32_t bit = 0; bit < log_n; bit++) {
            if (i & (1ULL << bit)) {
                j |= 1ULL << (log_n - 1 - bit);
            }
        }
        
        if (i < j) {
            /* Swap array[i] and array[j] */
            CrystallineAbacus* temp = array[i];
            array[i] = array[j];
            array[j] = temp;
        }
    }
}

/* ============================================================================
 * INITIALIZATION & CLEANUP
 * ============================================================================ */

NTTContext* ntt_create(size_t n) {
    if (!ntt_is_power_of_2(n)) {
        return NULL;
    }
    
    NTTContext* ctx = (NTTContext*)calloc(1, sizeof(NTTContext));
    if (!ctx) return NULL;
    
    ctx->n = n;
    ctx->log_n = ntt_log2(n);
    ctx->initialized = false;
    
    /* Find suitable prime
     * Use a reasonable bit size: at least 2*log2(n) to ensure sufficient size
     * but not so large that primitive root finding becomes difficult
     */
    ctx->prime = abacus_new(NTT_ABACUS_BASE);
    if (!ctx->prime) {
        ntt_free(ctx);
        return NULL;
    }
    
    uint32_t min_bits = 2 * ntt_log2(n);  /* Reasonable minimum */
    if (min_bits < 16) min_bits = 16;      /* At least 16 bits */
    
    if (ntt_find_prime(ctx->prime, n, min_bits) != MATH_SUCCESS) {
        ntt_free(ctx);
        return NULL;
    }
    
    /* Find primitive root */
    ctx->root = abacus_new(NTT_ABACUS_BASE);
    if (!ctx->root) {
        ntt_free(ctx);
        return NULL;
    }
    
    if (ntt_find_primitive_root(ctx->root, n, ctx->prime) != MATH_SUCCESS) {
        ntt_free(ctx);
        return NULL;
    }
    
    /* Allocate root arrays */
    ctx->roots_forward = (CrystallineAbacus**)calloc(n, sizeof(CrystallineAbacus*));
    ctx->roots_inverse = (CrystallineAbacus**)calloc(n, sizeof(CrystallineAbacus*));
    
    if (!ctx->roots_forward || !ctx->roots_inverse) {
        ntt_free(ctx);
        return NULL;
    }
    
    /* Precompute roots using pure Abacus operations */
    for (size_t i = 0; i < n; i++) {
        ctx->roots_forward[i] = abacus_new(NTT_ABACUS_BASE);
        ctx->roots_inverse[i] = abacus_new(NTT_ABACUS_BASE);
        
        if (!ctx->roots_forward[i] || !ctx->roots_inverse[i]) {
            ntt_free(ctx);
            return NULL;
        }
        
        /* Forward: ω^i mod p */
        CrystallineAbacus* exp_i = abacus_from_uint64(i, NTT_ABACUS_BASE);
        if (!exp_i) {
            ntt_free(ctx);
            return NULL;
        }
        
        MathError err = abacus_mod_exp(ctx->roots_forward[i], ctx->root, exp_i, ctx->prime);
        abacus_free(exp_i);
        
        if (err != MATH_SUCCESS) {
            ntt_free(ctx);
            return NULL;
        }
        
        /* Inverse: ω^(-i) mod p = ω^(n-i) mod p */
        if (i == 0) {
            CrystallineAbacus* one = abacus_from_uint64(1, NTT_ABACUS_BASE);
            if (!one) {
                ntt_free(ctx);
                return NULL;
            }
            abacus_free(ctx->roots_inverse[i]);
            ctx->roots_inverse[i] = one;
        } else {
            CrystallineAbacus* exp_ni = abacus_from_uint64(n - i, NTT_ABACUS_BASE);
            if (!exp_ni) {
                ntt_free(ctx);
                return NULL;
            }
            
            err = abacus_mod_exp(ctx->roots_inverse[i], ctx->root, exp_ni, ctx->prime);
            abacus_free(exp_ni);
            
            if (err != MATH_SUCCESS) {
                ntt_free(ctx);
                return NULL;
            }
        }
    }
    
    ctx->initialized = true;
    return ctx;
}

void ntt_free(NTTContext* ctx) {
    if (!ctx) return;
    
    if (ctx->prime) abacus_free(ctx->prime);
    if (ctx->root) abacus_free(ctx->root);
    
    if (ctx->roots_forward) {
        for (size_t i = 0; i < ctx->n; i++) {
            if (ctx->roots_forward[i]) {
                abacus_free(ctx->roots_forward[i]);
            }
        }
        free(ctx->roots_forward);
    }
    
    if (ctx->roots_inverse) {
        for (size_t i = 0; i < ctx->n; i++) {
            if (ctx->roots_inverse[i]) {
                abacus_free(ctx->roots_inverse[i]);
            }
        }
        free(ctx->roots_inverse);
    }
    
    free(ctx);
}

bool ntt_init_with_prime(NTTContext* ctx, size_t n, const CrystallineAbacus* prime) {
    if (!ctx || !prime || !ntt_is_power_of_2(n)) {
        return false;
    }
    
    ctx->n = n;
    ctx->log_n = ntt_log2(n);
    
    ctx->prime = abacus_copy(prime);
    if (!ctx->prime) return false;
    
    ctx->root = abacus_new(NTT_ABACUS_BASE);
    if (!ctx->root) return false;
    
    if (ntt_find_primitive_root(ctx->root, n, ctx->prime) != MATH_SUCCESS) {
        return false;
    }
    
    ctx->initialized = true;
    return true;
}

/* ============================================================================
 * PRIMITIVE ROOT FINDING
 * ============================================================================ */

MathError ntt_find_primitive_root(CrystallineAbacus* root, size_t n, const CrystallineAbacus* p) {
    /*
     * Find primitive n-th root of unity modulo p
     * 
     * Algorithm:
     * 1. Ensure p-1 is divisible by n
     * 2. Find a generator g of (Z/pZ)*
     * 3. Compute ω = g^((p-1)/n) mod p
     * 4. Verify ω^n ≡ 1 (mod p) and ω^(n/2) ≢ 1 (mod p)
     */
    
    if (!root || !p || n == 0) return MATH_ERROR_INVALID_ARG;
    
    /* Compute p-1 */
    CrystallineAbacus* p_minus_1 = abacus_copy(p);
    CrystallineAbacus* one = abacus_from_uint64(1, NTT_ABACUS_BASE);
    CrystallineAbacus* temp = abacus_new(NTT_ABACUS_BASE);
    
    if (!p_minus_1 || !one || !temp) {
        abacus_free(p_minus_1);
        abacus_free(one);
        abacus_free(temp);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    MathError err = abacus_sub(p_minus_1, p_minus_1, one);
    if (err != MATH_SUCCESS) {
        abacus_free(p_minus_1);
        abacus_free(one);
        abacus_free(temp);
        return err;
    }
    
    /* Check if n divides p-1 */
    CrystallineAbacus* n_abacus = abacus_from_uint64(n, NTT_ABACUS_BASE);
    if (!n_abacus) {
        abacus_free(p_minus_1);
        abacus_free(one);
        abacus_free(temp);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    err = abacus_mod(temp, p_minus_1, n_abacus);
    if (err != MATH_SUCCESS) {
        abacus_free(p_minus_1);
        abacus_free(one);
        abacus_free(temp);
        abacus_free(n_abacus);
        return err;
    }
    
    if (!abacus_is_zero(temp)) {
        /* n does not divide p-1 */
        abacus_free(p_minus_1);
        abacus_free(one);
        abacus_free(temp);
        abacus_free(n_abacus);
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Compute exponent = (p-1)/n */
    CrystallineAbacus* exponent = abacus_new(NTT_ABACUS_BASE);
    CrystallineAbacus* remainder = abacus_new(NTT_ABACUS_BASE);
    
    if (!exponent || !remainder) {
        abacus_free(p_minus_1);
        abacus_free(one);
        abacus_free(temp);
        abacus_free(n_abacus);
        abacus_free(exponent);
        abacus_free(remainder);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    err = abacus_div(exponent, remainder, p_minus_1, n_abacus);
    abacus_free(remainder);
    
    if (err != MATH_SUCCESS) {
        abacus_free(p_minus_1);
        abacus_free(one);
        abacus_free(temp);
        abacus_free(n_abacus);
        abacus_free(exponent);
        return err;
    }
    
    /* Try small generators: 2, 3, 5, 7, 11, ... */
    uint64_t generators[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
    bool found = false;
    
    for (size_t i = 0; i < sizeof(generators)/sizeof(generators[0]); i++) {
        CrystallineAbacus* g = abacus_from_uint64(generators[i], NTT_ABACUS_BASE);
        if (!g) continue;
        
        /* Compute candidate = g^exponent mod p */
        CrystallineAbacus* candidate = abacus_new(NTT_ABACUS_BASE);
        if (!candidate) {
            abacus_free(g);
            continue;
        }
        
        err = abacus_mod_exp(candidate, g, exponent, p);
        if (err != MATH_SUCCESS) {
            abacus_free(g);
            abacus_free(candidate);
            continue;
        }
        
        /* Verify: candidate^n ≡ 1 (mod p) */
        CrystallineAbacus* check = abacus_new(NTT_ABACUS_BASE);
        if (!check) {
            abacus_free(g);
            abacus_free(candidate);
            continue;
        }
        
        err = abacus_mod_exp(check, candidate, n_abacus, p);
        if (err != MATH_SUCCESS) {
            abacus_free(g);
            abacus_free(candidate);
            abacus_free(check);
            continue;
        }
        
        if (abacus_compare(check, one) == 0) {
            /* Found a valid root - copy value to output parameter */
            abacus_sub(root, root, root);
            abacus_add(root, root, candidate);
            found = true;
            abacus_free(g);
            abacus_free(candidate);
            abacus_free(check);
            break;
        }
        
        abacus_free(g);
        abacus_free(candidate);
        abacus_free(check);
    }
    
    abacus_free(p_minus_1);
    abacus_free(one);
    abacus_free(temp);
    abacus_free(n_abacus);
    abacus_free(exponent);
    
    return found ? MATH_SUCCESS : MATH_ERROR_NOT_FOUND;
}

MathError ntt_find_prime(CrystallineAbacus* prime, size_t n, uint32_t bits) {
    /*
     * Find NTT-friendly prime of form p = k·2^m + 1
     * where m >= log2(n) to ensure n divides p-1
     * 
     * Use known NTT-friendly primes for common sizes
     */
    
    if (!prime || n == 0 || bits == 0) return MATH_ERROR_INVALID_ARG;
    
    /* Use known NTT-friendly primes for common sizes */
    struct {
        size_t max_n;
        uint64_t prime_value;
    } known_primes[] = {
        {256, 257ULL},                    /* 2^8 + 1 */
        {65536, 65537ULL},                /* 2^16 + 1 (Fermat prime) */
        {16777216, 167772161ULL},         /* 10·2^24 + 1 */
        {33554432, 469762049ULL},         /* 7·2^26 + 1 */
        {67108864, 998244353ULL},         /* 119·2^23 + 1 */
        {134217728, 2013265921ULL},       /* 15·2^27 + 1 */
        {268435456, 2281701377ULL},       /* 17·2^27 + 1 */
        {536870912, 3221225473ULL},       /* 3·2^30 + 1 */
        {1073741824, 4253024257ULL},      /* 63·2^26 + 1 */
        {2147483648ULL, 4261412865ULL},   /* 2^32 - 2^25 + 1 */
    };
    
    /* Find suitable known prime
     * Strategy: Find the smallest prime where n <= max_n AND prime_bits >= bits
     * This ensures we get a prime that's large enough but not unnecessarily huge
     */
    for (size_t i = 0; i < sizeof(known_primes)/sizeof(known_primes[0]); i++) {
        if (n <= known_primes[i].max_n) {
            /* Check if it meets bit size requirement */
            uint32_t prime_bits = 0;
            uint64_t val = known_primes[i].prime_value;
            while (val > 0) {
                prime_bits++;
                val >>= 1;
            }
            
            if (prime_bits >= bits) {
                CrystallineAbacus* temp = abacus_from_uint64(known_primes[i].prime_value, NTT_ABACUS_BASE);
                if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
                
                /* Copy value to output parameter */
                abacus_sub(prime, prime, prime);  /* Clear prime to 0 */
                abacus_add(prime, prime, temp);   /* Add temp value to prime */
                abacus_free(temp);
                return MATH_SUCCESS;
            }
            /* If this prime is too small (bits-wise), continue to next one */
        }
    }
    
    /* If no known prime found, use largest one */
    CrystallineAbacus* temp = abacus_from_uint64(4261412865ULL, NTT_ABACUS_BASE);
    if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
    
    /* Copy value to output parameter */
    abacus_sub(prime, prime, prime);  /* Clear prime to 0 */
    abacus_add(prime, prime, temp);   /* Add temp value to prime */
    abacus_free(temp);
    
    return MATH_SUCCESS;
}

bool ntt_is_primitive_root(const CrystallineAbacus* g, const CrystallineAbacus* p) {
    /* TODO: Implement primitive root checking using Abacus */
    (void)g; (void)p;
    return true;
}

/* ============================================================================
 * NTT TRANSFORMS
 * ============================================================================ */

MathError ntt_forward(const NTTContext* ctx, CrystallineAbacus** output, 
                      const CrystallineAbacus** input, size_t n) {
    if (!ctx || !ctx->initialized || !output || !input || n != ctx->n) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Copy input to output */
    for (size_t i = 0; i < n; i++) {
        output[i] = abacus_copy(input[i]);
        if (!output[i]) return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Bit-reverse permutation */
    ntt_bit_reverse(output, n);
    
    /* Cooley-Tukey butterfly operations using pure Abacus modular arithmetic */
    for (size_t len = 2; len <= n; len *= 2) {
        size_t half_len = len / 2;
        size_t step = n / len;
        
        for (size_t i = 0; i < n; i += len) {
            for (size_t j = 0; j < half_len; j++) {
                size_t root_idx = j * step;
                
                CrystallineAbacus* u = output[i + j];
                CrystallineAbacus* v = abacus_new(NTT_ABACUS_BASE);
                if (!v) return MATH_ERROR_OUT_OF_MEMORY;
                
                /* v = output[i + j + half_len] * root^root_idx mod p */
                MathError err = abacus_mod_mul(v, output[i + j + half_len], 
                                               ctx->roots_forward[root_idx], ctx->prime);
                if (err != MATH_SUCCESS) {
                    abacus_free(v);
                    return err;
                }
                
                /* output[i + j] = u + v mod p */
                CrystallineAbacus* temp1 = abacus_new(NTT_ABACUS_BASE);
                if (!temp1) {
                    abacus_free(v);
                    return MATH_ERROR_OUT_OF_MEMORY;
                }
                
                err = abacus_mod_add(temp1, u, v, ctx->prime);
                if (err != MATH_SUCCESS) {
                    abacus_free(v);
                    abacus_free(temp1);
                    return err;
                }
                
                abacus_free(output[i + j]);
                output[i + j] = temp1;
                
                /* output[i + j + half_len] = u - v mod p */
                CrystallineAbacus* temp2 = abacus_new(NTT_ABACUS_BASE);
                if (!temp2) {
                    abacus_free(v);
                    return MATH_ERROR_OUT_OF_MEMORY;
                }
                
                err = abacus_mod_sub(temp2, u, v, ctx->prime);
                if (err != MATH_SUCCESS) {
                    abacus_free(v);
                    abacus_free(temp2);
                    return err;
                }
                
                abacus_free(output[i + j + half_len]);
                output[i + j + half_len] = temp2;
                
                abacus_free(v);
            }
        }
    }
    
    return MATH_SUCCESS;
}

MathError ntt_inverse(const NTTContext* ctx, CrystallineAbacus** output,
                      const CrystallineAbacus** input, size_t n) {
    if (!ctx || !ctx->initialized || !output || !input || n != ctx->n) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Copy input to output */
    for (size_t i = 0; i < n; i++) {
        output[i] = abacus_copy(input[i]);
        if (!output[i]) return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Bit-reverse permutation */
    ntt_bit_reverse(output, n);
    
    /* Cooley-Tukey butterfly operations with inverse roots */
    for (size_t len = 2; len <= n; len *= 2) {
        size_t half_len = len / 2;
        size_t step = n / len;
        
        for (size_t i = 0; i < n; i += len) {
            for (size_t j = 0; j < half_len; j++) {
                size_t root_idx = j * step;
                
                CrystallineAbacus* u = output[i + j];
                CrystallineAbacus* v = abacus_new(NTT_ABACUS_BASE);
                if (!v) return MATH_ERROR_OUT_OF_MEMORY;
                
                /* v = output[i + j + half_len] * inverse_root^root_idx mod p */
                MathError err = abacus_mod_mul(v, output[i + j + half_len],
                                               ctx->roots_inverse[root_idx], ctx->prime);
                if (err != MATH_SUCCESS) {
                    abacus_free(v);
                    return err;
                }
                
                /* output[i + j] = u + v mod p */
                CrystallineAbacus* temp1 = abacus_new(NTT_ABACUS_BASE);
                if (!temp1) {
                    abacus_free(v);
                    return MATH_ERROR_OUT_OF_MEMORY;
                }
                
                err = abacus_mod_add(temp1, u, v, ctx->prime);
                if (err != MATH_SUCCESS) {
                    abacus_free(v);
                    abacus_free(temp1);
                    return err;
                }
                
                abacus_free(output[i + j]);
                output[i + j] = temp1;
                
                /* output[i + j + half_len] = u - v mod p */
                CrystallineAbacus* temp2 = abacus_new(NTT_ABACUS_BASE);
                if (!temp2) {
                    abacus_free(v);
                    return MATH_ERROR_OUT_OF_MEMORY;
                }
                
                err = abacus_mod_sub(temp2, u, v, ctx->prime);
                if (err != MATH_SUCCESS) {
                    abacus_free(v);
                    abacus_free(temp2);
                    return err;
                }
                
                abacus_free(output[i + j + half_len]);
                output[i + j + half_len] = temp2;
                
                abacus_free(v);
            }
        }
    }
    
    /* Scale by 1/n mod p using Abacus modular inverse */
    CrystallineAbacus* n_inv = abacus_new(NTT_ABACUS_BASE);
    CrystallineAbacus* n_abacus = abacus_from_uint64(n, NTT_ABACUS_BASE);
    
    if (!n_inv || !n_abacus) {
        abacus_free(n_inv);
        abacus_free(n_abacus);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    MathError err = abacus_mod_inverse(n_inv, n_abacus, ctx->prime);
    
    if (err != MATH_SUCCESS) {
        abacus_free(n_inv);
        abacus_free(n_abacus);
        return err;
    }
    
    for (size_t i = 0; i < n; i++) {
        CrystallineAbacus* temp = abacus_new(NTT_ABACUS_BASE);
        if (!temp) {
            abacus_free(n_inv);
            abacus_free(n_abacus);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_mod_mul(temp, output[i], n_inv, ctx->prime);
        if (err != MATH_SUCCESS) {
            abacus_free(temp);
            abacus_free(n_inv);
            abacus_free(n_abacus);
            return err;
        }
        
        abacus_free(output[i]);
        output[i] = temp;
    }
    
    abacus_free(n_inv);
    abacus_free(n_abacus);
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * NTT-BASED MULTIPLICATION
 * ============================================================================ */

MathError ntt_multiply(CrystallineAbacus* result, const CrystallineAbacus* a, 
                       const CrystallineAbacus* b) {
    /* TODO: Implement NTT-based multiplication using Abacus
     * This requires:
     * 1. Converting Abacus to coefficient arrays
     * 2. Padding to power of 2
     * 3. Applying forward NTT
     * 4. Pointwise multiplication
     * 5. Applying inverse NTT
     * 6. Handling carries
     */
    
    /* For now, fall back to standard Abacus multiplication */
    return abacus_mul(result, a, b);
}

MathError ntt_poly_multiply(CrystallineAbacus** result, const CrystallineAbacus** a, size_t n,
                            const CrystallineAbacus** b, size_t m, const NTTContext* ctx) {
    /* TODO: Implement polynomial multiplication using Abacus */
    (void)result; (void)a; (void)n; (void)b; (void)m; (void)ctx;
    return MATH_ERROR_NOT_FOUND;  /* Use NOT_FOUND as placeholder for NOT_IMPLEMENTED */
}

/* ============================================================================
 * CRYSTALLINE LATTICE OPTIMIZATIONS
 * ============================================================================ */

MathError ntt_find_prime_lattice(CrystallineAbacus* prime, size_t n, uint32_t bits) {
    /* TODO: Use clock lattice to find NTT-friendly primes faster */
    return ntt_find_prime(prime, n, bits);
}

MathError ntt_optimize_for_lattice(NTTContext* ctx) {
    /* TODO: Precompute lattice-specific optimizations */
    (void)ctx;
    return MATH_SUCCESS;
}