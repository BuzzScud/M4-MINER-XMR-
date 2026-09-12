/**
 * @file test_generator.c
 * @brief Test program for Platonic solid generator
 */

#include "math/platonic_generator.h"
#include "math/schlafli.h"
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
// SCHLÄFLI SYMBOL TESTS
// ============================================================================

void test_schlafli_parser() {
    TEST("Schläfli Symbol Parser");
    
    // Test parsing
    SchlafliSymbol* sym = schlafli_parse("{3,3}");
    ASSERT_TRUE(sym != NULL);
    ASSERT_EQ(sym->length, 2);
    ASSERT_EQ(sym->components[0], 3);
    ASSERT_EQ(sym->components[1], 3);
    ASSERT_EQ(sym->dimension, 3);  // length + 1 = 3 (3D solid)
    ASSERT_TRUE(sym->is_valid);
    schlafli_free(sym);
    
    // Test cube
    sym = schlafli_parse("{4,3}");
    ASSERT_TRUE(sym != NULL);
    ASSERT_EQ(sym->length, 2);
    ASSERT_TRUE(sym->is_valid);
    schlafli_free(sym);
    
    // Test 4D
    sym = schlafli_parse("{3,3,3}");
    ASSERT_TRUE(sym != NULL);
    ASSERT_EQ(sym->length, 3);
    ASSERT_EQ(sym->dimension, 4);  // length + 1 = 4 (4D solid)
    ASSERT_TRUE(sym->is_valid);
    schlafli_free(sym);
}

// ============================================================================
// 3D SOLID TESTS
// ============================================================================

void test_tetrahedron() {
    TEST("Tetrahedron (3D Simplex)");
    
    PlatonicSolid* solid = platonic_generate_tetrahedron();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 3);
    ASSERT_EQ(solid->num_vertices, 4);
    ASSERT_EQ(solid->num_edges, 6);
    ASSERT_EQ(solid->num_faces, 4);
    ASSERT_EQ(solid->embedding_dim, 48);  // 4 × 12
    ASSERT_EQ(solid->hidden_dim, 72);     // 6 × 12
    ASSERT_EQ(solid->num_layers, 4);
    ASSERT_EQ(solid->num_heads, 12);
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("Tetrahedron properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

void test_cube() {
    TEST("Cube (3D Hypercube)");
    
    PlatonicSolid* solid = platonic_generate_cube();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 3);
    ASSERT_EQ(solid->num_vertices, 8);
    ASSERT_EQ(solid->num_edges, 12);
    ASSERT_EQ(solid->num_faces, 6);
    ASSERT_EQ(solid->embedding_dim, 96);  // 8 × 12
    ASSERT_EQ(solid->hidden_dim, 144);    // 12 × 12
    ASSERT_EQ(solid->num_layers, 6);
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("Cube properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

void test_octahedron() {
    TEST("Octahedron (3D Cross-polytope)");
    
    PlatonicSolid* solid = platonic_generate_octahedron();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 3);
    ASSERT_EQ(solid->num_vertices, 6);
    ASSERT_EQ(solid->num_edges, 12);
    ASSERT_EQ(solid->embedding_dim, 72);  // 6 × 12
    ASSERT_EQ(solid->hidden_dim, 144);    // 12 × 12
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("Octahedron properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

void test_dodecahedron() {
    TEST("Dodecahedron (3D, Golden Ratio)");
    
    PlatonicSolid* solid = platonic_generate_dodecahedron();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 3);
    ASSERT_EQ(solid->num_vertices, 20);
    ASSERT_EQ(solid->num_edges, 30);
    ASSERT_EQ(solid->num_faces, 12);
    ASSERT_EQ(solid->embedding_dim, 240);  // 20 × 12
    ASSERT_EQ(solid->hidden_dim, 360);     // 30 × 12
    ASSERT_EQ(solid->num_layers, 12);
    ASSERT_TRUE(solid->has_golden_ratio);
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("Dodecahedron properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

void test_icosahedron() {
    TEST("Icosahedron (3D, Maximum Symmetry)");
    
    PlatonicSolid* solid = platonic_generate_icosahedron();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 3);
    ASSERT_EQ(solid->num_vertices, 12);
    ASSERT_EQ(solid->num_edges, 30);
    ASSERT_EQ(solid->num_faces, 20);
    ASSERT_EQ(solid->embedding_dim, 144);  // 12 × 12
    ASSERT_EQ(solid->hidden_dim, 360);     // 30 × 12
    ASSERT_EQ(solid->num_layers, 20);
    ASSERT_TRUE(solid->has_golden_ratio);
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("Icosahedron properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

// ============================================================================
// 4D SOLID TESTS
// ============================================================================

void test_5cell() {
    TEST("5-cell (4D Simplex)");
    
    PlatonicSolid* solid = platonic_generate_5cell();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 4);
    ASSERT_EQ(solid->num_vertices, 5);
    ASSERT_EQ(solid->num_edges, 10);
    ASSERT_EQ(solid->num_faces, 10);
    ASSERT_EQ(solid->num_cells, 5);
    ASSERT_EQ(solid->embedding_dim, 60);   // 5 × 12
    ASSERT_EQ(solid->hidden_dim, 120);     // 10 × 12
    ASSERT_EQ(solid->num_layers, 10);
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("5-cell properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

void test_tesseract() {
    TEST("Tesseract (4D Hypercube)");
    
    PlatonicSolid* solid = platonic_generate_tesseract();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 4);
    ASSERT_EQ(solid->num_vertices, 16);
    ASSERT_EQ(solid->num_edges, 32);
    ASSERT_EQ(solid->num_faces, 24);
    ASSERT_EQ(solid->num_cells, 8);
    ASSERT_EQ(solid->embedding_dim, 192);  // 16 × 12
    ASSERT_EQ(solid->hidden_dim, 384);     // 32 × 12
    ASSERT_EQ(solid->num_layers, 24);
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("Tesseract properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

void test_16cell() {
    TEST("16-cell (4D Cross-polytope)");
    
    PlatonicSolid* solid = platonic_generate_16cell();
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 4);
    ASSERT_EQ(solid->num_vertices, 8);
    ASSERT_EQ(solid->num_edges, 24);
    ASSERT_EQ(solid->embedding_dim, 96);   // 8 × 12
    ASSERT_EQ(solid->hidden_dim, 288);     // 24 × 12
    ASSERT_TRUE(platonic_validate_euler(solid));
    
    printf("16-cell properties:\n");
    platonic_print(solid);
    
    platonic_free(solid);
}

// ============================================================================
// MAIN GENERATOR TESTS
// ============================================================================

void test_main_generator() {
    TEST("Main Generator Function");
    
    // Test 3D solids
    PlatonicSolid* tetra = platonic_generate(3, (uint32_t[]){3,3}, 2);
    ASSERT_TRUE(tetra != NULL);
    ASSERT_EQ(tetra->num_vertices, 4);
    platonic_free(tetra);
    
    PlatonicSolid* cube = platonic_generate(3, (uint32_t[]){4,3}, 2);
    ASSERT_TRUE(cube != NULL);
    ASSERT_EQ(cube->num_vertices, 8);
    platonic_free(cube);
    
    PlatonicSolid* octa = platonic_generate(3, (uint32_t[]){3,4}, 2);
    ASSERT_TRUE(octa != NULL);
    ASSERT_EQ(octa->num_vertices, 6);
    platonic_free(octa);
    
    PlatonicSolid* dodeca = platonic_generate(3, (uint32_t[]){5,3}, 2);
    ASSERT_TRUE(dodeca != NULL);
    ASSERT_EQ(dodeca->num_vertices, 20);
    platonic_free(dodeca);
    
    PlatonicSolid* icosa = platonic_generate(3, (uint32_t[]){3,5}, 2);
    ASSERT_TRUE(icosa != NULL);
    ASSERT_EQ(icosa->num_vertices, 12);
    platonic_free(icosa);
    
    // Test 4D solids
    PlatonicSolid* cell5 = platonic_generate(4, (uint32_t[]){3,3,3}, 3);
    ASSERT_TRUE(cell5 != NULL);
    ASSERT_EQ(cell5->num_vertices, 5);
    platonic_free(cell5);
    
    PlatonicSolid* tess = platonic_generate(4, (uint32_t[]){4,3,3}, 3);
    ASSERT_TRUE(tess != NULL);
    ASSERT_EQ(tess->num_vertices, 16);
    platonic_free(tess);
    
    PlatonicSolid* cell16 = platonic_generate(4, (uint32_t[]){3,3,4}, 3);
    ASSERT_TRUE(cell16 != NULL);
    ASSERT_EQ(cell16->num_vertices, 8);
    platonic_free(cell16);
    
    printf("Main generator: All tests passed!\n");
}

// ============================================================================
// DIMENSION SCALING TESTS
// ============================================================================

void test_simplex_scaling() {
    TEST("Simplex Dimension Scaling");
    
    for (uint32_t d = 3; d <= 10; d++) {
        PlatonicSolid* solid = platonic_generate_simplex(d);
        ASSERT_TRUE(solid != NULL);
        ASSERT_EQ(solid->dimension, d);
        ASSERT_EQ(solid->num_vertices, d + 1);
        ASSERT_TRUE(platonic_validate_euler(solid));
        
        printf("  %uD simplex: %lu vertices, %lu edges, %lu faces\n",
               d, (unsigned long)solid->num_vertices,
               (unsigned long)solid->num_edges,
               (unsigned long)solid->num_faces);
        
        platonic_free(solid);
    }
}

void test_hypercube_scaling() {
    TEST("Hypercube Dimension Scaling");
    
    for (uint32_t d = 3; d <= 8; d++) {
        PlatonicSolid* solid = platonic_generate_hypercube(d);
        ASSERT_TRUE(solid != NULL);
        ASSERT_EQ(solid->dimension, d);
        ASSERT_EQ(solid->num_vertices, 1ULL << d);  // 2^d
        ASSERT_TRUE(platonic_validate_euler(solid));
        
        printf("  %uD hypercube: %lu vertices, %lu edges, %lu faces\n",
               d, (unsigned long)solid->num_vertices,
               (unsigned long)solid->num_edges,
               (unsigned long)solid->num_faces);
        
        platonic_free(solid);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("=================================================\n");
    printf("Platonic Solid Generator Test Suite\n");
    printf("=================================================\n");
    
    // Schläfli tests
    printf("Starting Schläfli tests...\n");
    test_schlafli_parser();
    printf("Schläfli tests complete.\n");
    
    // 3D tests (all 5 Platonic solids)
    printf("Starting 3D tests...\n");
    test_tetrahedron();
    printf("Tetrahedron test complete.\n");
    test_cube();
    printf("Cube test complete.\n");
    test_octahedron();
    printf("Octahedron test complete.\n");
    test_dodecahedron();
    printf("Dodecahedron test complete.\n");
    test_icosahedron();
    printf("Icosahedron test complete.\n");
    
    // 4D tests
    printf("Starting 4D tests...\n");
    test_5cell();
    printf("5-cell test complete.\n");
    test_tesseract();
    printf("Tesseract test complete.\n");
    test_16cell();
    printf("16-cell test complete.\n");
    
    // Main generator test
    printf("Starting main generator test...\n");
    test_main_generator();
    printf("Main generator test complete.\n");
    
    // Scaling tests
    test_simplex_scaling();
    test_hypercube_scaling();
    
    // Summary
    printf("\n=================================================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("=================================================\n");
    
    return (tests_failed == 0) ? 0 : 1;
}