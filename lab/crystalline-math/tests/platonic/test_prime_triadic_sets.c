/**
 * @file test_prime_triadic_sets.c
 * @brief Test prime triadic set generation
 */

#include "math/prime_triadic_sets.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// ============================================================================
// TEST HELPERS
// ============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("\n=== Testing: %s ===\n", name);

#define ASSERT_TRUE(expr) \
    do { \
        if (expr) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
            printf("FAILED: %s (line %d)\n", #expr, __LINE__); \
        } \
    } while(0)

#define ASSERT_FALSE(expr) \
    do { \
        if (!(expr)) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
            printf("FAILED: !(%s) (line %d)\n", #expr, __LINE__); \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) == (b)) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
            printf("FAILED: %s == %s (%u != %u) (line %d)\n", \
                   #a, #b, (unsigned)(a), (unsigned)(b), __LINE__); \
        } \
    } while(0)

// ============================================================================
// TESTS
// ============================================================================

void test_exclusion_rules() {
    TEST("Exclusion Rules");
    
    // Test 5D with 2 sets
    printf("\n5D with 2 sets:\n");
    prime_triadic_print_exclusions(5, 2);
    
    ASSERT_TRUE(prime_triadic_is_excluded(2, 5, 2));  // 2 sets
    ASSERT_TRUE(prime_triadic_is_excluded(3, 5, 2));  // triadic
    ASSERT_TRUE(prime_triadic_is_excluded(5, 5, 2));  // dimension
    ASSERT_FALSE(prime_triadic_is_excluded(7, 5, 2)); // OK
    
    // Test 6D with 2 sets
    printf("\n6D with 2 sets:\n");
    prime_triadic_print_exclusions(6, 2);
    
    ASSERT_TRUE(prime_triadic_is_excluded(2, 6, 2));  // 2 sets
    ASSERT_TRUE(prime_triadic_is_excluded(3, 6, 2));  // triadic
    ASSERT_FALSE(prime_triadic_is_excluded(6, 6, 2)); // 6 not prime
    ASSERT_FALSE(prime_triadic_is_excluded(5, 6, 2)); // OK
    
    // Test 7D with 2 sets
    printf("\n7D with 2 sets:\n");
    prime_triadic_print_exclusions(7, 2);
    
    ASSERT_TRUE(prime_triadic_is_excluded(2, 7, 2));  // 2 sets
    ASSERT_TRUE(prime_triadic_is_excluded(3, 7, 2));  // triadic
    ASSERT_TRUE(prime_triadic_is_excluded(7, 7, 2));  // dimension
    ASSERT_FALSE(prime_triadic_is_excluded(5, 7, 2)); // OK
}

void test_5d_generation() {
    TEST("5D Triadic Set Generation");
    
    PrimeTriadicSets* sets = prime_triadic_preset_5d();
    ASSERT_TRUE(sets != NULL);
    
    if (sets) {
        printf("\n");
        prime_triadic_print(sets);
        
        // Should have 2 sets
        ASSERT_EQ(sets->num_sets, 2);
        
        // Should have 6 bases total
        ASSERT_EQ(sets->num_bases, 6);
        
        // Should exclude 2, 3, 5
        ASSERT_EQ(sets->num_excluded, 3);
        
        // First set should be {7, 11, 13}
        ASSERT_EQ(sets->sets[0].primes[0], 7);
        ASSERT_EQ(sets->sets[0].primes[1], 11);
        ASSERT_EQ(sets->sets[0].primes[2], 13);
        
        // Second set should be {17, 19, 23}
        ASSERT_EQ(sets->sets[1].primes[0], 17);
        ASSERT_EQ(sets->sets[1].primes[1], 19);
        ASSERT_EQ(sets->sets[1].primes[2], 23);
        
        // Validate
        ASSERT_TRUE(prime_triadic_validate_all(sets));
        
        prime_triadic_free(sets);
    }
}

void test_6d_generation() {
    TEST("6D Triadic Set Generation");
    
    PrimeTriadicSets* sets = prime_triadic_preset_6d();
    ASSERT_TRUE(sets != NULL);
    
    if (sets) {
        printf("\n");
        prime_triadic_print(sets);
        
        // Should have 2 sets
        ASSERT_EQ(sets->num_sets, 2);
        
        // Should have 6 bases total
        ASSERT_EQ(sets->num_bases, 6);
        
        // Should exclude 2, 3 (not 6, since 6 is not prime)
        ASSERT_EQ(sets->num_excluded, 2);
        
        // First set should be {5, 7, 11}
        ASSERT_EQ(sets->sets[0].primes[0], 5);
        ASSERT_EQ(sets->sets[0].primes[1], 7);
        ASSERT_EQ(sets->sets[0].primes[2], 11);
        
        // Second set should be {13, 17, 19}
        ASSERT_EQ(sets->sets[1].primes[0], 13);
        ASSERT_EQ(sets->sets[1].primes[1], 17);
        ASSERT_EQ(sets->sets[1].primes[2], 19);
        
        // Validate
        ASSERT_TRUE(prime_triadic_validate_all(sets));
        
        prime_triadic_free(sets);
    }
}

void test_7d_generation() {
    TEST("7D Triadic Set Generation");
    
    PrimeTriadicSets* sets = prime_triadic_preset_7d();
    ASSERT_TRUE(sets != NULL);
    
    if (sets) {
        printf("\n");
        prime_triadic_print(sets);
        
        // Should have 2 sets
        ASSERT_EQ(sets->num_sets, 2);
        
        // Should have 6 bases total
        ASSERT_EQ(sets->num_bases, 6);
        
        // Should exclude 2, 3, 7
        ASSERT_EQ(sets->num_excluded, 3);
        
        // First set should be {5, 11, 13}
        ASSERT_EQ(sets->sets[0].primes[0], 5);
        ASSERT_EQ(sets->sets[0].primes[1], 11);
        ASSERT_EQ(sets->sets[0].primes[2], 13);
        
        // Second set should be {17, 19, 23}
        ASSERT_EQ(sets->sets[1].primes[0], 17);
        ASSERT_EQ(sets->sets[1].primes[1], 19);
        ASSERT_EQ(sets->sets[1].primes[2], 23);
        
        // Validate
        ASSERT_TRUE(prime_triadic_validate_all(sets));
        
        prime_triadic_free(sets);
    }
}

void test_classical_3d() {
    TEST("Classical 3D (Historical)");
    
    PrimeTriadicSets* sets = prime_triadic_preset_classical_3d();
    ASSERT_TRUE(sets != NULL);
    
    if (sets) {
        printf("\n");
        prime_triadic_print(sets);
        
        // Should have 1 set
        ASSERT_EQ(sets->num_sets, 1);
        
        // Should have 3 bases total
        ASSERT_EQ(sets->num_bases, 3);
        
        // Set should be {2, 3, 5}
        ASSERT_EQ(sets->sets[0].primes[0], 2);
        ASSERT_EQ(sets->sets[0].primes[1], 3);
        ASSERT_EQ(sets->sets[0].primes[2], 5);
        
        printf("\nNote: Classical 3D set {2,3,5} violates the constraint\n");
        printf("(uses 3, which is the triadic structure itself)\n");
        printf("This is a historical/transitional case.\n");
        
        prime_triadic_free(sets);
    }
}

void test_4d_extended() {
    TEST("4D Extended (Historical)");
    
    PrimeTriadicSets* sets = prime_triadic_preset_4d();
    ASSERT_TRUE(sets != NULL);
    
    if (sets) {
        printf("\n");
        prime_triadic_print(sets);
        
        // Should have 2 sets
        ASSERT_EQ(sets->num_sets, 2);
        
        // Should have 6 bases total
        ASSERT_EQ(sets->num_bases, 6);
        
        // First set should be {2, 3, 5}
        ASSERT_EQ(sets->sets[0].primes[0], 2);
        ASSERT_EQ(sets->sets[0].primes[1], 3);
        ASSERT_EQ(sets->sets[0].primes[2], 5);
        
        // Second set should be {7, 11, 13}
        ASSERT_EQ(sets->sets[1].primes[0], 7);
        ASSERT_EQ(sets->sets[1].primes[1], 11);
        ASSERT_EQ(sets->sets[1].primes[2], 13);
        
        printf("\nNote: 4D extended set uses {2,3,5,7,11,13}\n");
        printf("This matches the current 186 tower system.\n");
        printf("Violates constraint (uses 2 and 3) but is historical.\n");
        
        prime_triadic_free(sets);
    }
}

void test_custom_generation() {
    TEST("Custom Generation");
    
    // Create custom config
    TriadicSetConfig config = {
        .dimension = 8,
        .num_sets = 3,
        .max_prime = 100
    };
    
    printf("\n8D with 3 sets:\n");
    prime_triadic_print_exclusions(8, 3);
    
    PrimeTriadicSets* sets = prime_triadic_generate(&config);
    ASSERT_TRUE(sets != NULL);
    
    if (sets) {
        printf("\n");
        prime_triadic_print(sets);
        
        // Should have 3 sets
        ASSERT_EQ(sets->num_sets, 3);
        
        // Should have 9 bases total
        ASSERT_EQ(sets->num_bases, 9);
        
        // Should exclude 2, 3 (8 not prime, 3 is prime)
        ASSERT_EQ(sets->num_excluded, 2);
        
        // Validate
        ASSERT_TRUE(prime_triadic_validate_all(sets));
        
        prime_triadic_free(sets);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("==============================================\n");
    printf("Prime Triadic Set Generation Tests\n");
    printf("==============================================\n");
    
    test_exclusion_rules();
    test_5d_generation();
    test_6d_generation();
    test_7d_generation();
    test_classical_3d();
    test_4d_extended();
    test_custom_generation();
    
    printf("\n==============================================\n");
    printf("Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("==============================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}