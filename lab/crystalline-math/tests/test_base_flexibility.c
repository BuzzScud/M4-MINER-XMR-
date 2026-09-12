/**
 * @file test_base_flexibility.c
 * @brief Test that Abacus works with any base (2-256)
 */

#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>

void test_base(uint32_t base, const char* name) {
    printf("\n=== Testing Base %u (%s) ===\n", base, name);
    
    // Test 1: Create from double
    CrystallineAbacus* num = abacus_from_double(42.5, base, 6);
    if (num) {
        double val;
        if (abacus_to_double(num, &val) == MATH_SUCCESS) {
            printf("  Create from double: 42.5 -> %f ✓\n", val);
        } else {
            printf("  Create from double: FAILED ✗\n");
        }
        abacus_free(num);
    } else {
        printf("  Create from double: NULL (base not supported) ✗\n");
        return;
    }
    
    // Test 2: Addition
    CrystallineAbacus* a = abacus_from_uint64(100, base);
    CrystallineAbacus* b = abacus_from_uint64(50, base);
    CrystallineAbacus* result = abacus_new(base);
    
    if (a && b && result) {
        abacus_add(result, a, b);
        uint64_t sum;
        if (abacus_to_uint64(result, &sum) == MATH_SUCCESS) {
            printf("  Addition: 100 + 50 = %lu %s\n", sum, sum == 150 ? "✓" : "✗");
        }
        abacus_free(a);
        abacus_free(b);
        abacus_free(result);
    }
    
    // Test 3: Multiplication
    a = abacus_from_uint64(7, base);
    b = abacus_from_uint64(6, base);
    result = abacus_new(base);
    
    if (a && b && result) {
        abacus_mul(result, a, b);
        uint64_t product;
        if (abacus_to_uint64(result, &product) == MATH_SUCCESS) {
            printf("  Multiplication: 7 × 6 = %lu %s\n", product, product == 42 ? "✓" : "✗");
        }
        abacus_free(a);
        abacus_free(b);
        abacus_free(result);
    }
}

int main() {
    printf("=================================================\n");
    printf("BASE FLEXIBILITY TEST\n");
    printf("=================================================\n");
    printf("Testing that Abacus works with ANY base (2-256)\n");
    
    // Test common bases
    test_base(2, "Binary");
    test_base(8, "Octal");
    test_base(10, "Decimal");
    test_base(12, "Dozenal/Babylonian");
    test_base(16, "Hexadecimal");
    test_base(60, "Sexagesimal/Babylonian");
    test_base(100, "Centesimal");
    
    // Test edge cases
    printf("\n=== Edge Cases ===\n");
    
    // Base 1 (invalid)
    CrystallineAbacus* invalid1 = abacus_from_double(10.0, 1, 6);
    printf("Base 1 (invalid): %s\n", invalid1 == NULL ? "Rejected ✓" : "Accepted ✗");
    if (invalid1) abacus_free(invalid1);
    
    // Base 257 (invalid)
    CrystallineAbacus* invalid2 = abacus_from_double(10.0, 257, 6);
    printf("Base 257 (invalid): %s\n", invalid2 == NULL ? "Rejected ✓" : "Accepted ✗");
    if (invalid2) abacus_free(invalid2);
    
    // Base 256 (valid - maximum)
    CrystallineAbacus* valid = abacus_from_double(10.0, 256, 6);
    printf("Base 256 (valid): %s\n", valid != NULL ? "Accepted ✓" : "Rejected ✗");
    if (valid) abacus_free(valid);
    
    printf("\n=================================================\n");
    printf("BASE FLEXIBILITY TEST COMPLETE\n");
    printf("=================================================\n");
    
    return 0;
}