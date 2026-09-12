/**
 * @file test_factorization_o1.c
 * @brief Test suite for O(1) factorization
 */

#include "math/factorization.h"
#include "math/prime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Test counters */
static int tests_passed = 0;
static int tests_total = 0;

/* ============================================================================
 * TEST UTILITIES
 * ============================================================================
 */

void print_factorization(const Factorization* fact) {
    printf("  Factors: ");
    for (size_t i = 0; i < fact->num_factors; i++) {
        if (i > 0) printf(" × ");
        printf("%lu", fact->factors[i].prime);
        if (fact->factors[i].exponent > 1) {
            printf("^%u", fact->factors[i].exponent);
        }
    }
    printf("\n");
}

bool verify_factorization(uint64_t n, const Factorization* fact) {
    /* Reconstruct n from factors */
    uint64_t product = 1;
    for (size_t i = 0; i < fact->num_factors; i++) {
        uint64_t prime = fact->factors[i].prime;
        uint32_t exp = fact->factors[i].exponent;
        
        for (uint32_t j = 0; j < exp; j++) {
            product *= prime;
        }
    }
    
    return product == n;
}

void test_factor(uint64_t n, const char* description) {
    tests_total++;
    printf("\nTest %d: Factor %lu (%s)\n", tests_total, n, description);
    
    Factorization fact;
    fact.capacity = 64;  /* Allocate space for up to 64 factors */
    fact.factors = (PrimeFactor*)malloc(fact.capacity * sizeof(PrimeFactor));
    fact.num_factors = 0;
    
    if (!fact.factors) {
        printf("  ✗ FAIL: Memory allocation failed\n");
        return;
    }
    
    MathError err = prime_factor(n, &fact);
    
    if (err != MATH_SUCCESS) {
        printf("  ✗ FAIL: Error %d\n", err);
        free(fact.factors);
        return;
    }
    
    print_factorization(&fact);
    
    if (verify_factorization(n, &fact)) {
        printf("  ✓ PASS: Factorization correct\n");
        tests_passed++;
    } else {
        printf("  ✗ FAIL: Factorization incorrect\n");
    }
    
    free(fact.factors);
}

/* ============================================================================
 * POSITION ANALYSIS TESTS
 * ============================================================================
 */

void test_position_multiplication() {
    printf("\n=== Position Multiplication Table ===\n");
    
    const uint32_t bases[] = {5, 7, 11};
    
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            uint64_t prod = bases[i] * bases[j];
            uint32_t prod_pos = prod % 12;
            
            printf("  %u × %u = %lu (pos %u)\n", 
                   bases[i], bases[j], prod, prod_pos);
        }
    }
}

void test_get_factor_positions() {
    printf("\n=== Factor Position Analysis ===\n");
    
    tests_total++;
    printf("\nTest %d: get_factor_positions(1)\n", tests_total);
    FactorPositions pos1 = get_factor_positions(1);
    printf("  Position 1 can be: ");
    for (size_t i = 0; i < pos1.count; i++) {
        printf("%u ", pos1.positions[i].base);
    }
    printf(" (count: %zu)\n", pos1.count);
    
    if (pos1.count > 0) {
        printf("  ✓ PASS\n");
        tests_passed++;
    } else {
        printf("  ✗ FAIL\n");
    }
    
    tests_total++;
    printf("\nTest %d: get_factor_positions(5)\n", tests_total);
    FactorPositions pos5 = get_factor_positions(5);
    printf("  Position 5 can be: ");
    for (size_t i = 0; i < pos5.count; i++) {
        printf("%u ", pos5.positions[i].base);
    }
    printf(" (count: %zu)\n", pos5.count);
    
    if (pos5.count > 0) {
        printf("  ✓ PASS\n");
        tests_passed++;
    } else {
        printf("  ✗ FAIL\n");
    }
}

/* ============================================================================
 * UTILITY TESTS
 * ============================================================================
 */

void test_isqrt() {
    printf("\n=== Integer Square Root Tests ===\n");
    
    const uint64_t test_cases[][2] = {
        {0, 0},
        {1, 1},
        {4, 2},
        {9, 3},
        {16, 4},
        {25, 5},
        {100, 10},
        {144, 12},
        {10000, 100}
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        tests_total++;
        uint64_t n = test_cases[i][0];
        uint64_t expected = test_cases[i][1];
        uint64_t result = isqrt(n);
        
        printf("Test %d: isqrt(%lu) = %lu (expected %lu)\n", 
               tests_total, n, result, expected);
        
        if (result == expected) {
            printf("  ✓ PASS\n");
            tests_passed++;
        } else {
            printf("  ✗ FAIL\n");
        }
    }
}

/* ============================================================================
 * FACTORIZATION TESTS
 * ============================================================================
 */

void test_small_composites() {
    printf("\n=== Small Composites ===\n");
    
    test_factor(15, "3 × 5");
    test_factor(21, "3 × 7");
    test_factor(35, "5 × 7");
    test_factor(77, "7 × 11");
    test_factor(91, "7 × 13");
}

void test_medium_composites() {
    printf("\n=== Medium Composites ===\n");
    
    test_factor(143, "11 × 13");
    test_factor(221, "13 × 17");
    test_factor(323, "17 × 19");
    test_factor(437, "19 × 23");
}

void test_prime_powers() {
    printf("\n=== Prime Powers ===\n");
    
    test_factor(4, "2²");
    test_factor(8, "2³");
    test_factor(9, "3²");
    test_factor(25, "5²");
    test_factor(49, "7²");
    test_factor(121, "11²");
    test_factor(125, "5³");
}

void test_products_of_three() {
    printf("\n=== Products of 3+ Primes ===\n");
    
    test_factor(30, "2 × 3 × 5");
    test_factor(105, "3 × 5 × 7");
    test_factor(385, "5 × 7 × 11");
    test_factor(1001, "7 × 11 × 13");
}

void test_large_semiprimes() {
    printf("\n=== Large Semiprimes ===\n");
    
    test_factor(9991 * 10007, "9991 × 10007");
    test_factor(10007 * 10009, "10007 × 10009");
}

void test_primes() {
    printf("\n=== Prime Numbers ===\n");
    
    test_factor(2, "prime");
    test_factor(3, "prime");
    test_factor(5, "prime");
    test_factor(7, "prime");
    test_factor(11, "prime");
    test_factor(13, "prime");
    test_factor(17, "prime");
    test_factor(19, "prime");
    test_factor(23, "prime");
}

/* ============================================================================
 * BENCHMARK TESTS
 * ============================================================================
 */

void benchmark_factorization() {
    printf("\n=== Factorization Benchmark ===\n");
    
    const uint64_t test_numbers[] = {
        1001,           /* 7 × 11 × 13 */
        9991 * 10007,   /* Large semiprime */
        12345,          /* 3 × 5 × 823 */
        99991,          /* Prime */
        100000          /* 2^5 × 5^5 */
    };
    
    for (size_t i = 0; i < sizeof(test_numbers) / sizeof(test_numbers[0]); i++) {
        uint64_t n = test_numbers[i];
        
        Factorization fact;
        fact.capacity = 64;
        fact.factors = (PrimeFactor*)malloc(fact.capacity * sizeof(PrimeFactor));
        fact.num_factors = 0;
        
        if (!fact.factors) {
            printf("\nNumber: %lu - Memory allocation failed\n", n);
            continue;
        }
        
        clock_t start = clock();
        MathError err = prime_factor(n, &fact);
        clock_t end = clock();
        
        double time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
        
        printf("\nNumber: %lu\n", n);
        if (err == MATH_SUCCESS) {
            print_factorization(&fact);
            printf("  Time: %.3f ms\n", time_ms);
        } else {
            printf("  Error: %d\n", err);
        }
        
        free(fact.factors);
    }
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */

int main() {
    printf("=================================================\n");
    printf("O(1) FACTORIZATION TEST SUITE\n");
    printf("=================================================\n");
    
    /* Position analysis tests */
    test_position_multiplication();
    test_get_factor_positions();
    
    /* Utility tests */
    test_isqrt();
    
    /* Factorization tests */
    test_small_composites();
    test_medium_composites();
    test_prime_powers();
    test_products_of_three();
    test_large_semiprimes();
    test_primes();
    
    /* Benchmark */
    benchmark_factorization();
    
    /* Summary */
    printf("\n=================================================\n");
    printf("RESULTS: %d/%d tests passed (%.1f%%)\n", 
           tests_passed, tests_total, 
           tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
    printf("=================================================\n");
    
    return tests_passed == tests_total ? 0 : 1;
}