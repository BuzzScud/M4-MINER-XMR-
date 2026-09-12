/**
 * Cross-Component Integration Tests - Simplified Version
 * 
 * Tests key interactions between components with correct API usage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "math/abacus.h"
#include "math/transcendental.h"

/* Ensure math.h functions are available */
#ifndef fabs
#define fabs(x) __builtin_fabs(x)
#endif

#define TEST_PRECISION 20
#define EPSILON 1e-10

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

void print_test_result(const char* test_name, int passed) {
    tests_run++;
    if (passed) {
        tests_passed++;
        printf("✓ %s\n", test_name);
    } else {
        tests_failed++;
        printf("✗ %s\n", test_name);
    }
}

// ============================================================================
// TEST SUITE 1: SPARSE + O(1) OPERATIONS
// ============================================================================

void test_sparse_addition() {
    printf("\n=== Test 1.1: Sparse Addition ===\n");
    
    int base = 10;
    
    // Create large sparse numbers
    CrystallineAbacus* a = abacus_from_uint64(1000000000ULL, base);  // 10^9
    CrystallineAbacus* b = abacus_from_uint64(1000000ULL, base);     // 10^6
    CrystallineAbacus* result = abacus_new(base);
    
    // Sparsify
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    printf("  a = 10^9, sparsity = %.2f%%\n", abacus_get_sparsity(a) * 100);
    printf("  b = 10^6, sparsity = %.2f%%\n", abacus_get_sparsity(b) * 100);
    
    // Add
    MathError err = abacus_add(result, a, b);
    
    uint64_t result_val = 0;
    abacus_to_uint64(result, &result_val);
    uint64_t expected = 1001000000ULL;
    
    int passed = (err == MATH_SUCCESS && result_val == expected);
    
    printf("  Result: %llu\n", (unsigned long long)result_val);
    printf("  Expected: %llu\n", (unsigned long long)expected);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    print_test_result("Sparse addition with large numbers", passed);
}

void test_sparse_multiplication() {
    printf("\n=== Test 1.2: Sparse Multiplication ===\n");
    
    int base = 10;
    
    CrystallineAbacus* a = abacus_from_uint64(1000000ULL, base);  // 10^6
    CrystallineAbacus* b = abacus_from_uint64(1000ULL, base);     // 10^3
    CrystallineAbacus* result = abacus_new(base);
    
    abacus_sparsify(a);
    abacus_sparsify(b);
    
    printf("  a = 10^6, sparsity = %.2f%%\n", abacus_get_sparsity(a) * 100);
    printf("  b = 10^3, sparsity = %.2f%%\n", abacus_get_sparsity(b) * 100);
    
    MathError err = abacus_mul(result, a, b);
    
    uint64_t result_val = 0;
    abacus_to_uint64(result, &result_val);
    uint64_t expected = 1000000000ULL;  // 10^9
    
    int passed = (err == MATH_SUCCESS && result_val == expected);
    
    printf("  Result: %llu\n", (unsigned long long)result_val);
    printf("  Expected: %llu\n", (unsigned long long)expected);
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    print_test_result("Sparse multiplication with large numbers", passed);
}

void test_exponentiation() {
    printf("\n=== Test 1.3: Exponentiation ===\n");
    
    int base = 10;
    
    CrystallineAbacus* a = abacus_from_uint64(10ULL, base);
    CrystallineAbacus* result = abacus_new(base);
    
    // Compute 10^6
    MathError err = abacus_pow_uint64(result, a, 6);
    
    uint64_t result_val = 0;
    abacus_to_uint64(result, &result_val);
    uint64_t expected = 1000000ULL;
    
    int passed = (err == MATH_SUCCESS && result_val == expected);
    
    printf("  10^6 = %llu\n", (unsigned long long)result_val);
    
    // Sparsify and check
    abacus_sparsify(result);
    printf("  Sparsity after sparsify = %.2f%%\n", abacus_get_sparsity(result) * 100);
    
    abacus_free(a);
    abacus_free(result);
    
    print_test_result("Exponentiation (10^6)", passed);
}

void test_memory_savings() {
    printf("\n=== Test 1.4: Memory Savings ===\n");
    
    int base = 10;
    
    CrystallineAbacus* a = abacus_from_uint64(1000000000ULL, base);  // 10^9
    
    size_t dense_mem = abacus_memory_usage(a);
    printf("  Dense memory: %zu bytes\n", dense_mem);
    
    abacus_sparsify(a);
    
    size_t sparse_mem = abacus_memory_usage(a);
    printf("  Sparse memory: %zu bytes\n", sparse_mem);
    
    double reduction = 100.0 * (1.0 - (double)sparse_mem / (double)dense_mem);
    printf("  Memory reduction: %.1f%%\n", reduction);
    
    int passed = (reduction > 70.0);  // Should be > 70% reduction
    
    abacus_free(a);
    
    print_test_result("Memory savings (>70% reduction)", passed);
}

// ============================================================================
// TEST SUITE 2: TRANSCENDENTAL FUNCTIONS
// ============================================================================

void test_transcendental_sqrt() {
    printf("\n=== Test 2.1: Square Root ===\n");
    
    int base = 60;
    int precision = TEST_PRECISION;
    
    CrystallineAbacus* a = abacus_from_double(4.0, base, precision);
    CrystallineAbacus* result = abacus_new(base);
    
    MathError err = math_sqrt_abacus(result, a, precision);
    
    double result_val = 0;
    abacus_to_double(result, &result_val);
    double expected = 2.0;
    
    int passed = (err == MATH_SUCCESS && fabs(result_val - expected) < EPSILON);
    
    printf("  sqrt(4) = %.10f\n", result_val);
    printf("  Expected: %.10f\n", expected);
    printf("  Error: %.2e\n", fabs(result_val - expected));
    
    abacus_free(a);
    abacus_free(result);
    
    print_test_result("Square root (sqrt(4) = 2)", passed);
}

void test_transcendental_sin_cos() {
    printf("\n=== Test 2.2: Sine and Cosine ===\n");
    
    int base = 60;
    int precision = TEST_PRECISION;
    
    // Test sin(0) = 0, cos(0) = 1
    CrystallineAbacus* zero = abacus_from_double(0.0, base, precision);
    CrystallineAbacus* sin_result = abacus_new(base);
    CrystallineAbacus* cos_result = abacus_new(base);
    
    MathError err1 = math_sin_abacus(sin_result, zero, precision);
    MathError err2 = math_cos_abacus(cos_result, zero, precision);
    
    double sin_val = 0, cos_val = 0;
    abacus_to_double(sin_result, &sin_val);
    abacus_to_double(cos_result, &cos_val);
    
    int passed = (err1 == MATH_SUCCESS && err2 == MATH_SUCCESS &&
                  fabs(sin_val) < EPSILON && fabs(cos_val - 1.0) < EPSILON);
    
    printf("  sin(0) = %.10f (expected 0.0)\n", sin_val);
    printf("  cos(0) = %.10f (expected 1.0)\n", cos_val);
    
    abacus_free(zero);
    abacus_free(sin_result);
    abacus_free(cos_result);
    
    print_test_result("Sine and cosine at zero", passed);
}

// ============================================================================
// TEST SUITE 3: DIFFERENT BASES
// ============================================================================

void test_different_bases() {
    printf("\n=== Test 3.1: Different Bases ===\n");
    
    int bases[] = {10, 12, 60, 100};
    int num_bases = 4;
    int all_passed = 1;
    
    for (int i = 0; i < num_bases; i++) {
        int base = bases[i];
        
        // Test basic arithmetic in this base
        CrystallineAbacus* a = abacus_from_uint64(100ULL, base);
        CrystallineAbacus* b = abacus_from_uint64(50ULL, base);
        CrystallineAbacus* result = abacus_new(base);
        
        abacus_add(result, a, b);
        
        uint64_t result_val = 0;
        abacus_to_uint64(result, &result_val);
        
        int passed = (result_val == 150ULL);
        
        printf("  Base %d: 100 + 50 = %llu %s\n", 
               base, (unsigned long long)result_val, 
               passed ? "✓" : "✗");
        
        if (!passed) all_passed = 0;
        
        abacus_free(a);
        abacus_free(b);
        abacus_free(result);
    }
    
    print_test_result("Arithmetic with different bases", all_passed);
}

// ============================================================================
// TEST SUITE 4: NTT INTEGRATION
// ============================================================================

void test_ntt_basic() {
    printf("\n=== Test 4.1: NTT Basic Operation ===\n");
    
    // Test that NTT context can be created
    NTTContext* ctx = ntt_create(64);  // 64 elements
    
    int passed = (ctx != NULL);
    
    if (ctx) {
        printf("  NTT context created successfully\n");
        printf("  Size: 64 elements\n");
        ntt_free(ctx);
    } else {
        printf("  Failed to create NTT context\n");
    }
    
    print_test_result("NTT context creation", passed);
}

// ============================================================================
// TEST SUITE 5: PRIME GENERATION
// ============================================================================

void test_prime_generation() {
    printf("\n=== Test 5.1: Prime Generation ===\n");
    
    // Test basic prime generation using prime_nth
    uint64_t primes[10];
    
    // Get first 10 primes using prime_nth (1-indexed)
    for (int i = 0; i < 10; i++) {
        primes[i] = prime_nth(i + 1);
    }
    
    // Expected first 10 primes: 2, 3, 5, 7, 11, 13, 17, 19, 23, 29
    uint64_t expected[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
    
    int passed = 1;
    for (int i = 0; i < 10; i++) {
        if (primes[i] != expected[i]) {
            passed = 0;
            break;
        }
    }
    
    printf("  First 10 primes: ");
    for (int i = 0; i < 10; i++) {
        printf("%llu ", (unsigned long long)primes[i]);
    }
    printf("\n");
    
    print_test_result("Prime generation (first 10 primes)", passed);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     CROSS-COMPONENT INTEGRATION TESTS                      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Test Suite 1: Sparse + O(1) Operations
    printf("\n");
    printf("┌────────────────────────────────────────────────────────────┐\n");
    printf("│ TEST SUITE 1: SPARSE + O(1) OPERATIONS                     │\n");
    printf("└────────────────────────────────────────────────────────────┘\n");
    
    test_sparse_addition();
    test_sparse_multiplication();
    test_exponentiation();
    test_memory_savings();
    
    // Test Suite 2: Transcendental Functions
    printf("\n");
    printf("┌────────────────────────────────────────────────────────────┐\n");
    printf("│ TEST SUITE 2: TRANSCENDENTAL FUNCTIONS                     │\n");
    printf("└────────────────────────────────────────────────────────────┘\n");
    
    test_transcendental_sqrt();
    test_transcendental_sin_cos();
    
    // Test Suite 3: Different Bases
    printf("\n");
    printf("┌────────────────────────────────────────────────────────────┐\n");
    printf("│ TEST SUITE 3: DIFFERENT BASES                              │\n");
    printf("└────────────────────────────────────────────────────────────┘\n");
    
    test_different_bases();
    
    // Test Suite 4: NTT Integration
    printf("\n");
    printf("┌────────────────────────────────────────────────────────────┐\n");
    printf("│ TEST SUITE 4: NTT INTEGRATION                              │\n");
    printf("└────────────────────────────────────────────────────────────┘\n");
    
    test_ntt_basic();
    
    // Test Suite 5: Prime Generation
    printf("\n");
    printf("┌────────────────────────────────────────────────────────────┐\n");
    printf("│ TEST SUITE 5: PRIME GENERATION                             │\n");
    printf("└────────────────────────────────────────────────────────────┘\n");
    
    test_prime_generation();
    
    // Print summary
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ TEST SUMMARY                                               ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total Tests:  %3d                                          ║\n", tests_run);
    printf("║ Passed:       %3d                                          ║\n", tests_passed);
    printf("║ Failed:       %3d                                          ║\n", tests_failed);
    printf("║ Success Rate: %3d%%                                         ║\n", 
           tests_run > 0 ? (tests_passed * 100 / tests_run) : 0);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return (tests_failed == 0) ? 0 : 1;
}