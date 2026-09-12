/**
 * @file test_prime.c
 * @brief Test suite for prime generation and clock lattice
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 */

#include "math/prime.h"
#include "math/clock.h"
#include "math/arithmetic.h"
#include <stdio.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, condition) do { \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", name); \
    } else { \
        tests_failed++; \
        printf("✗ %s\n", name); \
    } \
} while(0)

void test_primality(void) {
    printf("\n=== Testing Primality ===\n");
    
    /* Test known primes */
    TEST("2 is prime", prime_is_prime(2));
    TEST("3 is prime", prime_is_prime(3));
    TEST("5 is prime", prime_is_prime(5));
    TEST("7 is prime", prime_is_prime(7));
    TEST("11 is prime", prime_is_prime(11));
    TEST("13 is prime", prime_is_prime(13));
    TEST("17 is prime", prime_is_prime(17));
    TEST("19 is prime", prime_is_prime(19));
    TEST("23 is prime", prime_is_prime(23));
    TEST("29 is prime", prime_is_prime(29));
    TEST("31 is prime", prime_is_prime(31));
    TEST("97 is prime", prime_is_prime(97));
    TEST("101 is prime", prime_is_prime(101));
    
    /* Test known composites */
    TEST("1 is not prime", !prime_is_prime(1));
    TEST("4 is not prime", !prime_is_prime(4));
    TEST("6 is not prime", !prime_is_prime(6));
    TEST("8 is not prime", !prime_is_prime(8));
    TEST("9 is not prime", !prime_is_prime(9));
    TEST("10 is not prime", !prime_is_prime(10));
    TEST("15 is not prime", !prime_is_prime(15));
    TEST("21 is not prime", !prime_is_prime(21));
    TEST("25 is not prime", !prime_is_prime(25));
    TEST("100 is not prime", !prime_is_prime(100));
}

void test_nth_prime(void) {
    printf("\n=== Testing Nth Prime ===\n");
    
    TEST("1st prime is 2", prime_nth(1) == 2);
    TEST("2nd prime is 3", prime_nth(2) == 3);
    TEST("3rd prime is 5", prime_nth(3) == 5);
    TEST("4th prime is 7", prime_nth(4) == 7);
    TEST("5th prime is 11", prime_nth(5) == 11);
    TEST("6th prime is 13", prime_nth(6) == 13);
    TEST("10th prime is 29", prime_nth(10) == 29);
    TEST("25th prime is 97", prime_nth(25) == 97);
    
    /* Test edge cases */
    TEST("0th prime is 0", prime_nth(0) == 0);
}

void test_next_prev_prime(void) {
    printf("\n=== Testing Next/Previous Prime ===\n");
    
    TEST("Next prime after 2 is 3", prime_next(2) == 3);
    TEST("Next prime after 3 is 5", prime_next(3) == 5);
    TEST("Next prime after 5 is 7", prime_next(5) == 7);
    TEST("Next prime after 10 is 11", prime_next(10) == 11);
    TEST("Next prime after 14 is 17", prime_next(14) == 17);
    
    TEST("Previous prime before 3 is 2", prime_prev(3) == 2);
    TEST("Previous prime before 5 is 3", prime_prev(5) == 3);
    TEST("Previous prime before 7 is 5", prime_prev(7) == 5);
    TEST("Previous prime before 11 is 7", prime_prev(11) == 7);
    TEST("Previous prime before 20 is 19", prime_prev(20) == 19);
    
    /* Edge cases */
    TEST("Previous prime before 2 is 0", prime_prev(2) == 0);
    TEST("Next prime after 0 is 2", prime_next(0) == 2);
}

void test_prime_counting(void) {
    printf("\n=== Testing Prime Counting ===\n");
    
    TEST("Primes below 2: 0", prime_count_below(2) == 0);
    TEST("Primes below 3: 1", prime_count_below(3) == 1);
    TEST("Primes below 10: 4", prime_count_below(10) == 4);  // 2, 3, 5, 7
    TEST("Primes below 20: 8", prime_count_below(20) == 8);  // 2, 3, 5, 7, 11, 13, 17, 19
    TEST("Primes below 30: 10", prime_count_below(30) == 10);
    TEST("Primes below 100: 25", prime_count_below(100) == 25);
}

void test_prime_gaps(void) {
    printf("\n=== Testing Prime Gaps ===\n");
    
    /* Gap between consecutive primes */
    TEST("Gap after 2 is 1", prime_gap_next(2) == 1);  // 3 - 2 = 1
    TEST("Gap after 3 is 2", prime_gap_next(3) == 2);  // 5 - 3 = 2
    TEST("Gap after 7 is 4", prime_gap_next(7) == 4);  // 11 - 7 = 4
    
    TEST("Gap before 3 is 1", prime_gap_prev(3) == 1);  // 3 - 2 = 1
    TEST("Gap before 5 is 2", prime_gap_prev(5) == 2);  // 5 - 3 = 2
    TEST("Gap before 11 is 4", prime_gap_prev(11) == 4);  // 11 - 7 = 4
}

void test_prime_count_range(void) {
    printf("\n=== Testing Prime Count Range ===\n");
    
    TEST("Primes in [10, 20]: 4", prime_count_range(10, 20) == 4);  // 11, 13, 17, 19
    TEST("Primes in [2, 10]: 4", prime_count_range(2, 10) == 4);  // 2, 3, 5, 7
    TEST("Primes in [20, 30]: 2", prime_count_range(20, 30) == 2);  // 23, 29
}

void test_coprime(void) {
    printf("\n=== Testing Coprime ===\n");
    
    TEST("8 and 15 are coprime", prime_are_coprime(8, 15));
    TEST("14 and 25 are coprime", prime_are_coprime(14, 25));
    TEST("12 and 18 are not coprime", !prime_are_coprime(12, 18));
    TEST("10 and 15 are not coprime", !prime_are_coprime(10, 15));
}

void test_clock_lattice(void) {
    printf("\n=== Testing Clock Lattice ===\n");
    
    ClockContext ctx;
    TEST("Clock init succeeds", clock_init(&ctx) == MATH_SUCCESS);
    
    /* Test prime to position mapping */
    ClockPosition pos;
    TEST("Map prime 2 to position", clock_map_prime_to_position(2, &pos) == MATH_SUCCESS);
    TEST("Position for 2 is valid", clock_is_valid_position(&pos));
    
    TEST("Map prime 3 to position", clock_map_prime_to_position(3, &pos) == MATH_SUCCESS);
    TEST("Position for 3 is valid", clock_is_valid_position(&pos));
    
    TEST("Map prime 5 to position", clock_map_prime_to_position(5, &pos) == MATH_SUCCESS);
    TEST("Position for 5 is valid", clock_is_valid_position(&pos));
    
    TEST("Map prime 7 to position", clock_map_prime_to_position(7, &pos) == MATH_SUCCESS);
    TEST("Position for 7 is valid", clock_is_valid_position(&pos));
    
    /* Test clock validation */
    TEST("Prime 11 validates by clock", prime_validate_by_clock(11));
    TEST("Prime 13 validates by clock", prime_validate_by_clock(13));
    TEST("Composite 15 does not validate", !prime_validate_by_clock(15));
}

int main(void) {
    printf("Crystalline Math Library - Prime & Clock Lattice Tests\n");
    printf("======================================================\n");
    
    test_primality();
    test_nth_prime();
    test_next_prev_prime();
    test_prime_counting();
    test_prime_gaps();
    test_prime_count_range();
    test_coprime();
    test_clock_lattice();
    
    printf("\n======================================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    
    if (tests_failed > 0) {
        printf("FAILED: Some tests did not pass\n");
        return 1;
    }
    
    printf("SUCCESS: All tests passed!\n");
    return 0;
}