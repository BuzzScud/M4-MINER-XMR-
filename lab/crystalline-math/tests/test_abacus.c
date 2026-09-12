/**
 * @file test_abacus.c
 * @brief Test suite for crystalline abacus
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 */

#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST(name) \
    static bool test_##name(); \
    static void run_test_##name() { \
        tests_run++; \
        printf("Running test: %s... ", #name); \
        if (test_##name()) { \
            printf("PASS\n"); \
            tests_passed++; \
        } else { \
            printf("FAIL\n"); \
            tests_failed++; \
        } \
    } \
    static bool test_##name()

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("\n  Assertion failed: %s\n", #condition); \
            return false; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            printf("\n  Expected %llu, got %llu\n", (unsigned long long)(b), (unsigned long long)(a)); \
            return false; \
        } \
    } while(0)

/* ============================================================================
 * INITIALIZATION TESTS
 * ============================================================================
 */

TEST(create_base12) {
    CrystallineAbacus* abacus = abacus_new(12);
    ASSERT(abacus != NULL);
    ASSERT(abacus->base == 12);
    ASSERT(abacus->num_beads == 0);
    abacus_free(abacus);
    return true;
}

TEST(create_base60) {
    CrystallineAbacus* abacus = abacus_new(60);
    ASSERT(abacus != NULL);
    ASSERT(abacus->base == 60);
    abacus_free(abacus);
    return true;
}

TEST(create_base100) {
    CrystallineAbacus* abacus = abacus_new(100);
    ASSERT(abacus != NULL);
    ASSERT(abacus->base == 100);
    abacus_free(abacus);
    return true;
}

TEST(init_zero) {
    CrystallineAbacus* abacus = abacus_new(12);
    ASSERT(abacus != NULL);
    
    MathError err = abacus_init_zero(abacus);
    ASSERT(err == MATH_SUCCESS);
    ASSERT(abacus_is_zero(abacus));
    
    abacus_free(abacus);
    return true;
}

/* ============================================================================
 * CONVERSION TESTS
 * ============================================================================
 */

TEST(from_uint64_zero) {
    CrystallineAbacus* abacus = abacus_from_uint64(0, 12);
    ASSERT(abacus != NULL);
    ASSERT(abacus_is_zero(abacus));
    
    uint64_t value;
    ASSERT(abacus_to_uint64(abacus, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 0);
    
    abacus_free(abacus);
    return true;
}

TEST(from_uint64_small) {
    CrystallineAbacus* abacus = abacus_from_uint64(42, 12);
    ASSERT(abacus != NULL);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(abacus, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 42);
    
    abacus_free(abacus);
    return true;
}

TEST(from_uint64_large) {
    CrystallineAbacus* abacus = abacus_from_uint64(12345, 12);
    ASSERT(abacus != NULL);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(abacus, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 12345);
    
    abacus_free(abacus);
    return true;
}

TEST(from_uint64_base60) {
    CrystallineAbacus* abacus = abacus_from_uint64(3600, 60);
    ASSERT(abacus != NULL);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(abacus, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 3600);
    
    abacus_free(abacus);
    return true;
}

/* ============================================================================
 * ARITHMETIC TESTS
 * ============================================================================
 */

TEST(add_simple) {
    CrystallineAbacus* a = abacus_from_uint64(10, 12);
    CrystallineAbacus* b = abacus_from_uint64(20, 12);
    CrystallineAbacus* result = abacus_new(12);
    
    ASSERT(a != NULL && b != NULL && result != NULL);
    
    MathError err = abacus_add(result, a, b);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(result, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 30);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    return true;
}

TEST(add_with_carry) {
    CrystallineAbacus* a = abacus_from_uint64(11, 12);
    CrystallineAbacus* b = abacus_from_uint64(5, 12);
    CrystallineAbacus* result = abacus_new(12);
    
    ASSERT(a != NULL && b != NULL && result != NULL);
    
    MathError err = abacus_add(result, a, b);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(result, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 16);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    return true;
}

TEST(sub_simple) {
    CrystallineAbacus* a = abacus_from_uint64(30, 12);
    CrystallineAbacus* b = abacus_from_uint64(10, 12);
    CrystallineAbacus* result = abacus_new(12);
    
    ASSERT(a != NULL && b != NULL && result != NULL);
    
    MathError err = abacus_sub(result, a, b);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(result, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 20);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    return true;
}

TEST(mul_simple) {
    CrystallineAbacus* a = abacus_from_uint64(5, 12);
    CrystallineAbacus* b = abacus_from_uint64(7, 12);
    CrystallineAbacus* result = abacus_new(12);
    
    ASSERT(a != NULL && b != NULL && result != NULL);
    
    MathError err = abacus_mul(result, a, b);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(result, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 35);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    return true;
}

TEST(div_simple) {
    CrystallineAbacus* a = abacus_from_uint64(35, 12);
    CrystallineAbacus* b = abacus_from_uint64(7, 12);
    CrystallineAbacus* quotient = abacus_new(12);
    CrystallineAbacus* remainder = abacus_new(12);
    
    ASSERT(a != NULL && b != NULL && quotient != NULL && remainder != NULL);
    
    MathError err = abacus_div(quotient, remainder, a, b);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t q_value, r_value;
    ASSERT(abacus_to_uint64(quotient, &q_value) == MATH_SUCCESS);
    ASSERT(abacus_to_uint64(remainder, &r_value) == MATH_SUCCESS);
    ASSERT_EQ(q_value, 5);
    ASSERT_EQ(r_value, 0);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(quotient);
    abacus_free(remainder);
    return true;
}

TEST(div_with_remainder) {
    CrystallineAbacus* a = abacus_from_uint64(37, 12);
    CrystallineAbacus* b = abacus_from_uint64(7, 12);
    CrystallineAbacus* quotient = abacus_new(12);
    CrystallineAbacus* remainder = abacus_new(12);
    
    ASSERT(a != NULL && b != NULL && quotient != NULL && remainder != NULL);
    
    MathError err = abacus_div(quotient, remainder, a, b);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t q_value, r_value;
    ASSERT(abacus_to_uint64(quotient, &q_value) == MATH_SUCCESS);
    ASSERT(abacus_to_uint64(remainder, &r_value) == MATH_SUCCESS);
    ASSERT_EQ(q_value, 5);
    ASSERT_EQ(r_value, 2);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(quotient);
    abacus_free(remainder);
    return true;
}

/* ============================================================================
 * SHIFT TESTS
 * ============================================================================
 */

TEST(shift_left) {
    CrystallineAbacus* a = abacus_from_uint64(5, 12);
    CrystallineAbacus* result = abacus_new(12);
    
    ASSERT(a != NULL && result != NULL);
    
    MathError err = abacus_shift_left(result, a, 2);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(result, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 5 * 12 * 12);  /* 5 * base^2 */
    
    abacus_free(a);
    abacus_free(result);
    return true;
}

TEST(shift_right) {
    CrystallineAbacus* a = abacus_from_uint64(720, 12);  /* 5 * 12^2 */
    CrystallineAbacus* result = abacus_new(12);
    
    ASSERT(a != NULL && result != NULL);
    
    MathError err = abacus_shift_right(result, a, 2);
    ASSERT(err == MATH_SUCCESS);
    
    uint64_t value;
    ASSERT(abacus_to_uint64(result, &value) == MATH_SUCCESS);
    ASSERT_EQ(value, 5);
    
    abacus_free(a);
    abacus_free(result);
    return true;
}

/* ============================================================================
 * COMPARISON TESTS
 * ============================================================================
 */

TEST(compare_equal) {
    CrystallineAbacus* a = abacus_from_uint64(42, 12);
    CrystallineAbacus* b = abacus_from_uint64(42, 12);
    
    ASSERT(a != NULL && b != NULL);
    ASSERT(abacus_compare(a, b) == 0);
    
    abacus_free(a);
    abacus_free(b);
    return true;
}

TEST(compare_less) {
    CrystallineAbacus* a = abacus_from_uint64(10, 12);
    CrystallineAbacus* b = abacus_from_uint64(20, 12);
    
    ASSERT(a != NULL && b != NULL);
    ASSERT(abacus_compare(a, b) < 0);
    
    abacus_free(a);
    abacus_free(b);
    return true;
}

TEST(compare_greater) {
    CrystallineAbacus* a = abacus_from_uint64(30, 12);
    CrystallineAbacus* b = abacus_from_uint64(20, 12);
    
    ASSERT(a != NULL && b != NULL);
    ASSERT(abacus_compare(a, b) > 0);
    
    abacus_free(a);
    abacus_free(b);
    return true;
}

TEST(is_zero) {
    CrystallineAbacus* a = abacus_from_uint64(0, 12);
    CrystallineAbacus* b = abacus_from_uint64(42, 12);
    
    ASSERT(a != NULL && b != NULL);
    ASSERT(abacus_is_zero(a));
    ASSERT(!abacus_is_zero(b));
    
    abacus_free(a);
    abacus_free(b);
    return true;
}

/* ============================================================================
 * UTILITY TESTS
 * ============================================================================
 */

TEST(copy) {
    CrystallineAbacus* a = abacus_from_uint64(42, 12);
    ASSERT(a != NULL);
    
    CrystallineAbacus* b = abacus_copy(a);
    ASSERT(b != NULL);
    ASSERT(abacus_compare(a, b) == 0);
    
    uint64_t value_a, value_b;
    ASSERT(abacus_to_uint64(a, &value_a) == MATH_SUCCESS);
    ASSERT(abacus_to_uint64(b, &value_b) == MATH_SUCCESS);
    ASSERT_EQ(value_a, value_b);
    
    abacus_free(a);
    abacus_free(b);
    return true;
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */

int main(void) {
    printf("=== Crystalline Abacus Test Suite ===\n\n");
    
    /* Initialization tests */
    printf("--- Initialization Tests ---\n");
    run_test_create_base12();
    run_test_create_base60();
    run_test_create_base100();
    run_test_init_zero();
    
    /* Conversion tests */
    printf("\n--- Conversion Tests ---\n");
    run_test_from_uint64_zero();
    run_test_from_uint64_small();
    run_test_from_uint64_large();
    run_test_from_uint64_base60();
    
    /* Arithmetic tests */
    printf("\n--- Arithmetic Tests ---\n");
    run_test_add_simple();
    run_test_add_with_carry();
    run_test_sub_simple();
    run_test_mul_simple();
    run_test_div_simple();
    run_test_div_with_remainder();
    
    /* Shift tests */
    printf("\n--- Shift Tests ---\n");
    run_test_shift_left();
    run_test_shift_right();
    
    /* Comparison tests */
    printf("\n--- Comparison Tests ---\n");
    run_test_compare_equal();
    run_test_compare_less();
    run_test_compare_greater();
    run_test_is_zero();
    
    /* Utility tests */
    printf("\n--- Utility Tests ---\n");
    run_test_copy();
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed!\n");
        return 1;
    }
}