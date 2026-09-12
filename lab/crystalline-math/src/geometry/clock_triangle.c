/**
 * @file clock_triangle.c
 * @brief Implementation of 3D Clock Triangle for Babylonian Arithmetic
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "math/clock_triangle.h"
#include "math/constants.h"
#include "math/transcendental.h"
#include "math/arithmetic.h"

// Wrapper functions to use our own transcendental implementations
#define cos(x) math_cos(x)
#define sin(x) math_sin(x)
#define atan2(y,x) math_atan2(y,x)
#define sqrt(x) math_sqrt(x)
#define fabs(x) math_abs(x)

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Multiply a 3x3 matrix by a 3D vector
 * @param matrix 3x3 matrix (row-major order)
 * @param vec Input vector [x, y, z]
 * @param result Output vector [x', y', z']
 */
static void matrix_vector_multiply(const double matrix[9], 
                                   const double vec[3],
                                   double result[3]) {
    result[0] = matrix[0] * vec[0] + matrix[1] * vec[1] + matrix[2] * vec[2];
    result[1] = matrix[3] * vec[0] + matrix[4] * vec[1] + matrix[5] * vec[2];
    result[2] = matrix[6] * vec[0] + matrix[7] * vec[1] + matrix[8] * vec[2];
}

/**
 * @brief Create a rotation matrix for rotating around the z-axis
 * @param angle Rotation angle in radians
 * @param matrix Output 3x3 matrix (row-major order)
 */
static void create_z_rotation_matrix(double angle, double matrix[9]) {
    double cos_a = cos(angle);
    double sin_a = sin(angle);
    
    // Row 1
    matrix[0] = cos_a;
    matrix[1] = -sin_a;
    matrix[2] = 0.0;
    
    // Row 2
    matrix[3] = sin_a;
    matrix[4] = cos_a;
    matrix[5] = 0.0;
    
    // Row 3
    matrix[6] = 0.0;
    matrix[7] = 0.0;
    matrix[8] = 1.0;
}

/**
 * @brief Transpose a 3x3 matrix (for inverse rotation)
 * @param matrix Input matrix
 * @param result Output transposed matrix
 */
static void transpose_matrix(const double matrix[9], double result[9]) {
    result[0] = matrix[0];
    result[1] = matrix[3];
    result[2] = matrix[6];
    
    result[3] = matrix[1];
    result[4] = matrix[4];
    result[5] = matrix[7];
    
    result[6] = matrix[2];
    result[7] = matrix[5];
    result[8] = matrix[8];
}

/* ============================================================================
 * INITIALIZATION AND CLEANUP
 * ============================================================================
 */

MathError clock_triangle_init(ClockTriangle3D* triangle, 
                              double radius, 
                              double height) {
    if (!triangle) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    if (radius <= 0.0 || height <= 0.0) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Initialize vertices
    triangle->center[0] = 0.0;
    triangle->center[1] = 0.0;
    triangle->center[2] = 0.0;
    
    triangle->vertex_12[0] = 0.0;
    triangle->vertex_12[1] = radius;
    triangle->vertex_12[2] = height;
    
    triangle->vertex_3[0] = radius;
    triangle->vertex_3[1] = 0.0;
    triangle->vertex_3[2] = height;
    
    // Store geometric properties
    triangle->radius = radius;
    triangle->height = height;
    
    // Compute π gap
    triangle->pi_gap = clock_triangle_compute_pi_gap(radius);
    
    // Create rotation matrices for quadrant folding
    // Q1 → Q2: Rotate 90° counterclockwise
    create_z_rotation_matrix(MATH_PI / 2.0, triangle->rotation_q1_to_q2);
    
    // Q1 → Q3: Rotate 180°
    create_z_rotation_matrix(MATH_PI, triangle->rotation_q1_to_q3);
    
    // Q1 → Q4: Rotate 270° (or -90°)
    create_z_rotation_matrix(3.0 * MATH_PI / 2.0, triangle->rotation_q1_to_q4);
    
    // Create inverse rotations (transpose for orthogonal matrices)
    transpose_matrix(triangle->rotation_q1_to_q2, triangle->rotation_q2_to_q1);
    transpose_matrix(triangle->rotation_q1_to_q3, triangle->rotation_q3_to_q1);
    transpose_matrix(triangle->rotation_q1_to_q4, triangle->rotation_q4_to_q1);
    
    return MATH_SUCCESS;
}

void clock_triangle_free(ClockTriangle3D* triangle) {
    // No dynamic allocation, nothing to free
    // Provided for API consistency
    (void)triangle;
}

/* ============================================================================
 * POSITION MAPPING
 * ============================================================================
 */

MathError clock_triangle_map_to_3d(const ClockTriangle3D* triangle,
                                   uint8_t position,
                                   ClockPoint3D* point) {
    if (!triangle || !point) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    if (position > 11) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Convert position to angle (0 = 12 o'clock = 90°, clockwise)
    // Position 0 = 90° (12 o'clock)
    // Position 3 = 0° (3 o'clock)
    // Position 6 = 270° (6 o'clock)
    // Position 9 = 180° (9 o'clock)
    double angle = (90.0 - position * 30.0) * MATH_PI / 180.0;
    
    // Compute coordinates on the clock face
    point->coords[0] = triangle->radius * cos(angle);
    point->coords[1] = triangle->radius * sin(angle);
    point->coords[2] = triangle->height;
    
    // Store position
    point->position = position;
    
    // Determine quadrant
    point->quadrant = clock_triangle_get_quadrant(point);
    
    // Determine polarity
    point->polarity = clock_triangle_get_polarity(point);
    
    return MATH_SUCCESS;
}

MathError clock_triangle_map_from_3d(const ClockTriangle3D* triangle,
                                     const ClockPoint3D* point,
                                     uint8_t* position) {
    if (!triangle || !point || !position) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Compute angle from coordinates
    double angle = atan2(point->coords[1], point->coords[0]);
    
    // Convert to degrees
    double degrees = angle * 180.0 / MATH_PI;
    
    // Convert to clock position (0 = 12 o'clock = 90°)
    double pos_degrees = 90.0 - degrees;
    
    // Normalize to 0-360
    while (pos_degrees < 0.0) pos_degrees += 360.0;
    while (pos_degrees >= 360.0) pos_degrees -= 360.0;
    
    // Convert to position (0-11)
    *position = (uint8_t)((pos_degrees + 15.0) / 30.0) % 12;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * GEOMETRIC QUERIES
 * ============================================================================
 */

uint8_t clock_triangle_get_quadrant(const ClockPoint3D* point) {
    if (!point) return 0;
    
    double x = point->coords[0];
    double y = point->coords[1];
    
    if (x >= 0.0 && y >= 0.0) return 1;  // Q1
    if (x < 0.0 && y >= 0.0) return 2;   // Q2
    if (x < 0.0 && y < 0.0) return 3;    // Q3
    return 4;                             // Q4
}

int8_t clock_triangle_get_polarity(const ClockPoint3D* point) {
    if (!point) return 0;
    
    // Polarity is determined by z-coordinate
    return (point->coords[2] >= 0.0) ? 1 : -1;
}

double clock_triangle_distance(const ClockPoint3D* p1,
                               const ClockPoint3D* p2) {
    if (!p1 || !p2) return 0.0;
    
    double dx = p2->coords[0] - p1->coords[0];
    double dy = p2->coords[1] - p1->coords[1];
    double dz = p2->coords[2] - p1->coords[2];
    
    return sqrt(dx*dx + dy*dy + dz*dz);
}

double clock_triangle_compute_pi_gap(double radius) {
    // Distance from 12 o'clock to 3 o'clock (geometric)
    double geometric_distance = radius * sqrt(2.0);
    
    // Arc length from 12 o'clock to 3 o'clock (circular)
    double arc_length = MATH_PI * radius / 6.0;  // 30° = π/6 radians
    
    // The π gap
    return geometric_distance - arc_length;
}

/* ============================================================================
 * QUADRANT FOLDING AND UNFOLDING
 * ============================================================================
 */

MathError clock_triangle_fold_to_q1(const ClockTriangle3D* triangle,
                                    const ClockPoint3D* point,
                                    ClockPoint3D* result) {
    if (!triangle || !point || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Copy the point
    *result = *point;
    
    // If already in Q1, nothing to do
    if (point->quadrant == 1) {
        return MATH_SUCCESS;
    }
    
    // Select appropriate rotation matrix
    const double* rotation_matrix = NULL;
    switch (point->quadrant) {
        case 2:
            rotation_matrix = triangle->rotation_q2_to_q1;
            break;
        case 3:
            rotation_matrix = triangle->rotation_q3_to_q1;
            break;
        case 4:
            rotation_matrix = triangle->rotation_q4_to_q1;
            break;
        default:
            return MATH_ERROR_INVALID_ARG;
    }
    
    // Apply rotation
    matrix_vector_multiply(rotation_matrix, point->coords, result->coords);
    
    // Update quadrant (now in Q1)
    result->quadrant = 1;
    
    return MATH_SUCCESS;
}

MathError clock_triangle_unfold_from_q1(const ClockTriangle3D* triangle,
                                        const ClockPoint3D* point,
                                        uint8_t target_quadrant,
                                        ClockPoint3D* result) {
    if (!triangle || !point || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    if (target_quadrant < 1 || target_quadrant > 4) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Copy the point
    *result = *point;
    
    // If target is Q1, nothing to do
    if (target_quadrant == 1) {
        return MATH_SUCCESS;
    }
    
    // Select appropriate rotation matrix
    const double* rotation_matrix = NULL;
    switch (target_quadrant) {
        case 2:
            rotation_matrix = triangle->rotation_q1_to_q2;
            break;
        case 3:
            rotation_matrix = triangle->rotation_q1_to_q3;
            break;
        case 4:
            rotation_matrix = triangle->rotation_q1_to_q4;
            break;
    }
    
    // Apply rotation
    matrix_vector_multiply(rotation_matrix, point->coords, result->coords);
    
    // Update quadrant
    result->quadrant = target_quadrant;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * GEOMETRIC OPERATIONS
 * ============================================================================
 */

MathError clock_triangle_add(const ClockTriangle3D* triangle,
                             const ClockPoint3D* p1,
                             const ClockPoint3D* p2,
                             ClockPoint3D* result) {
    if (!triangle || !p1 || !p2 || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Step 2: FOLD both points to Q1
    ClockPoint3D p1_q1, p2_q1;
    MathError err;
    
    err = clock_triangle_fold_to_q1(triangle, p1, &p1_q1);
    if (err != MATH_SUCCESS) return err;
    
    err = clock_triangle_fold_to_q1(triangle, p2, &p2_q1);
    if (err != MATH_SUCCESS) return err;
    
    // Step 3: OPERATE - Vector addition in Q1
    result->coords[0] = p1_q1.coords[0] + p2_q1.coords[0];
    result->coords[1] = p1_q1.coords[1] + p2_q1.coords[1];
    result->coords[2] = p1_q1.coords[2] + p2_q1.coords[2];
    
    // Step 4: TRACK polarity
    result->polarity = clock_triangle_get_polarity(result);
    
    // Determine target quadrant for result
    result->quadrant = clock_triangle_get_quadrant(result);
    
    // Step 5: UNFOLD to appropriate quadrant (if needed)
    if (result->quadrant != 1) {
        ClockPoint3D temp = *result;
        err = clock_triangle_unfold_from_q1(triangle, &temp, result->quadrant, result);
        if (err != MATH_SUCCESS) return err;
    }
    
    // Step 6: MAP BACK to clock position
    err = clock_triangle_map_from_3d(triangle, result, &result->position);
    if (err != MATH_SUCCESS) return err;
    
    return MATH_SUCCESS;
}

MathError clock_triangle_multiply(const ClockTriangle3D* triangle,
                                  const ClockPoint3D* p1,
                                  const ClockPoint3D* p2,
                                  ClockPoint3D* result) {
    if (!triangle || !p1 || !p2 || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Step 2: FOLD both points to Q1
    ClockPoint3D p1_q1, p2_q1;
    MathError err;
    
    err = clock_triangle_fold_to_q1(triangle, p1, &p1_q1);
    if (err != MATH_SUCCESS) return err;
    
    err = clock_triangle_fold_to_q1(triangle, p2, &p2_q1);
    if (err != MATH_SUCCESS) return err;
    
    // Step 3: OPERATE - Geometric multiplication in Q1
    // For geometric multiplication, we use the distance from origin
    // and add the angles
    double r1 = sqrt(p1_q1.coords[0]*p1_q1.coords[0] + 
                     p1_q1.coords[1]*p1_q1.coords[1]);
    double r2 = sqrt(p2_q1.coords[0]*p2_q1.coords[0] + 
                     p2_q1.coords[1]*p2_q1.coords[1]);
    
    double angle1 = atan2(p1_q1.coords[1], p1_q1.coords[0]);
    double angle2 = atan2(p2_q1.coords[1], p2_q1.coords[0]);
    
    // Result: multiply radii, add angles
    double r_result = r1 * r2 / triangle->radius;  // Normalize by radius
    double angle_result = angle1 + angle2;
    
    result->coords[0] = r_result * cos(angle_result);
    result->coords[1] = r_result * sin(angle_result);
    result->coords[2] = p1_q1.coords[2] * p2_q1.coords[2] / triangle->height;
    
    // Step 4: TRACK polarity
    result->polarity = p1->polarity * p2->polarity;
    
    // Determine target quadrant for result
    result->quadrant = clock_triangle_get_quadrant(result);
    
    // Step 5: UNFOLD to appropriate quadrant (if needed)
    if (result->quadrant != 1) {
        ClockPoint3D temp = *result;
        err = clock_triangle_unfold_from_q1(triangle, &temp, result->quadrant, result);
        if (err != MATH_SUCCESS) return err;
    }
    
    // Step 6: MAP BACK to clock position
    err = clock_triangle_map_from_3d(triangle, result, &result->position);
    if (err != MATH_SUCCESS) return err;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

void clock_triangle_print_point(const ClockPoint3D* point) {
    if (!point) {
        printf("NULL point\n");
        return;
    }
    
    printf("ClockPoint3D:\n");
    printf("  Position: %u\n", point->position);
    printf("  Quadrant: %u\n", point->quadrant);
    printf("  Polarity: %d\n", point->polarity);
    printf("  Coords: (%.6f, %.6f, %.6f)\n", 
           point->coords[0], point->coords[1], point->coords[2]);
}

void clock_triangle_print(const ClockTriangle3D* triangle) {
    if (!triangle) {
        printf("NULL triangle\n");
        return;
    }
    
    printf("ClockTriangle3D:\n");
    printf("  Radius: %.6f\n", triangle->radius);
    printf("  Height: %.6f\n", triangle->height);
    printf("  π Gap: %.6f\n", triangle->pi_gap);
    printf("  Center: (%.6f, %.6f, %.6f)\n",
           triangle->center[0], triangle->center[1], triangle->center[2]);
    printf("  Vertex 12: (%.6f, %.6f, %.6f)\n",
           triangle->vertex_12[0], triangle->vertex_12[1], triangle->vertex_12[2]);
    printf("  Vertex 3: (%.6f, %.6f, %.6f)\n",
           triangle->vertex_3[0], triangle->vertex_3[1], triangle->vertex_3[2]);
}

bool clock_triangle_validate(const ClockTriangle3D* triangle) {
    if (!triangle) return false;
    
    // Check radius and height are positive
    if (triangle->radius <= 0.0 || triangle->height <= 0.0) {
        return false;
    }
    
    // Check center is at origin
    if (triangle->center[0] != 0.0 || 
        triangle->center[1] != 0.0 || 
        triangle->center[2] != 0.0) {
        return false;
    }
    
    // Check vertex_12 is at (0, r, h)
    if (triangle->vertex_12[0] != 0.0 ||
        fabs(triangle->vertex_12[1] - triangle->radius) > 1e-10 ||
        fabs(triangle->vertex_12[2] - triangle->height) > 1e-10) {
        return false;
    }
    
    // Check vertex_3 is at (r, 0, h)
    if (fabs(triangle->vertex_3[0] - triangle->radius) > 1e-10 ||
        triangle->vertex_3[1] != 0.0 ||
        fabs(triangle->vertex_3[2] - triangle->height) > 1e-10) {
        return false;
    }
    
    // Check π gap is correct
    double expected_gap = clock_triangle_compute_pi_gap(triangle->radius);
    if (fabs(triangle->pi_gap - expected_gap) > 1e-10) {
        return false;
    }
    
    return true;
}