/**
 * @file test_schlafli_enhanced.c
 * @brief Test suite for enhanced Schläfli validation
 */

#include "math/schlafli_math.h"
#include "math/schlafli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("\n=== Test: %s ===\n", name); \
        tests_run++; \
    } while(0)

#define ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ PASS: %s\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ FAIL: %s\n", message); \
        } \
    } while(0)

#define ASSERT_EQ(actual, expected, message) \
    do { \
        if ((actual) == (expected)) { \
            printf("✓ PASS: %s (got %lu)\n", message, (uint64_t)(actual)); \
            tests_passed++; \
        } else { \
            printf("✗ FAIL: %s (expected %lu, got %lu)\n", message, \
                   (uint64_t)(expected), (uint64_t)(actual)); \
        } \
    } while(0)

// ============================================================================
// TEST: 3D PLATONIC SOLIDS
// ============================================================================

void test_3d_tetrahedron() {
    TEST("3D Tetrahedron {3,3}");
    
    uint32_t components[] = {3, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 2);
    
    ASSERT(symbol != NULL, "Symbol created");
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    uint64_t F = schlafli_calculate_faces_general(symbol);
    
    ASSERT_EQ(V, 4, "Vertices = 4");
    ASSERT_EQ(E, 6, "Edges = 6");
    ASSERT_EQ(F, 4, "Faces = 4");
    
    // Check Euler characteristic: V - E + F = 2
    int64_t euler = (int64_t)V - (int64_t)E + (int64_t)F;
    ASSERT_EQ(euler, 2, "Euler characteristic = 2");
    
    // Check validation
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "Tetrahedron is valid");
    
    // Check f-vector
    FaceVector* fv = schlafli_calculate_f_vector(symbol);
    ASSERT(fv != NULL, "f-vector calculated");
    if (fv) {
        printf("  f-vector: [%lu, %lu, %lu]\n", fv->f_vector[0], fv->f_vector[1], fv->f_vector[2]);
        schlafli_free_f_vector(fv);
    }
    
    schlafli_free(symbol);
}

void test_3d_cube() {
    TEST("3D Cube {4,3}");
    
    uint32_t components[] = {4, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 2);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    uint64_t F = schlafli_calculate_faces_general(symbol);
    
    ASSERT_EQ(V, 8, "Vertices = 8");
    ASSERT_EQ(E, 12, "Edges = 12");
    ASSERT_EQ(F, 6, "Faces = 6");
    
    int64_t euler = (int64_t)V - (int64_t)E + (int64_t)F;
    ASSERT_EQ(euler, 2, "Euler characteristic = 2");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "Cube is valid");
    
    schlafli_free(symbol);
}

void test_3d_octahedron() {
    TEST("3D Octahedron {3,4}");
    
    uint32_t components[] = {3, 4};
    SchlafliSymbol* symbol = schlafli_create(components, 2);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    uint64_t F = schlafli_calculate_faces_general(symbol);
    
    ASSERT_EQ(V, 6, "Vertices = 6");
    ASSERT_EQ(E, 12, "Edges = 12");
    ASSERT_EQ(F, 8, "Faces = 8");
    
    int64_t euler = (int64_t)V - (int64_t)E + (int64_t)F;
    ASSERT_EQ(euler, 2, "Euler characteristic = 2");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "Octahedron is valid");
    
    schlafli_free(symbol);
}

void test_3d_dodecahedron() {
    TEST("3D Dodecahedron {5,3}");
    
    uint32_t components[] = {5, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 2);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    uint64_t F = schlafli_calculate_faces_general(symbol);
    
    ASSERT_EQ(V, 20, "Vertices = 20");
    ASSERT_EQ(E, 30, "Edges = 30");
    ASSERT_EQ(F, 12, "Faces = 12");
    
    int64_t euler = (int64_t)V - (int64_t)E + (int64_t)F;
    ASSERT_EQ(euler, 2, "Euler characteristic = 2");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "Dodecahedron is valid");
    
    schlafli_free(symbol);
}

void test_3d_icosahedron() {
    TEST("3D Icosahedron {3,5}");
    
    uint32_t components[] = {3, 5};
    SchlafliSymbol* symbol = schlafli_create(components, 2);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    uint64_t F = schlafli_calculate_faces_general(symbol);
    
    ASSERT_EQ(V, 12, "Vertices = 12");
    ASSERT_EQ(E, 30, "Edges = 30");
    ASSERT_EQ(F, 20, "Faces = 20");
    
    int64_t euler = (int64_t)V - (int64_t)E + (int64_t)F;
    ASSERT_EQ(euler, 2, "Euler characteristic = 2");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "Icosahedron is valid");
    
    schlafli_free(symbol);
}

// ============================================================================
// TEST: 4D POLYCHORA
// ============================================================================

void test_4d_5cell() {
    TEST("4D 5-cell {3,3,3}");
    
    uint32_t components[] = {3, 3, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 3);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    uint64_t F = schlafli_calculate_faces_general(symbol);
    uint64_t C = schlafli_calculate_cells_general(symbol);
    
    ASSERT_EQ(V, 5, "Vertices = 5");
    ASSERT_EQ(E, 10, "Edges = 10");
    ASSERT_EQ(F, 10, "Faces = 10");
    ASSERT_EQ(C, 5, "Cells = 5");
    
    // Check Euler characteristic: V - E + F - C = 0
    int64_t euler = (int64_t)V - (int64_t)E + (int64_t)F - (int64_t)C;
    ASSERT_EQ(euler, 0, "Euler characteristic = 0");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "5-cell is valid");
    
    schlafli_free(symbol);
}

void test_4d_tesseract() {
    TEST("4D Tesseract {4,3,3}");
    
    uint32_t components[] = {4, 3, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 3);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    
    ASSERT_EQ(V, 16, "Vertices = 16");
    ASSERT_EQ(E, 32, "Edges = 32");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "Tesseract is valid");
    
    schlafli_free(symbol);
}

void test_4d_16cell() {
    TEST("4D 16-cell {3,3,4}");
    
    uint32_t components[] = {3, 3, 4};
    SchlafliSymbol* symbol = schlafli_create(components, 3);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    uint64_t C = schlafli_calculate_cells_general(symbol);
    
    ASSERT_EQ(V, 8, "Vertices = 8");
    ASSERT_EQ(E, 24, "Edges = 24");
    ASSERT_EQ(C, 16, "Cells = 16");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "16-cell is valid");
    
    schlafli_free(symbol);
}

// ============================================================================
// TEST: INVALID POLYTOPES
// ============================================================================

void test_invalid_polytopes() {
    TEST("Invalid Polytopes");
    
    // {6,3} - hexagonal tiling (infinite, not a polytope)
    uint32_t comp1[] = {6, 3};
    SchlafliSymbol* sym1 = schlafli_create(comp1, 2);
    bool valid1 = schlafli_is_valid_regular_polytope(sym1);
    ASSERT(!valid1, "{6,3} is invalid (infinite tiling)");
    schlafli_free(sym1);
    
    // {3,6} - triangular tiling (infinite, not a polytope)
    uint32_t comp2[] = {3, 6};
    SchlafliSymbol* sym2 = schlafli_create(comp2, 2);
    bool valid2 = schlafli_is_valid_regular_polytope(sym2);
    ASSERT(!valid2, "{3,6} is invalid (infinite tiling)");
    schlafli_free(sym2);
    
    // {4,4} - square tiling (infinite, not a polytope)
    uint32_t comp3[] = {4, 4};
    SchlafliSymbol* sym3 = schlafli_create(comp3, 2);
    bool valid3 = schlafli_is_valid_regular_polytope(sym3);
    ASSERT(!valid3, "{4,4} is invalid (infinite tiling)");
    schlafli_free(sym3);
    
    // {5,5} - pentagonal tiling (impossible)
    uint32_t comp4[] = {5, 5};
    SchlafliSymbol* sym4 = schlafli_create(comp4, 2);
    bool valid4 = schlafli_is_valid_regular_polytope(sym4);
    ASSERT(!valid4, "{5,5} is invalid (impossible)");
    schlafli_free(sym4);
}

// ============================================================================
// TEST: HIGHER DIMENSIONS
// ============================================================================

void test_5d_simplex() {
    TEST("5D Simplex {3,3,3,3}");
    
    uint32_t components[] = {3, 3, 3, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 4);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    
    ASSERT_EQ(V, 6, "Vertices = 6");
    ASSERT_EQ(E, 15, "Edges = 15");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "5D simplex is valid");
    
    FaceVector* fv = schlafli_calculate_f_vector(symbol);
    if (fv) {
        printf("  ");
        schlafli_print_f_vector(fv);
        schlafli_free_f_vector(fv);
    }
    
    schlafli_free(symbol);
}

void test_10d_simplex() {
    TEST("10D Simplex {3,3,3,3,3,3,3,3,3}");
    
    uint32_t components[] = {3, 3, 3, 3, 3, 3, 3, 3, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 9);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    
    ASSERT_EQ(V, 11, "Vertices = 11");
    ASSERT_EQ(E, 55, "Edges = 55");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "10D simplex is valid");
    
    schlafli_free(symbol);
}

void test_10d_hypercube() {
    TEST("10D Hypercube {4,3,3,3,3,3,3,3,3}");
    
    uint32_t components[] = {4, 3, 3, 3, 3, 3, 3, 3, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 9);
    
    uint64_t V = schlafli_calculate_vertices_general(symbol);
    uint64_t E = schlafli_calculate_edges_general(symbol);
    
    ASSERT_EQ(V, 1024, "Vertices = 1024 (2^10)");
    ASSERT_EQ(E, 5120, "Edges = 5120 (10 * 2^9)");
    
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    ASSERT(valid, "10D hypercube is valid");
    
    schlafli_free(symbol);
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Enhanced Schläfli Validation Test Suite                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // 3D Platonic solids
    test_3d_tetrahedron();
    test_3d_cube();
    test_3d_octahedron();
    test_3d_dodecahedron();
    test_3d_icosahedron();
    
    // 4D polychora
    test_4d_5cell();
    test_4d_tesseract();
    test_4d_16cell();
    
    // Invalid polytopes
    test_invalid_polytopes();
    
    // Higher dimensions
    test_5d_simplex();
    test_10d_simplex();
    test_10d_hypercube();
    
    // Summary
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test Summary                                              ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Tests Run:    %-3d                                         ║\n", tests_run);
    printf("║  Tests Passed: %-3d                                         ║\n", tests_passed);
    printf("║  Success Rate: %.1f%%                                       ║\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}