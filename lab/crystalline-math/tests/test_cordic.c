/**
 * @file test_cordic.c
 * @brief Test CORDIC pure Abacus trigonometric functions
 */

#include "math/cordic.h"
#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>

int tests_passed = 0;
int tests_total = 0;

void test_cordic_sincos() {
    printf("\n=== Test 1: CORDIC sin/cos ===\n");
    
    uint32_t base = 12;
    uint32_t precision = 16;
    
    // Test sin(0) = 0, cos(0) = 1
    tests_total++;
    CrystallineAbacus* angle = abacus_from_double(0.0, base, 10);
    CrystallineAbacus* sin_result = abacus_new(base);
    CrystallineAbacus* cos_result = abacus_new(base);
    
    MathError err = cordic_sincos(sin_result, cos_result, angle, precision);
    if (err == MATH_SUCCESS) {
        double sin_val, cos_val;
        abacus_to_double(sin_result, &sin_val);
        abacus_to_double(cos_result, &cos_val);
        
        printf("  sin(0) = %f (expected 0.0)\n", sin_val);
        printf("  cos(0) = %f (expected 1.0)\n", cos_val);
        
        if (sin_val < 0.01 && cos_val > 0.99 && cos_val < 1.01) {
            printf("  ✓ PASS\n");
            tests_passed++;
        } else {
            printf("  ✗ FAIL\n");
        }
    } else {
        printf("  ✗ FAIL: Error %d\n", err);
    }
    
    abacus_free(angle);
    abacus_free(sin_result);
    abacus_free(cos_result);
}

int main() {
    printf("=================================================\n");
    printf("CORDIC PURE ABACUS TESTS\n");
    printf("=================================================\n");
    printf("Testing pure Abacus trigonometric functions\n");
    printf("NO double intermediates - true arbitrary precision!\n");
    
    test_cordic_sincos();
    
    printf("\n=================================================\n");
    printf("RESULTS: %d/%d tests passed\n", tests_passed, tests_total);
    printf("=================================================\n");
    
    return tests_passed == tests_total ? 0 : 1;
}