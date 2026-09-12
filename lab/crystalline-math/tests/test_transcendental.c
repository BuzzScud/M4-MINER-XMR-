/**
 * @file test_transcendental.c
 * @brief Test suite for transcendental functions
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 */

#include "math/transcendental.h"
#include "math/arithmetic.h"
#include "math/validation.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_EPSILON 1e-6  // Relaxed epsilon for transcendental functions

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

void test_power_and_roots(void) {
    printf("\n=== Testing Power and Root Functions ===\n");
    
    TEST_APPROX("Square root: sqrt(4)", math_sqrt(4.0), 2.0);
    TEST_APPROX("Square root: sqrt(9)", math_sqrt(9.0), 3.0);
    TEST_APPROX("Square root: sqrt(2)", math_sqrt(2.0), 1.41421356);
    
    TEST_APPROX("Cube root: cbrt(8)", math_cbrt(8.0), 2.0);
    TEST_APPROX("Cube root: cbrt(27)", math_cbrt(27.0), 3.0);
    TEST_APPROX("Cube root: cbrt(-8)", math_cbrt(-8.0), -2.0);
    
    TEST_APPROX("Power: 2^3", math_pow(2.0, 3.0), 8.0);
    TEST_APPROX("Power: 3^2", math_pow(3.0, 2.0), 9.0);
    TEST_APPROX("Power: 2^10", math_pow(2.0, 10.0), 1024.0);
    
    // Test special cases
    TEST_APPROX("Power: x^0 = 1", math_pow(5.0, 0.0), 1.0);
    TEST_APPROX("Power: x^1 = x", math_pow(5.0, 1.0), 5.0);
    TEST_APPROX("Power: 1^x = 1", math_pow(1.0, 5.0), 1.0);
}

void test_exponential_and_logarithm(void) {
    printf("\n=== Testing Exponential and Logarithm ===\n");
    
    TEST_APPROX("Exponential: exp(0)", math_exp(0.0), 1.0);
    TEST_APPROX("Exponential: exp(1)", math_exp(1.0), 2.71828183);
    TEST_APPROX("Exponential: exp(2)", math_exp(2.0), 7.38905610);
    
    TEST_APPROX("Natural log: ln(1)", math_log(1.0), 0.0);
    TEST_APPROX("Natural log: ln(e)", math_log(2.71828183), 1.0);
    TEST_APPROX("Natural log: ln(10)", math_log(10.0), 2.30258509);
    
    TEST_APPROX("Log base 10: log10(1)", math_log10(1.0), 0.0);
    TEST_APPROX("Log base 10: log10(10)", math_log10(10.0), 1.0);
    TEST_APPROX("Log base 10: log10(100)", math_log10(100.0), 2.0);
    
    TEST_APPROX("Log base 2: log2(1)", math_log2(1.0), 0.0);
    TEST_APPROX("Log base 2: log2(2)", math_log2(2.0), 1.0);
    TEST_APPROX("Log base 2: log2(8)", math_log2(8.0), 3.0);
}

void test_trigonometric(void) {
    printf("\n=== Testing Trigonometric Functions ===\n");
    
    const double PI_VALUE = 3.14159265358979323846;
    
    TEST_APPROX("Sine: sin(0)", math_sin(0.0), 0.0);
    TEST_APPROX("Sine: sin(π/2)", math_sin(PI_VALUE/2.0), 1.0);
    TEST_APPROX("Sine: sin(π)", math_sin(PI_VALUE), 0.0);
    
    TEST_APPROX("Cosine: cos(0)", math_cos(0.0), 1.0);
    TEST_APPROX("Cosine: cos(π/2)", math_cos(PI_VALUE/2.0), 0.0);
    TEST_APPROX("Cosine: cos(π)", math_cos(PI_VALUE), -1.0);
    
    TEST_APPROX("Tangent: tan(0)", math_tan(0.0), 0.0);
    TEST_APPROX("Tangent: tan(π/4)", math_tan(PI_VALUE/4.0), 1.0);
}

void test_inverse_trigonometric(void) {
    printf("\n=== Testing Inverse Trigonometric Functions ===\n");
    
    const double PI_VALUE = 3.14159265358979323846;
    
    TEST_APPROX("Arcsine: asin(0)", math_asin(0.0), 0.0);
    TEST_APPROX("Arcsine: asin(1)", math_asin(1.0), PI_VALUE/2.0);
    TEST_APPROX("Arcsine: asin(-1)", math_asin(-1.0), -PI_VALUE/2.0);
    
    TEST_APPROX("Arccosine: acos(1)", math_acos(1.0), 0.0);
    TEST_APPROX("Arccosine: acos(0)", math_acos(0.0), PI_VALUE/2.0);
    TEST_APPROX("Arccosine: acos(-1)", math_acos(-1.0), PI_VALUE);
    
    TEST_APPROX("Arctangent: atan(0)", math_atan(0.0), 0.0);
    TEST_APPROX("Arctangent: atan(1)", math_atan(1.0), PI_VALUE/4.0);
    
    TEST_APPROX("Arctangent2: atan2(1, 1)", math_atan2(1.0, 1.0), PI_VALUE/4.0);
    TEST_APPROX("Arctangent2: atan2(1, 0)", math_atan2(1.0, 0.0), PI_VALUE/2.0);
}

void test_hyperbolic(void) {
    printf("\n=== Testing Hyperbolic Functions ===\n");
    
    TEST_APPROX("Hyperbolic sine: sinh(0)", math_sinh(0.0), 0.0);
    TEST_APPROX("Hyperbolic sine: sinh(1)", math_sinh(1.0), 1.17520119);
    
    TEST_APPROX("Hyperbolic cosine: cosh(0)", math_cosh(0.0), 1.0);
    TEST_APPROX("Hyperbolic cosine: cosh(1)", math_cosh(1.0), 1.54308063);
    
    TEST_APPROX("Hyperbolic tangent: tanh(0)", math_tanh(0.0), 0.0);
    TEST_APPROX("Hyperbolic tangent: tanh(1)", math_tanh(1.0), 0.76159416);
}

void test_special_cases(void) {
    printf("\n=== Testing Special Cases ===\n");
    
    // Test sqrt of negative returns NaN
    double sqrt_neg = math_sqrt(-1.0);
    TEST("sqrt(-1) returns NaN", math_is_nan(sqrt_neg));
    
    // Test log of zero returns -Inf
    double log_zero = math_log(0.0);
    TEST("log(0) returns -Inf", math_is_inf(log_zero) && log_zero < 0);
    
    // Test log of negative returns NaN
    double log_neg = math_log(-1.0);
    TEST("log(-1) returns NaN", math_is_nan(log_neg));
}

int main(void) {
    printf("Crystalline Math Library - Transcendental Tests\n");
    printf("================================================\n");
    
    test_power_and_roots();
    test_exponential_and_logarithm();
    test_trigonometric();
    test_inverse_trigonometric();
    test_hyperbolic();
    test_special_cases();
    
    printf("\n================================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    
    if (tests_failed > 0) {
        printf("FAILED: Some tests did not pass\n");
        return 1;
    }
    
    printf("SUCCESS: All tests passed!\n");
    return 0;
}