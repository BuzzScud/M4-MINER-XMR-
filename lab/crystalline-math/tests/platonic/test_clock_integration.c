/**
 * @file test_clock_integration.c
 * @brief Test suite for clock lattice integration with Platonic generators
 */

#include "math/platonic_clock.h"
#include "math/platonic_generator.h"
#include "math/platonic_generator_abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Color codes for output
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_RESET "\033[0m"

// Test macros
#define TEST_START(name) \
    printf(COLOR_BLUE "TEST: %s" COLOR_RESET "\n", name); \
    tests_run++;

#define TEST_PASS() \
    printf(COLOR_GREEN "  ✓ PASS" COLOR_RESET "\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf(COLOR_RED "  ✗ FAIL: %s" COLOR_RESET "\n", msg); \
    tests_failed++;

#define ASSERT(condition, msg) \
    if (!(condition)) { \
        TEST_FAIL(msg); \
        return; \
    }

#define ASSERT_EQ(a, b, msg) \
    if ((a) != (b)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (expected %llu, got %llu)", msg, (unsigned long long)(b), (unsigned long long)(a)); \
        TEST_FAIL(buf); \
        return; \
    }

#define ASSERT_SUCCESS(err, msg) \
    if ((err) != MATH_SUCCESS) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (error code: %d)", msg, err); \
        TEST_FAIL(buf); \
        return; \
    }

/* ============================================================================
 * TEST: Vertex to Prime Mapping
 * ============================================================================
 */

void test_vertex_to_prime() {
    TEST_START("Vertex to Prime Mapping");
    
    // Test first few vertices
    ASSERT_EQ(platonic_vertex_to_prime(0), 2, "Vertex 0 should map to prime 2");
    ASSERT_EQ(platonic_vertex_to_prime(1), 3, "Vertex 1 should map to prime 3");
    ASSERT_EQ(platonic_vertex_to_prime(2), 5, "Vertex 2 should map to prime 5");
    ASSERT_EQ(platonic_vertex_to_prime(3), 7, "Vertex 3 should map to prime 7");
    ASSERT_EQ(platonic_vertex_to_prime(4), 11, "Vertex 4 should map to prime 11");
    
    TEST_PASS();
}

void test_prime_to_vertex() {
    TEST_START("Prime to Vertex Mapping");
    
    // Test first few primes
    ASSERT_EQ(platonic_prime_to_vertex(2), 0, "Prime 2 should map to vertex 0");
    ASSERT_EQ(platonic_prime_to_vertex(3), 1, "Prime 3 should map to vertex 1");
    ASSERT_EQ(platonic_prime_to_vertex(5), 2, "Prime 5 should map to vertex 2");
    ASSERT_EQ(platonic_prime_to_vertex(7), 3, "Prime 7 should map to vertex 3");
    ASSERT_EQ(platonic_prime_to_vertex(11), 4, "Prime 11 should map to vertex 4");
    
    // Test non-prime
    ASSERT_EQ(platonic_prime_to_vertex(4), UINT64_MAX, "Non-prime 4 should return UINT64_MAX");
    
    TEST_PASS();
}

/* ============================================================================
 * TEST: Prime to Clock Position
 * ============================================================================
 */

void test_prime_to_clock_position() {
    TEST_START("Prime to Clock Position");
    
    ClockPosition pos;
    MathError err;
    
    // Test first prime (2)
    err = platonic_prime_to_clock_position(2, &pos);
    ASSERT_SUCCESS(err, "Should map prime 2 to clock position");
    printf("  Prime 2 -> Ring %u, Position %u, Angle %.4f, Radius %.4f\n",
           pos.ring, pos.position, pos.angle, pos.radius);
    
    // Test second prime (3)
    err = platonic_prime_to_clock_position(3, &pos);
    ASSERT_SUCCESS(err, "Should map prime 3 to clock position");
    printf("  Prime 3 -> Ring %u, Position %u, Angle %.4f, Radius %.4f\n",
           pos.ring, pos.position, pos.angle, pos.radius);
    
    // Test larger prime (23)
    err = platonic_prime_to_clock_position(23, &pos);
    ASSERT_SUCCESS(err, "Should map prime 23 to clock position");
    printf("  Prime 23 -> Ring %u, Position %u, Angle %.4f, Radius %.4f\n",
           pos.ring, pos.position, pos.angle, pos.radius);
    
    TEST_PASS();
}

void test_vertex_to_clock_position() {
    TEST_START("Vertex to Clock Position");
    
    ClockPosition pos;
    MathError err;
    
    // Test first few vertices
    for (uint64_t v = 0; v < 5; v++) {
        err = platonic_vertex_to_clock_position(v, &pos);
        ASSERT_SUCCESS(err, "Should map vertex to clock position");
        
        uint64_t prime = platonic_vertex_to_prime(v);
        printf("  Vertex %llu (Prime %llu) -> Ring %u, Position %u\n",
               (unsigned long long)v, (unsigned long long)prime,
               pos.ring, pos.position);
    }
    
    TEST_PASS();
}

/* ============================================================================
 * TEST: Clock Position to Coordinates
 * ============================================================================
 */

void test_clock_to_coordinates_3d() {
    TEST_START("Clock to Coordinates (3D)");
    
    ClockPosition pos;
    CrystallineAbacus** coords = NULL;
    MathError err;
    
    // Get clock position for vertex 0 (prime 2)
    err = platonic_vertex_to_clock_position(0, &pos);
    ASSERT_SUCCESS(err, "Should get clock position");
    
    // Generate 3D coordinates
    err = platonic_clock_to_coordinates(&pos, 3, &coords, 12, 6);
    ASSERT_SUCCESS(err, "Should generate 3D coordinates");
    ASSERT(coords != NULL, "Coordinates should not be NULL");
    
    // Validate coordinates
    ASSERT(platonic_validate_coordinates(coords, 3), "Coordinates should be valid");
    
    // Print coordinates
    printf("  Vertex 0 (Prime 2) 3D coordinates:\n");
    for (uint32_t i = 0; i < 3; i++) {
        double value;
        abacus_to_double(coords[i], &value);
        printf("    coord[%u] = %.6f\n", i, value);
    }
    
    // Clean up
    for (uint32_t i = 0; i < 3; i++) {
        abacus_free(coords[i]);
    }
    free(coords);
    
    TEST_PASS();
}

void test_clock_to_coordinates_4d() {
    TEST_START("Clock to Coordinates (4D)");
    
    ClockPosition pos;
    CrystallineAbacus** coords = NULL;
    MathError err;
    
    // Get clock position for vertex 1 (prime 3)
    err = platonic_vertex_to_clock_position(1, &pos);
    ASSERT_SUCCESS(err, "Should get clock position");
    
    // Generate 4D coordinates
    err = platonic_clock_to_coordinates(&pos, 4, &coords, 12, 6);
    ASSERT_SUCCESS(err, "Should generate 4D coordinates");
    ASSERT(coords != NULL, "Coordinates should not be NULL");
    
    // Validate coordinates
    ASSERT(platonic_validate_coordinates(coords, 4), "Coordinates should be valid");
    
    // Print coordinates
    printf("  Vertex 1 (Prime 3) 4D coordinates:\n");
    for (uint32_t i = 0; i < 4; i++) {
        double value;
        abacus_to_double(coords[i], &value);
        printf("    coord[%u] = %.6f\n", i, value);
    }
    
    // Clean up
    for (uint32_t i = 0; i < 4; i++) {
        abacus_free(coords[i]);
    }
    free(coords);
    
    TEST_PASS();
}

void test_vertex_to_coordinates() {
    TEST_START("Vertex to Coordinates (Direct)");
    
    CrystallineAbacus** coords = NULL;
    MathError err;
    
    // Generate coordinates directly from vertex index
    err = platonic_vertex_to_coordinates(0, 3, &coords, 12, 6);
    ASSERT_SUCCESS(err, "Should generate coordinates from vertex");
    ASSERT(coords != NULL, "Coordinates should not be NULL");
    
    // Validate coordinates
    ASSERT(platonic_validate_coordinates(coords, 3), "Coordinates should be valid");
    
    // Clean up
    for (uint32_t i = 0; i < 3; i++) {
        abacus_free(coords[i]);
    }
    free(coords);
    
    TEST_PASS();
}

/* ============================================================================
 * TEST: Coordinate Distance
 * ============================================================================
 */

void test_coordinate_distance() {
    TEST_START("Coordinate Distance");
    
    CrystallineAbacus** coords1 = NULL;
    CrystallineAbacus** coords2 = NULL;
    CrystallineAbacus* distance = NULL;
    MathError err;
    
    // Generate coordinates for two vertices
    err = platonic_vertex_to_coordinates(0, 3, &coords1, 12, 6);
    ASSERT_SUCCESS(err, "Should generate coords1");
    
    err = platonic_vertex_to_coordinates(1, 3, &coords2, 12, 6);
    ASSERT_SUCCESS(err, "Should generate coords2");
    
    // Compute distance
    distance = platonic_coordinate_distance(coords1, coords2, 3);
    ASSERT(distance != NULL, "Distance should not be NULL");
    
    double dist_value;
    abacus_to_double(distance, &dist_value);
    printf("  Distance between vertex 0 and vertex 1: %.6f\n", dist_value);
    ASSERT(dist_value > 0.0, "Distance should be positive");
    
    // Clean up
    for (uint32_t i = 0; i < 3; i++) {
        abacus_free(coords1[i]);
        abacus_free(coords2[i]);
    }
    free(coords1);
    free(coords2);
    // Note: distance is already freed by abacus_free, don't free again
    abacus_free(distance);
    
    TEST_PASS();
}

/* ============================================================================
 * TEST: 12-Fold Symmetry
 * ============================================================================
 */

void test_12fold_rotation() {
    TEST_START("12-Fold Rotation");
    
    CrystallineAbacus** coords = NULL;
    CrystallineAbacus** rotated = NULL;
    MathError err;
    
    // Generate coordinates
    err = platonic_vertex_to_coordinates(0, 3, &coords, 12, 6);
    ASSERT_SUCCESS(err, "Should generate coordinates");
    
    // Allocate rotated array
    rotated = (CrystallineAbacus**)calloc(3, sizeof(CrystallineAbacus*));
    ASSERT(rotated != NULL, "Should allocate rotated array");
    
    // Apply rotation
    err = platonic_apply_12fold_rotation(coords, 3, 1, rotated);
    ASSERT_SUCCESS(err, "Should apply rotation");
    
    // Validate rotated coordinates
    ASSERT(platonic_validate_coordinates(rotated, 3), "Rotated coordinates should be valid");
    
    printf("  Original coordinates:\n");
    for (uint32_t i = 0; i < 3; i++) {
        double value;
        abacus_to_double(coords[i], &value);
        printf("    coord[%u] = %.6f\n", i, value);
    }
    
    printf("  Rotated coordinates (30°):\n");
    for (uint32_t i = 0; i < 3; i++) {
        double value;
        abacus_to_double(rotated[i], &value);
        printf("    coord[%u] = %.6f\n", i, value);
    }
    
    // Clean up
    for (uint32_t i = 0; i < 3; i++) {
        abacus_free(coords[i]);
        if (rotated[i]) abacus_free(rotated[i]);
    }
    free(coords);
    free(rotated);
    
    TEST_PASS();
}

void test_has_12fold_symmetry() {
    TEST_START("Check 12-Fold Symmetry");
    
    CrystallineAbacus** coords = NULL;
    MathError err;
    
    // Generate coordinates
    err = platonic_vertex_to_coordinates(0, 3, &coords, 12, 6);
    ASSERT_SUCCESS(err, "Should generate coordinates");
    
    // Check symmetry
    bool has_symmetry = platonic_has_12fold_symmetry(coords, 3);
    printf("  Coordinates have 12-fold symmetry: %s\n", has_symmetry ? "YES" : "NO");
    
    // Clean up
    for (uint32_t i = 0; i < 3; i++) {
        abacus_free(coords[i]);
    }
    free(coords);
    
    TEST_PASS();
}

/* ============================================================================
 * TEST: Deterministic Behavior
 * ============================================================================
 */

void test_deterministic_coordinates() {
    TEST_START("Deterministic Coordinate Generation");
    
    CrystallineAbacus** coords1 = NULL;
    CrystallineAbacus** coords2 = NULL;
    MathError err;
    
    // Generate coordinates twice for same vertex
    err = platonic_vertex_to_coordinates(5, 3, &coords1, 12, 6);
    ASSERT_SUCCESS(err, "Should generate coords1");
    
    err = platonic_vertex_to_coordinates(5, 3, &coords2, 12, 6);
    ASSERT_SUCCESS(err, "Should generate coords2");
    
    // Compare coordinates
    bool identical = true;
    for (uint32_t i = 0; i < 3; i++) {
        double v1, v2;
        abacus_to_double(coords1[i], &v1);
        abacus_to_double(coords2[i], &v2);
        
        // Check if values are identical (within floating point precision)
        if (v1 != v2) {
            printf("  Mismatch at coord[%u]: %.10f vs %.10f\n", i, v1, v2);
            identical = false;
        }
    }
    
    ASSERT(identical, "Coordinates should be identical for same vertex");
    printf("  ✓ Coordinates are deterministic\n");
    
    // Clean up
    for (uint32_t i = 0; i < 3; i++) {
        abacus_free(coords1[i]);
        abacus_free(coords2[i]);
    }
    free(coords1);
    free(coords2);
    
    TEST_PASS();
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */

int main() {
    printf("\n");
    printf(COLOR_YELLOW "========================================\n");
    printf("Clock Lattice Integration Test Suite\n");
    printf("========================================\n" COLOR_RESET);
    printf("\n");
    
    // Run all tests
    test_vertex_to_prime();
    test_prime_to_vertex();
    test_prime_to_clock_position();
    test_vertex_to_clock_position();
    test_clock_to_coordinates_3d();
    test_clock_to_coordinates_4d();
    test_vertex_to_coordinates();
    test_coordinate_distance();
    test_12fold_rotation();
    test_has_12fold_symmetry();
    test_deterministic_coordinates();
    
    // Print summary
    printf("\n");
    printf(COLOR_YELLOW "========================================\n");
    printf("Test Summary\n");
    printf("========================================\n" COLOR_RESET);
    printf("Total tests:  %d\n", tests_run);
    printf(COLOR_GREEN "Passed:       %d\n" COLOR_RESET, tests_passed);
    if (tests_failed > 0) {
        printf(COLOR_RED "Failed:       %d\n" COLOR_RESET, tests_failed);
    } else {
        printf("Failed:       %d\n", tests_failed);
    }
    printf("\n");
    
    return (tests_failed == 0) ? 0 : 1;
}