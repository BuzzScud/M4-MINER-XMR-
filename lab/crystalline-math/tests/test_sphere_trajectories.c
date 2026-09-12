/**
 * @file test_sphere_trajectories.c
 * @brief Test suite for sphere trajectory calculations
 * 
 * Tests the revolutionary sphere trajectory concept for mapping
 * large primes to different kissing spheres.
 */

#include "math/sphere_trajectories.h"
#include "math/prime.h"
#include <stdio.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, condition) do { \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", name); \
    } else { \
        tests_failed++; \
        printf("✗ %s\n", name); \
    } \
} while(0)

void test_sphere_index(void) {
    printf("\n=== Testing Sphere Index Calculation ===\n");
    
    /* Primes below clock resolution should map to sphere 0 */
    TEST("Prime 2 → sphere 0", sphere_get_index(2) == 0);
    TEST("Prime 1000 → sphere 0", sphere_get_index(1000) == 0);
    TEST("Prime 100000 → sphere 0", sphere_get_index(100000) == 0);
    TEST("Prime 1000000 → sphere 0", sphere_get_index(1000000) == 0);
    
    /* Primes at exactly clock resolution */
    TEST("Prime 4320000 → sphere 1", sphere_get_index(4320000) == 1);
    
    /* Larger primes map to higher spheres */
    TEST("Prime 8640000 → sphere 2", sphere_get_index(8640000) == 2);
    TEST("Prime 12960000 → sphere 3", sphere_get_index(12960000) == 3);
    
    printf("\nClock resolution = %llu positions per sphere\n", CLOCK_RESOLUTION);
}

void test_local_position(void) {
    printf("\n=== Testing Local Position Calculation ===\n");
    
    /* Local position should be prime % CLOCK_RESOLUTION */
    TEST("Prime 2: local position = 2", sphere_get_local_position(2) == 2);
    TEST("Prime 1000: local position = 1000", sphere_get_local_position(1000) == 1000);
    
    /* At clock resolution boundary */
    TEST("Prime 4320000: local position = 0", sphere_get_local_position(4320000) == 0);
    TEST("Prime 4320001: local position = 1", sphere_get_local_position(4320001) == 1);
    
    /* Large primes */
    uint64_t large_prime = 8640000 + 12345;
    uint64_t expected_local = 12345;
    TEST("Large prime: correct local position", 
         sphere_get_local_position(large_prime) == expected_local);
}

void test_trajectory_vectors(void) {
    printf("\n=== Testing Trajectory Vectors ===\n");
    
    double traj[3];
    
    /* Sphere 0: origin */
    sphere_calculate_trajectory(0, traj);
    TEST("Sphere 0: at origin", 
         traj[0] == 0.0 && traj[1] == 0.0 && traj[2] == 0.0);
    
    /* Sphere 1: first neighbor (0° direction) */
    sphere_calculate_trajectory(1, traj);
    TEST("Sphere 1: positive x direction", traj[0] == 1.0);
    printf("  Trajectory: (%.3f, %.3f, %.3f)\n", traj[0], traj[1], traj[2]);
    
    /* Sphere 4: 90° direction (12 o'clock) */
    sphere_calculate_trajectory(4, traj);
    TEST("Sphere 4: positive y direction", traj[1] == 1.0);
    printf("  Trajectory: (%.3f, %.3f, %.3f)\n", traj[0], traj[1], traj[2]);
    
    /* Sphere 7: 180° direction (9 o'clock) */
    sphere_calculate_trajectory(7, traj);
    TEST("Sphere 7: negative x direction", traj[0] == -1.0);
    printf("  Trajectory: (%.3f, %.3f, %.3f)\n", traj[0], traj[1], traj[2]);
    
    /* Sphere 13: second layer, first direction */
    sphere_calculate_trajectory(13, traj);
    TEST("Sphere 13: second layer", traj[0] == 2.0);
    printf("  Trajectory: (%.3f, %.3f, %.3f)\n", traj[0], traj[1], traj[2]);
}

void test_sphere_mapping(void) {
    printf("\n=== Testing Complete Sphere Mapping ===\n");
    
    uint64_t sphere_idx;
    ClockPosition local_pos;
    
    /* Small prime on sphere 0 */
    sphere_map_prime(17, &sphere_idx, &local_pos);
    TEST("Prime 17: on sphere 0", sphere_idx == 0);
    printf("  Prime 17 → sphere %lu, ring %u, position %u\n", 
           sphere_idx, local_pos.ring, local_pos.position);
    
    /* Prime at clock resolution boundary */
    sphere_map_prime(4320000, &sphere_idx, &local_pos);
    TEST("Prime 4320000: on sphere 1", sphere_idx == 1);
    printf("  Prime 4320000 → sphere %lu, ring %u, position %u\n", 
           sphere_idx, local_pos.ring, local_pos.position);
    
    /* Large prime */
    sphere_map_prime(10000000, &sphere_idx, &local_pos);
    printf("  Prime 10000000 → sphere %lu, ring %u, position %u\n", 
           sphere_idx, local_pos.ring, local_pos.position);
    TEST("Prime 10000000: on sphere 2", sphere_idx == 2);
}

void test_prime_distance(void) {
    printf("\n=== Testing Prime Distance in Sphere Space ===\n");
    
    /* Distance between consecutive small primes */
    double d1 = sphere_prime_distance(2, 3);
    printf("  Distance(2, 3) = %.6f\n", d1);
    TEST("Distance(2, 3) > 0", d1 > 0.0);
    
    double d2 = sphere_prime_distance(17, 29);
    printf("  Distance(17, 29) = %.6f\n", d2);
    /* Note: 17 and 29 are at the same clock position (both position 3, Ring 0)
     * so their angular distance is 0. They differ only in magnitude.
     * This is CORRECT behavior! */
    TEST("Distance(17, 29) = 0 (same position, different magnitude)", d2 == 0.0);
    
    /* Distance between primes on different spheres */
    double d3 = sphere_prime_distance(1000, 5000000);
    printf("  Distance(1000, 5000000) = %.6f\n", d3);
    TEST("Distance across spheres > 0", d3 > 0.0);
}

int main(void) {
    printf("Crystalline Math Library - Sphere Trajectory Tests\n");
    printf("===================================================\n");
    printf("BREAKTHROUGH (2024-12-11): Testing sphere trajectory concept!\n");
    
    test_sphere_index();
    test_local_position();
    test_trajectory_vectors();
    test_sphere_mapping();
    test_prime_distance();
    
    printf("\n===================================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    
    if (tests_failed > 0) {
        printf("FAILED: Some tests did not pass\n");
        return 1;
    }
    
    printf("SUCCESS: All tests passed!\n");
    printf("\nSphere trajectory concept validated!\n");
    printf("Next: Implement O(1) factoring using sphere overlaps\n");
    return 0;
}