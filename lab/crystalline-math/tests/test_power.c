/**
 * @file test_power.c
 * @brief Test exponentiation functions (pow, powmod)
 */

#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>

int tests_passed = 0;
int tests_total = 0;

void test_pow_basic() {
    printf("\n=== Test 1: Basic Exponentiation ===\n");
    
    uint32_t base = 10;
    
    // Test 2^3 = 8
    tests_total++;
    CrystallineAbacus* base_num = abacus_from_uint64(2, base);
    CrystallineAbacus* exp_num = abacus_from_uint64(3, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 8) {
            printf("  2^3 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  2^3 = %lu (expected 8) ✗\n", val);
        }
    } else {
        printf("  2^3 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
    
    // Test 3^4 = 81
    tests_total++;
    base_num = abacus_from_uint64(3, base);
    exp_num = abacus_from_uint64(4, base);
    result = abacus_new(base);
    
    err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 81) {
            printf("  3^4 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  3^4 = %lu (expected 81) ✗\n", val);
        }
    } else {
        printf("  3^4 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
    
    // Test 10^5 = 100000
    tests_total++;
    base_num = abacus_from_uint64(10, base);
    exp_num = abacus_from_uint64(5, base);
    result = abacus_new(base);
    
    err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 100000) {
            printf("  10^5 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  10^5 = %lu (expected 100000) ✗\n", val);
        }
    } else {
        printf("  10^5 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
}

void test_pow_edge_cases() {
    printf("\n=== Test 2: Edge Cases ===\n");
    
    uint32_t base = 10;
    
    // Test x^0 = 1
    tests_total++;
    CrystallineAbacus* base_num = abacus_from_uint64(42, base);
    CrystallineAbacus* exp_num = abacus_from_uint64(0, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 1) {
            printf("  42^0 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  42^0 = %lu (expected 1) ✗\n", val);
        }
    } else {
        printf("  42^0 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
    
    // Test x^1 = x
    tests_total++;
    base_num = abacus_from_uint64(42, base);
    exp_num = abacus_from_uint64(1, base);
    result = abacus_new(base);
    
    err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 42) {
            printf("  42^1 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  42^1 = %lu (expected 42) ✗\n", val);
        }
    } else {
        printf("  42^1 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
    
    // Test 0^n = 0 (n > 0)
    tests_total++;
    base_num = abacus_from_uint64(0, base);
    exp_num = abacus_from_uint64(5, base);
    result = abacus_new(base);
    
    err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 0) {
            printf("  0^5 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  0^5 = %lu (expected 0) ✗\n", val);
        }
    } else {
        printf("  0^5 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
}

void test_pow_large() {
    printf("\n=== Test 3: Large Exponents ===\n");
    
    uint32_t base = 10;
    
    // Test 2^10 = 1024
    tests_total++;
    CrystallineAbacus* base_num = abacus_from_uint64(2, base);
    CrystallineAbacus* exp_num = abacus_from_uint64(10, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 1024) {
            printf("  2^10 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  2^10 = %lu (expected 1024) ✗\n", val);
        }
    } else {
        printf("  2^10 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
    
    // Test 2^20 = 1048576
    tests_total++;
    base_num = abacus_from_uint64(2, base);
    exp_num = abacus_from_uint64(20, base);
    result = abacus_new(base);
    
    err = abacus_pow(result, base_num, exp_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 1048576) {
            printf("  2^20 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  2^20 = %lu (expected 1048576) ✗\n", val);
        }
    } else {
        printf("  2^20 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(result);
}

void test_powmod() {
    printf("\n=== Test 4: Modular Exponentiation ===\n");
    
    uint32_t base = 10;
    
    // Test 2^10 mod 1000 = 24
    tests_total++;
    CrystallineAbacus* base_num = abacus_from_uint64(2, base);
    CrystallineAbacus* exp_num = abacus_from_uint64(10, base);
    CrystallineAbacus* mod_num = abacus_from_uint64(1000, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_powmod(result, base_num, exp_num, mod_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 24) {
            printf("  2^10 mod 1000 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  2^10 mod 1000 = %lu (expected 24) ✗\n", val);
        }
    } else {
        printf("  2^10 mod 1000 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(mod_num);
    abacus_free(result);
    
    // Test 7^100 mod 13 = 9 (cryptographic example)
    tests_total++;
    base_num = abacus_from_uint64(7, base);
    exp_num = abacus_from_uint64(100, base);
    mod_num = abacus_from_uint64(13, base);
    result = abacus_new(base);
    
    err = abacus_powmod(result, base_num, exp_num, mod_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 9) {
            printf("  7^100 mod 13 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  7^100 mod 13 = %lu (expected 9) ✗\n", val);
        }
    } else {
        printf("  7^100 mod 1000 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(mod_num);
    abacus_free(result);
    
    // Test 3^13 mod 12 = 3 (demonstrates clock wrapping)
    tests_total++;
    base_num = abacus_from_uint64(3, base);
    exp_num = abacus_from_uint64(13, base);
    mod_num = abacus_from_uint64(12, base);
    result = abacus_new(base);
    
    err = abacus_powmod(result, base_num, exp_num, mod_num);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 3) {
            printf("  3^13 mod 12 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  3^13 mod 12 = %lu (expected 3) ✗\n", val);
        }
    } else {
        printf("  3^13 mod 12 failed with error %d ✗\n", err);
    }
    
    abacus_free(base_num);
    abacus_free(exp_num);
    abacus_free(mod_num);
    abacus_free(result);
}

int main() {
    printf("=================================================\n");
    printf("EXPONENTIATION TESTS\n");
    printf("=================================================\n");
    
    test_pow_basic();
    test_pow_edge_cases();
    test_pow_large();
    test_powmod();
    
    printf("\n=================================================\n");
    printf("RESULTS: %d/%d tests passed\n", tests_passed, tests_total);
    printf("=================================================\n");
    
    return tests_passed == tests_total ? 0 : 1;
}