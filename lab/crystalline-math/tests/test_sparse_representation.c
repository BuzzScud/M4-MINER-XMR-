/**
 * @file test_sparse_representation.c
 * @brief Test sparse representation and memory optimization
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

void test_sparse_conversion() {
    printf("\n=== Test 1: Sparse Conversion ===\n");
    
    // Create a sparse number: 1,000,000,000 (1 billion)
    CrystallineAbacus* num = abacus_from_uint64(1000000000, 10);
    
    // Check initial state (should be dense)
    test_result("Initial state is dense", !abacus_is_sparse(num));
    
    // Get initial memory usage
    size_t dense_memory = abacus_memory_usage(num);
    printf("  Dense memory: %zu bytes\n", dense_memory);
    
    // Convert to sparse
    MathError err = abacus_sparsify(num);
    test_result("Sparsify succeeds", err == MATH_SUCCESS);
    test_result("Now in sparse mode", abacus_is_sparse(num));
    
    // Get sparse memory usage
    size_t sparse_memory = abacus_memory_usage(num);
    printf("  Sparse memory: %zu bytes\n", sparse_memory);
    
    // Calculate reduction
    double reduction = 100.0 * (1.0 - ((double)sparse_memory / (double)dense_memory));
    printf("  Memory reduction: %.1f%%\n", reduction);
    test_result("Memory reduction > 50%", reduction > 50.0);
    
    // Convert back to dense
    err = abacus_densify(num);
    test_result("Densify succeeds", err == MATH_SUCCESS);
    test_result("Back in dense mode", !abacus_is_sparse(num));
    
    // Verify value is still correct
    uint64_t value;
    abacus_to_uint64(num, &value);
    test_result("Value preserved (1,000,000,000)", value == 1000000000);
    
    abacus_free(num);
}

void test_sparsity_calculation() {
    printf("\n=== Test 2: Sparsity Calculation ===\n");
    
    // Test 1: All zeros (100% sparse)
    CrystallineAbacus* zero = abacus_from_uint64(0, 10);
    double sparsity = abacus_get_sparsity(zero);
    printf("  Zero sparsity: %.2f\n", sparsity);
    test_result("Zero is 100% sparse", sparsity >= 0.99);
    abacus_free(zero);
    
    // Test 2: Dense number (low sparsity)
    CrystallineAbacus* dense = abacus_from_uint64(123456789, 10);
    sparsity = abacus_get_sparsity(dense);
    printf("  Dense number (123456789) sparsity: %.2f\n", sparsity);
    test_result("Dense number has low sparsity", sparsity < 0.2);
    abacus_free(dense);
    
    // Test 3: Sparse number (high sparsity)
    CrystallineAbacus* sparse = abacus_from_uint64(1000000000, 10);
    sparsity = abacus_get_sparsity(sparse);
    printf("  Sparse number (1,000,000,000) sparsity: %.2f\n", sparsity);
    test_result("Sparse number has high sparsity", sparsity > 0.8);
    abacus_free(sparse);
}

void test_automatic_optimization() {
    printf("\n=== Test 3: Automatic Optimization ===\n");
    
    // Create a sparse number
    CrystallineAbacus* num = abacus_from_uint64(1000000, 10);
    
    // Should start dense
    test_result("Starts in dense mode", !abacus_is_sparse(num));
    
    // Optimize - should switch to sparse
    MathError err = abacus_optimize_representation(num);
    test_result("Optimization succeeds", err == MATH_SUCCESS);
    test_result("Switched to sparse mode", abacus_is_sparse(num));
    
    // Convert to dense number by adding digits
    CrystallineAbacus* dense_num = abacus_from_uint64(123456, 10);
    
    // Should start dense
    test_result("Dense number starts dense", !abacus_is_sparse(dense_num));
    
    // Optimize - should stay dense
    err = abacus_optimize_representation(dense_num);
    test_result("Optimization succeeds", err == MATH_SUCCESS);
    test_result("Stays in dense mode", !abacus_is_sparse(dense_num));
    
    abacus_free(num);
    abacus_free(dense_num);
}

void test_memory_usage() {
    printf("\n=== Test 4: Memory Usage Comparison ===\n");
    
    // Test various number sizes
    uint64_t test_values[] = {
        10,                  // Small
        1000,                // Medium
        1000000,             // Large sparse
        123456789,           // Large dense
        1000000000           // Very large sparse
    };
    
    printf("\n  Number          | Dense (bytes) | Sparse (bytes) | Reduction\n");
    printf("  ----------------|---------------|----------------|----------\n");
    
    for (size_t i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        CrystallineAbacus* num = abacus_from_uint64(test_values[i], 10);
        
        // Get dense memory
        size_t dense_mem = abacus_memory_usage(num);
        
        // Convert to sparse
        abacus_sparsify(num);
        size_t sparse_mem = abacus_memory_usage(num);
        
        // Calculate reduction
        double reduction = 100.0 * (1.0 - ((double)sparse_mem / (double)dense_mem));
        
        printf("  %-15llu | %13zu | %14zu | %7.1f%%\n",
               (unsigned long long)test_values[i], dense_mem, sparse_mem, reduction);
        
        abacus_free(num);
    }
}

void test_operations_with_sparse() {
    printf("\n=== Test 5: Operations with Sparse Numbers ===\n");
    
    // Create two sparse numbers
    CrystallineAbacus* a = abacus_from_uint64(1000000, 10);
    CrystallineAbacus* b = abacus_from_uint64(2000000, 10);
    
    // Convert to sparse
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    test_result("A is sparse", abacus_is_sparse(a));
    test_result("B is sparse", abacus_is_sparse(b));
    
    // Note: Actual sparse arithmetic operations will be implemented in Phase 2
    // For now, we test that sparse numbers can be converted back to dense for operations
    
    // Convert back to dense for operations
    abacus_densify(a);
    abacus_densify(b);
    
    test_result("A converted back to dense", !abacus_is_sparse(a));
    test_result("B converted back to dense", !abacus_is_sparse(b));
    
    // Perform addition
    CrystallineAbacus* result = abacus_new(10);
    MathError err = abacus_add(result, a, b);
    test_result("Addition succeeds", err == MATH_SUCCESS);
    
    // Verify result
    uint64_t value;
    abacus_to_uint64(result, &value);
    test_result("Result is correct (3,000,000)", value == 3000000);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

int main() {
    printf("=================================================\n");
    printf("SPARSE REPRESENTATION TESTS\n");
    printf("=================================================\n");
    printf("Testing memory optimization for sparse numbers\n");
    
    test_sparse_conversion();
    test_sparsity_calculation();
    test_automatic_optimization();
    test_memory_usage();
    test_operations_with_sparse();
    
    printf("\n=================================================\n");
    printf("RESULTS: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    printf("=================================================\n");
    
    return (tests_failed == 0) ? 0 : 1;
}