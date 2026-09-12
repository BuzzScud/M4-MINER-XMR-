/**
 * @file transcendental.h
 * @brief Transcendental functions (double precision)
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * This module provides double-precision transcendental functions.
 * All implementations are self-contained and do not depend on math.h.
 * 
 * Implementations use:
 * - Taylor series expansions
 * - CORDIC algorithms
 * - Newton-Raphson iteration
 * - Lookup tables with interpolation
 * 
 * For arbitrary precision, see bigfixed.h
 */

#ifndef MATH_TRANSCENDENTAL_H
#define MATH_TRANSCENDENTAL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * POWER AND ROOT FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Square root
 * @param x Input value (must be >= 0)
 * @return √x
 * @note Returns NaN if x < 0
 * 
 * Implementation: Newton-Raphson iteration
 */
double math_sqrt(double x);

/**
 * @brief Cube root
 * @param x Input value
 * @return ∛x
 * 
 * Implementation: Newton-Raphson iteration
 */
double math_cbrt(double x);

/**
 * @brief Power function
 * @param x Base
 * @param y Exponent
 * @return x^y
 * 
 * Implementation: exp(y * log(x)) for general case
 */
double math_pow(double x, double y);

/**
 * @brief Integer power (optimized)
 * @param x Base
 * @param n Integer exponent
 * @return x^n
 * 
 * Implementation: Binary exponentiation
 */
double math_powi(double x, int n);

/* ============================================================================
 * EXPONENTIAL AND LOGARITHM FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Natural exponential function
 * @param x Exponent
 * @return e^x
 * 
 * Implementation: Taylor series with range reduction
 */
double math_exp(double x);

/**
 * @brief Exponential minus one (for small x)
 * @param x Exponent
 * @return e^x - 1
 * 
 * Implementation: Optimized for |x| << 1
 */
double math_expm1(double x);

/**
 * @brief Natural logarithm
 * @param x Input value (must be > 0)
 * @return ln(x)
 * @note Returns NaN if x <= 0
 * 
 * Implementation: Taylor series with range reduction
 */
double math_log(double x);

/**
 * @brief Natural logarithm plus one (for small x)
 * @param x Input value (must be > -1)
 * @return ln(1 + x)
 * 
 * Implementation: Optimized for |x| << 1
 */
double math_log1p(double x);

/**
 * @brief Base-10 logarithm
 * @param x Input value (must be > 0)
 * @return log₁₀(x)
 */
double math_log10(double x);

/**
 * @brief Base-2 logarithm
 * @param x Input value (must be > 0)
 * @return log₂(x)
 */
double math_log2(double x);

/* ============================================================================
 * TRIGONOMETRIC FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Sine function
 * @param x Angle in radians
 * @return sin(x)
 * 
 * Implementation: CORDIC algorithm or Taylor series
 */
double math_sin(double x);

/**
 * @brief Cosine function
 * @param x Angle in radians
 * @return cos(x)
 * 
 * Implementation: CORDIC algorithm or Taylor series
 */
double math_cos(double x);

/**
 * @brief Tangent function
 * @param x Angle in radians
 * @return tan(x)
 * 
 * Implementation: sin(x) / cos(x)
 */
double math_tan(double x);

/**
 * @brief Compute sine and cosine simultaneously
 * @param x Angle in radians
 * @param sin_x Output for sin(x)
 * @param cos_x Output for cos(x)
 * 
 * More efficient than calling sin and cos separately
 */
void math_sincos(double x, double *sin_x, double *cos_x);

/* ============================================================================
 * INVERSE TRIGONOMETRIC FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Arcsine function
 * @param x Input value (must be in [-1, 1])
 * @return arcsin(x) in radians
 * @note Returns NaN if |x| > 1
 */
double math_asin(double x);

/**
 * @brief Arccosine function
 * @param x Input value (must be in [-1, 1])
 * @return arccos(x) in radians
 * @note Returns NaN if |x| > 1
 */
double math_acos(double x);

/**
 * @brief Arctangent function
 * @param x Input value
 * @return arctan(x) in radians
 */
double math_atan(double x);

/**
 * @brief Two-argument arctangent
 * @param y Y coordinate
 * @param x X coordinate
 * @return arctan(y/x) in radians, with correct quadrant
 * 
 * Returns angle in range [-π, π]
 */
double math_atan2(double y, double x);

/* ============================================================================
 * HYPERBOLIC FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Hyperbolic sine
 * @param x Input value
 * @return sinh(x) = (e^x - e^(-x)) / 2
 */
double math_sinh(double x);

/**
 * @brief Hyperbolic cosine
 * @param x Input value
 * @return cosh(x) = (e^x + e^(-x)) / 2
 */
double math_cosh(double x);

/**
 * @brief Hyperbolic tangent
 * @param x Input value
 * @return tanh(x) = sinh(x) / cosh(x)
 */
double math_tanh(double x);

/**
 * @brief Inverse hyperbolic sine
 * @param x Input value
 * @return asinh(x) = ln(x + √(x² + 1))
 */
double math_asinh(double x);

/**
 * @brief Inverse hyperbolic cosine
 * @param x Input value (must be >= 1)
 * @return acosh(x) = ln(x + √(x² - 1))
 */
double math_acosh(double x);

/**
 * @brief Inverse hyperbolic tangent
 * @param x Input value (must be in (-1, 1))
 * @return atanh(x) = 0.5 * ln((1+x)/(1-x))
 */
double math_atanh(double x);

/* ============================================================================
 * ROUNDING FUNCTIONS
 * ============================================================================
 */

/* Note: math_floor, math_ceil, math_round, math_trunc are declared in arithmetic.h
 * These are basic arithmetic operations, not transcendental functions. */

/**
 * @brief Floating-point modulo
 * @param x Dividend
 * @param y Divisor
 * @return x mod y (remainder with same sign as x)
 */
double math_fmod(double x, double y);

/**
 * @brief IEEE remainder
 * @param x Dividend
 * @param y Divisor
 * @return IEEE remainder of x/y
 */
double math_remainder(double x, double y);

#ifdef __cplusplus
}
#endif

#endif /* MATH_TRANSCENDENTAL_H */
/* ============================================================================
 * ABACUS TRANSCENDENTAL FUNCTIONS (ARBITRARY PRECISION)
 * ============================================================================
 * 
 * These functions provide arbitrary precision transcendental operations
 * using CrystallineAbacus for both input and output.
 * 
 * All operations are pure Abacus arithmetic with no precision loss.
 * Precision is controlled by the number of terms in series expansions.
 */

#include "abacus.h"

/**
 * @brief Square root with Abacus input/output
 * @param result Output (√x)
 * @param x Input value (must be >= 0)
 * @param precision Number of decimal places of precision
 * @return Error code
 * 
 * Implementation: Newton-Raphson iteration in pure Abacus arithmetic
 * Complexity: O(log(precision) × log(magnitude))
 */
MathError math_sqrt_abacus(CrystallineAbacus* result,
                           const CrystallineAbacus* x,
                           uint32_t precision);

/**
 * @brief Sine function with Abacus input/output
 * @param result Output (sin(x))
 * @param x Input angle in radians (Abacus)
 * @param precision Number of decimal places of precision
 * @return Error code
 * 
 * Implementation: Taylor series in pure Abacus arithmetic
 * sin(x) = x - x³/3! + x⁵/5! - x⁷/7! + ...
 * 
 * Complexity: O(precision × log(magnitude))
 */
MathError math_sin_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision);

/**
 * @brief Cosine function with Abacus input/output
 * @param result Output (cos(x))
 * @param x Input angle in radians (Abacus)
 * @param precision Number of decimal places of precision
 * @return Error code
 * 
 * Implementation: Taylor series in pure Abacus arithmetic
 * cos(x) = 1 - x²/2! + x⁴/4! - x⁶/6! + ...
 * 
 * Complexity: O(precision × log(magnitude))
 */
MathError math_cos_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision);

/**
 * @brief Two-argument arctangent with Abacus input/output
 * @param result Output (atan2(y, x)) in radians
 * @param y Y coordinate (Abacus)
 * @param x X coordinate (Abacus)
 * @param precision Number of decimal places of precision
 * @return Error code
 * 
 * Implementation: CORDIC algorithm in pure Abacus arithmetic
 * Returns angle in range [-π, π]
 * 
 * Complexity: O(precision × log(magnitude))
 */
MathError math_atan2_abacus(CrystallineAbacus* result,
                            const CrystallineAbacus* y,
                            const CrystallineAbacus* x,
                            uint32_t precision);


/**
 * @brief Natural logarithm with Abacus input/output
 * @param result Output: ln(x)
 * @param x Input value (must be > 0)
 * @param precision Decimal precision
 * @return MATH_SUCCESS or error code
 */
MathError math_log_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision);

/**
 * @brief Exponential function with Abacus input/output
 * @param result Output: e^x
 * @param x Input value
 * @param precision Decimal precision
 * @return MATH_SUCCESS or error code
 */
MathError math_exp_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision);
/**
 * @brief Compute sine and cosine simultaneously with Abacus
 * @param sin_result Output for sin(x)
 * @param cos_result Output for cos(x)
 * @param x Input angle in radians (Abacus)
 * @param precision Number of decimal places of precision
 * @return Error code
 * 
 * More efficient than calling sin and cos separately.
 * Uses combined Taylor series computation.
 */
MathError math_sincos_abacus(CrystallineAbacus* sin_result,
                             CrystallineAbacus* cos_result,
                             const CrystallineAbacus* x,
                             uint32_t precision);

