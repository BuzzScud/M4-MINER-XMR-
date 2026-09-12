/**
 * @file abacus_geometric.h
 * @brief Geometric Operations for CrystallineAbacus using Babylonian Pattern
 * 
 * This module implements geometric arithmetic operations using the revolutionary
 * 6-step Babylonian pattern with the 3D clock triangle structure.
 * 
 * The 6-Step Pattern:
 * 1. MAP: Number → Clock Position → 3D Point
 * 2. FOLD: 3D Point → First Quadrant (Q1)
 * 3. OPERATE: Geometric transformation in Q1
 * 4. TRACK: Polarity oscillation
 * 5. UNFOLD: Q1 → Original Quadrant
 * 6. MAP BACK: 3D Point → Clock Position → Number
 * 
 * All operations are O(1) complexity.
 */

#ifndef MATH_ABACUS_GEOMETRIC_H
#define MATH_ABACUS_GEOMETRIC_H

#include "types.h"
#include "abacus.h"
#include "clock.h"
#include "clock_triangle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * BRIDGE FUNCTIONS (Abacus ↔ Clock ↔ 3D)
 * ============================================================================
 */

/**
 * @brief Convert abacus number to clock position
 * @param abacus Input abacus number
 * @param pos Output clock position
 * @return MATH_SUCCESS or error code
 * 
 * Converts an arbitrary precision abacus number to a clock position
 * on the Babylonian lattice. This is step 1 of the 6-step pattern.
 */
MathError abacus_to_clock_position(const CrystallineAbacus* abacus,
                                   ClockPosition* pos);

/**
 * @brief Convert clock position to abacus number
 * @param pos Input clock position
 * @param abacus Output abacus number
 * @return MATH_SUCCESS or error code
 * 
 * Converts a clock position back to an arbitrary precision abacus number.
 * This is step 6 of the 6-step pattern.
 */
MathError clock_position_to_abacus(const ClockPosition* pos,
                                   CrystallineAbacus* abacus);

/**
 * @brief Convert clock position to 3D point on triangle
 * @param pos Input clock position
 * @param triangle The clock triangle structure
 * @param point Output 3D point
 * @return MATH_SUCCESS or error code
 * 
 * Maps a clock position to a 3D point on the clock triangle.
 * Part of step 1 of the 6-step pattern.
 */
MathError clock_position_to_3d_point(const ClockPosition* pos,
                                     const ClockTriangle3D* triangle,
                                     ClockPoint3D* point);

/**
 * @brief Convert 3D point to clock position
 * @param point Input 3D point
 * @param triangle The clock triangle structure
 * @param pos Output clock position
 * @return MATH_SUCCESS or error code
 * 
 * Maps a 3D point back to a clock position.
 * Part of step 6 of the 6-step pattern.
 */
MathError point_3d_to_clock_position(const ClockPoint3D* point,
                                     const ClockTriangle3D* triangle,
                                     ClockPosition* pos);

/* ============================================================================
 * GEOMETRIC OPERATIONS
 * ============================================================================
 */

/**
 * @brief Add two abacus numbers using geometric operations
 * @param a First operand
 * @param b Second operand
 * @param result Output (a + b)
 * @return MATH_SUCCESS or error code
 * 
 * Performs addition using the 6-step Babylonian pattern:
 * 1. MAP both operands to 3D points
 * 2. FOLD both points to Q1
 * 3. OPERATE: Vector addition in Q1
 * 4. TRACK polarity changes
 * 5. UNFOLD result to appropriate quadrant
 * 6. MAP BACK to abacus number
 * 
 * Complexity: O(1) for the geometric operations
 * (Note: Abacus conversion may be O(n) where n is number of beads)
 */
MathError abacus_add_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result);

/**
 * @brief Multiply two abacus numbers using geometric operations
 * @param a First operand
 * @param b Second operand
 * @param result Output (a × b)
 * @return MATH_SUCCESS or error code
 * 
 * Performs multiplication using the 6-step Babylonian pattern:
 * 1. MAP both operands to 3D points
 * 2. FOLD both points to Q1
 * 3. OPERATE: Geometric multiplication in Q1
 *    - Convert to polar coordinates
 *    - Multiply radii, add angles
 *    - Convert back to Cartesian
 * 4. TRACK polarity changes (multiply polarities)
 * 5. UNFOLD result to appropriate quadrant
 * 6. MAP BACK to abacus number
 * 
 * Complexity: O(1) for the geometric operations
 * (Note: Abacus conversion may be O(n) where n is number of beads)
 */
MathError abacus_mul_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result);

/**
 * @brief Subtract two abacus numbers using geometric operations
 * @param a First operand
 * @param b Second operand
 * @param result Output (a - b)
 * @return MATH_SUCCESS or error code
 * 
 * Performs subtraction using the 6-step Babylonian pattern.
 * Subtraction is addition with negated second operand.
 */
MathError abacus_sub_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result);

/**
 * @brief Divide two abacus numbers using geometric operations
 * @param a Numerator
 * @param b Denominator
 * @param result Output (a / b)
 * @return MATH_SUCCESS or error code
 * 
 * Performs division using the 6-step Babylonian pattern.
 * Division is multiplication with reciprocal of second operand.
 */
MathError abacus_div_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Initialize a clock triangle for geometric operations
 * @param triangle Triangle structure to initialize
 * @return MATH_SUCCESS or error code
 * 
 * Creates a standard clock triangle with radius=1.0 and height=1.0
 * for use in geometric operations.
 */
MathError abacus_geometric_init_triangle(ClockTriangle3D* triangle);

/**
 * @brief Compare geometric operation result with standard operation
 * @param a First operand
 * @param b Second operand
 * @param geometric_result Result from geometric operation
 * @param standard_result Result from standard operation
 * @return true if results match within tolerance, false otherwise
 * 
 * Used for validation and testing.
 */
bool abacus_geometric_compare_results(const CrystallineAbacus* geometric_result,
                                      const CrystallineAbacus* standard_result);

#ifdef __cplusplus
}
#endif

#endif // MATH_ABACUS_GEOMETRIC_H