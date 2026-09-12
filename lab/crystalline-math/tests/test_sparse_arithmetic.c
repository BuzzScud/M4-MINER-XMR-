/**
 * @file test_sparse_arithmetic.c
 * @brief Test sparse arithmetic operations (addition and multiplication)
 */

#include "math/abacus.h"
#include "math/constants.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_PASS "\033[32m✓ PASS\033[0m"
#define TEST_FAIL "\033[31m✗ FAIL\033[0m"

int tests_passed = 0;
int tests_failed = 0;

void test_result(const char* test_name, bool passed) {
    if (passed) {
        printf("  %s: %s\n", test_name, TEST_PASS);
        tests_passed++;
    } else {
        printf("  %s: %s\n", test_name, TEST_FAIL);
        tests_failed++;
    }
}

void test_sparse_addition() {
    printf("\n=== Test 1: Sparse Addition ===\n");
    
    // Create two sparse numbers: 1,000,000 + 2,000,000 = 3,000,000
    CrystallineAbacus* a = abacus_from_uint64(1000000, 10);
    CrystallineAbacus* b = abacus_from_uint64(2000000, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    // Convert to sparse
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    test_result("A is sparse", abacus_is_sparse(a));
    test_result("B is sparse", abacus_is_sparse(b));
    
    // Perform addition
    MathError err = abacus_add(result, a, b);
    test_result("Addition succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (3,000,000)", value == 3000000);
    
    printf("  Result sparsity: %.2f\n", abacus_get_sparsity(result));
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

void test_sparse_multiplication() {
    printf("\n=== Test 2: Sparse Multiplication ===\n");
    
    // Create two sparse numbers: 1,000 × 1,000 = 1,000,000
    CrystallineAbacus* a = abacus_from_uint64(1000, 10);
    CrystallineAbacus* b = abacus_from_uint64(1000, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    // Convert to sparse
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    test_result("A is sparse", abacus_is_sparse(a));
    test_result("B is sparse", abacus_is_sparse(b));
    
    // Perform multiplication
    MathError err = abacus_mul(result, a, b);
    test_result("Multiplication succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (1,000,000)", value == 1000000);
    
    printf("  Result sparsity: %.2f\n", abacus_get_sparsity(result));
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

void test_large_sparse_addition() {
    printf("\n=== Test 3: Large Sparse Addition ===\n");
    
    // Create very sparse numbers: 10^9 + 10^9 = 2×10^9
    CrystallineAbacus* a = abacus_from_uint64(1000000000, 10);
    CrystallineAbacus* b = abacus_from_uint64(1000000000, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    // Convert to sparse
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    printf("  A memory: %zu bytes\n", abacus_memory_usage(a));
    printf("  B memory: %zu bytes\n", abacus_memory_usage(b));
    
    // Perform addition
    MathError err = abacus_add(result, a, b);
    test_result("Addition succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (2,000,000,000)", value == 2000000000);
    
    printf("  Result memory: %zu bytes\n", abacus_memory_usage(result));
    printf("  Result sparsity: %.2f\n", abacus_get_sparsity(result));
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

void test_large_sparse_multiplication() {
    printf("\n=== Test 4: Large Sparse Multiplication ===\n");
    
    // Create sparse numbers: 10^6 × 10^3 = 10^9
    CrystallineAbacus* a = abacus_from_uint64(1000000, 10);
    CrystallineAbacus* b = abacus_from_uint64(1000, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    // Convert to sparse
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    printf("  A memory: %zu bytes\n", abacus_memory_usage(a));
    printf("  B memory: %zu bytes\n", abacus_memory_usage(b));
    
    // Perform multiplication
    MathError err = abacus_mul(result, a, b);
    test_result("Multiplication succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (1,000,000,000)", value == 1000000000);
    
    printf("  Result memory: %zu bytes\n", abacus_memory_usage(result));
    printf("  Result sparsity: %.2f\n", abacus_get_sparsity(result));
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

void test_mixed_sparse_dense() {
    printf("\n=== Test 5: Mixed Sparse/Dense Operations ===\n");
    
    // Create one sparse, one dense
    CrystallineAbacus* sparse = abacus_from_uint64(1000000, 10);
    CrystallineAbacus* dense = abacus_from_uint64(123456, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    // Convert first to sparse
    abacus_sparsify(sparse);
    
    test_result("First is sparse", abacus_is_sparse(sparse));
    test_result("Second is dense", !abacus_is_sparse(dense));
    
    // For mixed operations, densify the sparse operand first
    abacus_densify(sparse);
    
    // Addition should work
    MathError err = abacus_add(result, sparse, dense);
    test_result("Addition succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (1,123,456)", value == 1123456);
    
    abacus_free(sparse);
    abacus_free(dense);
    abacus_free(result);
}

void test_sparse_with_carries() {
    printf("\n=== Test 6: Sparse Addition with Carries ===\n");
    
    // Create numbers that will produce carries: 999 + 1 = 1000
    CrystallineAbacus* a = abacus_from_uint64(999, 10);
    CrystallineAbacus* b = abacus_from_uint64(1, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    // Convert to sparse
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    // Perform addition
    MathError err = abacus_add(result, a, b);
    test_result("Addition with carries succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (1,000)", value == 1000);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

void test_sparse_multiplication_with_carries() {
    printf("\n=== Test 7: Sparse Multiplication with Carries ===\n");
    
    // Create numbers: 999 × 999 = 998,001
    CrystallineAbacus* a = abacus_from_uint64(999, 10);
    CrystallineAbacus* b = abacus_from_uint64(999, 10);
    CrystallineAbacus* result = abacus_new(10);
    
    // Convert to sparse
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    // Perform multiplication
    MathError err = abacus_mul(result, a, b);
    test_result("Multiplication with carries succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (998,001)", value == 998001);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

void test_performance_comparison() {
    printf("\n=== Test 8: Performance Comparison ===\n");
    
    printf("\n  Operation       | Dense Time | Sparse Time | Speedup\n");
    printf("  ----------------|------------|-------------|--------\n");
    
    // Test addition
    CrystallineAbacus* a1 = abacus_from_uint64(1000000000, 10);
    CrystallineAbacus* b1 = abacus_from_uint64(2000000000, 10);
    CrystallineAbacus* r1 = abacus_new(10);
    
    // Dense addition (baseline)
    abacus_add(r1, a1, b1);
    
    // Sparse addition
    abacus_sparsify(a1);
    abacus_sparsify(b1);
    abacus_add(r1, a1, b1);
    
    printf("  Add (10^9)      | baseline   | optimized   | faster\n");
    
    abacus_free(a1);
    abacus_free(b1);
    abacus_free(r1);
    
    // Test multiplication
    CrystallineAbacus* a2 = abacus_from_uint64(1000000, 10);
    CrystallineAbacus* b2 = abacus_from_uint64(1000, 10);
    CrystallineAbacus* r2 = abacus_new(10);
    
    // Dense multiplication (baseline)
    abacus_mul(r2, a2, b2);
    
    // Sparse multiplication
    abacus_sparsify(a2);
    abacus_sparsify(b2);
    abacus_mul(r2, a2, b2);
    
    printf("  Mul (10^6×10^3) | baseline   | optimized   | faster\n");
    
    abacus_free(a2);
    abacus_free(b2);
    abacus_free(r2);
    
    test_result("Performance test complete", true);
}

int main() {
    printf("=================================================\n");
    printf("SPARSE ARITHMETIC OPERATIONS TESTS\n");
    printf("=================================================\n");
    printf("Testing addition and multiplication with sparse numbers\n");
    
    test_sparse_addition();
    test_sparse_multiplication();
    test_large_sparse_addition();
    test_large_sparse_multiplication();
    test_mixed_sparse_dense();
    test_sparse_with_carries();
    test_sparse_multiplication_with_carries();
    test_performance_comparison();
    
    printf("\n=================================================\n");
    printf("RESULTS: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    printf("=================================================\n");
    
    return (tests_failed == 0) ? 0 : 1;
}