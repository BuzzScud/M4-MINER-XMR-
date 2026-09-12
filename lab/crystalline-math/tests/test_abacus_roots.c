/**
 * @file test_abacus_roots.c
 * @brief Tests for square root and nth root operations
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 */

#include "math/abacus.h"
#include "math/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s\n", name); \
        tests_run++; \
    } while(0)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ❌ FAILED: %s\n", message); \
            return; \
        } \
    } while(0)

#define PASS() \
    do { \
        printf("  ✅ PASSED\n"); \
        tests_passed++; \
    } while(0)

/* ============================================================================
 * SQUARE ROOT TESTS
 * ============================================================================
 */

void test_sqrt_perfect_squares() {
    TEST("Square root of perfect squares");
    
    uint64_t test_cases[][2] = {
        {4, 2},      // √4 = 2
        {9, 3},      // √9 = 3
        {16, 4},     // √16 = 4
        {25, 5},     // √25 = 5
        {100, 10},   // √100 = 10
        {144, 12},   // √144 = 12
        {10000, 100} // √10000 = 100
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint64_t n = test_cases[i][0];
        uint64_t expected = test_cases[i][1];
        
        CrystallineAbacus *input = abacus_from_uint64(n, 10);
        CrystallineAbacus *result = abacus_new(10);
        
        MathError err = abacus_sqrt(result, input);
        ASSERT(err == MATH_SUCCESS, "sqrt computation failed");
        
        uint64_t sqrt_value;
        err = abacus_to_uint64(result, &sqrt_value);
        ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
        ASSERT(sqrt_value == expected, "sqrt result incorrect");
        
        abacus_free(input);
        abacus_free(result);
    }
    
    PASS();
}

void test_sqrt_non_perfect_squares() {
    TEST("Square root of non-perfect squares");
    
    uint64_t test_cases[][2] = {
        {5, 2},      // √5 = 2 (floor)
        {10, 3},     // √10 = 3
        {50, 7},     // √50 = 7
        {1000, 31},  // √1000 = 31
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint64_t n = test_cases[i][0];
        uint64_t expected = test_cases[i][1];
        
        CrystallineAbacus *input = abacus_from_uint64(n, 10);
        CrystallineAbacus *result = abacus_new(10);
        
        MathError err = abacus_sqrt(result, input);
        ASSERT(err == MATH_SUCCESS, "sqrt computation failed");
        
        uint64_t sqrt_value;
        err = abacus_to_uint64(result, &sqrt_value);
        ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
        ASSERT(sqrt_value == expected, "sqrt result incorrect");
        
        /* Verify: sqrt_value² ≤ n < (sqrt_value+1)² */
        ASSERT(sqrt_value * sqrt_value <= n, "sqrt² should be ≤ n");
        ASSERT((sqrt_value + 1) * (sqrt_value + 1) > n, "(sqrt+1)² should be > n");
        
        abacus_free(input);
        abacus_free(result);
    }
    
    PASS();
}

void test_sqrt_edge_cases() {
    TEST("Square root edge cases");
    
    /* Test √0 = 0 */
    CrystallineAbacus *zero = abacus_from_uint64(0, 10);
    CrystallineAbacus *result = abacus_new(10);
    
    MathError err = abacus_sqrt(result, zero);
    ASSERT(err == MATH_SUCCESS, "sqrt(0) failed");
    ASSERT(abacus_is_zero(result), "Expected sqrt(0) = 0");
    
    abacus_free(zero);
    
    /* Test √1 = 1 */
    CrystallineAbacus *one = abacus_from_uint64(1, 10);
    err = abacus_sqrt(result, one);
    ASSERT(err == MATH_SUCCESS, "sqrt(1) failed");
    
    uint64_t sqrt_value;
    err = abacus_to_uint64(result, &sqrt_value);
    ASSERT(err == MATH_SUCCESS, "Conversion failed");
    ASSERT(sqrt_value == 1, "Expected sqrt(1) = 1");
    
    abacus_free(one);
    abacus_free(result);
    
    PASS();
}

/* ============================================================================
 * NTH ROOT TESTS
 * ============================================================================
 */

void test_cube_root() {
    TEST("Cube root");
    
    uint64_t test_cases[][2] = {
        {8, 2},      // ³√8 = 2
        {27, 3},     // ³√27 = 3
        {64, 4},     // ³√64 = 4
        {125, 5},    // ³√125 = 5
        {1000, 10},  // ³√1000 = 10
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint64_t n = test_cases[i][0];
        uint64_t expected = test_cases[i][1];
        
        CrystallineAbacus *input = abacus_from_uint64(n, 10);
        CrystallineAbacus *result = abacus_new(10);
        
        MathError err = abacus_root(result, input, 3);
        ASSERT(err == MATH_SUCCESS, "cube root computation failed");
        
        uint64_t root_value;
        err = abacus_to_uint64(result, &root_value);
        ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
        ASSERT(root_value == expected, "cube root result incorrect");
        
        abacus_free(input);
        abacus_free(result);
    }
    
    PASS();
}

void test_fourth_root() {
    TEST("Fourth root");
    
    uint64_t test_cases[][2] = {
        {16, 2},     // ⁴√16 = 2
        {81, 3},     // ⁴√81 = 3
        {256, 4},    // ⁴√256 = 4
        {625, 5},    // ⁴√625 = 5
        {10000, 10}, // ⁴√10000 = 10
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint64_t n = test_cases[i][0];
        uint64_t expected = test_cases[i][1];
        
        CrystallineAbacus *input = abacus_from_uint64(n, 10);
        CrystallineAbacus *result = abacus_new(10);
        
        MathError err = abacus_root(result, input, 4);
        ASSERT(err == MATH_SUCCESS, "4th root computation failed");
        
        uint64_t root_value;
        err = abacus_to_uint64(result, &root_value);
        ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
        ASSERT(root_value == expected, "4th root result incorrect");
        
        abacus_free(input);
        abacus_free(result);
    }
    
    PASS();
}

void test_root_edge_cases() {
    TEST("Root edge cases");
    
    /* Test root=1 (identity) */
    CrystallineAbacus *n = abacus_from_uint64(42, 10);
    CrystallineAbacus *result = abacus_new(10);
    
    MathError err = abacus_root(result, n, 1);
    ASSERT(err == MATH_SUCCESS, "root=1 failed");
    
    uint64_t root_value;
    err = abacus_to_uint64(result, &root_value);
    ASSERT(err == MATH_SUCCESS, "Conversion failed");
    ASSERT(root_value == 42, "Expected 1st root = n");
    
    /* Test root=2 (should delegate to sqrt) */
    CrystallineAbacus *sixteen = abacus_from_uint64(16, 10);
    err = abacus_root(result, sixteen, 2);
    ASSERT(err == MATH_SUCCESS, "root=2 failed");
    
    err = abacus_to_uint64(result, &root_value);
    ASSERT(err == MATH_SUCCESS, "Conversion failed");
    ASSERT(root_value == 4, "Expected 2nd root of 16 = 4");
    
    abacus_free(n);
    abacus_free(sixteen);
    abacus_free(result);
    
    PASS();
}

void test_root_different_bases() {
    TEST("Root in different bases");
    
    /* Test √144 in base 12 */
    CrystallineAbacus *n = abacus_from_uint64(144, 12);
    CrystallineAbacus *result = abacus_new(12);
    
    MathError err = abacus_sqrt(result, n);
    ASSERT(err == MATH_SUCCESS, "sqrt in base 12 failed");
    
    uint64_t sqrt_value;
    err = abacus_to_uint64(result, &sqrt_value);
    ASSERT(err == MATH_SUCCESS, "Conversion failed");
    ASSERT(sqrt_value == 12, "Expected √144 = 12");
    
    abacus_free(n);
    abacus_free(result);
    
    PASS();
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  ABACUS SQRT/ROOT TESTS\n");
    printf("========================================\n\n");
    
    /* Square root tests */
    test_sqrt_perfect_squares();
    test_sqrt_non_perfect_squares();
    test_sqrt_edge_cases();
    
    /* Nth root tests */
    test_cube_root();
    test_fourth_root();
    test_root_edge_cases();
    test_root_different_bases();
    
    printf("\n");
    printf("========================================\n");
    printf("  RESULTS: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}