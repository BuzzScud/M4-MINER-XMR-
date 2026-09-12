/**
 * @file arithmetic.c
 * @brief Core arithmetic operations implementation
 * 
 * Replaces: arithmetic_legacy.c
 * 
 * Self-contained double-precision arithmetic.
 * No dependencies on math.h or external libraries.
 * 
 * NOTE: This maintains the existing double-precision API for compatibility.
 * For arbitrary precision, use CrystallineAbacus directly (see abacus.h).
 */

#include "math/arithmetic.h"
#include "math/validation.h"
#include <stdint.h>

/* ============================================================================
 * BASIC ARITHMETIC
 * ============================================================================
 */

double math_add(double a, double b) {
    return a + b;
}

double math_sub(double a, double b) {
    return a - b;
}

double math_mul(double a, double b) {
    return a * b;
}

double math_div(double a, double b) {
    if (b == 0.0) {
        // Return NaN for division by zero
        return 0.0 / 0.0;
    }
    return a / b;
}

double math_mod(double a, double b) {
    if (b == 0.0) {
        return 0.0 / 0.0;  // NaN
    }
    
    // C99 fmod behavior: result has same sign as dividend (a)
    double quotient = math_trunc(a / b);
    double result = a - b * quotient;
    
    return result;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

double math_abs(double x) {
    return (x < 0.0) ? -x : x;
}

double math_min(double a, double b) {
    return (a < b) ? a : b;
}

double math_max(double a, double b) {
    return (a > b) ? a : b;
}

double math_clamp(double x, double min_val, double max_val) {
    if (x < min_val) return min_val;
    if (x > max_val) return max_val;
    return x;
}

bool math_approx_equal(double a, double b, double epsilon) {
    return math_abs(a - b) < epsilon;
}

// Rounding functions
double math_floor(double x) {
    if (math_is_nan(x) || math_is_inf(x)) {
        return x;
    }
    
    int64_t i = (int64_t)x;
    if (x < 0.0 && x != (double)i) {
        return (double)(i - 1);
    }
    return (double)i;
}

double math_ceil(double x) {
    if (math_is_nan(x) || math_is_inf(x)) {
        return x;
    }
    
    int64_t i = (int64_t)x;
    if (x > 0.0 && x != (double)i) {
        return (double)(i + 1);
    }
    return (double)i;
}

double math_round(double x) {
    if (math_is_nan(x) || math_is_inf(x)) {
        return x;
    }
    
    if (x >= 0.0) {
        return math_floor(x + 0.5);
    } else {
        return math_ceil(x - 0.5);
    }
}

double math_trunc(double x) {
    if (math_is_nan(x) || math_is_inf(x)) {
        return x;
    }
    
    return (double)((int64_t)x);
}

/* ============================================================================
 * END OF ARITHMETIC IMPLEMENTATION
 * ============================================================================
 * 
 * This implementation is IDENTICAL to arithmetic_legacy.c but with:
 * ✅ Clean structure and documentation
 * ✅ No external dependencies (self-contained)
 * ✅ Proper validation using validation.h
 * 
 * Future enhancement: For arbitrary precision arithmetic, users should
 * call CrystallineAbacus functions directly (see math/abacus.h).
 * 
 * The abacus provides:
 * - Arbitrary precision (no overflow)
 * - Multiple bases (binary, decimal, base-60, etc.)
 * - Geometric operations on clock lattice
 * - Full MASTER_PLAN compliance
 */