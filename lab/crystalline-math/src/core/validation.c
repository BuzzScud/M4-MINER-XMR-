/**
 * @file validation.c
 * @brief Implementation of validation functions
 * 
 * Provides O(1) validation for special floating-point values.
 * Most functions are inline in the header for maximum performance.
 */

#include "math/validation.h"
#include <stdint.h>

/**
 * Check if value is normal (not zero, not subnormal, not infinite, not NaN)
 * 
 * Uses IEEE 754 bit pattern analysis for O(1) detection.
 */
int math_is_normal(double x) {
    if (math_is_nan(x) || math_is_inf(x) || math_is_zero(x)) {
        return 0;
    }
    
    // Check for subnormal numbers using IEEE 754 representation
    // A subnormal number has exponent = 0 and mantissa != 0
    union {
        double d;
        uint64_t u;
    } value;
    
    value.d = x;
    
    // Extract exponent (bits 52-62)
    uint64_t exponent = (value.u >> 52) & 0x7FF;
    
    // Normal numbers have exponent in range [1, 2046]
    return (exponent > 0) && (exponent < 0x7FF);
}