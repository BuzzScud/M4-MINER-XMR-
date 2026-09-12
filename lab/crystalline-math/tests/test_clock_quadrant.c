/**
 * @file test_clock_quadrant.c
 * @brief Comprehensive tests for Clock Quadrant Folding System
 */

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include "math/clock.h"
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
 * TEST 1: Quadrant Detection
 * ============================================================================
 */
void test_quadrant_detection() {
    printf("\n=== Test 1: Quadrant Detection ===\n");
    
    ClockPosition pos;
    
    // Test Q1: 0° to 90°
    pos.angle = 0.0;  // 0° (3 o'clock)
    TEST_ASSERT(clock_get_quadrant(&pos) == 1, "0° is in Q1");
    
    pos.angle = MATH_PI / 4.0;  // 45°
    TEST_ASSERT(clock_get_quadrant(&pos) == 1, "45° is in Q1");
    
    pos.angle = MATH_PI / 2.0 - 0.01;  // Just before 90°
    TEST_ASSERT(clock_get_quadrant(&pos) == 1, "89° is in Q1");
    
    // Test Q4: 90° to 180°
    pos.angle = MATH_PI / 2.0;  // 90° (12 o'clock)
    TEST_ASSERT(clock_get_quadrant(&pos) == 4, "90° is in Q4");
    
    pos.angle = 3.0 * MATH_PI / 4.0;  // 135°
    TEST_ASSERT(clock_get_quadrant(&pos) == 4, "135° is in Q4");
    
    pos.angle = MATH_PI - 0.01;  // Just before 180°
    TEST_ASSERT(clock_get_quadrant(&pos) == 4, "179° is in Q4");
    
    // Test Q3: 180° to 270°
    pos.angle = MATH_PI;  // 180° (9 o'clock)
    TEST_ASSERT(clock_get_quadrant(&pos) == 3, "180° is in Q3");
    
    pos.angle = 5.0 * MATH_PI / 4.0;  // 225°
    TEST_ASSERT(clock_get_quadrant(&pos) == 3, "225° is in Q3");
    
    pos.angle = 3.0 * MATH_PI / 2.0 - 0.01;  // Just before 270°
    TEST_ASSERT(clock_get_quadrant(&pos) == 3, "269° is in Q3");
    
    // Test Q2: 270° to 360°
    pos.angle = 3.0 * MATH_PI / 2.0;  // 270° (6 o'clock)
    TEST_ASSERT(clock_get_quadrant(&pos) == 2, "270° is in Q2");
    
    pos.angle = 7.0 * MATH_PI / 4.0;  // 315°
    TEST_ASSERT(clock_get_quadrant(&pos) == 2, "315° is in Q2");
    
    pos.angle = MATH_TWO_PI - 0.01;  // Just before 360°
    TEST_ASSERT(clock_get_quadrant(&pos) == 2, "359° is in Q2");
}

/* ============================================================================
 * TEST 2: Folding to Q1
 * ============================================================================
 */
void test_folding_to_q1() {
    printf("\n=== Test 2: Folding to Q1 ===\n");
    
    ClockPosition pos, folded;
    int8_t polarity_change;
    MathError err;
    
    // Test folding from Q1 (no change)
    pos.angle = MATH_PI / 4.0;  // 45° in Q1
    pos.quadrant = 1;
    pos.polarity = 1;
    err = clock_fold_to_q1(&pos, &folded, &polarity_change);
    TEST_ASSERT(err == MATH_SUCCESS, "Folding from Q1 succeeds");
    TEST_ASSERT(folded.quadrant == 1, "Q1 stays in Q1");
    TEST_ASSERT(polarity_change == 1, "Q1 has no polarity change");
    
    // Test folding from Q2
    pos.angle = 7.0 * MATH_PI / 4.0;  // 315° in Q2
    pos.quadrant = 2;
    pos.polarity = 1;
    err = clock_fold_to_q1(&pos, &folded, &polarity_change);
    TEST_ASSERT(err == MATH_SUCCESS, "Folding from Q2 succeeds");
    TEST_ASSERT(folded.quadrant == 1, "Q2 folds to Q1");
    TEST_ASSERT(polarity_change == 1, "Q2 has no polarity change");
    
    // Test folding from Q3
    pos.angle = 5.0 * MATH_PI / 4.0;  // 225° in Q3
    pos.quadrant = 3;
    pos.polarity = 1;
    err = clock_fold_to_q1(&pos, &folded, &polarity_change);
    TEST_ASSERT(err == MATH_SUCCESS, "Folding from Q3 succeeds");
    TEST_ASSERT(folded.quadrant == 1, "Q3 folds to Q1");
    TEST_ASSERT(polarity_change == -1, "Q3 flips polarity");
    
    // Test folding from Q4
    pos.angle = 3.0 * MATH_PI / 4.0;  // 135° in Q4
    pos.quadrant = 4;
    pos.polarity = 1;
    err = clock_fold_to_q1(&pos, &folded, &polarity_change);
    TEST_ASSERT(err == MATH_SUCCESS, "Folding from Q4 succeeds");
    TEST_ASSERT(folded.quadrant == 1, "Q4 folds to Q1");
    TEST_ASSERT(polarity_change == 1, "Q4 has no polarity change");
}

/* ============================================================================
 * TEST 3: Unfolding from Q1
 * ============================================================================
 */
void test_unfolding_from_q1() {
    printf("\n=== Test 3: Unfolding from Q1 ===\n");
    
    ClockPosition pos, unfolded;
    MathError err;
    
    pos.angle = MATH_PI / 4.0;  // 45° in Q1
    pos.quadrant = 1;
    pos.polarity = 1;
    
    // Test unfolding to Q1 (no change)
    err = clock_unfold_from_q1(&pos, 1, &unfolded, 1);
    TEST_ASSERT(err == MATH_SUCCESS, "Unfolding to Q1 succeeds");
    TEST_ASSERT(unfolded.quadrant == 1, "Q1 stays in Q1");
    
    // Test unfolding to Q2
    err = clock_unfold_from_q1(&pos, 2, &unfolded, 1);
    TEST_ASSERT(err == MATH_SUCCESS, "Unfolding to Q2 succeeds");
    TEST_ASSERT(unfolded.quadrant == 2, "Unfolds to Q2");
    TEST_ASSERT(clock_get_quadrant(&unfolded) == 2, "Q2 angle is correct");
    
    // Test unfolding to Q3
    err = clock_unfold_from_q1(&pos, 3, &unfolded, 1);
    TEST_ASSERT(err == MATH_SUCCESS, "Unfolding to Q3 succeeds");
    TEST_ASSERT(unfolded.quadrant == 3, "Unfolds to Q3");
    TEST_ASSERT(clock_get_quadrant(&unfolded) == 3, "Q3 angle is correct");
    
    // Test unfolding to Q4
    err = clock_unfold_from_q1(&pos, 4, &unfolded, 1);
    TEST_ASSERT(err == MATH_SUCCESS, "Unfolding to Q4 succeeds");
    TEST_ASSERT(unfolded.quadrant == 4, "Unfolds to Q4");
    TEST_ASSERT(clock_get_quadrant(&unfolded) == 4, "Q4 angle is correct");
}

/* ============================================================================
 * TEST 4: Fold and Unfold Round-Trip
 * ============================================================================
 */
void test_fold_unfold_roundtrip() {
    printf("\n=== Test 4: Fold and Unfold Round-Trip ===\n");
    
    // Test round-trip for each quadrant
    for (uint8_t q = 1; q <= 4; q++) {
        ClockPosition original, folded, unfolded;
        int8_t polarity_change;
        
        // Create position in quadrant q
        switch (q) {
            case 1: original.angle = MATH_PI / 4.0; break;      // 45°
            case 2: original.angle = 7.0 * MATH_PI / 4.0; break; // 315°
            case 3: original.angle = 5.0 * MATH_PI / 4.0; break; // 225°
            case 4: original.angle = 3.0 * MATH_PI / 4.0; break; // 135°
        }
        original.quadrant = q;
        original.polarity = 1;
        
        // Fold to Q1
        clock_fold_to_q1(&original, &folded, &polarity_change);
        
        // Unfold back to original quadrant
        clock_unfold_from_q1(&folded, q, &unfolded, polarity_change);
        
        // Check that we're back in the same quadrant
        TEST_ASSERT(unfolded.quadrant == original.quadrant, 
                   "Round-trip preserves quadrant");
        
        // Check polarity
        TEST_ASSERT(unfolded.polarity == original.polarity,
                   "Round-trip preserves polarity");
    }
}

/* ============================================================================
 * TEST 5: Polarity Oscillation Tracking
 * ============================================================================
 */
void test_polarity_oscillations() {
    printf("\n=== Test 5: Polarity Oscillation Tracking ===\n");
    
    ClockPosition start, end;
    int oscillations;
    
    // Test Q1 → Q1 (no oscillation)
    start.angle = MATH_PI / 6.0;  // 30° in Q1
    end.angle = MATH_PI / 3.0;    // 60° in Q1
    oscillations = clock_track_polarity_oscillations(&start, &end);
    TEST_ASSERT(oscillations == 0, "Q1 → Q1 has no oscillations");
    
    // Test Q1 → Q2 (no oscillation)
    start.angle = MATH_PI / 4.0;        // 45° in Q1
    end.angle = 7.0 * MATH_PI / 4.0;    // 315° in Q2
    oscillations = clock_track_polarity_oscillations(&start, &end);
    TEST_ASSERT(oscillations == 0, "Q1 → Q2 has no oscillations");
    
    // Test Q1 → Q3 (1 oscillation - 180° flip)
    start.angle = MATH_PI / 4.0;        // 45° in Q1
    end.angle = 5.0 * MATH_PI / 4.0;    // 225° in Q3
    oscillations = clock_track_polarity_oscillations(&start, &end);
    TEST_ASSERT(oscillations == 1, "Q1 → Q3 has 1 oscillation");
    
    // Test Q3 → Q1 (1 oscillation - 180° flip)
    start.angle = 5.0 * MATH_PI / 4.0;  // 225° in Q3
    end.angle = MATH_PI / 4.0;          // 45° in Q1
    oscillations = clock_track_polarity_oscillations(&start, &end);
    TEST_ASSERT(oscillations == 1, "Q3 → Q1 has 1 oscillation");
    
    // Test Q2 → Q4 (1 oscillation)
    start.angle = 7.0 * MATH_PI / 4.0;  // 315° in Q2
    end.angle = 3.0 * MATH_PI / 4.0;    // 135° in Q4
    oscillations = clock_track_polarity_oscillations(&start, &end);
    TEST_ASSERT(oscillations == 1, "Q2 → Q4 has 1 oscillation");
    
    // Test Q4 → Q2 (1 oscillation)
    start.angle = 3.0 * MATH_PI / 4.0;  // 135° in Q4
    end.angle = 7.0 * MATH_PI / 4.0;    // 315° in Q2
    oscillations = clock_track_polarity_oscillations(&start, &end);
    TEST_ASSERT(oscillations == 1, "Q4 → Q2 has 1 oscillation");
}

/* ============================================================================
 * TEST 6: Integration with Clock Position
 * ============================================================================
 */
void test_clock_position_integration() {
    printf("\n=== Test 6: Integration with Clock Position ===\n");
    
    ClockContext ctx;
    clock_init(&ctx);
    
    ClockPosition pos;
    
    // Test that clock_map_prime_to_position sets quadrant
    MathError err = clock_map_prime_to_position(5, &pos);
    TEST_ASSERT(err == MATH_SUCCESS, "Mapping prime to position succeeds");
    TEST_ASSERT(pos.quadrant >= 1 && pos.quadrant <= 4, "Quadrant is set");
    TEST_ASSERT(pos.polarity == 1, "Polarity is initialized to +1");
    
    // Test with different primes
    err = clock_map_prime_to_position(7, &pos);
    TEST_ASSERT(err == MATH_SUCCESS, "Mapping prime 7 succeeds");
    TEST_ASSERT(pos.quadrant >= 1 && pos.quadrant <= 4, "Quadrant is valid");
    
    err = clock_map_prime_to_position(11, &pos);
    TEST_ASSERT(err == MATH_SUCCESS, "Mapping prime 11 succeeds");
    TEST_ASSERT(pos.quadrant >= 1 && pos.quadrant <= 4, "Quadrant is valid");
    
    clock_cleanup(&ctx);
}

/* ============================================================================
 * TEST 7: Polarity Tracking with Operations
 * ============================================================================
 */
void test_polarity_with_operations() {
    printf("\n=== Test 7: Polarity Tracking with Operations ===\n");
    
    ClockPosition pos1, pos2, folded1, folded2;
    int8_t pol1, pol2;
    
    // Simulate addition: Q1 + Q1
    pos1.angle = MATH_PI / 6.0;  // 30° in Q1
    pos1.quadrant = 1;
    pos1.polarity = 1;
    
    pos2.angle = MATH_PI / 3.0;  // 60° in Q1
    pos2.quadrant = 1;
    pos2.polarity = 1;
    
    clock_fold_to_q1(&pos1, &folded1, &pol1);
    clock_fold_to_q1(&pos2, &folded2, &pol2);
    
    int8_t result_polarity = pos1.polarity * pol1 * pos2.polarity * pol2;
    TEST_ASSERT(result_polarity == 1, "Q1 + Q1 has positive polarity");
    
    // Simulate addition: Q1 + Q3 (should flip polarity)
    pos2.angle = 5.0 * MATH_PI / 4.0;  // 225° in Q3
    pos2.quadrant = 3;
    pos2.polarity = 1;
    
    clock_fold_to_q1(&pos2, &folded2, &pol2);
    result_polarity = pos1.polarity * pol1 * pos2.polarity * pol2;
    TEST_ASSERT(result_polarity == -1, "Q1 + Q3 flips polarity");
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */
int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║      CLOCK QUADRANT FOLDING COMPREHENSIVE TEST SUITE      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_quadrant_detection();
    test_folding_to_q1();
    test_unfolding_from_q1();
    test_fold_unfold_roundtrip();
    test_polarity_oscillations();
    test_clock_position_integration();
    test_polarity_with_operations();
    
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