/**
 * @file test_rainbow.c
 * @brief Test suite for rainbow table
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 */

#include "math/rainbow.h"
#include "math/prime.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST(name) \
    static bool test_##name(); \
    static void run_test_##name() { \
        tests_run++; \
        printf("Running test: %s... ", #name); \
        if (test_##name()) { \
            printf("PASS\n"); \
            tests_passed++; \
        } else { \
            printf("FAIL\n"); \
            tests_failed++; \
        } \
    } \
    static bool test_##name()

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("\n  Assertion failed: %s\n", #condition); \
            return false; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            printf("\n  Expected %llu, got %llu\n", (unsigned long long)(b), (unsigned long long)(a)); \
            return false; \
        } \
    } while(0)

/* ============================================================================
 * INITIALIZATION TESTS
 * ============================================================================
 */

TEST(init_cleanup) {
    RainbowTable table;
    
    MathError err = rainbow_init(&table, 100);
    ASSERT(err == MATH_SUCCESS);
    ASSERT(table.entries != NULL);
    ASSERT(table.capacity == 100);
    ASSERT(table.size == 0);
    
    rainbow_cleanup(&table);
    ASSERT(table.entries == NULL);
    ASSERT(table.size == 0);
    
    return true;
}

TEST(init_default_capacity) {
    RainbowTable table;
    
    MathError err = rainbow_init(&table, 0);  /* Use default */
    ASSERT(err == MATH_SUCCESS);
    ASSERT(table.capacity == 1000);  /* Default capacity */
    
    rainbow_cleanup(&table);
    return true;
}

/* ============================================================================
 * POPULATION TESTS
 * ============================================================================
 */

TEST(populate_count_small) {
    RainbowTable table;
    rainbow_init(&table, 0);
    
    /* Populate with first 10 primes */
    MathError err = rainbow_populate_count(&table, 10);
    ASSERT(err == MATH_SUCCESS);
    ASSERT(table.size == 10);
    ASSERT(table.max_prime == 29);  /* 10th prime */
    ASSERT(table.max_index == 10);
    
    rainbow_cleanup(&table);
    return true;
}

TEST(populate_count_medium) {
    RainbowTable table;
    rainbow_init(&table, 0);
    
    /* Populate with first 100 primes */
    MathError err = rainbow_populate_count(&table, 100);
    ASSERT(err == MATH_SUCCESS);
    ASSERT(table.size == 100);
    ASSERT(table.max_prime == 541);  /* 100th prime */
    ASSERT(table.max_index == 100);
    
    rainbow_cleanup(&table);
    return true;
}

TEST(populate_to_prime) {
    RainbowTable table;
    rainbow_init(&table, 0);
    
    /* Populate up to 100 */
    MathError err = rainbow_populate_to_prime(&table, 100);
    ASSERT(err == MATH_SUCCESS);
    ASSERT(table.size == 25);  /* 25 primes up to 100 */
    ASSERT(table.max_prime == 97);
    
    rainbow_cleanup(&table);
    return true;
}

/* ============================================================================
 * LOOKUP TESTS
 * ============================================================================
 */

TEST(lookup_by_index) {
    RainbowTable table;
    rainbow_init(&table, 0);
    rainbow_populate_count(&table, 100);
    
    uint64_t prime;
    
    /* Test first prime */
    MathError err = rainbow_lookup_by_index(&table, 1, &prime);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(prime, 2);
    
    /* Test 10th prime */
    err = rainbow_lookup_by_index(&table, 10, &prime);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(prime, 29);
    
    /* Test 100th prime */
    err = rainbow_lookup_by_index(&table, 100, &prime);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(prime, 541);
    
    /* Test out of range */
    err = rainbow_lookup_by_index(&table, 101, &prime);
    ASSERT(err == MATH_ERROR_OUT_OF_RANGE);
    
    rainbow_cleanup(&table);
    return true;
}

TEST(lookup_position) {
    RainbowTable table;
    rainbow_init(&table, 0);
    rainbow_populate_count(&table, 100);
    
    ClockPosition pos;
    
    /* Lookup position of prime 2 */
    MathError err = rainbow_lookup_position(&table, 2, &pos);
    ASSERT(err == MATH_SUCCESS);
    ASSERT(pos.ring == 0);
    ASSERT(pos.position == 0);
    
    /* Lookup position of prime 7 */
    err = rainbow_lookup_position(&table, 7, &pos);
    ASSERT(err == MATH_SUCCESS);
    ASSERT(pos.ring == 0);
    ASSERT(pos.position == 6);
    
    /* Lookup non-existent prime */
    err = rainbow_lookup_position(&table, 1009, &pos);
    ASSERT(err == MATH_ERROR_NOT_FOUND);
    
    rainbow_cleanup(&table);
    return true;
}

TEST(lookup_index) {
    RainbowTable table;
    rainbow_init(&table, 0);
    rainbow_populate_count(&table, 100);
    
    uint64_t index;
    
    /* Lookup index of prime 2 */
    MathError err = rainbow_lookup_index(&table, 2, &index);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(index, 1);
    
    /* Lookup index of prime 29 */
    err = rainbow_lookup_index(&table, 29, &index);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(index, 10);
    
    /* Lookup index of prime 541 */
    err = rainbow_lookup_index(&table, 541, &index);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(index, 100);
    
    rainbow_cleanup(&table);
    return true;
}

/* ============================================================================
 * NAVIGATION TESTS
 * ============================================================================
 */

TEST(next_prime) {
    RainbowTable table;
    rainbow_init(&table, 0);
    rainbow_populate_count(&table, 100);
    
    uint64_t next;
    
    /* Next after 2 */
    MathError err = rainbow_next_prime(&table, 2, &next);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(next, 3);
    
    /* Next after 7 */
    err = rainbow_next_prime(&table, 7, &next);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(next, 11);
    
    /* Next after last prime (should fail) */
    err = rainbow_next_prime(&table, 541, &next);
    ASSERT(err == MATH_ERROR_OUT_OF_RANGE);
    
    rainbow_cleanup(&table);
    return true;
}

TEST(prev_prime) {
    RainbowTable table;
    rainbow_init(&table, 0);
    rainbow_populate_count(&table, 100);
    
    uint64_t prev;
    
    /* Prev before 3 */
    MathError err = rainbow_prev_prime(&table, 3, &prev);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(prev, 2);
    
    /* Prev before 11 */
    err = rainbow_prev_prime(&table, 11, &prev);
    ASSERT(err == MATH_SUCCESS);
    ASSERT_EQ(prev, 7);
    
    /* Prev before first prime (should fail) */
    err = rainbow_prev_prime(&table, 2, &prev);
    ASSERT(err == MATH_ERROR_OUT_OF_RANGE);
    
    rainbow_cleanup(&table);
    return true;
}

/* ============================================================================
 * QUERY TESTS
 * ============================================================================
 */

TEST(contains) {
    RainbowTable table;
    rainbow_init(&table, 0);
    rainbow_populate_count(&table, 100);
    
    /* Test primes in table */
    ASSERT(rainbow_contains(&table, 2) == true);
    ASSERT(rainbow_contains(&table, 7) == true);
    ASSERT(rainbow_contains(&table, 541) == true);
    
    /* Test primes not in table */
    ASSERT(rainbow_contains(&table, 1009) == false);
    
    /* Test non-primes */
    ASSERT(rainbow_contains(&table, 4) == false);
    ASSERT(rainbow_contains(&table, 100) == false);
    
    rainbow_cleanup(&table);
    return true;
}

TEST(size_and_max) {
    RainbowTable table;
    rainbow_init(&table, 0);
    
    ASSERT(rainbow_size(&table) == 0);
    ASSERT(rainbow_max_prime(&table) == 0);
    
    rainbow_populate_count(&table, 50);
    
    ASSERT(rainbow_size(&table) == 50);
    ASSERT(rainbow_max_prime(&table) == 229);  /* 50th prime */
    
    rainbow_cleanup(&table);
    return true;
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */

int main(void) {
    printf("=== Rainbow Table Test Suite ===\n\n");
    
    /* Initialization tests */
    printf("--- Initialization Tests ---\n");
    run_test_init_cleanup();
    run_test_init_default_capacity();
    
    /* Population tests */
    printf("\n--- Population Tests ---\n");
    run_test_populate_count_small();
    run_test_populate_count_medium();
    run_test_populate_to_prime();
    
    /* Lookup tests */
    printf("\n--- Lookup Tests ---\n");
    run_test_lookup_by_index();
    run_test_lookup_position();
    run_test_lookup_index();
    
    /* Navigation tests */
    printf("\n--- Navigation Tests ---\n");
    run_test_next_prime();
    run_test_prev_prime();
    
    /* Query tests */
    printf("\n--- Query Tests ---\n");
    run_test_contains();
    run_test_size_and_max();
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed!\n");
        return 1;
    }
}