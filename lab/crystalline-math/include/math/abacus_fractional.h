/**
 * @file abacus_fractional.h
 * @brief Pure Abacus fractional operations (no double intermediate)
 * 
 * This header defines fractional operations using only Abacus arithmetic,
 * achieving true arbitrary precision without double precision loss.
 */

#ifndef MATH_ABACUS_FRACTIONAL_H
#define MATH_ABACUS_FRACTIONAL_H

#include "math/types.h"
#include "math/abacus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FRACTIONAL DIVISION
 * ============================================================================
 */

/**
 * @brief Divide with fractional result (pure Abacus, no double)
 * 
 * Computes a / b with arbitrary precision using only Abacus operations.
 * This eliminates the precision loss from double intermediate values.
 * 
 * Algorithm:
 * 1. Compute integer quotient and remainder
 * 2. Compute fractional digits by repeated division
 * 3. Store fractional digits with negative exponents
 * 4. Combine integer and fractional parts
 * 
 * Example: 1 / 3 in base 10 with precision 5
 *   Result: 0.33333 (stored as beads with exponents -1, -2, -3, -4, -5)
 * 
 * @param result Output quotient with fractional part
 * @param a Dividend
 * @param b Divisor
 * @param precision Number of fractional digits to compute
 * @return MATH_SUCCESS or error code
 * 
 * Complexity: O(precision × log n) where n is number size
 * Precision: Arbitrary (limited only by memory and precision parameter)
 */
MathError abacus_div_fractional(CrystallineAbacus* result,
                                const CrystallineAbacus* a,
                                const CrystallineAbacus* b,
                                uint32_t precision);

/* ============================================================================
 * PURE ABACUS SQUARE ROOT
 * ============================================================================
 */

/**
 * @brief Compute square root using pure Abacus Newton-Raphson
 * 
 * Uses Newton-Raphson iteration: x_{n+1} = (x_n + a/x_n) / 2
 * All operations use pure Abacus arithmetic for arbitrary precision.
 * 
 * Convergence: Quadratic (doubles correct digits each iteration)
 * Typical iterations needed: 5-10 for high precision
 * 
 * @param result Output square root
 * @param a Input value (must be non-negative)
 * @param precision Number of iterations (5-10 typical, more = better)
 * @return MATH_SUCCESS or error code
 * 
 * Complexity: O(precision × log n) where n is number size
 * Precision: Arbitrary (doubles each iteration)
 * 
 * Example:
 *   abacus_sqrt_pure(result, a, 10);  // ~20 decimal digits precision
 */
MathError abacus_sqrt_pure(CrystallineAbacus* result,
                           const CrystallineAbacus* a,
                           uint32_t precision);

#ifdef __cplusplus
}
#endif

#endif /* MATH_ABACUS_FRACTIONAL_H */