/**
 * @file abacus.h
 * @brief Crystalline Abacus for geometric arithmetic operations
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * The Crystalline Abacus represents numbers geometrically on the clock lattice
 * and performs arithmetic operations using geometric transformations.
 * 
 * Revolutionary Approach:
 * - Numbers are represented as positions on the Babylonian clock
 * - Arithmetic operations become geometric transformations
 * - Leverages the crystalline lattice structure
 * - Self-contained geometric arithmetic system (no BigInt dependency)
 * 
 * Babylonian Mathematics - Universal Base Support:
 * - Supports ALL bases >= 2 (binary, octal, decimal, hexadecimal, etc.)
 * - Traditional Babylonian bases: 12, 60, 100 (clock rings)
 * - Base conversion without fractions (pure geometric transformation)
 * - Fractional support through negative weight exponents
 * 
 * Structure:
 * - Each "bead" is a position on the clock lattice
 * - Multiple beads represent multi-digit numbers
 * - Any base >= 2 supported (not limited to 12, 60, 100)
 * 
 * Example:
 *   Number 157 in base 12:
 *   157 = 13*12 + 1 = [13, 1] in base 12
 *   Represented as 2 beads at positions corresponding to 13 and 1
 */

#ifndef MATH_ABACUS_H
#define MATH_ABACUS_H

#include "math/types.h"
#include "math/clock.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ABACUS STRUCTURE
 * ============================================================================
 */

/**
 * @brief A bead on the crystalline abacus (Dense representation)
 * 
 * Each bead represents a digit in the number, positioned on the clock lattice.
 * Supports both integer and fractional positions through weight_exponent.
 * 
 * Examples:
 *   weight_exponent = 2  → weight = base^2  (hundreds place)
 *   weight_exponent = 1  → weight = base^1  (tens place)
 *   weight_exponent = 0  → weight = base^0  (ones place)
 *   weight_exponent = -1 → weight = base^-1 (tenths place)
 *   weight_exponent = -2 → weight = base^-2 (hundredths place)
 */
typedef struct {
    ClockPosition position;  /**< Position on clock lattice */
    uint32_t value;          /**< Digit value (0 to base-1) */
    int32_t weight_exponent; /**< Weight exponent (can be negative for fractions) */
} AbacusBead;

/**
 * @brief A sparse bead (Memory-optimized representation)
 * 
 * Stores only non-zero digits with their positions.
 * Uses 8 bytes instead of 32 bytes per bead (75% memory reduction).
 * 
 * Ideal for sparse numbers (many zeros):
 *   - Large numbers: 1,000,000,000 (only 1 non-zero digit)
 *   - Scientific notation: 1.23 × 10^100 (only 3 non-zero digits)
 *   - Small fractions: 0.000000001 (only 1 non-zero digit)
 */
typedef struct {
    uint32_t value;          /**< Digit value (0 to base-1) */
    int32_t weight_exponent; /**< Position in number */
} SparseBead;



/**
 * @brief Crystalline Abacus structure (Hybrid Dense/Sparse representation)
 * 
 * Represents a number as a collection of beads on the clock lattice.
 * Supports both integer and fractional numbers through weight exponents.
 * 
 * MEMORY OPTIMIZATION:
 * - Dense mode: Stores all beads (including zeros) - 32 bytes per bead
 * - Sparse mode: Stores only non-zero beads - 8 bytes per bead
 * - Automatic switching based on sparsity (>50% zeros → sparse mode)
 * - Memory savings: 75-98% for sparse numbers
 * 
 * Dense Example: 157.25 in base 12
 *   beads[0]: value=3, weight_exponent=-1  (3 * 12^-1 = 0.25)
 *   beads[1]: value=1, weight_exponent=0   (1 * 12^0 = 1)
 *   beads[2]: value=13, weight_exponent=1  (13 * 12^1 = 156)
 *   Total: 156 + 1 + 0.25 = 157.25
 * 
 * Sparse Example: 1,000,000,000 in base 10
 *   sparse_beads[0]: value=1, weight_exponent=9  (1 * 10^9)
 *   Total: 1,000,000,000
 *   Memory: 8 bytes instead of 320 bytes (96% reduction)
 */
typedef struct {
    // Dense representation (used when most digits are non-zero)
    AbacusBead* beads;       /**< Array of beads (ordered by weight_exponent) */
    size_t num_beads;        /**< Number of beads */
    size_t capacity;         /**< Allocated capacity */
    
    // Sparse representation (used when many digits are zero)
    SparseBead* sparse_beads; /**< Array of non-zero beads only */
    size_t num_nonzero;       /**< Number of non-zero beads */
    size_t sparse_capacity;   /**< Allocated sparse capacity */
    
    // Representation mode
    bool is_sparse;          /**< true = sparse mode, false = dense mode */
    
    // Common fields
    uint32_t base;           /**< Number base (any base >= 2) */
    bool negative;           /**< Sign of the number */
    int32_t min_exponent;    /**< Minimum weight exponent (for fractional precision) */
    int32_t max_exponent;    /**< Maximum weight exponent (highest position) */
} CrystallineAbacus;

/* ============================================================================
 * INITIALIZATION AND CLEANUP
 * ============================================================================
 */

/**
 * @brief Create a new crystalline abacus
 * @param base Number base (any base >= 2)
 *             Traditional Babylonian: 12, 60, 100
 *             Common: 2 (binary), 8 (octal), 10 (decimal), 16 (hex)
 *             Any positive integer >= 2 is supported
 * @return Pointer to new abacus, or NULL on error
 */
CrystallineAbacus* abacus_new(uint32_t base);

/**
 * @brief Free an abacus
 * @param abacus Abacus to free
 */
void abacus_free(CrystallineAbacus* abacus);

/**
 * @brief Initialize abacus to zero
 * @param abacus Abacus to initialize
 * @return MATH_SUCCESS or error code
 */
MathError abacus_init_zero(CrystallineAbacus* abacus);

/* ============================================================================
 * CONVERSION
 * ============================================================================
 */

/**
 * @brief Create abacus from uint64
 * @param value Value to convert
 * @param base Number base (any base >= 2)
 * @return Pointer to new abacus, or NULL on error
 */
CrystallineAbacus* abacus_from_uint64(uint64_t value, uint32_t base);

/**
 * @brief Convert abacus to uint64
 * @param abacus Abacus to convert
 * @param value Output value
 * @return MATH_SUCCESS or error code
 */
MathError abacus_to_uint64(const CrystallineAbacus* abacus, uint64_t* value);

/**
 * @brief Create abacus from double (with fractional part)
 * @param value Value to convert
 * @param base Number base (any base >= 2)
 * @param precision Number of fractional digits (negative exponents)
 * @return Pointer to new abacus, or NULL on error
 * 
 * Example: abacus_from_double(157.25, 12, 2)
 *   Creates abacus with beads for 157.25 in base 12
 *   precision=2 means 2 fractional digits (12^-1, 12^-2)
 */
CrystallineAbacus* abacus_from_double(double value, uint32_t base, int32_t precision);

/**
 * @brief Convert abacus to double
 * @param abacus Abacus to convert
 * @param value Output value
 * @return MATH_SUCCESS or error code
 */
MathError abacus_to_double(const CrystallineAbacus* abacus, double* value);

/**
 * @brief Set precision (number of fractional digits)
 * @param abacus Abacus to modify
 * @param precision Number of fractional digits (negative exponents)
 * @return MATH_SUCCESS or error code
 * 
 * Adjusts min_exponent to support the specified precision.
 * Pads with zeros if needed, truncates if reducing precision.
 */
MathError abacus_set_precision(CrystallineAbacus* abacus, int32_t precision);

/**
 * @brief Get precision (number of fractional digits)
 * @param abacus Abacus to query
 * @return Number of fractional digits (0 if integer only)
 */
int32_t abacus_get_precision(const CrystallineAbacus* abacus);

/**
 * @brief Round abacus to specified precision
 * @param result Output abacus
 * @param a Input abacus
 * @param precision Number of fractional digits to keep
 * @return MATH_SUCCESS or error code
 */
MathError abacus_round(CrystallineAbacus* result, const CrystallineAbacus* a, int32_t precision);

/**
 * @brief Truncate abacus to specified precision
 * @param result Output abacus
 * @param a Input abacus
 * @param precision Number of fractional digits to keep
 * @return MATH_SUCCESS or error code
 */
MathError abacus_truncate(CrystallineAbacus* result, const CrystallineAbacus* a, int32_t precision);

/* ============================================================================
 * GEOMETRIC ARITHMETIC OPERATIONS
 * ============================================================================
 */

/**
 * @brief Add two abacuses (geometric addition)
 * @param result Output abacus (must be pre-allocated)
 * @param a First abacus
 * @param b Second abacus
 * @return MATH_SUCCESS or error code
 * 
 * Performs addition using geometric transformations on the clock lattice.
 */
MathError abacus_add(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Subtract two abacuses (geometric subtraction)
 * @param result Output abacus (must be pre-allocated)
 * @param a First abacus
 * @param b Second abacus
 * @return MATH_SUCCESS or error code
 */
MathError abacus_sub(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Multiply two abacuses (geometric multiplication)
 * @param result Output abacus (must be pre-allocated)
 * @param a First abacus
 * @param b Second abacus
 * @return MATH_SUCCESS or error code
 */
MathError abacus_mul(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Divide two abacuses (geometric division)
 * @param quotient Output quotient (must be pre-allocated)
 * @param remainder Output remainder (must be pre-allocated, can be NULL)
 * @param a Dividend
 * @param b Divisor
 * @return MATH_SUCCESS or error code
 */
MathError abacus_div(CrystallineAbacus* quotient, CrystallineAbacus* remainder,
                     const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Shift abacus left (multiply by base^n)
 * @param result Output abacus (must be pre-allocated)
 * @param a Input abacus
 * @param n Number of positions to shift
 * @return MATH_SUCCESS or error code
 * 
 * Geometric operation: rotates positions on clock lattice.
 */
MathError abacus_shift_left(CrystallineAbacus* result, const CrystallineAbacus* a, size_t n);

/**
 * @brief Shift abacus right (divide by base^n)
 * @param result Output abacus (must be pre-allocated)
 * @param a Input abacus
 * @param n Number of positions to shift
 * @return MATH_SUCCESS or error code
 */
MathError abacus_shift_right(CrystallineAbacus* result, const CrystallineAbacus* a, size_t n);

/* ============================================================================
 * COMPARISON
 * ============================================================================
 */

/**
 * @brief Compare two abacuses
 * @param a First abacus
 * @param b Second abacus
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int abacus_compare(const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Check if abacus is zero
 * @param abacus Abacus to check
 * @return true if zero
 */
bool abacus_is_zero(const CrystallineAbacus* abacus);

/**
 * @brief Check if abacus is negative
 * @param abacus Abacus to check
 * @return true if negative
 */
bool abacus_is_negative(const CrystallineAbacus* abacus);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Copy an abacus
 * @param src Source abacus
 * @return Pointer to new abacus, or NULL on error
 */
CrystallineAbacus* abacus_copy(const CrystallineAbacus* src);

/**
 * @brief Normalize abacus (remove leading zeros, handle carries)
 * @param abacus Abacus to normalize
 * @return MATH_SUCCESS or error code
 */
MathError abacus_normalize(CrystallineAbacus* abacus);

/**
 * @brief Get string representation of abacus
 * @param abacus Abacus to convert
 * @return String representation (caller must free), or NULL on error
 */
char* abacus_to_string(const CrystallineAbacus* abacus);

/**
 * @brief Print abacus for debugging
 * @param abacus Abacus to print
 */
void abacus_print(const CrystallineAbacus* abacus);

/* ============================================================================
 * MODULAR ARITHMETIC OPERATIONS
 * ============================================================================ */

/**
 * @brief Modular reduction (a mod m)
 * @param result Output abacus (must be pre-allocated)
 * @param a Input abacus
 * @param modulus Modulus
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_mod(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* modulus);

/**
 * @brief Modular addition ((a + b) mod m)
 * @param result Output abacus (must be pre-allocated)
 * @param a First abacus
 * @param b Second abacus
 * @param modulus Modulus
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_mod_add(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b, const CrystallineAbacus* modulus);

/**
 * @brief Modular subtraction ((a - b) mod m)
 * @param result Output abacus (must be pre-allocated)
 * @param a First abacus
 * @param b Second abacus
 * @param modulus Modulus
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_mod_sub(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b, const CrystallineAbacus* modulus);

/**
 * @brief Modular multiplication ((a * b) mod m)
 * @param result Output abacus (must be pre-allocated)
 * @param a First abacus
 * @param b Second abacus
 * @param modulus Modulus
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_mod_mul(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b, const CrystallineAbacus* modulus);

/**
 * @brief Modular exponentiation ((base ^ exponent) mod m)
 * Uses binary exponentiation for efficiency
 * @param result Output abacus (must be pre-allocated)
 * @param base Base
 * @param exponent Exponent
 * @param modulus Modulus
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_mod_exp(CrystallineAbacus* result, const CrystallineAbacus* base, const CrystallineAbacus* exponent, const CrystallineAbacus* modulus);

/**
 * @brief Modular multiplicative inverse (a^-1 mod m)
 * Uses extended Euclidean algorithm
 * @param result Output abacus (must be pre-allocated)
 * @param a Input abacus
 * @param modulus Modulus
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_mod_inverse(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* modulus);

/* ============================================================================
 * GCD, LCM, AND COPRIMALITY OPERATIONS
 * ============================================================================
 */

/**
 * @brief Compute GCD of two arbitrary precision numbers
 * 
 * Uses the Euclidean algorithm to compute the greatest common divisor.
 * This is a pure geometric operation on the CrystallineAbacus.
 * 
 * @param result Output: GCD of a and b (always positive)
 * @param a First number
 * @param b Second number
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_gcd(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Compute LCM of two arbitrary precision numbers
 * 
 * Uses the formula: lcm(a, b) = (a * b) / gcd(a, b)
 * 
 * @param result Output: LCM of a and b (always positive)
 * @param a First number
 * @param b Second number
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_lcm(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Test if two arbitrary precision numbers are coprime
 * 
 * Two numbers are coprime if gcd(a, b) = 1
 * 
 * @param a First number
 * @param b Second number
 * @return true if coprime, false otherwise
 */
bool abacus_coprime(const CrystallineAbacus* a, const CrystallineAbacus* b);

/**
 * @brief Integer square root using Newton-Raphson method
 * 
 * Finds the largest integer x such that x² ≤ n
 * 
 * Mathematical Foundation:
 * Uses Newton-Raphson iteration: x_new = (x + n/x) / 2
 * Converges quadratically to √n
 * 
 * Geometric Interpretation:
 * Finding the side length of a square with area n
 * 
 * @param result Output: floor(√n)
 * @param n Input number (must be non-negative)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_sqrt(CrystallineAbacus* result, const CrystallineAbacus* n);

/**
 * @brief Integer nth root using Newton-Raphson method
 * 
 * Finds the largest integer x such that x^root ≤ n
 * 
 * Mathematical Foundation:
 * Uses Newton-Raphson iteration: x_new = ((root-1)*x + n/x^(root-1)) / root
 * Converges to the nth root of n
 * 
 * Geometric Interpretation:
 * Finding the edge length of an n-dimensional hypercube with volume n
 * 
 * @param result Output: floor(root√n)
 * @param n Input number
 * @param root The root to extract (2 for square root, 3 for cube root, etc.)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_root(CrystallineAbacus* result, const CrystallineAbacus* n, uint32_t root);

/**
 * @brief Compute base^exponent using binary exponentiation
 * 
 * Uses exponentiation by squaring for O(log n) complexity.
 * Each multiplication is O(1) for numbers <= 2^64.
 * 
 * Algorithm:
 * - Convert exponent to binary representation
 * - For each bit, square the current result
 * - If bit is 1, multiply by base
 * 
 * Example: 3^13 = 3^8 × 3^4 × 3^1 (binary: 1101)
 * 
 * Complexity: O(log n) operations, each O(1) for small numbers
 * 
 * @param result Output: base^exponent
 * @param base Base number
 * @param exponent Exponent (must be non-negative)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_pow(CrystallineAbacus* result,
                     const CrystallineAbacus* base,
                     const CrystallineAbacus* exponent);

/**
 * @brief Fast path for small exponents (uint64_t)
 * 
 * More efficient when exponent fits in uint64_t.
 * 
 * @param result Output: base^exponent
 * @param base Base number
 * @param exponent Exponent as uint64_t
 * @return MATH_SUCCESS or error code
 */
MathError abacus_pow_uint64(CrystallineAbacus* result,
                            const CrystallineAbacus* base,
                            uint64_t exponent);

/**
 * @brief Compute (base^exponent) mod modulus
 * 
 * Uses binary exponentiation with modulo at each step.
 * Critical for cryptography (RSA, Diffie-Hellman).
 * 
 * Key insight: (a × b) mod m = ((a mod m) × (b mod m)) mod m
 * This keeps intermediate results small (always < modulus).
 * 
 * Complexity: O(log n) operations, each O(1) for small numbers
 * 
 * @param result Output: (base^exponent) mod modulus
 * @param base Base number
 * @param exponent Exponent (must be non-negative)
 * @param modulus Modulus (must be > 0)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_powmod(CrystallineAbacus* result,
                       const CrystallineAbacus* base,
                       const CrystallineAbacus* exponent,
                       const CrystallineAbacus* modulus);

/* ============================================================================
 * BASE CONVERSION
 * ============================================================================ */

/**
 * @brief Convert abacus to a different base
 * 
 * Babylonian mathematics supports conversion between ANY bases without
 * requiring fractions. This is a pure geometric transformation on the
 * clock lattice.
 * 
 * @param result Output abacus in new base (will be created)
 * @param source Input abacus in original base
 * @param new_base Target base (any base >= 2)
 * @return MATH_SUCCESS on success, error code on failure
 * 
 * Example:
 *   Convert 157 from base 12 to base 10:
 *   Input:  [13, 1] in base 12 = 13*12 + 1 = 157
 *   Output: [1, 5, 7] in base 10 = 1*100 + 5*10 + 7 = 157
 */
MathError abacus_convert_base(CrystallineAbacus** result, const CrystallineAbacus* source, uint32_t new_base);

/**
 * @brief Get the base of an abacus
 * @param abacus Input abacus
 * @return Base of the abacus, or 0 on error
 */
uint32_t abacus_get_base(const CrystallineAbacus* abacus);

/* ============================================================================
 * SPARSE REPRESENTATION (Memory Optimization)
 * ============================================================================
 */

/**
 * @brief Convert dense representation to sparse representation
 * 
 * Stores only non-zero beads, reducing memory usage by 75-98%.
 * Ideal for numbers with many zeros (sparse numbers).
 * 
 * Memory savings:
 * - Dense: 32 bytes per bead (includes ClockPosition)
 * - Sparse: 8 bytes per non-zero bead
 * - Example: 1,000,000,000 → 320 bytes → 8 bytes (97.5% reduction)
 * 
 * @param abacus Abacus to convert (modified in place)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_sparsify(CrystallineAbacus* abacus);

/**
 * @brief Convert sparse representation to dense representation
 * 
 * Expands sparse beads to full dense representation.
 * Used when dense operations are more efficient.
 * 
 * @param abacus Abacus to convert (modified in place)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_densify(CrystallineAbacus* abacus);

/**
 * @brief Automatically optimize representation based on sparsity
 * 
 * Switches between sparse and dense modes based on:
 * - Sparsity threshold (>50% zeros → sparse)
 * - Number size (>100 beads → sparse)
 * - Operation efficiency
 * 
 * This is called automatically after operations to maintain
 * optimal memory usage and performance.
 * 
 * @param abacus Abacus to optimize (modified in place)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_optimize_representation(CrystallineAbacus* abacus);

/**
 * @brief Get sparsity ratio (percentage of zero beads)
 * 
 * Returns value between 0.0 (all non-zero) and 1.0 (all zeros).
 * Used to determine if sparse representation is beneficial.
 * 
 * @param abacus Input abacus
 * @return Sparsity ratio (0.0 to 1.0), or -1.0 on error
 */
double abacus_get_sparsity(const CrystallineAbacus* abacus);

/**
 * @brief Check if abacus is in sparse mode
 * 
 * @param abacus Input abacus
 * @return true if sparse, false if dense
 */
bool abacus_is_sparse(const CrystallineAbacus* abacus);

/**
 * @brief Get memory usage in bytes
 * 
 * Calculates total memory used by the abacus structure,
 * including all allocated arrays.
 * 
 * @param abacus Input abacus
 * @return Memory usage in bytes, or 0 on error
 */
size_t abacus_memory_usage(const CrystallineAbacus* abacus);

/**
 * @brief Add two sparse numbers (internal function)
 * 
 * Optimized addition for sparse representations.
 * Only processes non-zero beads, resulting in O(k) complexity
 * where k = number of non-zero beads.
 * 
 * Algorithm:
 * 1. Merge sparse beads by weight_exponent
 * 2. Add values at same exponent
 * 3. Handle carries
 * 4. Remove zeros from result
 * 
 * @param result Output (will be in sparse mode)
 * @param a First operand (must be sparse)
 * @param b Second operand (must be sparse)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_add_sparse(CrystallineAbacus* result,
                            const CrystallineAbacus* a,
                            const CrystallineAbacus* b);

/**
 * @brief Multiply two sparse numbers (internal function)
 * 
 * Optimized multiplication for sparse representations.
 * Only processes non-zero beads, resulting in O(k²) complexity
 * where k = number of non-zero beads.
 * 
 * Algorithm:
 * 1. For each pair of non-zero beads: multiply values, add exponents
 * 2. Combine terms with same exponent
 * 3. Handle carries
 * 4. Remove zeros from result
 * 
 * @param result Output (will be in sparse mode)
 * @param a First operand (must be sparse)
 * @param b Second operand (must be sparse)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_mul_sparse(CrystallineAbacus* result,
                            const CrystallineAbacus* a,
                            const CrystallineAbacus* b);

#ifdef __cplusplus
}
#endif

#endif /* MATH_ABACUS_H */
