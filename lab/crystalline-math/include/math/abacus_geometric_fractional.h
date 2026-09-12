/**
 * @file abacus_geometric_fractional.h
 * @brief Fractional and Floating-Point Geometric Operations
 * 
 * This module extends the geometric operations to support fractional
 * division and floating-point results using the 6-step Babylonian pattern.
 * 
 * Week 2 - Phase 1: Fractional Division & Floating-Point Support
 */

#ifndef MATH_ABACUS_GEOMETRIC_FRACTIONAL_H
#define MATH_ABACUS_GEOMETRIC_FRACTIONAL_H

#include "types.h"
#include "abacus.h"
#include "abacus_fractional.h"
#include "clock.h"
#include "clock_triangle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FRACTIONAL DIVISION OPERATIONS
 * ============================================================================
 */

/**
 * @brief Divide two abacus numbers with fractional result
 * @param a Numerator
 * @param b Denominator
 * @param result Output (a / b) with fractional part
 * @param precision Number of decimal places for result
 * @return MATH_SUCCESS or error code
 * 
 * Performs division using the 6-step Babylonian pattern with support
 * for fractional results. Unlike abacus_div_geometric which truncates,
 * this function preserves the fractional part.
 * 
 * Example: 7 / 2 = 3.5 (not 3)
 * 
 * Complexity: O(1) for the geometric operations
 */
MathError abacus_div_geometric_fractional(const CrystallineAbacus* a,
                                         const CrystallineAbacus* b,
                                         CrystallineAbacus* result,
                                         int32_t precision);

/**
 * @brief Divide with remainder using geometric operations
 * @param a Numerator
 * @param b Denominator
 * @param quotient Output quotient
 * @param remainder Output remainder
 * @return MATH_SUCCESS or error code
 * 
 * Performs division and returns both quotient and remainder.
 * Uses the 6-step Babylonian pattern.
 * 
 * Example: 7 / 2 = quotient: 3, remainder: 1
 */
MathError abacus_divmod_geometric(const CrystallineAbacus* a,
                                  const CrystallineAbacus* b,
                                  CrystallineAbacus* quotient,
                                  CrystallineAbacus* remainder);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Round fractional result to specified precision
 * @param value Input value
 * @param result Output rounded value
 * @param precision Number of decimal places
 * @return MATH_SUCCESS or error code
 * 
 * Rounds a fractional abacus value to the specified number of
 * decimal places using geometric rounding.
 */
MathError abacus_round_geometric(const CrystallineAbacus* value,
                                CrystallineAbacus* result,
                                int32_t precision);

/**
 * @brief Get fractional part of abacus number
 * @param value Input value
 * @param fractional Output fractional part
 * @return MATH_SUCCESS or error code
 * 
 * Extracts the fractional part of a number.
 * Example: 3.75 → 0.75
 */
MathError abacus_get_fractional_part(const CrystallineAbacus* value,
                                    CrystallineAbacus* fractional);

/**
 * @brief Get integer part of abacus number
 * @param value Input value
 * @param integer Output integer part
 * @return MATH_SUCCESS or error code
 * 
 * Extracts the integer part of a number.
 * Example: 3.75 → 3
 */
MathError abacus_get_integer_part(const CrystallineAbacus* value,
                                 CrystallineAbacus* integer);

/**
 * @brief Convert fractional abacus to double with precision
 * @param abacus Input abacus number
 * @param value Output double value
 * @param precision Number of decimal places
 * @return MATH_SUCCESS or error code
 * 
 * Converts an abacus number to double with specified precision.
 */
MathError abacus_to_double_precise(const CrystallineAbacus* abacus,
                                  double* value,
                                  int32_t precision);

#ifdef __cplusplus
}
#endif

#endif // MATH_ABACUS_GEOMETRIC_FRACTIONAL_H