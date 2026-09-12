/**
 * @file cordic.h
 * @brief CORDIC algorithm for pure Abacus trigonometric functions
 * 
 * CORDIC (COordinate Rotation DIgital Computer) provides pure Abacus
 * implementations of trigonometric functions without any double intermediates.
 * 
 * This achieves true arbitrary precision for sin/cos operations.
 */

#ifndef MATH_CORDIC_H
#define MATH_CORDIC_H

#include "types.h"
#include "abacus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CORDIC rotation in vectoring mode
 * 
 * Rotates vector (x, y) by angle z using CORDIC algorithm.
 * Pure Abacus implementation - no floating point!
 * 
 * Complexity: O(iterations) where each iteration is O(1)
 * 
 * @param x Input/output x coordinate (Abacus)
 * @param y Input/output y coordinate (Abacus)
 * @param z Angle to rotate in radians (Abacus)
 * @param iterations Number of CORDIC iterations (precision)
 * @param base Number base for Abacus operations
 * @return MATH_SUCCESS or error code
 */
MathError cordic_rotate(CrystallineAbacus* x,
                       CrystallineAbacus* y,
                       CrystallineAbacus* z,
                       uint32_t iterations,
                       uint32_t base);

/**
 * @brief Compute sin and cos using CORDIC
 * 
 * Pure Abacus implementation - TRUE arbitrary precision!
 * No double intermediates, no floating point errors.
 * 
 * Algorithm:
 * - Start with vector (1, 0)
 * - Rotate by predetermined angles
 * - Converges to (cos θ, sin θ)
 * - Apply CORDIC gain factor K
 * 
 * Complexity: O(precision) iterations, each O(1)
 * 
 * @param sin_result Output: sin(angle) as Abacus
 * @param cos_result Output: cos(angle) as Abacus
 * @param angle Input angle in radians as Abacus
 * @param precision Number of CORDIC iterations (16 recommended)
 * @return MATH_SUCCESS or error code
 */
MathError cordic_sincos(CrystallineAbacus* sin_result,
                       CrystallineAbacus* cos_result,
                       const CrystallineAbacus* angle,
                       uint32_t precision);

#ifdef __cplusplus
}
#endif

#endif /* MATH_CORDIC_H */