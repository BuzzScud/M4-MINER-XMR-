/**
 * @file test_schlafli_validator.c
 * @brief Test mathematical validation for Schläfli symbols
 */

#include "math/schlafli.h"
#include "math/schlafli_validator.h"
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
            printf("FAILED: %s == %s (%ld != %ld) (line %d)\n", \
                   #a, #b, (long)(a), (long)(b), __LINE__); \
        } \
    } while(0)

// ============================================================================
// MATHEMATICAL VALIDATION TESTS
// ============================================================================

void test_angle_sum_validation() {
    TEST("Angle Sum Validation");
    
    // Valid 3D Platonic solids
    SchlafliSymbol* tetrahedron = schlafli_parse("{3,3}");
    ASSERT_TRUE(schlafli_validate_angle_sum(tetrahedron));
    schlafli_free(tetrahedron);
    
    SchlafliSymbol* cube = schlafli_parse("{4,3}");
    ASSERT_TRUE(schlafli_validate_angle_sum(cube));
    schlafli_free(cube);
    
    SchlafliSymbol* octahedron = schlafli_parse("{3,4}");
    ASSERT_TRUE(schlafli_validate_angle_sum(octahedron));
    schlafli_free(octahedron);
    
    SchlafliSymbol* dodecahedron = schlafli_parse("{5,3}");
    ASSERT_TRUE(schlafli_validate_angle_sum(dodecahedron));
    schlafli_free(dodecahedron);
    
    SchlafliSymbol* icosahedron = schlafli_parse("{3,5}");
    ASSERT_TRUE(schlafli_validate_angle_sum(icosahedron));
    schlafli_free(icosahedron);
    
    // Invalid: {6,3} would have angle sum = 6 × 120° = 720° = 2π (not < 2π)
    SchlafliSymbol* invalid1 = schlafli_parse("{6,3}");
    ASSERT_FALSE(schlafli_validate_angle_sum(invalid1));
    schlafli_free(invalid1);
    
    // Invalid: {3,6} would have angle sum = 6 × 60° = 360° = 2π (not < 2π)
    SchlafliSymbol* invalid2 = schlafli_parse("{3,6}");
    ASSERT_FALSE(schlafli_validate_angle_sum(invalid2));
    schlafli_free(invalid2);
    
    // Invalid: {4,4} would have angle sum = 4 × 90° = 360° = 2π (not < 2π)
    SchlafliSymbol* invalid3 = schlafli_parse("{4,4}");
    ASSERT_FALSE(schlafli_validate_angle_sum(invalid3));
    schlafli_free(invalid3);
    
    printf("✓ Angle sum validation working correctly\n");
}

void test_euler_validation() {
    TEST("Euler Characteristic Validation");
    
    // 3D Platonic solids (V - E + F = 2)
    SchlafliSymbol* tetrahedron = schlafli_parse("{3,3}");
    ASSERT_TRUE(schlafli_validate_euler(tetrahedron));
    schlafli_free(tetrahedron);
    
    SchlafliSymbol* cube = schlafli_parse("{4,3}");
    ASSERT_TRUE(schlafli_validate_euler(cube));
    schlafli_free(cube);
    
    SchlafliSymbol* octahedron = schlafli_parse("{3,4}");
    ASSERT_TRUE(schlafli_validate_euler(octahedron));
    schlafli_free(octahedron);
    
    SchlafliSymbol* dodecahedron = schlafli_parse("{5,3}");
    ASSERT_TRUE(schlafli_validate_euler(dodecahedron));
    schlafli_free(dodecahedron);
    
    SchlafliSymbol* icosahedron = schlafli_parse("{3,5}");
    ASSERT_TRUE(schlafli_validate_euler(icosahedron));
    schlafli_free(icosahedron);
    
    // 4D polychora (V - E + F - C = 0)
    SchlafliSymbol* cell5 = schlafli_parse("{3,3,3}");
    ASSERT_TRUE(schlafli_validate_euler(cell5));
    schlafli_free(cell5);
    
    SchlafliSymbol* tesseract = schlafli_parse("{4,3,3}");
    ASSERT_TRUE(schlafli_validate_euler(tesseract));
    schlafli_free(tesseract);
    
    SchlafliSymbol* cell16 = schlafli_parse("{3,3,4}");
    ASSERT_TRUE(schlafli_validate_euler(cell16));
    schlafli_free(cell16);
    
    SchlafliSymbol* cell24 = schlafli_parse("{3,4,3}");
    ASSERT_TRUE(schlafli_validate_euler(cell24));
    schlafli_free(cell24);
    
    SchlafliSymbol* cell120 = schlafli_parse("{5,3,3}");
    ASSERT_TRUE(schlafli_validate_euler(cell120));
    schlafli_free(cell120);
    
    SchlafliSymbol* cell600 = schlafli_parse("{3,3,5}");
    ASSERT_TRUE(schlafli_validate_euler(cell600));
    schlafli_free(cell600);
    
    printf("✓ Euler characteristic validation working correctly\n");
}

void test_mathematical_validation() {
    TEST("Comprehensive Mathematical Validation");
    
    // All 5 Platonic solids should be valid
    SchlafliSymbol* tetrahedron = schlafli_parse("{3,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(tetrahedron));
    schlafli_free(tetrahedron);
    
    SchlafliSymbol* cube = schlafli_parse("{4,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(cube));
    schlafli_free(cube);
    
    SchlafliSymbol* octahedron = schlafli_parse("{3,4}");
    ASSERT_TRUE(schlafli_validate_mathematical(octahedron));
    schlafli_free(octahedron);
    
    SchlafliSymbol* dodecahedron = schlafli_parse("{5,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(dodecahedron));
    schlafli_free(dodecahedron);
    
    SchlafliSymbol* icosahedron = schlafli_parse("{3,5}");
    ASSERT_TRUE(schlafli_validate_mathematical(icosahedron));
    schlafli_free(icosahedron);
    
    // All 6 regular 4D polychora should be valid
    SchlafliSymbol* cell5 = schlafli_parse("{3,3,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(cell5));
    schlafli_free(cell5);
    
    SchlafliSymbol* tesseract = schlafli_parse("{4,3,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(tesseract));
    schlafli_free(tesseract);
    
    SchlafliSymbol* cell16 = schlafli_parse("{3,3,4}");
    ASSERT_TRUE(schlafli_validate_mathematical(cell16));
    schlafli_free(cell16);
    
    SchlafliSymbol* cell24 = schlafli_parse("{3,4,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(cell24));
    schlafli_free(cell24);
    
    SchlafliSymbol* cell120 = schlafli_parse("{5,3,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(cell120));
    schlafli_free(cell120);
    
    SchlafliSymbol* cell600 = schlafli_parse("{3,3,5}");
    ASSERT_TRUE(schlafli_validate_mathematical(cell600));
    schlafli_free(cell600);
    
    // Invalid symbols should fail
    SchlafliSymbol* invalid1 = schlafli_parse("{6,3}");
    ASSERT_FALSE(schlafli_validate_mathematical(invalid1));
    schlafli_free(invalid1);
    
    SchlafliSymbol* invalid2 = schlafli_parse("{3,6}");
    ASSERT_FALSE(schlafli_validate_mathematical(invalid2));
    schlafli_free(invalid2);
    
    SchlafliSymbol* invalid3 = schlafli_parse("{4,4}");
    ASSERT_FALSE(schlafli_validate_mathematical(invalid3));
    schlafli_free(invalid3);
    
    printf("✓ Mathematical validation working correctly\n");
}

void test_property_discovery() {
    TEST("Polytope Property Discovery");
    
    // Test vertex counting
    SchlafliSymbol* tetrahedron = schlafli_parse("{3,3}");
    ASSERT_EQ(schlafli_calculate_vertices(tetrahedron), 4);
    schlafli_free(tetrahedron);
    
    SchlafliSymbol* cube = schlafli_parse("{4,3}");
    ASSERT_EQ(schlafli_calculate_vertices(cube), 8);
    schlafli_free(cube);
    
    SchlafliSymbol* octahedron = schlafli_parse("{3,4}");
    ASSERT_EQ(schlafli_calculate_vertices(octahedron), 6);
    schlafli_free(octahedron);
    
    SchlafliSymbol* dodecahedron = schlafli_parse("{5,3}");
    ASSERT_EQ(schlafli_calculate_vertices(dodecahedron), 20);
    schlafli_free(dodecahedron);
    
    SchlafliSymbol* icosahedron = schlafli_parse("{3,5}");
    ASSERT_EQ(schlafli_calculate_vertices(icosahedron), 12);
    schlafli_free(icosahedron);
    
    // Test 4D vertex counting
    SchlafliSymbol* cell5 = schlafli_parse("{3,3,3}");
    ASSERT_EQ(schlafli_calculate_vertices(cell5), 5);
    schlafli_free(cell5);
    
    SchlafliSymbol* tesseract = schlafli_parse("{4,3,3}");
    ASSERT_EQ(schlafli_calculate_vertices(tesseract), 16);
    schlafli_free(tesseract);
    
    SchlafliSymbol* cell24 = schlafli_parse("{3,4,3}");
    ASSERT_EQ(schlafli_calculate_vertices(cell24), 24);
    schlafli_free(cell24);
    
    SchlafliSymbol* cell120 = schlafli_parse("{5,3,3}");
    ASSERT_EQ(schlafli_calculate_vertices(cell120), 600);
    schlafli_free(cell120);
    
    SchlafliSymbol* cell600 = schlafli_parse("{3,3,5}");
    ASSERT_EQ(schlafli_calculate_vertices(cell600), 120);
    schlafli_free(cell600);
    
    printf("✓ Property discovery working correctly\n");
}

void test_comprehensive_validation() {
    TEST("Comprehensive Validation with Diagnostics");
    
    // Test valid polytope
    SchlafliSymbol* tetrahedron = schlafli_parse("{3,3}");
    SchlafliValidationResult result = schlafli_validate_comprehensive(tetrahedron);
    
    printf("\nTetrahedron {3,3} validation:\n");
    schlafli_print_validation(&result);
    
    ASSERT_TRUE(result.is_valid);
    ASSERT_TRUE(result.angle_sum_valid);
    ASSERT_TRUE(result.euler_valid);
    ASSERT_EQ(result.euler_characteristic, 2);
    
    schlafli_free(tetrahedron);
    
    // Test invalid polytope
    SchlafliSymbol* invalid = schlafli_parse("{6,3}");
    result = schlafli_validate_comprehensive(invalid);
    
    printf("\nInvalid {6,3} validation:\n");
    schlafli_print_validation(&result);
    
    ASSERT_FALSE(result.is_valid);
    ASSERT_FALSE(result.angle_sum_valid);
    
    schlafli_free(invalid);
    
    printf("✓ Comprehensive validation working correctly\n");
}

void test_nD_polytopes() {
    TEST("nD Polytope Validation");
    
    // 5D simplex {3,3,3,3}
    SchlafliSymbol* simplex5d = schlafli_parse("{3,3,3,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(simplex5d));
    ASSERT_EQ(schlafli_calculate_vertices(simplex5d), 6);  // 5+1
    schlafli_free(simplex5d);
    
    // 5D hypercube {4,3,3,3}
    SchlafliSymbol* hypercube5d = schlafli_parse("{4,3,3,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(hypercube5d));
    ASSERT_EQ(schlafli_calculate_vertices(hypercube5d), 32);  // 2^5
    schlafli_free(hypercube5d);
    
    // 5D cross-polytope {3,3,3,4}
    SchlafliSymbol* cross5d = schlafli_parse("{3,3,3,4}");
    ASSERT_TRUE(schlafli_validate_mathematical(cross5d));
    ASSERT_EQ(schlafli_calculate_vertices(cross5d), 10);  // 2×5
    schlafli_free(cross5d);
    
    // 10D simplex
    SchlafliSymbol* simplex10d = schlafli_parse("{3,3,3,3,3,3,3,3,3}");
    ASSERT_TRUE(schlafli_validate_mathematical(simplex10d));
    ASSERT_EQ(schlafli_calculate_vertices(simplex10d), 11);  // 10+1
    schlafli_free(simplex10d);
    
    printf("✓ nD polytope validation working correctly\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("==============================================\n");
    printf("Schläfli Symbol Mathematical Validation Tests\n");
    printf("==============================================\n");
    
    test_angle_sum_validation();
    test_euler_validation();
    test_mathematical_validation();
    test_property_discovery();
    test_comprehensive_validation();
    test_nD_polytopes();
    
    printf("\n==============================================\n");
    printf("Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("==============================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}