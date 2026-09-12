/**
 * @file ntt.h
 * @brief Number Theoretic Transform (NTT) using Crystalline Abacus
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * NTT is the modular arithmetic analog of FFT, implemented using pure
 * crystalline clock lattice geometry through the Abacus.
 * 
 * Key Advantages:
 * - Works in Z/pZ (modular arithmetic) - no complex numbers
 * - Uses primitive roots of unity in modular arithmetic
 * - Exact arithmetic (no floating point errors)
 * - Perfect fit for crystalline lattice prime-based coordinates
 * - O(n log n) complexity for multiplication
 * - Pure geometric operations on clock lattice
 * 
 * Applications:
 * - Fast large number multiplication: O(n log n) vs O(n²)
 * - Fast polynomial multiplication
 * - Convolution in modular arithmetic
 * - CLLM attention optimization
 * 
 * Algorithm:
 * 1. Choose prime p where p = k·2^n + 1
 * 2. Find primitive 2^n-th root of unity ω in Z/pZ
 * 3. Apply NTT using ω (similar to FFT structure)
 * 4. Multiply pointwise
 * 5. Apply inverse NTT
 */

#ifndef MATH_NTT_H
#define MATH_NTT_H

#include "math/types.h"
#include "math/abacus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * NTT CONTEXT
 * ============================================================================ */

/**
 * @brief NTT Context - stores precomputed values for efficient NTT
 * 
 * All values stored as crystalline abacus positions on clock lattice
 */
typedef struct {
    CrystallineAbacus *prime;              /**< Prime modulus p = k·2^n + 1 */
    CrystallineAbacus *root;               /**< Primitive 2^n-th root of unity */
    CrystallineAbacus **roots_forward;     /**< Precomputed roots for forward NTT */
    CrystallineAbacus **roots_inverse;     /**< Precomputed roots for inverse NTT */
    size_t n;                              /**< Transform size (power of 2) */
    size_t log_n;                          /**< log2(n) */
    bool initialized;                      /**< Initialization flag */
} NTTContext;

/* ============================================================================
 * INITIALIZATION & CLEANUP
 * ============================================================================ */

/**
 * @brief Create and initialize NTT context
 * @param n Transform size (must be power of 2)
 * @return Initialized NTT context, or NULL on failure
 */
NTTContext* ntt_create(size_t n);

/**
 * @brief Free NTT context
 * @param ctx NTT context to free
 */
void ntt_free(NTTContext* ctx);

/**
 * @brief Initialize NTT context with specific prime
 * @param ctx NTT context to initialize
 * @param n Transform size (must be power of 2)
 * @param prime Prime modulus (must be of form k·2^m + 1 where m >= log2(n))
 * @return true if successful, false if failed
 */
bool ntt_init_with_prime(NTTContext* ctx, size_t n, const CrystallineAbacus* prime);

/* ============================================================================
 * PRIMITIVE ROOT FINDING
 * ============================================================================ */

/**
 * @brief Find primitive n-th root of unity modulo prime p
 * 
 * A primitive n-th root of unity ω satisfies:
 * - ω^n ≡ 1 (mod p)
 * - ω^k ≢ 1 (mod p) for 0 < k < n
 * 
 * @param root Output: primitive n-th root of unity
 * @param n Root degree (must divide p-1)
 * @param p Prime modulus
 * @return MATH_SUCCESS if found, error code otherwise
 */
MathError ntt_find_primitive_root(CrystallineAbacus* root, size_t n, const CrystallineAbacus* p);

/**
 * @brief Find suitable NTT prime
 * 
 * Finds prime p of form p = k·2^m + 1 where m >= log2(n)
 * 
 * @param prime Output: suitable prime
 * @param n Minimum transform size needed
 * @param bits Desired bit size of prime
 * @return MATH_SUCCESS if found, error code otherwise
 */
MathError ntt_find_prime(CrystallineAbacus* prime, size_t n, uint32_t bits);

/**
 * @brief Check if g is a primitive root modulo p
 * @param g Candidate primitive root
 * @param p Prime modulus
 * @return true if primitive root, false otherwise
 */
bool ntt_is_primitive_root(const CrystallineAbacus* g, const CrystallineAbacus* p);

/* ============================================================================
 * NTT TRANSFORMS
 * ============================================================================ */

/**
 * @brief Forward Number Theoretic Transform
 * 
 * Transforms sequence a[0..n-1] to frequency domain A[0..n-1]
 * Formula: A[k] = Σ(i=0 to n-1) a[i] · ω^(ik) mod p
 * 
 * Complexity: O(n log n)
 * 
 * @param ctx NTT context (must be initialized)
 * @param output Output: transformed sequence (length n)
 * @param input Input: sequence to transform (length n)
 * @param n Sequence length (must be power of 2)
 * @return MATH_SUCCESS if successful, error code otherwise
 */
MathError ntt_forward(const NTTContext* ctx, CrystallineAbacus** output, 
                      const CrystallineAbacus** input, size_t n);

/**
 * @brief Inverse Number Theoretic Transform
 * 
 * Transforms frequency domain A[0..n-1] back to time domain a[0..n-1]
 * Formula: a[i] = (1/n) · Σ(k=0 to n-1) A[k] · ω^(-ik) mod p
 * 
 * Complexity: O(n log n)
 * 
 * @param ctx NTT context (must be initialized)
 * @param output Output: inverse transformed sequence (length n)
 * @param input Input: frequency domain sequence (length n)
 * @param n Sequence length (must be power of 2)
 * @return MATH_SUCCESS if successful, error code otherwise
 */
MathError ntt_inverse(const NTTContext* ctx, CrystallineAbacus** output,
                      const CrystallineAbacus** input, size_t n);

/* ============================================================================
 * NTT-BASED MULTIPLICATION
 * ============================================================================ */

/**
 * @brief Fast large number multiplication using NTT
 * 
 * Multiplies two numbers using Number Theoretic Transform.
 * 
 * Complexity: O(n log n) where n = len(a) + len(b)
 * 
 * Comparison for n = 10,000 digits:
 * - Naive: O(n²) = 100,000,000 operations
 * - Karatsuba: O(n^1.585) = ~398,000 operations
 * - NTT: O(n log n) = ~130,000 operations (3x faster!)
 * 
 * @param result Output: a × b
 * @param a First number
 * @param b Second number
 * @return MATH_SUCCESS if successful, error code otherwise
 */
MathError ntt_multiply(CrystallineAbacus* result, const CrystallineAbacus* a, 
                       const CrystallineAbacus* b);

/**
 * @brief NTT-based polynomial multiplication
 * 
 * Multiplies two polynomials represented as coefficient arrays.
 * 
 * @param result Output: product polynomial (length n+m-1)
 * @param a First polynomial coefficients (length n)
 * @param n Length of first polynomial
 * @param b Second polynomial coefficients (length m)
 * @param m Length of second polynomial
 * @param ctx NTT context
 * @return MATH_SUCCESS if successful, error code otherwise
 */
MathError ntt_poly_multiply(CrystallineAbacus** result, const CrystallineAbacus** a, size_t n,
                            const CrystallineAbacus** b, size_t m, const NTTContext* ctx);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Check if n is a power of 2
 * @param n Number to check
 * @return true if power of 2, false otherwise
 */
bool ntt_is_power_of_2(size_t n);

/**
 * @brief Round up to next power of 2
 * @param n Input number
 * @return Smallest power of 2 >= n
 */
size_t ntt_next_power_of_2(size_t n);

/**
 * @brief Compute log2 of n (assuming n is power of 2)
 * @param n Power of 2
 * @return log2(n)
 */
uint32_t ntt_log2(size_t n);

/**
 * @brief Bit-reverse permutation
 * 
 * Rearranges array elements according to bit-reversed indices.
 * Required for Cooley-Tukey NTT algorithm.
 * 
 * @param array Array to permute
 * @param n Array length (must be power of 2)
 */
void ntt_bit_reverse(CrystallineAbacus** array, size_t n);

/* ============================================================================
 * CRYSTALLINE LATTICE OPTIMIZATIONS
 * ============================================================================ */

/**
 * @brief Find NTT prime using clock lattice structure
 * 
 * Uses crystalline lattice prime distribution to find suitable primes faster.
 * 
 * @param prime Output: suitable prime
 * @param n Transform size
 * @param bits Desired bit size
 * @return MATH_SUCCESS if found, error code otherwise
 */
MathError ntt_find_prime_lattice(CrystallineAbacus* prime, size_t n, uint32_t bits);

/**
 * @brief Optimize NTT for crystalline lattice structure
 * 
 * Precomputes additional values for lattice-aware NTT.
 * 
 * @param ctx NTT context
 * @return MATH_SUCCESS if successful, error code otherwise
 */
MathError ntt_optimize_for_lattice(NTTContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* MATH_NTT_H */