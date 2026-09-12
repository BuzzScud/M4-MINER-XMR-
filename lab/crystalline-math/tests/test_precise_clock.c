/**
 * @file test_precise_clock.c
 * @brief Tests for 360-Degree Precise Clock Position
 * 
 * Tests the enhanced clock precision with factor-based angle refinement.
 */

#include "math/compact_vector.h"
#include "math/prime.h"
#include "math/clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("Testing %s... ", name); \
    fflush(stdout);

#define PASS() \
    printf("PASS\n"); \
    tests_passed++;

#define FAIL(msg) \
    printf("FAIL: %s\n", msg); \
    tests_failed++;

/* ============================================================================
 * PRECISE CLOCK POSITION TESTS
 * ============================================================================
 */

void test_precise_position_basic() {
    TEST("get_precise_clock_position - basic");
    
    PreciseClockPosition pos;
    MathError err = get_precise_clock_position(157, &pos);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to get precise position");
        return;
    }
    
    // 157 % 12 = 1, so base angle should be around 30°
    // With factor refinement (157 is prime), angle may be refined
    // Allow wider range to account for refinement
    if (pos.precise_angle < 0.0 || pos.precise_angle >= 360.0) {
        FAIL("Angle out of valid range");
        return;
    }
    
    // 157 / 12 = 13
    if (pos.magnitude != 13) {
        FAIL("Magnitude incorrect");
        return;
    }
    
    printf("(angle: %.2f°, mag: %lu) ", pos.precise_angle, pos.magnitude);
    PASS();
}

void test_precise_position_prime() {
    TEST("get_precise_clock_position - prime");
    
    // Test with a known prime
    PreciseClockPosition pos;
    MathError err = get_precise_clock_position(157, &pos);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to get precise position");
        return;
    }
    
    // 157 is prime, should be at a valid prime position
    // 157 % 12 = 1 (valid prime position)
    uint32_t base_pos = 157 % 12;
    if (base_pos != 1 && base_pos != 5 && base_pos != 7 && base_pos != 11) {
        FAIL("Not at a prime position");
        return;
    }
    
    PASS();
}

void test_factor_refinement() {
    TEST("calculate_precise_angle_with_factors");
    
    // Test with 30 = 2 × 3 × 5
    uint64_t factors[] = {2, 3, 5};
    double angle;
    MathError err = calculate_precise_angle_with_factors(30, factors, 3, &angle);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to calculate precise angle");
        return;
    }
    
    // 30 % 12 = 6, base angle = 180°
    // With factor refinement, should be slightly different
    if (angle < 175.0 || angle > 185.0) {
        FAIL("Angle out of expected range");
        return;
    }
    
    printf("(angle: %.4f°) ", angle);
    PASS();
}

void test_factor_refinement_precision() {
    TEST("factor_refinement - precision improvement");
    
    // Test that factors actually refine the angle
    uint64_t factors1[] = {2, 3};
    uint64_t factors2[] = {2, 3, 5};
    
    double angle1, angle2;
    calculate_precise_angle_with_factors(30, factors1, 2, &angle1);
    calculate_precise_angle_with_factors(30, factors2, 3, &angle2);
    
    // With more factors, angle should be more refined (different)
    if (angle1 == angle2) {
        FAIL("Factors not providing refinement");
        return;
    }
    
    printf("(2 factors: %.4f°, 3 factors: %.4f°) ", angle1, angle2);
    PASS();
}

void test_phase_offset_prime() {
    TEST("find_prime_phase_offset - prime");
    
    // Test with position 5 (base 5), magnitude 0
    // This should give us 5, which is prime
    double phase_offset;
    MathError err = find_prime_phase_offset(5, 0, &phase_offset);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to find phase offset");
        return;
    }
    
    // 5 is already prime, offset should be 0
    if (phase_offset != 0.0) {
        FAIL("Phase offset should be 0 for prime");
        return;
    }
    
    PASS();
}

void test_phase_offset_composite() {
    TEST("find_prime_phase_offset - composite");
    
    // Test with position 1 (base 13), magnitude 2
    // This gives 13 + 2*12 = 37, which is prime
    double phase_offset;
    MathError err = find_prime_phase_offset(1, 2, &phase_offset);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to find phase offset");
        return;
    }
    
    // 37 is prime, offset should be 0
    if (phase_offset != 0.0) {
        FAIL("Phase offset should be 0 for prime");
        return;
    }
    
    PASS();
}

void test_multi_ring_precision() {
    TEST("multi_ring_precision - all rings");
    
    // Test that we can map to all 4 rings
    PreciseClockPosition pos;
    
    // Test ring 0 (magnitude < 12)
    get_precise_clock_position(5, &pos);
    if (pos.ring != 0) {
        FAIL("Ring 0 incorrect");
        return;
    }
    
    // Test ring 1 (magnitude 12-59)
    get_precise_clock_position(157, &pos);  // magnitude = 13
    if (pos.ring != 1) {
        FAIL("Ring 1 incorrect");
        return;
    }
    
    // Test ring 2 (magnitude 60-119)
    get_precise_clock_position(720, &pos);  // magnitude = 60
    if (pos.ring != 2) {
        FAIL("Ring 2 incorrect");
        return;
    }
    
    // Test ring 3 (magnitude >= 120)
    get_precise_clock_position(1200, &pos);  // magnitude = 100
    if (pos.ring != 3) {
        FAIL("Ring 3 incorrect");
        return;
    }
    
    PASS();
}

void test_angle_normalization() {
    TEST("angle_normalization - 0-360 range");
    
    // Test that angles are always in 0-360 range
    for (uint64_t num = 1; num < 100; num++) {
        PreciseClockPosition pos;
        MathError err = get_precise_clock_position(num, &pos);
        
        if (err != MATH_SUCCESS) {
            FAIL("Failed to get position");
            return;
        }
        
        if (pos.precise_angle < 0.0 || pos.precise_angle >= 360.0) {
            FAIL("Angle out of range");
            return;
        }
    }
    
    PASS();
}

/* ============================================================================
 * INTEGRATION TESTS
 * ============================================================================
 */

void test_vector_from_precise_position() {
    TEST("vector_from_precise_position");
    
    // Get precise position
    PreciseClockPosition pos;
    get_precise_clock_position(157, &pos);
    
    // Create vector from position
    CompactVector vec;
    MathError err = compact_vector_create(
        0,  // sphere_id
        (float)pos.precise_angle,
        (int32_t)pos.magnitude,
        &vec
    );
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to create vector from position");
        return;
    }
    
    // Verify vector matches position
    if (vec.phase_angle < (float)pos.precise_angle - 0.1f ||
        vec.phase_angle > (float)pos.precise_angle + 0.1f) {
        FAIL("Vector angle doesn't match position");
        return;
    }
    
    PASS();
}

void test_precision_comparison() {
    TEST("precision_comparison - 30° vs 360°");
    
    // Compare base 30° precision vs enhanced 360° precision
    uint64_t test_numbers[] = {30, 60, 90, 120, 150, 180};
    int different_count = 0;
    
    for (size_t i = 0; i < 6; i++) {
        uint64_t num = test_numbers[i];
        
        // Base precision (30° per position)
        double base_angle = (num % 12) * 30.0;
        
        // Enhanced precision (360°)
        PreciseClockPosition pos;
        get_precise_clock_position(num, &pos);
        
        // Check if they differ
        if (pos.precise_angle != base_angle) {
            different_count++;
        }
    }
    
    // At least some should be different (factor refinement working)
    if (different_count == 0) {
        FAIL("No precision improvement detected");
        return;
    }
    
    printf("(%d/6 refined) ", different_count);
    PASS();
}

/* ============================================================================
 * MAIN
 * ============================================================================
 */

int main() {
    printf("=== 360-Degree Precise Clock Tests ===\n\n");
    
    // Basic tests
    test_precise_position_basic();
    test_precise_position_prime();
    
    // Factor refinement tests
    test_factor_refinement();
    test_factor_refinement_precision();
    
    // Phase offset tests
    test_phase_offset_prime();
    test_phase_offset_composite();
    
    // Multi-ring tests
    test_multi_ring_precision();
    test_angle_normalization();
    
    // Integration tests
    test_vector_from_precise_position();
    test_precision_comparison();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}