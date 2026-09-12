/**
 * @file arithmetic.h
 * @brief Core arithmetic operations (double precision)
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * This module provides basic double-precision arithmetic operations.
 * All functions are self-contained and do not depend on math.h.
 * 
 * For arbitrary precision, see abacus.h (crystalline clock lattice)
 */

#ifndef MATH_ARITHMETIC_H
#define MATH_ARITHMETIC_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * BASIC ARITHMETIC
 * ============================================================================
 */

/**
 * @brief Add two numbers
 * @param a First operand
 * @param b Second operand
 * @return a + b
 */
double math_add(double a, double b);

/**
 * @brief Subtract two numbers
 * @param a First operand
 * @param b Second operand
 * @return a - b
 */
double math_sub(double a, double b);

/**
 * @brief Multiply two numbers
 * @param a First operand
 * @param b Second operand
 * @return a * b
 */
double math_mul(double a, double b);

/**
 * @brief Divide two numbers
 * @param a Numerator
 * @param b Denominator
 * @return a / b
 * @note Returns NaN if b is zero
 */
double math_div(double a, double b);

/**
 * @brief Compute modulo (remainder)
 * @param a Dividend
 * @param b Divisor
 * @return a mod b
 */
double math_mod(double a, double b);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Absolute value
 * @param x Input value
 * @return |x|
 */
double math_abs(double x);

/**
 * @brief Minimum of two values
 * @param a First value
 * @param b Second value
 * @return min(a, b)
 */
double math_min(double a, double b);

/**
 * @brief Maximum of two values
 * @param a First value
 * @param b Second value
 * @return max(a, b)
 */
double math_max(double a, double b);

/**
 * @brief Clamp value to range
 * @param x Value to clamp
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @return Clamped value
 */
double math_clamp(double x, double min_val, double max_val);

/**
 * @brief Sign of a number
 * @param x Input value
 * @return -1 if x < 0, 0 if x == 0, 1 if x > 0
 * 
 * NOTE: Implementation is in validation.h as static inline for O(1) performance
 */
// int math_sign(double x);  // Removed - conflicts with validation.h static inline

/**
 * @brief Check if two numbers are approximately equal
 * @param a First value
 * @param b Second value
 * @param epsilon Tolerance
 * @return true if |a - b| < epsilon
 */
bool math_approx_equal(double a, double b, double epsilon);

/**
 * @brief Check if value is NaN
 * @param x Value to check
 * @return true if x is NaN
 * 
 * NOTE: Implementation is in validation.h as static inline for O(1) performance
 */
// bool math_is_nan(double x);  // Removed - conflicts with validation.h static inline

/**
 * @brief Check if value is infinite
 * @param x Value to check
 * @return true if x is infinite
 * 
 * NOTE: Implementation is in validation.h as static inline for O(1) performance
 */
// bool math_is_inf(double x);  // Removed - conflicts with validation.h static inline

/**
 * @brief Check if value is finite
 * @param x Value to check
 * @return true if x is finite (not NaN or infinite)
 * 
 * NOTE: Implementation is in validation.h as static inline for O(1) performance
 */
// bool math_is_finite(double x);  // Removed - conflicts with validation.h static inline

/**
 * @brief Round down to nearest integer
 * @param x Value to round
 * @return Largest integer <= x
 */
double math_floor(double x);

/**
 * @brief Round up to nearest integer
 * @param x Value to round
 * @return Smallest integer >= x
 */
double math_ceil(double x);

/**
 * @brief Round to nearest integer
 * @param x Value to round
 * @return Nearest integer (ties round away from zero)
 */
double math_round(double x);

/**
 * @brief Truncate to integer (round toward zero)
 * @param x Value to truncate
 * @return Integer part of x
 */
double math_trunc(double x);

#ifdef __cplusplus
}
#endif

#endif /* MATH_ARITHMETIC_H */