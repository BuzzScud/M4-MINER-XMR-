#include "math/abacus.h"
#include <stdio.h>

int main(void) {
    printf("Testing Abacus division...\n\n");
    
    // Test 10 / 2 = 5
    CrystallineAbacus* ten = abacus_from_double(10.0, 12, 6);
    CrystallineAbacus* two = abacus_from_double(2.0, 12, 6);
    CrystallineAbacus* result = abacus_new(12);
    abacus_init_zero(result);
    
    printf("Testing 10 / 2:\n");
    MathError err = abacus_div(result, NULL, ten, two);
    printf("  Error code: %d\n", err);
    
    double result_val;
    abacus_to_double(result, &result_val);
    printf("  Result: %.2f (expected 5.0)\n", result_val);
    
    abacus_free(ten);
    abacus_free(two);
    abacus_free(result);
    
    // Test 3.236 / 2 = 1.618
    printf("\nTesting 3.236 / 2:\n");
    CrystallineAbacus* numerator = abacus_from_double(3.236, 12, 6);
    CrystallineAbacus* divisor = abacus_from_double(2.0, 12, 6);
    CrystallineAbacus* quotient = abacus_new(12);
    abacus_init_zero(quotient);
    
    err = abacus_div(quotient, NULL, numerator, divisor);
    printf("  Error code: %d\n", err);
    
    double quotient_val;
    abacus_to_double(quotient, &quotient_val);
    printf("  Result: %.10f (expected 1.618)\n", quotient_val);
    
    abacus_free(numerator);
    abacus_free(divisor);
    abacus_free(quotient);
    
    return 0;
}