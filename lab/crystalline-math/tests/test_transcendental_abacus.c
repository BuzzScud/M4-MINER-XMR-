#include <stdio.h>
#include <stdlib.h>
#include "math/transcendental.h"
#include "math/abacus.h"
#include "math/arithmetic.h"

int main() {
    printf("=== Testing Abacus Transcendental Functions ===\n\n");
    
    uint32_t base = 60;  // Babylonian base
    uint32_t precision = 10;
    int tests_passed = 0;
    int tests_total = 0;
    
    // Test 1: sqrt_abacus
    printf("Test 1: sqrt_abacus(4) should be ~2.0\n");
    tests_total++;
    CrystallineAbacus* four = abacus_from_uint64(4, base);
    CrystallineAbacus* sqrt_result = abacus_new(base);
    
    MathError err = math_sqrt_abacus(sqrt_result, four, precision);
    if (err == MATH_SUCCESS) {
        double result_val;
        abacus_to_double(sqrt_result, &result_val);
        printf("  Result: %.6f\n", result_val);
        if (math_abs(result_val - 2.0) < 0.001) {
            printf("  PASS\n");
            tests_passed++;
        } else {
            printf("  FAIL: Expected ~2.0\n");
        }
    } else {
        printf("  FAIL: Error %d\n", err);
    }
    abacus_free(four);
    abacus_free(sqrt_result);
    
    // Test 2: sin_abacus(0) should be 0
    printf("\nTest 2: sin_abacus(0) should be 0\n");
    tests_total++;
    CrystallineAbacus* zero = abacus_from_uint64(0, base);
    CrystallineAbacus* sin_result = abacus_new(base);
    
    err = math_sin_abacus(sin_result, zero, precision);
    if (err == MATH_SUCCESS) {
        double result_val;
        abacus_to_double(sin_result, &result_val);
        printf("  Result: %.6f\n", result_val);
        if (math_abs(result_val) < 0.001) {
            printf("  PASS\n");
            tests_passed++;
        } else {
            printf("  FAIL: Expected ~0.0\n");
        }
    } else {
        printf("  FAIL: Error %d\n", err);
    }
    abacus_free(zero);
    abacus_free(sin_result);
    
    // Test 3: cos_abacus(0) should be 1
    printf("\nTest 3: cos_abacus(0) should be 1\n");
    tests_total++;
    CrystallineAbacus* zero2 = abacus_from_uint64(0, base);
    CrystallineAbacus* cos_result = abacus_new(base);
    
    err = math_cos_abacus(cos_result, zero2, precision);
    if (err == MATH_SUCCESS) {
        double result_val;
        abacus_to_double(cos_result, &result_val);
        printf("  Result: %.6f\n", result_val);
        if (math_abs(result_val - 1.0) < 0.001) {
            printf("  PASS\n");
            tests_passed++;
        } else {
            printf("  FAIL: Expected ~1.0\n");
        }
    } else {
        printf("  FAIL: Error %d\n", err);
    }
    abacus_free(zero2);
    abacus_free(cos_result);
    
    // Test 4: sin_abacus(π/2) should be ~1
    printf("\nTest 4: sin_abacus(π/2) should be ~1\n");
    tests_total++;
    double pi_over_2 = 1.5707963267948966;
    CrystallineAbacus* pi2 = abacus_from_double(pi_over_2, base, precision);
    CrystallineAbacus* sin_pi2 = abacus_new(base);
    
    err = math_sin_abacus(sin_pi2, pi2, precision);
    if (err == MATH_SUCCESS) {
        double result_val;
        abacus_to_double(sin_pi2, &result_val);
        printf("  Result: %.6f\n", result_val);
        if (math_abs(result_val - 1.0) < 0.001) {
            printf("  PASS\n");
            tests_passed++;
        } else {
            printf("  FAIL: Expected ~1.0\n");
        }
    } else {
        printf("  FAIL: Error %d\n", err);
    }
    abacus_free(pi2);
    abacus_free(sin_pi2);
    
    // Test 5: atan2_abacus(1, 1) should be ~π/4
    printf("\nTest 5: atan2_abacus(1, 1) should be ~π/4 (~0.785)\n");
    tests_total++;
    CrystallineAbacus* one_y = abacus_from_uint64(1, base);
    CrystallineAbacus* one_x = abacus_from_uint64(1, base);
    CrystallineAbacus* atan2_result = abacus_new(base);
    
    printf("  Debug: Calling math_atan2_abacus...\n");
    err = math_atan2_abacus(atan2_result, one_y, one_x, precision);
    printf("  Debug: Returned error code: %d\n", err);
    
    if (err == MATH_SUCCESS) {
        double result_val;
        MathError conv_err = abacus_to_double(atan2_result, &result_val);
        printf("  Debug: Conversion error: %d\n", conv_err);
        printf("  Result: %.6f\n", result_val);
        printf("  Result num_beads: %zu\n", atan2_result->num_beads);
        
        double pi_over_4 = 0.7853981633974483;
        if (math_abs(result_val - pi_over_4) < 0.01) {
            printf("  PASS\n");
            tests_passed++;
        } else {
            printf("  FAIL: Expected ~0.785, got %.6f\n", result_val);
        }
    } else {
        printf("  FAIL: Error %d\n", err);
    }
    abacus_free(one_y);
    abacus_free(one_x);
    abacus_free(atan2_result);
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d/%d\n", tests_passed, tests_total);
    
    if (tests_passed == tests_total) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED\n");
        return 1;
    }
}
