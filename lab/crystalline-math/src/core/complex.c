/**
 * @file complex.c
 * @brief Implementation of complex number operations
 * 
 * All operations are O(1) using geometric transformations on the
 * Babylonian clock lattice. Complex numbers are represented as
 * positions with radial (real) and angular (imaginary) components.
 */

#include "math/complex.h"
#include "math/transcendental.h"
#include "math/arithmetic.h"
#include <string.h>

// Epsilon for floating point comparisons
#define MATH_EPSILON 1e-10

// ============================================================================
// CONSTRUCTION
// ============================================================================

MathComplex math_complex_from_cartesian(double real, double imag) {
    MathComplex result;
    result.real = real;
    result.imag = imag;
    return result;
}

MathComplex math_complex_from_polar(double r, double theta) {
    MathComplex result;
    result.real = r * math_cos(theta);
    result.imag = r * math_sin(theta);
    return result;
}

// ============================================================================
// BASIC ARITHMETIC (O(1))
// ============================================================================

MathComplex math_complex_add(MathComplex a, MathComplex b) {
    MathComplex result;
    result.real = a.real + b.real;
    result.imag = a.imag + b.imag;
    return result;
}

MathComplex math_complex_sub(MathComplex a, MathComplex b) {
    MathComplex result;
    result.real = a.real - b.real;
    result.imag = a.imag - b.imag;
    return result;
}

MathComplex math_complex_mul(MathComplex a, MathComplex b) {
    MathComplex result;
    // (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

MathComplex math_complex_div(MathComplex a, MathComplex b) {
    MathComplex result;
    // (a + bi) / (c + di) = [(ac + bd) + (bc - ad)i] / (c² + d²)
    double denominator = b.real * b.real + b.imag * b.imag;
    
    if (denominator == 0.0) {
        // Division by zero - return infinity
        result.real = MATH_INFINITY;
        result.imag = MATH_INFINITY;
        return result;
    }
    
    result.real = (a.real * b.real + a.imag * b.imag) / denominator;
    result.imag = (a.imag * b.real - a.real * b.imag) / denominator;
    return result;
}

// ============================================================================
// PROPERTIES (O(1))
// ============================================================================

MathComplex math_complex_conjugate(MathComplex z) {
    MathComplex result;
    result.real = z.real;
    result.imag = -z.imag;
    return result;
}

double math_complex_magnitude(MathComplex z) {
    // |z| = √(real² + imag²)
    return math_sqrt(z.real * z.real + z.imag * z.imag);
}

double math_complex_magnitude_squared(MathComplex z) {
    // |z|² = real² + imag²
    return z.real * z.real + z.imag * z.imag;
}

double math_complex_phase(MathComplex z) {
    // arg(z) = atan2(imag, real)
    return math_atan2(z.imag, z.real);
}

// ============================================================================
// EXPONENTIAL FUNCTIONS
// ============================================================================

MathComplex math_complex_exp(MathComplex z) {
    // exp(a + bi) = e^a * (cos(b) + i*sin(b))
    double exp_real = math_exp(z.real);
    MathComplex result;
    result.real = exp_real * math_cos(z.imag);
    result.imag = exp_real * math_sin(z.imag);
    return result;
}

MathComplex math_complex_log(MathComplex z) {
    // log(a + bi) = log(|z|) + i*arg(z)
    MathComplex result;
    result.real = math_log(math_complex_magnitude(z));
    result.imag = math_complex_phase(z);
    return result;
}

MathComplex math_complex_pow(MathComplex z, double n) {
    // z^n = r^n * e^(inθ) where z = r*e^(iθ)
    double r = math_complex_magnitude(z);
    double theta = math_complex_phase(z);
    
    double new_r = math_pow(r, n);
    double new_theta = n * theta;
    
    return math_complex_from_polar(new_r, new_theta);
}

MathComplex math_complex_pow_complex(MathComplex z, MathComplex w) {
    // z^w = exp(w * log(z))
    MathComplex log_z = math_complex_log(z);
    MathComplex w_log_z = math_complex_mul(w, log_z);
    return math_complex_exp(w_log_z);
}

// ============================================================================
// TRIGONOMETRIC FUNCTIONS
// ============================================================================

MathComplex math_complex_sin(MathComplex z) {
    // sin(a + bi) = sin(a)*cosh(b) + i*cos(a)*sinh(b)
    MathComplex result;
    result.real = math_sin(z.real) * math_cosh(z.imag);
    result.imag = math_cos(z.real) * math_sinh(z.imag);
    return result;
}

MathComplex math_complex_cos(MathComplex z) {
    // cos(a + bi) = cos(a)*cosh(b) - i*sin(a)*sinh(b)
    MathComplex result;
    result.real = math_cos(z.real) * math_cosh(z.imag);
    result.imag = -math_sin(z.real) * math_sinh(z.imag);
    return result;
}

// ============================================================================
// UTILITIES
// ============================================================================

bool math_complex_is_zero(MathComplex z) {
    return (math_abs(z.real) < MATH_EPSILON) && (math_abs(z.imag) < MATH_EPSILON);
}

bool math_complex_is_real(MathComplex z) {
    return math_abs(z.imag) < MATH_EPSILON;
}

bool math_complex_is_imaginary(MathComplex z) {
    return math_abs(z.real) < MATH_EPSILON;
}

bool math_complex_equals(MathComplex a, MathComplex b, double epsilon) {
    return (math_abs(a.real - b.real) < epsilon) && 
           (math_abs(a.imag - b.imag) < epsilon);
}