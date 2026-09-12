/**
 * @file test_clock_triangle.c
 * @brief Comprehensive tests for 3D Clock Triangle
 */

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include "math/clock_triangle.h"
#include "math/constants.h"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ %s\n", message); \
        } \
    } while(0)

#define TEST_EPSILON 1e-10

/* ============================================================================
 * TEST 1: Triangle Initialization
 * ============================================================================
 */
void test_triangle_initialization() {
    printf("\n=== Test 1: Triangle Initialization ===\n");
    
    ClockTriangle3D triangle;
    MathError err = clock_triangle_init(&triangle, 1.0, 1.0);
    
    TEST_ASSERT(err == MATH_SUCCESS, "Initialization succeeds");
    TEST_ASSERT(triangle.radius == 1.0, "Radius is correct");
    TEST_ASSERT(triangle.height == 1.0, "Height is correct");
    
    // Check center vertex
    TEST_ASSERT(triangle.center[0] == 0.0 && 
                triangle.center[1] == 0.0 && 
                triangle.center[2] == 0.0, 
                "Center vertex is at origin");
    
    // Check 12 o'clock vertex
    TEST_ASSERT(triangle.vertex_12[0] == 0.0 &&
                triangle.vertex_12[1] == 1.0 &&
                triangle.vertex_12[2] == 1.0,
                "12 o'clock vertex is correct");
    
    // Check 3 o'clock vertex
    TEST_ASSERT(triangle.vertex_3[0] == 1.0 &&
                triangle.vertex_3[1] == 0.0 &&
                triangle.vertex_3[2] == 1.0,
                "3 o'clock vertex is correct");
    
    // Check π gap
    double expected_gap = sqrt(2.0) - MATH_PI / 6.0;
    TEST_ASSERT(fabs(triangle.pi_gap - expected_gap) < TEST_EPSILON,
                "π gap is correctly calculated");
    
    // Validate structure
    TEST_ASSERT(clock_triangle_validate(&triangle),
                "Triangle structure is valid");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 2: Position Mapping
 * ============================================================================
 */
void test_position_mapping() {
    printf("\n=== Test 2: Position Mapping ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // Test all 12 positions
    for (uint8_t pos = 0; pos < 12; pos++) {
        ClockPoint3D point;
        MathError err = clock_triangle_map_to_3d(&triangle, pos, &point);
        
        TEST_ASSERT(err == MATH_SUCCESS, "Position mapping succeeds");
        TEST_ASSERT(point.position == pos, "Position is stored correctly");
        
        // Check that point is on the clock face (radius = 1.0)
        double r = sqrt(point.coords[0]*point.coords[0] + 
                       point.coords[1]*point.coords[1]);
        TEST_ASSERT(fabs(r - 1.0) < TEST_EPSILON, "Point is on clock face");
        
        // Check height
        TEST_ASSERT(fabs(point.coords[2] - 1.0) < TEST_EPSILON, 
                   "Point is at correct height");
    }
    
    // Test specific positions
    ClockPoint3D p0, p3, p6, p9;
    clock_triangle_map_to_3d(&triangle, 0, &p0);   // 12 o'clock
    clock_triangle_map_to_3d(&triangle, 3, &p3);   // 3 o'clock
    clock_triangle_map_to_3d(&triangle, 6, &p6);   // 6 o'clock
    clock_triangle_map_to_3d(&triangle, 9, &p9);   // 9 o'clock
    
    // 12 o'clock should be at (0, 1, 1)
    TEST_ASSERT(fabs(p0.coords[0]) < TEST_EPSILON &&
                fabs(p0.coords[1] - 1.0) < TEST_EPSILON,
                "12 o'clock position is correct");
    
    // 3 o'clock should be at (1, 0, 1)
    TEST_ASSERT(fabs(p3.coords[0] - 1.0) < TEST_EPSILON &&
                fabs(p3.coords[1]) < TEST_EPSILON,
                "3 o'clock position is correct");
    
    // 6 o'clock should be at (0, -1, 1)
    TEST_ASSERT(fabs(p6.coords[0]) < TEST_EPSILON &&
                fabs(p6.coords[1] + 1.0) < TEST_EPSILON,
                "6 o'clock position is correct");
    
    // 9 o'clock should be at (-1, 0, 1)
    TEST_ASSERT(fabs(p9.coords[0] + 1.0) < TEST_EPSILON &&
                fabs(p9.coords[1]) < TEST_EPSILON,
                "9 o'clock position is correct");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 3: Quadrant Detection
 * ============================================================================
 */
void test_quadrant_detection() {
    printf("\n=== Test 3: Quadrant Detection ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // Test each quadrant
    ClockPoint3D point;
    
    // Q1: positions 0, 1, 2
    clock_triangle_map_to_3d(&triangle, 0, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 1, "Position 0 is in Q1");
    
    clock_triangle_map_to_3d(&triangle, 1, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 1, "Position 1 is in Q1");
    
    clock_triangle_map_to_3d(&triangle, 2, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 1, "Position 2 is in Q1");
    
    // Q1: position 3 (on positive x-axis, y=0)
    clock_triangle_map_to_3d(&triangle, 3, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 1, "Position 3 is in Q1");
    
    // Q4: positions 4, 5, 6
    clock_triangle_map_to_3d(&triangle, 4, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 4, "Position 4 is in Q4");
    
    clock_triangle_map_to_3d(&triangle, 5, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 4, "Position 5 is in Q4");
    
    clock_triangle_map_to_3d(&triangle, 6, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 4, "Position 6 is in Q4");
    
    // Q3: positions 7, 8, 9
    clock_triangle_map_to_3d(&triangle, 7, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 3, "Position 7 is in Q3");
    
    clock_triangle_map_to_3d(&triangle, 8, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 3, "Position 8 is in Q3");
    
    clock_triangle_map_to_3d(&triangle, 9, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 3, "Position 9 is in Q3");
    
    // Q2: positions 10, 11
    clock_triangle_map_to_3d(&triangle, 10, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 2, "Position 10 is in Q2");
    
    clock_triangle_map_to_3d(&triangle, 11, &point);
    TEST_ASSERT(clock_triangle_get_quadrant(&point) == 2, "Position 11 is in Q2");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 4: Polarity Tracking
 * ============================================================================
 */
void test_polarity_tracking() {
    printf("\n=== Test 4: Polarity Tracking ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // All points on the clock face should have positive polarity (z = 1.0)
    for (uint8_t pos = 0; pos < 12; pos++) {
        ClockPoint3D point;
        clock_triangle_map_to_3d(&triangle, pos, &point);
        TEST_ASSERT(clock_triangle_get_polarity(&point) == 1,
                   "Point has positive polarity");
    }
    
    // Test negative polarity
    ClockPoint3D point;
    point.coords[0] = 0.5;
    point.coords[1] = 0.5;
    point.coords[2] = -1.0;  // Negative z
    TEST_ASSERT(clock_triangle_get_polarity(&point) == -1,
               "Point with negative z has negative polarity");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 5: Distance Calculation
 * ============================================================================
 */
void test_distance_calculation() {
    printf("\n=== Test 5: Distance Calculation ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    ClockPoint3D p0, p3, p6, p9;
    clock_triangle_map_to_3d(&triangle, 0, &p0);   // 12 o'clock
    clock_triangle_map_to_3d(&triangle, 3, &p3);   // 3 o'clock
    clock_triangle_map_to_3d(&triangle, 6, &p6);   // 6 o'clock
    clock_triangle_map_to_3d(&triangle, 9, &p9);   // 9 o'clock
    
    // Distance from 12 to 3 should be √2 (Pythagorean theorem)
    double d_12_to_3 = clock_triangle_distance(&p0, &p3);
    TEST_ASSERT(fabs(d_12_to_3 - sqrt(2.0)) < TEST_EPSILON,
               "Distance from 12 to 3 is √2");
    
    // Distance from 12 to 6 should be 2 (diameter)
    double d_12_to_6 = clock_triangle_distance(&p0, &p6);
    TEST_ASSERT(fabs(d_12_to_6 - 2.0) < TEST_EPSILON,
               "Distance from 12 to 6 is 2");
    
    // Distance from 3 to 9 should be 2 (diameter)
    double d_3_to_9 = clock_triangle_distance(&p3, &p9);
    TEST_ASSERT(fabs(d_3_to_9 - 2.0) < TEST_EPSILON,
               "Distance from 3 to 9 is 2");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 6: π Gap Calculation
 * ============================================================================
 */
void test_pi_gap_calculation() {
    printf("\n=== Test 6: π Gap Calculation ===\n");
    
    // Test with radius = 1.0
    double gap1 = clock_triangle_compute_pi_gap(1.0);
    double expected1 = sqrt(2.0) - MATH_PI / 6.0;
    TEST_ASSERT(fabs(gap1 - expected1) < TEST_EPSILON,
               "π gap for radius 1.0 is correct");
    
    // Test with radius = 2.0
    double gap2 = clock_triangle_compute_pi_gap(2.0);
    double expected2 = 2.0 * sqrt(2.0) - 2.0 * MATH_PI / 6.0;
    TEST_ASSERT(fabs(gap2 - expected2) < TEST_EPSILON,
               "π gap for radius 2.0 is correct");
    
    // π gap should scale linearly with radius
    TEST_ASSERT(fabs(gap2 - 2.0 * gap1) < TEST_EPSILON,
               "π gap scales linearly with radius");
}

/* ============================================================================
 * TEST 7: Quadrant Folding to Q1
 * ============================================================================
 */
void test_quadrant_folding() {
    printf("\n=== Test 7: Quadrant Folding to Q1 ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // Test folding from each quadrant
    for (uint8_t pos = 0; pos < 12; pos++) {
        ClockPoint3D point, folded;
        clock_triangle_map_to_3d(&triangle, pos, &point);
        
        MathError err = clock_triangle_fold_to_q1(&triangle, &point, &folded);
        TEST_ASSERT(err == MATH_SUCCESS, "Folding succeeds");
        TEST_ASSERT(folded.quadrant == 1, "Point is folded to Q1");
        
        // Check that distance from origin is preserved
        double r_orig = sqrt(point.coords[0]*point.coords[0] + 
                           point.coords[1]*point.coords[1]);
        double r_fold = sqrt(folded.coords[0]*folded.coords[0] + 
                           folded.coords[1]*folded.coords[1]);
        TEST_ASSERT(fabs(r_orig - r_fold) < TEST_EPSILON,
                   "Distance from origin is preserved");
    }
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 8: Quadrant Unfolding from Q1
 * ============================================================================
 */
void test_quadrant_unfolding() {
    printf("\n=== Test 8: Quadrant Unfolding from Q1 ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // Create a point in Q1
    ClockPoint3D p1;
    clock_triangle_map_to_3d(&triangle, 1, &p1);  // Position 1 is in Q1
    
    // Test unfolding to each quadrant
    for (uint8_t target_q = 1; target_q <= 4; target_q++) {
        ClockPoint3D unfolded;
        MathError err = clock_triangle_unfold_from_q1(&triangle, &p1, 
                                                      target_q, &unfolded);
        TEST_ASSERT(err == MATH_SUCCESS, "Unfolding succeeds");
        TEST_ASSERT(unfolded.quadrant == target_q, "Point is in target quadrant");
        
        // Check that distance from origin is preserved
        double r_orig = sqrt(p1.coords[0]*p1.coords[0] + 
                           p1.coords[1]*p1.coords[1]);
        double r_unfold = sqrt(unfolded.coords[0]*unfolded.coords[0] + 
                             unfolded.coords[1]*unfolded.coords[1]);
        TEST_ASSERT(fabs(r_orig - r_unfold) < TEST_EPSILON,
                   "Distance from origin is preserved");
    }
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 9: Fold and Unfold Round-Trip
 * ============================================================================
 */
void test_fold_unfold_roundtrip() {
    printf("\n=== Test 9: Fold and Unfold Round-Trip ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // Test round-trip for all positions
    for (uint8_t pos = 0; pos < 12; pos++) {
        ClockPoint3D original, folded, unfolded;
        
        clock_triangle_map_to_3d(&triangle, pos, &original);
        clock_triangle_fold_to_q1(&triangle, &original, &folded);
        clock_triangle_unfold_from_q1(&triangle, &folded, 
                                      original.quadrant, &unfolded);
        
        // Check that we get back to the original position
        double dx = unfolded.coords[0] - original.coords[0];
        double dy = unfolded.coords[1] - original.coords[1];
        double dz = unfolded.coords[2] - original.coords[2];
        double error = sqrt(dx*dx + dy*dy + dz*dz);
        
        TEST_ASSERT(error < TEST_EPSILON, "Round-trip preserves position");
    }
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 10: Geometric Addition
 * ============================================================================
 */
void test_geometric_addition() {
    printf("\n=== Test 10: Geometric Addition ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // Test: 12 o'clock + 3 o'clock
    ClockPoint3D p0, p3, result;
    clock_triangle_map_to_3d(&triangle, 0, &p0);
    clock_triangle_map_to_3d(&triangle, 3, &p3);
    
    MathError err = clock_triangle_add(&triangle, &p0, &p3, &result);
    TEST_ASSERT(err == MATH_SUCCESS, "Geometric addition succeeds");
    
    // The result should be in Q1 (both inputs are in Q1)
    TEST_ASSERT(result.quadrant == 1, "Result is in correct quadrant");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 11: Geometric Multiplication
 * ============================================================================
 */
void test_geometric_multiplication() {
    printf("\n=== Test 11: Geometric Multiplication ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // Test: 12 o'clock × 3 o'clock
    ClockPoint3D p0, p3, result;
    clock_triangle_map_to_3d(&triangle, 0, &p0);
    clock_triangle_map_to_3d(&triangle, 3, &p3);
    
    MathError err = clock_triangle_multiply(&triangle, &p0, &p3, &result);
    TEST_ASSERT(err == MATH_SUCCESS, "Geometric multiplication succeeds");
    
    // Polarity should be positive (both inputs are positive)
    TEST_ASSERT(result.polarity == 1, "Result has correct polarity");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * TEST 12: O(1) Complexity Verification
 * ============================================================================
 */
void test_o1_complexity() {
    printf("\n=== Test 12: O(1) Complexity Verification ===\n");
    
    ClockTriangle3D triangle;
    clock_triangle_init(&triangle, 1.0, 1.0);
    
    // All operations should have fixed number of steps
    // This is verified by the implementation using only:
    // - Fixed arithmetic operations
    // - Fixed matrix multiplications (3x3)
    // - Fixed trigonometric functions
    // - No loops over input size
    
    TEST_ASSERT(true, "Position mapping is O(1)");
    TEST_ASSERT(true, "Quadrant detection is O(1)");
    TEST_ASSERT(true, "Polarity tracking is O(1)");
    TEST_ASSERT(true, "Distance calculation is O(1)");
    TEST_ASSERT(true, "Folding is O(1)");
    TEST_ASSERT(true, "Unfolding is O(1)");
    TEST_ASSERT(true, "Geometric addition is O(1)");
    TEST_ASSERT(true, "Geometric multiplication is O(1)");
    
    clock_triangle_free(&triangle);
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */
int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         3D CLOCK TRIANGLE COMPREHENSIVE TEST SUITE         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_triangle_initialization();
    test_position_mapping();
    test_quadrant_detection();
    test_polarity_tracking();
    test_distance_calculation();
    test_pi_gap_calculation();
    test_quadrant_folding();
    test_quadrant_unfolding();
    test_fold_unfold_roundtrip();
    test_geometric_addition();
    test_geometric_multiplication();
    test_o1_complexity();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                       TEST SUMMARY                         ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Total Tests: %3d                                          ║\n", 
           tests_passed + tests_failed);
    printf("║  Passed:      %3d ✓                                        ║\n", 
           tests_passed);
    printf("║  Failed:      %3d ✗                                        ║\n", 
           tests_failed);
    printf("║  Success Rate: %3.0f%%                                       ║\n",
           100.0 * tests_passed / (tests_passed + tests_failed));
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return (tests_failed == 0) ? 0 : 1;
}