/**
 * @file validation.h
 * @brief Validation and special value checking functions
 * 
 * Provides O(1) validation functions for checking special floating-point
 * values (NaN, infinity, etc.) without depending on external math.h.
 * 
 * DESIGN PRINCIPLES:
 * - Self-contained (no math.h dependency)
 * - O(1) operations using IEEE 754 properties
 * - Compatible with C99 standard
 * - Babylonian mathematics approach
 */

#ifndef MATH_VALIDATION_H
#define MATH_VALIDATION_H

#include "types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SPECIAL VALUE CHECKING (O(1))
// ============================================================================

/**
 * Check if value is NaN (Not a Number)
 * 
 * Uses the property that NaN is the only value not equal to itself.
 * 
 * @param x Value to check
 * @return 1 if NaN, 0 otherwise
 * 
 * Complexity: O(1)
 */
static inline int math_is_nan(double x) {
    return x != x;
}

/**
 * Check if value is positive or negative infinity
 * 
 * @param x Value to check
 * @return 1 if infinite, 0 otherwise
 * 
 * Complexity: O(1)
 */
static inline int math_is_inf(double x) {
    return (x == MATH_INFINITY) || (x == MATH_NEG_INFINITY);
}

/**
 * Check if value is finite (not NaN and not infinite)
 * 
 * @param x Value to check
 * @return 1 if finite, 0 otherwise
 * 
 * Complexity: O(1)
 */
static inline int math_is_finite(double x) {
    return !math_is_nan(x) && !math_is_inf(x);
}

/**
 * Check if value is normal (finite, non-zero, not subnormal)
 * 
 * A normal number has a non-zero exponent in IEEE 754 representation.
 * 
 * @param x Value to check
 * @return 1 if normal, 0 otherwise
 * 
 * Complexity: O(1)
 */
int math_is_normal(double x);

/**
 * Check if value is positive infinity
 * 
 * @param x Value to check
 * @return 1 if positive infinity, 0 otherwise
 * 
 * Complexity: O(1)
 */
static inline int math_is_positive_inf(double x) {
    return x == MATH_INFINITY;
}

/**
 * Check if value is negative infinity
 * 
 * @param x Value to check
 * @return 1 if negative infinity, 0 otherwise
 * 
 * Complexity: O(1)
 */
static inline int math_is_negative_inf(double x) {
    return x == MATH_NEG_INFINITY;
}

/**
 * Check if value is zero (positive or negative)
 * 
 * @param x Value to check
 * @return 1 if zero, 0 otherwise
 * 
 * Complexity: O(1)
 */
static inline int math_is_zero(double x) {
    return x == 0.0 || x == -0.0;
}

/**
 * Get sign of value (-1, 0, or 1)
 * 
 * @param x Value to check
 * @return -1 if negative, 0 if zero, 1 if positive
 * 
 * Complexity: O(1)
 */
static inline int math_sign(double x) {
    if (x > 0.0) return 1;
    if (x < 0.0) return -1;
    return 0;
}

/**
 * Check if two values are approximately equal
 * 
 * @param a First value
 * @param b Second value
 * @param epsilon Tolerance
 * @return 1 if approximately equal, 0 otherwise
 * 
 * Complexity: O(1)
 * 
 * Note: Declared in arithmetic.h, not redefined here to avoid conflicts
 */
// Removed inline definition - use declaration from arithmetic.h

#ifdef __cplusplus
}
#endif

#endif // MATH_VALIDATION_H