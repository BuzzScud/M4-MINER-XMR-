#include "math/abacus.h"
#include "math/transcendental.h"
#include "math/constants.h"
#include <stdio.h>

int main() {
    printf("=== Simple sqrt test ===\n\n");
    
    uint32_t base = 12;
    
    // Test 1: sqrt(4) = 2
    printf("Test 1: sqrt(4)\n");
    CrystallineAbacus* four = abacus_from_uint64(4, base);
    CrystallineAbacus* result1 = abacus_new(base);
    math_sqrt_abacus(result1, four, 15);
    double val1;
    abacus_to_double(result1, &val1);
    printf("  Result: %f (expected 2.0)\n", val1);
    abacus_free(four);
    abacus_free(result1);
    
    // Test 2: sqrt(2) = 1.414...
    printf("\nTest 2: sqrt(2)\n");
    CrystallineAbacus* two = abacus_from_uint64(2, base);
    
    // Check what value is stored
    double two_val;
    abacus_to_double(two, &two_val);
    printf("  Input value: %f\n", two_val);
    
    CrystallineAbacus* result2 = abacus_new(base);
    MathError err = math_sqrt_abacus(result2, two, 15);
    printf("  Error code: %d\n", err);
    
    double val2;
    abacus_to_double(result2, &val2);
    printf("  Result: %f (expected 1.414...)\n", val2);
    printf("  Expected: %f\n", MATH_SQRT2);
    
    abacus_free(two);
    abacus_free(result2);
    
    // Test 3: sqrt(2.0) from double
    printf("\nTest 3: sqrt(2.0) from double\n");
    CrystallineAbacus* two_d = abacus_from_double(2.0, base, 15);
    if (two_d) {
        double two_d_val;
        abacus_to_double(two_d, &two_d_val);
        printf("  Input value: %f\n", two_d_val);
        
        CrystallineAbacus* result3 = abacus_new(base);
        err = math_sqrt_abacus(result3, two_d, 15);
        printf("  Error code: %d\n", err);
        
        double val3;
        abacus_to_double(result3, &val3);
        printf("  Result: %f (expected 1.414...)\n", val3);
        
        abacus_free(two_d);
        abacus_free(result3);
    } else {
        printf("  Failed to create Abacus from 2.0\n");
    }
    
    return 0;
}