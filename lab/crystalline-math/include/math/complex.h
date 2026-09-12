/**
 * @file complex.h
 * @brief Complex number operations for Crystalline Math Library
 * 
 * Provides O(1) complex number operations using Babylonian clock lattice
 * representation. Complex numbers are represented as positions on the
 * clock lattice with radial (real) and angular (imaginary) components.
 * 
 * DESIGN PRINCIPLES:
 * - Self-contained (no external complex.h dependency)
 * - O(1) operations using geometric transformations
 * - Babylonian clock lattice representation
 * - 12-fold symmetry preserved
 * 
 * REPRESENTATION:
 * - Real part: Radial distance from origin
 * - Imaginary part: Angular position on clock
 * - Operations are geometric transformations on the clock
 */

#ifndef MATH_COMPLEX_H
#define MATH_COMPLEX_H

#include "types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Complex number type using Babylonian representation
 * 
 * Represents a complex number z = a + bi where:
 * - real: Real component (a)
 * - imag: Imaginary component (b)
 * 
 * Can also be viewed as:
 * - Polar form: z = r * e^(iθ)
 * - Clock position: (radius, angle)
 */
typedef struct {
    double real;
    double imag;
} MathComplex;

// ============================================================================
// CONSTRUCTION
// ============================================================================

/**
 * Create complex number from Cartesian coordinates
 * 
 * @param real Real component
 * @param imag Imaginary component
 * @return Complex number z = real + imag*i
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_from_cartesian(double real, double imag);

/**
 * Create complex number from polar coordinates
 * 
 * @param r Magnitude (radius)
 * @param theta Phase angle in radians
 * @return Complex number z = r * e^(iθ)
 * 
 * Uses Babylonian clock representation:
 * - real = r * cos(θ)
 * - imag = r * sin(θ)
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_from_polar(double r, double theta);

// ============================================================================
// BASIC ARITHMETIC (O(1))
// ============================================================================

/**
 * Add two complex numbers
 * 
 * (a + bi) + (c + di) = (a+c) + (b+d)i
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_add(MathComplex a, MathComplex b);

/**
 * Subtract two complex numbers
 * 
 * (a + bi) - (c + di) = (a-c) + (b-d)i
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_sub(MathComplex a, MathComplex b);

/**
 * Multiply two complex numbers
 * 
 * (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_mul(MathComplex a, MathComplex b);

/**
 * Divide two complex numbers
 * 
 * (a + bi) / (c + di) = [(ac + bd) + (bc - ad)i] / (c² + d²)
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_div(MathComplex a, MathComplex b);

// ============================================================================
// PROPERTIES (O(1))
// ============================================================================

/**
 * Compute complex conjugate
 * 
 * conj(a + bi) = a - bi
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_conjugate(MathComplex z);

/**
 * Compute magnitude (absolute value)
 * 
 * |a + bi| = √(a² + b²)
 * 
 * Complexity: O(1)
 */
double math_complex_magnitude(MathComplex z);

/**
 * Compute squared magnitude (faster than magnitude)
 * 
 * |a + bi|² = a² + b²
 * 
 * Complexity: O(1)
 */
double math_complex_magnitude_squared(MathComplex z);

/**
 * Compute phase angle
 * 
 * arg(a + bi) = atan2(b, a)
 * 
 * Returns angle in radians [-π, π]
 * 
 * Complexity: O(1)
 */
double math_complex_phase(MathComplex z);

// ============================================================================
// EXPONENTIAL FUNCTIONS
// ============================================================================

/**
 * Complex exponential
 * 
 * exp(a + bi) = e^a * (cos(b) + i*sin(b))
 * 
 * Uses Euler's formula: e^(iθ) = cos(θ) + i*sin(θ)
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_exp(MathComplex z);

/**
 * Complex natural logarithm
 * 
 * log(a + bi) = log(|z|) + i*arg(z)
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_log(MathComplex z);

/**
 * Complex power
 * 
 * z^n for real exponent n
 * 
 * Uses: z^n = r^n * e^(inθ) where z = r*e^(iθ)
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_pow(MathComplex z, double n);

/**
 * Complex power with complex exponent
 * 
 * z^w = exp(w * log(z))
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_pow_complex(MathComplex z, MathComplex w);

// ============================================================================
// TRIGONOMETRIC FUNCTIONS
// ============================================================================

/**
 * Complex sine
 * 
 * sin(a + bi) = sin(a)*cosh(b) + i*cos(a)*sinh(b)
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_sin(MathComplex z);

/**
 * Complex cosine
 * 
 * cos(a + bi) = cos(a)*cosh(b) - i*sin(a)*sinh(b)
 * 
 * Complexity: O(1)
 */
MathComplex math_complex_cos(MathComplex z);

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * Check if complex number is zero
 * 
 * Complexity: O(1)
 */
bool math_complex_is_zero(MathComplex z);

/**
 * Check if complex number is real (imaginary part ≈ 0)
 * 
 * Complexity: O(1)
 */
bool math_complex_is_real(MathComplex z);

/**
 * Check if complex number is imaginary (real part ≈ 0)
 * 
 * Complexity: O(1)
 */
bool math_complex_is_imaginary(MathComplex z);

/**
 * Compare two complex numbers for equality (within epsilon)
 * 
 * Complexity: O(1)
 */
bool math_complex_equals(MathComplex a, MathComplex b, double epsilon);

#ifdef __cplusplus
}
#endif

#endif // MATH_COMPLEX_H