/**
 * @file test_abacus_fractional.c
 * @brief Test suite for pure Abacus fractional operations
 */

#include "math/abacus_fractional.h"
#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Ensure math.h functions are available */
#ifndef fabs
#define fabs(x) __builtin_fabs(x)
#endif
#ifndef sqrt
#define sqrt(x) __builtin_sqrt(x)
#endif

/* Test counters */
static int tests_passed = 0;
static int tests_total = 0;

/* Helper to compare Abacus with expected double value */
bool abacus_approx_equal(const CrystallineAbacus* abacus, double expected, double epsilon) {
    double actual;
    if (abacus_to_double(abacus, &actual) != MATH_SUCCESS) {
        return false;
    }
    return fabs(actual - expected) < epsilon;
}

/* ============================================================================
 * FRACTIONAL DIVISION TESTS
 * ============================================================================
 */

void test_div_fractional_simple() {
    printf("\n=== Test: Simple Fractional Division ===\n");
    fflush(stdout);
    
    tests_total++;
    printf("Test %d: 1 / 2 = 0.5\n", tests_total);
    fflush(stdout);
    
    printf("[DEBUG] Creating abacus objects...\n");
    fflush(stdout);
    CrystallineAbacus* one = abacus_from_uint64(1, 10);
    printf("[DEBUG] one created\n");
    fflush(stdout);
    CrystallineAbacus* two = abacus_from_uint64(2, 10);
    printf("[DEBUG] two created\n");
    fflush(stdout);
    CrystallineAbacus* result = abacus_new(10);
    printf("[DEBUG] result created\n");
    fflush(stdout);
    
    printf("[DEBUG] Calling abacus_div_fractional...\n");
    fflush(stdout);
    MathError err = abacus_div_fractional(result, one, two, 10);
    printf("[DEBUG] abacus_div_fractional returned: %d\n", err);
    fflush(stdout);
    
    printf("[DEBUG] Checking result...\n");
    fflush(stdout);
    
    if (err == MATH_SUCCESS && abacus_approx_equal(result, 0.5, 0.0001)) {
        printf("[DEBUG] Test passed!\n");
        fflush(stdout);
        printf("  ✓ PASS: Result = 0.5\n");
        tests_passed++;
    } else {
        double actual;
        abacus_to_double(result, &actual);
        printf("  ✗ FAIL: Expected 0.5, got %.10f\n", actual);
    }
    
    printf("[DEBUG] Freeing memory...\n");
    fflush(stdout);
    abacus_free(one);
    printf("[DEBUG] one freed\n");
    fflush(stdout);
    abacus_free(two);
    printf("[DEBUG] two freed\n");
    fflush(stdout);
    abacus_free(result);
    printf("[DEBUG] result freed\n");
    fflush(stdout);
    printf("[DEBUG] test_div_fractional_simple complete\n");
    fflush(stdout);
}

void test_div_fractional_third() {
    printf("\n=== Test: One Third ===\n");
    
    tests_total++;
    printf("Test %d: 1 / 3 = 0.333...\n", tests_total);
    
    CrystallineAbacus* one = abacus_from_uint64(1, 10);
    CrystallineAbacus* three = abacus_from_uint64(3, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    MathError err = abacus_div_fractional(result, one, three, 10);
    
    if (err == MATH_SUCCESS && abacus_approx_equal(result, 1.0/3.0, 0.0001)) {
        printf("  ✓ PASS: Result ≈ 0.333...\n");
        tests_passed++;
    } else {
        double actual;
        abacus_to_double(result, &actual);
        printf("  ✗ FAIL: Expected 0.333..., got %.10f\n", actual);
    }
    
    abacus_free(one);
    abacus_free(three);
    abacus_free(result);
}

void test_div_fractional_complex() {
    printf("\n=== Test: Complex Fraction ===\n");
    
    tests_total++;
    printf("Test %d: 22 / 7 = 3.142857...\n", tests_total);
    
    CrystallineAbacus* twentytwo = abacus_from_uint64(22, 10);
    CrystallineAbacus* seven = abacus_from_uint64(7, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    MathError err = abacus_div_fractional(result, twentytwo, seven, 10);
    
    if (err == MATH_SUCCESS && abacus_approx_equal(result, 22.0/7.0, 0.0001)) {
        printf("  ✓ PASS: Result ≈ 3.142857\n");
        tests_passed++;
    } else {
        double actual;
        abacus_to_double(result, &actual);
        printf("  ✗ FAIL: Expected 3.142857, got %.10f\n", actual);
    }
    
    abacus_free(twentytwo);
    abacus_free(seven);
    abacus_free(result);
}

void test_div_fractional_precision() {
    printf("\n=== Test: High Precision ===\n");
    
    tests_total++;
    printf("Test %d: 1 / 7 with 20 digits precision\n", tests_total);
    
    CrystallineAbacus* one = abacus_from_uint64(1, 10);
    CrystallineAbacus* seven = abacus_from_uint64(7, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    MathError err = abacus_div_fractional(result, one, seven, 20);
    
    if (err == MATH_SUCCESS) {
        double actual;
        abacus_to_double(result, &actual);
        double expected = 1.0 / 7.0;
        double error = fabs(actual - expected);
        
        printf("  Result: %.20f\n", actual);
        printf("  Expected: %.20f\n", expected);
        printf("  Error: %.2e\n", error);
        
        if (error < 1e-15) {
            printf("  ✓ PASS: High precision achieved\n");
            tests_passed++;
        } else {
            printf("  ✗ FAIL: Precision not sufficient\n");
        }
    } else {
        printf("  ✗ FAIL: Error %d\n", err);
    }
    
    abacus_free(one);
    abacus_free(seven);
    abacus_free(result);
}

/* ============================================================================
 * SQUARE ROOT TESTS
 * ============================================================================
 */

void test_sqrt_pure_simple() {
    printf("\n=== Test: Simple Square Roots ===\n");
    fflush(stdout);
    
    const uint64_t test_values[] = {4, 9, 16, 25, 100};
    const double expected[] = {2.0, 3.0, 4.0, 5.0, 10.0};
    
    printf("[DEBUG] Starting loop over %zu test values\n", sizeof(test_values) / sizeof(test_values[0]));
    fflush(stdout);
    
    for (size_t i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        printf("[DEBUG] Test iteration %zu: sqrt(%lu)\n", i, test_values[i]);
        fflush(stdout);
        tests_total++;
        printf("Test %d: sqrt(%lu) = %.1f\n", tests_total, test_values[i], expected[i]);
        fflush(stdout);
        
        printf("[DEBUG] Creating input abacus...\n");
        fflush(stdout);
        CrystallineAbacus* input = abacus_from_uint64(test_values[i], 10);
        printf("[DEBUG] Creating result abacus...\n");
        fflush(stdout);
        CrystallineAbacus* result = abacus_new(10);
        
        printf("[DEBUG] Calling abacus_sqrt_pure...\n");
        fflush(stdout);
        MathError err = abacus_sqrt_pure(result, input, 10);
        printf("[DEBUG] abacus_sqrt_pure returned: %d\n", err);
        fflush(stdout);
        
        if (err == MATH_SUCCESS && abacus_approx_equal(result, expected[i], 0.0001)) {
            printf("  ✓ PASS\n");
            tests_passed++;
        } else {
            double actual;
            abacus_to_double(result, &actual);
            printf("  ✗ FAIL: Expected %.1f, got %.10f\n", expected[i], actual);
        }
        
        abacus_free(input);
        abacus_free(result);
    }
}

void test_sqrt_pure_precision() {
    printf("\n=== Test: High Precision Square Root ===\n");
    
    tests_total++;
    printf("Test %d: sqrt(2) with high precision\n", tests_total);
    
    CrystallineAbacus* two = abacus_from_uint64(2, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    MathError err = abacus_sqrt_pure(result, two, 15);
    
    if (err == MATH_SUCCESS) {
        double actual;
        abacus_to_double(result, &actual);
        double expected = sqrt(2.0);
        double error = fabs(actual - expected);
        
        printf("  Result: %.15f\n", actual);
        printf("  Expected: %.15f\n", expected);
        printf("  Error: %.2e\n", error);
        
        if (error < 1e-10) {
            printf("  ✓ PASS: High precision achieved\n");
            tests_passed++;
        } else {
            printf("  ✗ FAIL: Precision not sufficient\n");
        }
    } else {
        printf("  ✗ FAIL: Error %d\n", err);
    }
    
    abacus_free(two);
    abacus_free(result);
}

void test_sqrt_pure_convergence() {
    printf("\n=== Test: Newton-Raphson Convergence ===\n");
    
    tests_total++;
    printf("Test %d: sqrt(10) convergence test\n", tests_total);
    
    CrystallineAbacus* ten = abacus_from_uint64(10, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    double expected = sqrt(10.0);
    
    printf("  Iterations | Result      | Error\n");
    printf("  -----------|-------------|----------\n");
    
    bool converged = false;
    for (uint32_t iterations = 1; iterations <= 10; iterations++) {
        MathError err = abacus_sqrt_pure(result, ten, iterations);
        
        if (err == MATH_SUCCESS) {
            double actual;
            abacus_to_double(result, &actual);
            double error = fabs(actual - expected);
            
            printf("  %10u | %11.8f | %.2e\n", iterations, actual, error);
            
            if (error < 1e-10 && !converged) {
                converged = true;
            }
        }
    }
    
    if (converged) {
        printf("  ✓ PASS: Converged to high precision\n");
        tests_passed++;
    } else {
        printf("  ✗ FAIL: Did not converge\n");
    }
    
    abacus_free(ten);
    abacus_free(result);
}

/* ============================================================================
 * EDGE CASES
 * ============================================================================
 */

void test_edge_cases() {
    printf("\n=== Test: Edge Cases ===\n");
    
    /* Test 1: Division by 1 */
    tests_total++;
    printf("Test %d: 5 / 1 = 5\n", tests_total);
    
    CrystallineAbacus* five = abacus_from_uint64(5, 10);
    CrystallineAbacus* one = abacus_from_uint64(1, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    MathError err = abacus_div_fractional(result, five, one, 5);
    
    if (err == MATH_SUCCESS && abacus_approx_equal(result, 5.0, 0.0001)) {
        printf("  ✓ PASS\n");
        tests_passed++;
    } else {
        printf("  ✗ FAIL\n");
    }
    
    abacus_free(five);
    abacus_free(one);
    abacus_free(result);
    
    /* Test 2: sqrt(0) = 0 */
    tests_total++;
    printf("Test %d: sqrt(0) = 0\n", tests_total);
    
    CrystallineAbacus* zero = abacus_from_uint64(0, 10);
    result = abacus_new(10);
    
    err = abacus_sqrt_pure(result, zero, 5);
    
    if (err == MATH_SUCCESS && abacus_is_zero(result)) {
        printf("  ✓ PASS\n");
        tests_passed++;
    } else {
        printf("  ✗ FAIL\n");
    }
    
    abacus_free(zero);
    abacus_free(result);
    
    /* Test 3: sqrt(1) = 1 */
    tests_total++;
    printf("Test %d: sqrt(1) = 1\n", tests_total);
    
    one = abacus_from_uint64(1, 10);
    result = abacus_new(10);
    
    err = abacus_sqrt_pure(result, one, 5);
    
    if (err == MATH_SUCCESS && abacus_approx_equal(result, 1.0, 0.0001)) {
        printf("  ✓ PASS\n");
        tests_passed++;
    } else {
        printf("  ✗ FAIL\n");
    }
    
    abacus_free(one);
    abacus_free(result);
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */

int main() {
    printf("=================================================\n");
    printf("PURE ABACUS FRACTIONAL OPERATIONS TEST SUITE\n");
    printf("=================================================\n");
    printf("Testing arbitrary precision without double loss\n");
    
    printf("\n[DEBUG] Starting fractional division tests...\n");
    fflush(stdout);
    
    /* Fractional division tests */
    printf("[DEBUG] Calling test_div_fractional_simple...\n");
    fflush(stdout);
    test_div_fractional_simple();
    printf("[DEBUG] test_div_fractional_simple done\n");
    fflush(stdout);
    
    printf("[DEBUG] Calling test_div_fractional_third...\n");
    fflush(stdout);
    test_div_fractional_third();
    printf("[DEBUG] test_div_fractional_third done\n");
    fflush(stdout);
    
    printf("[DEBUG] Calling test_div_fractional_complex...\n");
    fflush(stdout);
    test_div_fractional_complex();
    printf("[DEBUG] test_div_fractional_complex done\n");
    fflush(stdout);
    
    printf("[DEBUG] Calling test_div_fractional_precision...\n");
    fflush(stdout);
    test_div_fractional_precision();
    printf("[DEBUG] test_div_fractional_precision done\n");
    fflush(stdout);
    
    /* Square root tests */
    printf("[DEBUG] Calling test_sqrt_pure_simple...\n");
    fflush(stdout);
    test_sqrt_pure_simple();
    printf("[DEBUG] test_sqrt_pure_simple done\n");
    fflush(stdout);
    test_sqrt_pure_precision();
    test_sqrt_pure_convergence();
    
    /* Edge cases */
    test_edge_cases();
    
    /* Summary */
    printf("\n=================================================\n");
    printf("RESULTS: %d/%d tests passed (%.1f%%)\n", 
           tests_passed, tests_total, 
           tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
    printf("=================================================\n");
    
    return tests_passed == tests_total ? 0 : 1;
}