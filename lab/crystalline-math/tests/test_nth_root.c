/**
 * @file test_nth_root.c
 * @brief Test nth root functions
 */

#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>

int tests_passed = 0;
int tests_total = 0;

void test_sqrt() {
    printf("\n=== Test 1: Square Root (n=2) ===\n");
    
    uint32_t base = 10;
    
    // Test sqrt(4) = 2
    tests_total++;
    CrystallineAbacus* num = abacus_from_uint64(4, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_root(result, num, 2);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 2) {
            printf("  √4 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  √4 = %lu (expected 2) ✗\n", val);
        }
    } else {
        printf("  √4 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
    
    // Test sqrt(100) = 10
    tests_total++;
    num = abacus_from_uint64(100, base);
    result = abacus_new(base);
    
    err = abacus_root(result, num, 2);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 10) {
            printf("  √100 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  √100 = %lu (expected 10) ✗\n", val);
        }
    } else {
        printf("  √100 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
}

void test_cube_root() {
    printf("\n=== Test 2: Cube Root (n=3) ===\n");
    
    uint32_t base = 10;
    
    // Test ∛8 = 2
    tests_total++;
    CrystallineAbacus* num = abacus_from_uint64(8, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_root(result, num, 3);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 2) {
            printf("  ∛8 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ∛8 = %lu (expected 2) ✗\n", val);
        }
    } else {
        printf("  ∛8 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
    
    // Test ∛27 = 3
    tests_total++;
    num = abacus_from_uint64(27, base);
    result = abacus_new(base);
    
    err = abacus_root(result, num, 3);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 3) {
            printf("  ∛27 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ∛27 = %lu (expected 3) ✗\n", val);
        }
    } else {
        printf("  ∛27 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
    
    // Test ∛1000 = 10
    tests_total++;
    num = abacus_from_uint64(1000, base);
    result = abacus_new(base);
    
    err = abacus_root(result, num, 3);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 10) {
            printf("  ∛1000 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ∛1000 = %lu (expected 10) ✗\n", val);
        }
    } else {
        printf("  ∛1000 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
}

void test_higher_roots() {
    printf("\n=== Test 3: Higher Roots (n=4,5,6) ===\n");
    
    uint32_t base = 10;
    
    // Test ⁴√16 = 2
    tests_total++;
    CrystallineAbacus* num = abacus_from_uint64(16, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_root(result, num, 4);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 2) {
            printf("  ⁴√16 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ⁴√16 = %lu (expected 2) ✗\n", val);
        }
    } else {
        printf("  ⁴√16 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
    
    // Test ⁵√32 = 2
    tests_total++;
    num = abacus_from_uint64(32, base);
    result = abacus_new(base);
    
    err = abacus_root(result, num, 5);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 2) {
            printf("  ⁵√32 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ⁵√32 = %lu (expected 2) ✗\n", val);
        }
    } else {
        printf("  ⁵√32 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
    
    // Test ⁶√64 = 2
    tests_total++;
    num = abacus_from_uint64(64, base);
    result = abacus_new(base);
    
    err = abacus_root(result, num, 6);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 2) {
            printf("  ⁶√64 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ⁶√64 = %lu (expected 2) ✗\n", val);
        }
    } else {
        printf("  ⁶√64 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
}

void test_edge_cases() {
    printf("\n=== Test 4: Edge Cases ===\n");
    
    uint32_t base = 10;
    
    // Test ⁿ√0 = 0
    tests_total++;
    CrystallineAbacus* num = abacus_from_uint64(0, base);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = abacus_root(result, num, 3);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 0) {
            printf("  ∛0 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ∛0 = %lu (expected 0) ✗\n", val);
        }
    } else {
        printf("  ∛0 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
    
    // Test ⁿ√1 = 1
    tests_total++;
    num = abacus_from_uint64(1, base);
    result = abacus_new(base);
    
    err = abacus_root(result, num, 5);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 1) {
            printf("  ⁵√1 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ⁵√1 = %lu (expected 1) ✗\n", val);
        }
    } else {
        printf("  ⁵√1 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
    
    // Test ¹√n = n
    tests_total++;
    num = abacus_from_uint64(42, base);
    result = abacus_new(base);
    
    err = abacus_root(result, num, 1);
    if (err == MATH_SUCCESS) {
        uint64_t val;
        if (abacus_to_uint64(result, &val) == MATH_SUCCESS && val == 42) {
            printf("  ¹√42 = %lu ✓\n", val);
            tests_passed++;
        } else {
            printf("  ¹√42 = %lu (expected 42) ✗\n", val);
        }
    } else {
        printf("  ¹√42 failed with error %d ✗\n", err);
    }
    
    abacus_free(num);
    abacus_free(result);
}

int main() {
    printf("=================================================\n");
    printf("NTH ROOT TESTS\n");
    printf("=================================================\n");
    
    test_sqrt();
    test_cube_root();
    test_higher_roots();
    test_edge_cases();
    
    printf("\n=================================================\n");
    printf("RESULTS: %d/%d tests passed\n", tests_passed, tests_total);
    printf("=================================================\n");
    
    return tests_passed == tests_total ? 0 : 1;
}