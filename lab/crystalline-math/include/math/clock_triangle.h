/**
 * @file clock_triangle.h
 * @brief 3D Clock Triangle Structure for Babylonian Arithmetic
 * 
 * This module implements the revolutionary 3D clock triangle structure
 * that enables O(1) geometric operations for Babylonian arithmetic.
 * 
 * The clock triangle has three vertices:
 * - Center: (0, 0, 0)
 * - 12 o'clock: (0, r, h)
 * - 3 o'clock: (r, 0, h)
 * 
 * Where r is the radius and h is the height above the origin.
 * 
 * Key Features:
 * - O(1) position mapping
 * - Polarity tracking via z-coordinate
 * - Quadrant folding via 3D rotations
 * - π gap calculation for geometric operations
 * 
 * This is the foundation for the 6-step Babylonian pattern:
 * 1. MAP: Number → Clock Position → 3D Point
 * 2. FOLD: 3D Point → First Quadrant (Q1)
 * 3. OPERATE: Geometric transformation in Q1
 * 4. TRACK: Polarity changes during operation
 * 5. UNFOLD: Q1 → Original Quadrant
 * 6. MAP BACK: 3D Point → Clock Position → Number
 */

#ifndef MATH_CLOCK_TRIANGLE_H
#define MATH_CLOCK_TRIANGLE_H

#include "types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================
 */

/**
 * @brief 3D point on the clock triangle
 */
typedef struct {
    double coords[3];      /**< (x, y, z) coordinates */
    uint8_t quadrant;      /**< Which quadrant (1-4) */
    int8_t polarity;       /**< +1 or -1 */
    uint8_t position;      /**< Clock position (0-11) */
} ClockPoint3D;

/**
 * @brief 3D clock triangle structure
 */
typedef struct {
    // The three vertices
    double center[3];      /**< (0, 0, 0) */
    double vertex_12[3];   /**< (0, r, h) - 12 o'clock position */
    double vertex_3[3];    /**< (r, 0, h) - 3 o'clock position */
    
    // Geometric properties
    double radius;         /**< Radius of clock face */
    double height;         /**< Height above origin */
    double pi_gap;         /**< The π gap value */
    
    // Transformation matrices for quadrant rotations (3x3 matrices)
    double rotation_q1_to_q2[9];  /**< Q1 → Q2 rotation */
    double rotation_q1_to_q3[9];  /**< Q1 → Q3 rotation */
    double rotation_q1_to_q4[9];  /**< Q1 → Q4 rotation */
    
    // Inverse rotations (for unfolding)
    double rotation_q2_to_q1[9];  /**< Q2 → Q1 rotation */
    double rotation_q3_to_q1[9];  /**< Q3 → Q1 rotation */
    double rotation_q4_to_q1[9];  /**< Q4 → Q1 rotation */
} ClockTriangle3D;

/* ============================================================================
 * INITIALIZATION AND CLEANUP
 * ============================================================================
 */

/**
 * @brief Initialize a 3D clock triangle
 * @param triangle Triangle structure to initialize
 * @param radius Radius of the clock face
 * @param height Height above the origin
 * @return MATH_SUCCESS or error code
 * 
 * Creates the triangle with specified radius and height, computes the π gap,
 * and sets up all rotation matrices for quadrant folding/unfolding.
 * 
 * Typical values:
 * - radius = 1.0 (unit circle)
 * - height = 1.0 (creates equilateral triangle in 3D)
 */
MathError clock_triangle_init(ClockTriangle3D* triangle, 
                              double radius, 
                              double height);

/**
 * @brief Free resources associated with a clock triangle
 * @param triangle Triangle to free
 * 
 * Currently no dynamic allocation, but provided for API consistency.
 */
void clock_triangle_free(ClockTriangle3D* triangle);

/* ============================================================================
 * POSITION MAPPING
 * ============================================================================
 */

/**
 * @brief Map a clock position to a 3D point
 * @param triangle The clock triangle
 * @param pos Clock position (0-11)
 * @param point Output 3D point
 * @return MATH_SUCCESS or error code
 * 
 * Maps a clock position (0-11) to a 3D point on the triangle.
 * The point includes coordinates, quadrant, polarity, and position.
 * 
 * Position mapping:
 * - 0: 12 o'clock (top)
 * - 3: 3 o'clock (right)
 * - 6: 6 o'clock (bottom)
 * - 9: 9 o'clock (left)
 */
MathError clock_triangle_map_to_3d(const ClockTriangle3D* triangle,
                                   uint8_t position,
                                   ClockPoint3D* point);

/**
 * @brief Map a 3D point back to a clock position
 * @param triangle The clock triangle
 * @param point 3D point on the triangle
 * @param position Output clock position (0-11)
 * @return MATH_SUCCESS or error code
 * 
 * Inverse of clock_triangle_map_to_3d.
 * Given a 3D point, determines its clock position.
 */
MathError clock_triangle_map_from_3d(const ClockTriangle3D* triangle,
                                     const ClockPoint3D* point,
                                     uint8_t* position);

/* ============================================================================
 * GEOMETRIC QUERIES
 * ============================================================================
 */

/**
 * @brief Determine which quadrant a point is in
 * @param point 3D point
 * @return Quadrant number (1-4)
 * 
 * Quadrants:
 * - Q1: x ≥ 0, y ≥ 0 (positions 0-2)
 * - Q2: x < 0, y ≥ 0 (positions 3-5)
 * - Q3: x < 0, y < 0 (positions 6-8)
 * - Q4: x ≥ 0, y < 0 (positions 9-11)
 */
uint8_t clock_triangle_get_quadrant(const ClockPoint3D* point);

/**
 * @brief Determine the polarity of a point
 * @param point 3D point
 * @return +1 or -1
 * 
 * Polarity is determined by the z-coordinate and position:
 * - Positive polarity: z ≥ 0
 * - Negative polarity: z < 0
 * 
 * Polarity tracks sign changes during operations.
 */
int8_t clock_triangle_get_polarity(const ClockPoint3D* point);

/**
 * @brief Compute Euclidean distance between two points
 * @param p1 First point
 * @param p2 Second point
 * @return Distance
 * 
 * Computes the 3D Euclidean distance:
 * d = √((x₂-x₁)² + (y₂-y₁)² + (z₂-z₁)²)
 */
double clock_triangle_distance(const ClockPoint3D* p1,
                               const ClockPoint3D* p2);

/**
 * @brief Compute the π gap for a given radius
 * @param radius Radius of the clock face
 * @return The π gap value
 * 
 * The π gap is the difference between geometric and circular distance
 * from 12 o'clock to 3 o'clock:
 * 
 * gap = r√2 - πr/6
 * 
 * This gap is fundamental to Babylonian arithmetic operations.
 */
double clock_triangle_compute_pi_gap(double radius);

/* ============================================================================
 * QUADRANT FOLDING AND UNFOLDING
 * ============================================================================
 */

/**
 * @brief Fold a point from any quadrant to Q1
 * @param triangle The clock triangle
 * @param point Input point (any quadrant)
 * @param result Output point (in Q1)
 * @return MATH_SUCCESS or error code
 * 
 * This is step 2 of the 6-step Babylonian pattern.
 * Rotates the point to the first quadrant using the appropriate
 * rotation matrix.
 * 
 * The original quadrant is preserved in the result for unfolding.
 */
MathError clock_triangle_fold_to_q1(const ClockTriangle3D* triangle,
                                    const ClockPoint3D* point,
                                    ClockPoint3D* result);

/**
 * @brief Unfold a point from Q1 back to its original quadrant
 * @param triangle The clock triangle
 * @param point Input point (in Q1)
 * @param target_quadrant Target quadrant (1-4)
 * @param result Output point (in target quadrant)
 * @return MATH_SUCCESS or error code
 * 
 * This is step 5 of the 6-step Babylonian pattern.
 * Rotates the point from Q1 back to the specified quadrant using
 * the inverse rotation matrix.
 */
MathError clock_triangle_unfold_from_q1(const ClockTriangle3D* triangle,
                                        const ClockPoint3D* point,
                                        uint8_t target_quadrant,
                                        ClockPoint3D* result);

/* ============================================================================
 * GEOMETRIC OPERATIONS
 * ============================================================================
 */

/**
 * @brief Add two points geometrically
 * @param triangle The clock triangle
 * @param p1 First point
 * @param p2 Second point
 * @param result Output point
 * @return MATH_SUCCESS or error code
 * 
 * Performs geometric addition using the 6-step pattern:
 * 1. MAP both points to 3D
 * 2. FOLD both to Q1
 * 3. OPERATE: Vector addition in Q1
 * 4. TRACK polarity changes
 * 5. UNFOLD result to appropriate quadrant
 * 6. MAP BACK to clock position
 * 
 * Complexity: O(1)
 */
MathError clock_triangle_add(const ClockTriangle3D* triangle,
                             const ClockPoint3D* p1,
                             const ClockPoint3D* p2,
                             ClockPoint3D* result);

/**
 * @brief Multiply two points geometrically
 * @param triangle The clock triangle
 * @param p1 First point
 * @param p2 Second point
 * @param result Output point
 * @return MATH_SUCCESS or error code
 * 
 * Performs geometric multiplication using the 6-step pattern:
 * 1. MAP both points to 3D
 * 2. FOLD both to Q1
 * 3. OPERATE: Geometric product in Q1
 * 4. TRACK polarity changes
 * 5. UNFOLD result to appropriate quadrant
 * 6. MAP BACK to clock position
 * 
 * Complexity: O(1)
 */
MathError clock_triangle_multiply(const ClockTriangle3D* triangle,
                                  const ClockPoint3D* p1,
                                  const ClockPoint3D* p2,
                                  ClockPoint3D* result);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Print a 3D point (for debugging)
 * @param point Point to print
 */
void clock_triangle_print_point(const ClockPoint3D* point);

/**
 * @brief Print the clock triangle structure (for debugging)
 * @param triangle Triangle to print
 */
void clock_triangle_print(const ClockTriangle3D* triangle);

/**
 * @brief Validate a clock triangle structure
 * @param triangle Triangle to validate
 * @return true if valid, false otherwise
 * 
 * Checks:
 * - Vertices are correct
 * - Radius and height are positive
 * - π gap is correct
 * - Rotation matrices are orthogonal
 */
bool clock_triangle_validate(const ClockTriangle3D* triangle);

#ifdef __cplusplus
}
#endif

#endif // MATH_CLOCK_TRIANGLE_H