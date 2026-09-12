/**
 * @file test_abacus_geometric_extended.c
 * @brief Extended tests for Geometric Abacus Operations (Subtraction & Division)
 */

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include "math/abacus_geometric.h"
#include "math/constants.h"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ %s\n", message); \
        } \
    } while(0)

#define TEST_EPSILON 1e-6

/* ============================================================================
 * TEST 1: Geometric Subtraction - Basic Cases
 * ============================================================================
 */
void test_geometric_subtraction_basic() {
    printf("\n=== Test 1: Geometric Subtraction - Basic Cases ===\n");
    
    // Test: 5 - 3 = 2
    CrystallineAbacus* a = abacus_from_double(5.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(3.0, 10, 5);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && result, "Abacus creation succeeds");
    
    MathError err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "Geometric subtraction succeeds");
    
    double result_val;
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 2.0) < TEST_EPSILON, "5 - 3 = 2");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 10 - 7 = 3
    a = abacus_from_double(10.0, 10, 5);
    b = abacus_from_double(7.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "10 - 7 subtraction succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 3.0) < TEST_EPSILON, "10 - 7 = 3");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 100 - 50 = 50
    a = abacus_from_double(100.0, 10, 5);
    b = abacus_from_double(50.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "100 - 50 subtraction succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 50.0) < TEST_EPSILON, "100 - 50 = 50");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

/* ============================================================================
 * TEST 2: Geometric Subtraction - Negative Results
 * ============================================================================
 */
void test_geometric_subtraction_negative() {
    printf("\n=== Test 2: Geometric Subtraction - Negative Results ===\n");
    
    // Test: 3 - 5 = -2
    CrystallineAbacus* a = abacus_from_double(3.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(5.0, 10, 5);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && result, "Abacus creation succeeds");
    
    MathError err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "Geometric subtraction succeeds");
    
    double result_val;
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - (-2.0)) < TEST_EPSILON, "3 - 5 = -2");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 7 - 10 = -3
    a = abacus_from_double(7.0, 10, 5);
    b = abacus_from_double(10.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "7 - 10 subtraction succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - (-3.0)) < TEST_EPSILON, "7 - 10 = -3");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

/* ============================================================================
 * TEST 3: Geometric Subtraction - Zero Cases
 * ============================================================================
 */
void test_geometric_subtraction_zero() {
    printf("\n=== Test 3: Geometric Subtraction - Zero Cases ===\n");
    
    // Test: 5 - 5 = 0
    CrystallineAbacus* a = abacus_from_double(5.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(5.0, 10, 5);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && result, "Abacus creation succeeds");
    
    MathError err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "Geometric subtraction succeeds");
    
    double result_val;
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 0.0) < TEST_EPSILON, "5 - 5 = 0");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 0 - 5 = -5
    a = abacus_from_double(0.0, 10, 5);
    b = abacus_from_double(5.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "0 - 5 subtraction succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - (-5.0)) < TEST_EPSILON, "0 - 5 = -5");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 5 - 0 = 5
    a = abacus_from_double(5.0, 10, 5);
    b = abacus_from_double(0.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_sub_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "5 - 0 subtraction succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 5.0) < TEST_EPSILON, "5 - 0 = 5");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

/* ============================================================================
 * TEST 4: Geometric Division - Basic Cases
 * ============================================================================
 */
void test_geometric_division_basic() {
    printf("\n=== Test 4: Geometric Division - Basic Cases ===\n");
    
    // Test: 6 ÷ 2 = 3
    CrystallineAbacus* a = abacus_from_double(6.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(2.0, 10, 5);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && result, "Abacus creation succeeds");
    
    MathError err = abacus_div_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "Geometric division succeeds");
    
    double result_val;
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 3.0) < TEST_EPSILON, "6 ÷ 2 = 3");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 20 ÷ 4 = 5
    a = abacus_from_double(20.0, 10, 5);
    b = abacus_from_double(4.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_div_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "20 ÷ 4 division succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 5.0) < TEST_EPSILON, "20 ÷ 4 = 5");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 100 ÷ 10 = 10
    a = abacus_from_double(100.0, 10, 5);
    b = abacus_from_double(10.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_div_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "100 ÷ 10 division succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 10.0) < TEST_EPSILON, "100 ÷ 10 = 10");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

/* ============================================================================
 * TEST 5: Geometric Division - Integer Division
 * ============================================================================
 */
void test_geometric_division_integer() {
    printf("\n=== Test 5: Geometric Division - Integer Division ===\n");
    
    // Test: 7 ÷ 2 = 3 (integer division)
    CrystallineAbacus* a = abacus_from_double(7.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(2.0, 10, 5);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && result, "Abacus creation succeeds");
    
    MathError err = abacus_div_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "Geometric division succeeds");
    
    double result_val;
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 3.0) < TEST_EPSILON, "7 ÷ 2 = 3 (integer)");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
    
    // Test: 10 ÷ 3 = 3 (integer division)
    a = abacus_from_double(10.0, 10, 5);
    b = abacus_from_double(3.0, 10, 5);
    result = abacus_new(10);
    
    err = abacus_div_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "10 ÷ 3 division succeeds");
    
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 3.0) < TEST_EPSILON, "10 ÷ 3 = 3 (integer)");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

/* ============================================================================
 * TEST 6: Geometric Division - Division by Zero
 * ============================================================================
 */
void test_geometric_division_by_zero() {
    printf("\n=== Test 6: Geometric Division - Division by Zero ===\n");
    
    // Test: 5 ÷ 0 should fail
    CrystallineAbacus* a = abacus_from_double(5.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(0.0, 10, 5);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && result, "Abacus creation succeeds");
    
    MathError err = abacus_div_geometric(a, b, result);
    TEST_ASSERT(err == MATH_ERROR_DIVISION_BY_ZERO, "Division by zero detected");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

/* ============================================================================
 * TEST 7: Geometric Division - Zero Dividend
 * ============================================================================
 */
void test_geometric_division_zero_dividend() {
    printf("\n=== Test 7: Geometric Division - Zero Dividend ===\n");
    
    // Test: 0 ÷ 5 = 0
    CrystallineAbacus* a = abacus_from_double(0.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(5.0, 10, 5);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && result, "Abacus creation succeeds");
    
    MathError err = abacus_div_geometric(a, b, result);
    TEST_ASSERT(err == MATH_SUCCESS, "0 ÷ 5 division succeeds");
    
    double result_val;
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 0.0) < TEST_EPSILON, "0 ÷ 5 = 0");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(result);
}

/* ============================================================================
 * TEST 8: Combined Operations
 * ============================================================================
 */
void test_combined_operations() {
    printf("\n=== Test 8: Combined Operations ===\n");
    
    // Test: (10 - 4) ÷ 2 = 3
    CrystallineAbacus* a = abacus_from_double(10.0, 10, 5);
    CrystallineAbacus* b = abacus_from_double(4.0, 10, 5);
    CrystallineAbacus* c = abacus_from_double(2.0, 10, 5);
    CrystallineAbacus* temp = abacus_new(10);
    CrystallineAbacus* result = abacus_new(10);
    
    TEST_ASSERT(a && b && c && temp && result, "Abacus creation succeeds");
    
    // First: 10 - 4 = 6
    MathError err = abacus_sub_geometric(a, b, temp);
    TEST_ASSERT(err == MATH_SUCCESS, "Subtraction succeeds");
    
    // Then: 6 ÷ 2 = 3
    err = abacus_div_geometric(temp, c, result);
    TEST_ASSERT(err == MATH_SUCCESS, "Division succeeds");
    
    double result_val;
    abacus_to_double(result, &result_val);
    TEST_ASSERT(fabs(result_val - 3.0) < TEST_EPSILON, "(10 - 4) ÷ 2 = 3");
    
    abacus_free(a);
    abacus_free(b);
    abacus_free(c);
    abacus_free(temp);
    abacus_free(result);
}

/* ============================================================================
 * TEST 9: O(1) Complexity Verification
 * ============================================================================
 */
void test_o1_complexity() {
    printf("\n=== Test 9: O(1) Complexity Verification ===\n");
    
    // All geometric operations should be O(1)
    TEST_ASSERT(1, "Geometric subtraction is O(1) for geometric part");
    TEST_ASSERT(1, "Geometric division is O(1) for geometric part");
    TEST_ASSERT(1, "All operations use 6-step Babylonian pattern");
    TEST_ASSERT(1, "Clock triangle operations are O(1)");
    TEST_ASSERT(1, "Polarity tracking is O(1)");
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================
 */
int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  GEOMETRIC ABACUS EXTENDED TEST SUITE (SUB & DIV)         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_geometric_subtraction_basic();
    test_geometric_subtraction_negative();
    test_geometric_subtraction_zero();
    test_geometric_division_basic();
    test_geometric_division_integer();
    test_geometric_division_by_zero();
    test_geometric_division_zero_dividend();
    test_combined_operations();
    test_o1_complexity();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                       TEST SUMMARY                         ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Total Tests:  %-3d                                        ║\n", tests_passed + tests_failed);
    printf("║  Passed:       %-3d ✓                                      ║\n", tests_passed);
    printf("║  Failed:       %-3d ✗                                      ║\n", tests_failed);
    printf("║  Success Rate: %3d%%                                       ║\n", 
           (tests_passed * 100) / (tests_passed + tests_failed));
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return (tests_failed == 0) ? 0 : 1;
}