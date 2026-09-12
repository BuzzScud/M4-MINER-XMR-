/**
 * @file test_abacus_gcd.c
 * @brief Tests for arbitrary precision GCD, LCM, and coprimality
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 */

#include "math/abacus.h"
#include "math/clock.h"
#include "math/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s\n", name); \
        tests_run++; \
    } while(0)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ❌ FAILED: %s\n", message); \
            return; \
        } \
    } while(0)

#define PASS() \
    do { \
        printf("  ✅ PASSED\n"); \
        tests_passed++; \
    } while(0)

/* ============================================================================
 * GCD TESTS
 * ============================================================================
 */

void test_gcd_small_numbers() {
    TEST("GCD of small numbers");
    
    CrystallineAbacus *a = abacus_from_uint64(48, 10);
    CrystallineAbacus *b = abacus_from_uint64(18, 10);
    CrystallineAbacus *result = abacus_new(10);
    
    /* Test gcd(48, 18) = 6 */
    MathError err = abacus_gcd(result, a, b);
    ASSERT(err == MATH_SUCCESS, "GCD computation failed");
    
    uint64_t gcd_value;
    err = abacus_to_uint64(result, &gcd_value);
    ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
    ASSERT(gcd_value == 6, "Expected gcd(48, 18) = 6");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    PASS();
}

void test_gcd_coprime() {
    TEST("GCD of coprime numbers");
    
    CrystallineAbacus *a = abacus_from_uint64(17, 10);
    CrystallineAbacus *b = abacus_from_uint64(19, 10);
    CrystallineAbacus *result = abacus_new(10);
    
    /* Test gcd(17, 19) = 1 */
    MathError err = abacus_gcd(result, a, b);
    ASSERT(err == MATH_SUCCESS, "GCD computation failed");
    
    uint64_t gcd_value;
    err = abacus_to_uint64(result, &gcd_value);
    ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
    ASSERT(gcd_value == 1, "Expected gcd(17, 19) = 1");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    PASS();
}

void test_gcd_large_numbers() {
    TEST("GCD of large numbers");
    
    /* gcd(123456789 * 1000, 123456789 * 500) = 123456789 * 500 = 61728394500 */
    CrystallineAbacus *base = abacus_from_uint64(123456789, 10);
    CrystallineAbacus *mult1 = abacus_from_uint64(1000, 10);
    CrystallineAbacus *mult2 = abacus_from_uint64(500, 10);
    
    CrystallineAbacus *a = abacus_new(10);
    CrystallineAbacus *b = abacus_new(10);
    CrystallineAbacus *result = abacus_new(10);
    CrystallineAbacus *expected = abacus_new(10);
    
    abacus_mul(a, base, mult1);
    abacus_mul(b, base, mult2);
    abacus_mul(expected, base, mult2);
    
    MathError err = abacus_gcd(result, a, b);
    ASSERT(err == MATH_SUCCESS, "GCD computation failed");
    
    int cmp = abacus_compare(result, expected);
    ASSERT(cmp == 0, "GCD result doesn't match expected value");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    abacus_free(base);
    abacus_free(mult1);
    abacus_free(mult2);
    abacus_free(expected);
    
    PASS();
}

void test_gcd_with_zero() {
    TEST("GCD with zero");
    
    CrystallineAbacus *a = abacus_from_uint64(0, 10);
    CrystallineAbacus *b = abacus_from_uint64(5, 10);
    CrystallineAbacus *result = abacus_new(10);
    
    /* Test gcd(0, 5) = 5 */
    MathError err = abacus_gcd(result, a, b);
    ASSERT(err == MATH_SUCCESS, "GCD computation failed");
    
    uint64_t gcd_value;
    err = abacus_to_uint64(result, &gcd_value);
    ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
    ASSERT(gcd_value == 5, "Expected gcd(0, 5) = 5");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    PASS();
}

/* ============================================================================
 * LCM TESTS
 * ============================================================================
 */

void test_lcm_small_numbers() {
    TEST("LCM of small numbers");
    
    CrystallineAbacus *a = abacus_from_uint64(12, 10);
    CrystallineAbacus *b = abacus_from_uint64(18, 10);
    CrystallineAbacus *result = abacus_new(10);
    
    /* Test lcm(12, 18) = 36 */
    MathError err = abacus_lcm(result, a, b);
    ASSERT(err == MATH_SUCCESS, "LCM computation failed");
    
    uint64_t lcm_value;
    err = abacus_to_uint64(result, &lcm_value);
    ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
    ASSERT(lcm_value == 36, "Expected lcm(12, 18) = 36");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    PASS();
}

void test_lcm_coprime() {
    TEST("LCM of coprime numbers");
    
    CrystallineAbacus *a = abacus_from_uint64(7, 10);
    CrystallineAbacus *b = abacus_from_uint64(5, 10);
    CrystallineAbacus *result = abacus_new(10);
    
    /* Test lcm(7, 5) = 35 (coprime) */
    MathError err = abacus_lcm(result, a, b);
    ASSERT(err == MATH_SUCCESS, "LCM computation failed");
    
    uint64_t lcm_value;
    err = abacus_to_uint64(result, &lcm_value);
    ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
    ASSERT(lcm_value == 35, "Expected lcm(7, 5) = 35");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    PASS();
}

/* ============================================================================
 * COPRIMALITY TESTS
 * ============================================================================
 */

void test_coprime_true() {
    TEST("Coprimality test (true)");
    
    CrystallineAbacus *a = abacus_from_uint64(17, 10);
    CrystallineAbacus *b = abacus_from_uint64(19, 10);
    
    bool coprime = abacus_coprime(a, b);
    ASSERT(coprime == true, "Expected 17 and 19 to be coprime");
    
    abacus_free(a);
    abacus_free(b);
    
    PASS();
}

void test_coprime_false() {
    TEST("Coprimality test (false)");
    
    CrystallineAbacus *a = abacus_from_uint64(12, 10);
    CrystallineAbacus *b = abacus_from_uint64(18, 10);
    
    bool coprime = abacus_coprime(a, b);
    ASSERT(coprime == false, "Expected 12 and 18 to NOT be coprime");
    
    abacus_free(a);
    abacus_free(b);
    
    PASS();
}

void test_coprime_large_primes() {
    TEST("Coprimality with large primes");
    
    CrystallineAbacus *a = abacus_from_uint64(1000003, 10);
    CrystallineAbacus *b = abacus_from_uint64(1000033, 10);
    
    bool coprime = abacus_coprime(a, b);
    ASSERT(coprime == true, "Expected large primes to be coprime");
    
    abacus_free(a);
    abacus_free(b);
    
    PASS();
}

/* ============================================================================
 * BABYLONIAN BASE TESTS
 * ============================================================================
 */

void test_gcd_babylonian_base() {
    TEST("GCD in Babylonian base 12");
    
    CrystallineAbacus *a = abacus_from_uint64(144, 12);
    CrystallineAbacus *b = abacus_from_uint64(60, 12);
    CrystallineAbacus *result = abacus_new(12);
    
    /* Test gcd(144, 60) = 12 */
    MathError err = abacus_gcd(result, a, b);
    ASSERT(err == MATH_SUCCESS, "GCD computation failed");
    
    uint64_t gcd_value;
    err = abacus_to_uint64(result, &gcd_value);
    ASSERT(err == MATH_SUCCESS, "Conversion to uint64 failed");
    ASSERT(gcd_value == 12, "Expected gcd(144, 60) = 12");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    PASS();
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  ABACUS GCD/LCM/COPRIME TESTS\n");
    printf("========================================\n\n");
    
    /* GCD tests */
    test_gcd_small_numbers();
    test_gcd_coprime();
    test_gcd_large_numbers();
    test_gcd_with_zero();
    
    /* LCM tests */
    test_lcm_small_numbers();
    test_lcm_coprime();
    
    /* Coprimality tests */
    test_coprime_true();
    test_coprime_false();
    test_coprime_large_primes();
    
    /* Babylonian base tests */
    test_gcd_babylonian_base();
    
    printf("\n");
    printf("========================================\n");
    printf("  RESULTS: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}