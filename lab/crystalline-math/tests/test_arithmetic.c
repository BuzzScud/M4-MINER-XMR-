/**
 * @file test_arithmetic.c
 * @brief Test suite for arithmetic operations
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 */

#include "math/arithmetic.h"
#include "math/validation.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_EPSILON 1e-10

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

#define TEST_APPROX(name, actual, expected) do { \
    double diff = math_abs((actual) - (expected)); \
    if (diff < TEST_EPSILON) { \
        tests_passed++; \
        printf("✓ %s (%.10f ≈ %.10f)\n", name, actual, expected); \
    } else { \
        tests_failed++; \
        printf("✗ %s (%.10f != %.10f, diff=%.10e)\n", name, actual, expected, diff); \
    } \
} while(0)

void test_basic_arithmetic(void) {
    printf("\n=== Testing Basic Arithmetic ===\n");
    
    TEST_APPROX("Addition: 2 + 3", math_add(2.0, 3.0), 5.0);
    TEST_APPROX("Subtraction: 5 - 3", math_sub(5.0, 3.0), 2.0);
    TEST_APPROX("Multiplication: 4 * 5", math_mul(4.0, 5.0), 20.0);
    TEST_APPROX("Division: 10 / 2", math_div(10.0, 2.0), 5.0);
    TEST_APPROX("Division: 7 / 2", math_div(7.0, 2.0), 3.5);
    
    // Test division by zero returns NaN
    double div_zero = math_div(1.0, 0.0);
    TEST("Division by zero returns NaN", math_is_nan(div_zero));
}

void test_modulo(void) {
    printf("\n=== Testing Modulo ===\n");
    
    TEST_APPROX("Modulo: 10 % 3", math_mod(10.0, 3.0), 1.0);
    TEST_APPROX("Modulo: 7.5 % 2.5", math_mod(7.5, 2.5), 0.0);
    TEST_APPROX("Modulo: -10 % 3", math_mod(-10.0, 3.0), -1.0);
    TEST_APPROX("Modulo: 10 % -3", math_mod(10.0, -3.0), 1.0);
}

void test_utility_functions(void) {
    printf("\n=== Testing Utility Functions ===\n");
    
    TEST_APPROX("Absolute value: abs(-5)", math_abs(-5.0), 5.0);
    TEST_APPROX("Absolute value: abs(5)", math_abs(5.0), 5.0);
    TEST_APPROX("Absolute value: abs(0)", math_abs(0.0), 0.0);
    
    TEST_APPROX("Min: min(3, 5)", math_min(3.0, 5.0), 3.0);
    TEST_APPROX("Min: min(5, 3)", math_min(5.0, 3.0), 3.0);
    
    TEST_APPROX("Max: max(3, 5)", math_max(3.0, 5.0), 5.0);
    TEST_APPROX("Max: max(5, 3)", math_max(5.0, 3.0), 5.0);
    
    TEST_APPROX("Clamp: clamp(5, 0, 10)", math_clamp(5.0, 0.0, 10.0), 5.0);
    TEST_APPROX("Clamp: clamp(-5, 0, 10)", math_clamp(-5.0, 0.0, 10.0), 0.0);
    TEST_APPROX("Clamp: clamp(15, 0, 10)", math_clamp(15.0, 0.0, 10.0), 10.0);
}

void test_sign_and_comparison(void) {
    printf("\n=== Testing Sign and Comparison ===\n");
    
    TEST("Sign: sign(-5) = -1", math_sign(-5.0) == -1);
    TEST("Sign: sign(0) = 0", math_sign(0.0) == 0);
    TEST("Sign: sign(5) = 1", math_sign(5.0) == 1);
    
    TEST("Approx equal: 1.0 ≈ 1.0000000001", math_approx_equal(1.0, 1.0000000001, 1e-8));
    TEST("Not approx equal: 1.0 ≉ 1.001", !math_approx_equal(1.0, 1.001, 1e-8));
}

void test_special_values(void) {
    printf("\n=== Testing Special Values ===\n");
    
    double nan = 0.0 / 0.0;
    double inf = 1.0 / 0.0;
    double neg_inf = -1.0 / 0.0;
    
    TEST("Is NaN: NaN", math_is_nan(nan));
    TEST("Is not NaN: 1.0", !math_is_nan(1.0));
    
    TEST("Is Inf: +Inf", math_is_inf(inf));
    TEST("Is Inf: -Inf", math_is_inf(neg_inf));
    TEST("Is not Inf: 1.0", !math_is_inf(1.0));
    
    TEST("Is finite: 1.0", math_is_finite(1.0));
    TEST("Is not finite: NaN", !math_is_finite(nan));
    TEST("Is not finite: +Inf", !math_is_finite(inf));
}

int main(void) {
    printf("Crystalline Math Library - Arithmetic Tests\n");
    printf("=============================================\n");
    
    test_basic_arithmetic();
    test_modulo();
    test_utility_functions();
    test_sign_and_comparison();
    test_special_values();
    
    printf("\n=============================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    
    if (tests_failed > 0) {
        printf("FAILED: Some tests did not pass\n");
        return 1;
    }
    
    printf("SUCCESS: All tests passed!\n");
    return 0;
}