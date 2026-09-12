/**
 * @file test_abacus_generator.c
 * @brief Test Abacus-based Platonic solid generators
 */

#include "math/platonic_generator_abacus.h"
#include "math/polytope_abacus.h"
#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>

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

void test_simplex_abacus() {
    TEST("Simplex with Abacus (base 12, precision 6)");
    
    PlatonicSolidAbacus* solid = platonic_generate_simplex_abacus(3, 12, 6);
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 3);
    ASSERT_EQ(solid->num_vertices, 4);
    ASSERT_EQ(solid->num_edges, 6);
    ASSERT_EQ(solid->num_faces, 4);
    ASSERT_EQ(solid->abacus_base, 12);
    ASSERT_EQ(solid->abacus_precision, 6);
    
    // Verify coordinates are Abacus
    ASSERT_TRUE(solid->vertex_coords != NULL);
    ASSERT_TRUE(solid->vertex_coords[0] != NULL);
    
    printf("Tetrahedron (Abacus):\n");
    printf("  Vertices: %lu\n", (unsigned long)solid->num_vertices);
    printf("  Edges: %lu\n", (unsigned long)solid->num_edges);
    printf("  Faces: %lu\n", (unsigned long)solid->num_faces);
    printf("  Base: %u\n", solid->abacus_base);
    printf("  Precision: %d\n", solid->abacus_precision);
    
    platonic_abacus_free(solid);
}

void test_hypercube_abacus() {
    TEST("Hypercube with Abacus (base 60, precision 8)");
    
    PlatonicSolidAbacus* solid = platonic_generate_hypercube_abacus(3, 60, 8);
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 3);
    ASSERT_EQ(solid->num_vertices, 8);
    ASSERT_EQ(solid->num_edges, 12);
    ASSERT_EQ(solid->num_faces, 6);
    ASSERT_EQ(solid->abacus_base, 60);
    ASSERT_EQ(solid->abacus_precision, 8);
    
    printf("Cube (Abacus):\n");
    printf("  Vertices: %lu\n", (unsigned long)solid->num_vertices);
    printf("  Edges: %lu\n", (unsigned long)solid->num_edges);
    printf("  Faces: %lu\n", (unsigned long)solid->num_faces);
    printf("  Base: %u\n", solid->abacus_base);
    printf("  Precision: %d\n", solid->abacus_precision);
    
    platonic_abacus_free(solid);
}

void test_conversion() {
    TEST("Conversion between double and Abacus");
    
    // Generate with double
    extern PlatonicSolid* platonic_generate_tetrahedron(void);
    PlatonicSolid* solid_double = platonic_generate_tetrahedron();
    ASSERT_TRUE(solid_double != NULL);
    
    // Convert to Abacus
    PlatonicSolidAbacus* solid_abacus = platonic_double_to_abacus(solid_double, 12, 6);
    ASSERT_TRUE(solid_abacus != NULL);
    ASSERT_EQ(solid_abacus->num_vertices, solid_double->num_vertices);
    ASSERT_EQ(solid_abacus->num_edges, solid_double->num_edges);
    ASSERT_EQ(solid_abacus->num_faces, solid_double->num_faces);
    
    // Convert back to double
    PlatonicSolid* solid_double2 = platonic_abacus_to_double(solid_abacus);
    ASSERT_TRUE(solid_double2 != NULL);
    ASSERT_EQ(solid_double2->num_vertices, solid_double->num_vertices);
    ASSERT_EQ(solid_double2->num_edges, solid_double->num_edges);
    ASSERT_EQ(solid_double2->num_faces, solid_double->num_faces);
    
    printf("Conversion test passed\n");
    
    extern void platonic_free(PlatonicSolid*);
    platonic_free(solid_double);
    platonic_free(solid_double2);
    platonic_abacus_free(solid_abacus);
}

void test_high_dimension() {
    TEST("High-dimensional simplex (10D) with Abacus");
    
    PlatonicSolidAbacus* solid = platonic_generate_simplex_abacus(10, 12, 10);
    ASSERT_TRUE(solid != NULL);
    ASSERT_EQ(solid->dimension, 10);
    ASSERT_EQ(solid->num_vertices, 11);
    
    printf("10D Simplex (Abacus):\n");
    printf("  Vertices: %lu\n", (unsigned long)solid->num_vertices);
    printf("  Edges: %lu\n", (unsigned long)solid->num_edges);
    printf("  Faces: %lu\n", (unsigned long)solid->num_faces);
    
    platonic_abacus_free(solid);
}

void test_golden_ratio_solids() {
    TEST("Dodecahedron with Abacus (golden ratio)");
    
    PlatonicSolidAbacus* dodeca = platonic_generate_dodecahedron_abacus(12, 10);
    ASSERT_TRUE(dodeca != NULL);
    ASSERT_EQ(dodeca->dimension, 3);
    ASSERT_EQ(dodeca->num_vertices, 20);
    ASSERT_EQ(dodeca->num_edges, 30);
    ASSERT_EQ(dodeca->num_faces, 12);
    ASSERT_TRUE(dodeca->has_golden_ratio);
    
    printf("Dodecahedron (Abacus with golden ratio):\n");
    printf("  Vertices: %lu\n", (unsigned long)dodeca->num_vertices);
    printf("  Edges: %lu\n", (unsigned long)dodeca->num_edges);
    printf("  Faces: %lu\n", (unsigned long)dodeca->num_faces);
    printf("  Has golden ratio: %s\n", dodeca->has_golden_ratio ? "YES" : "NO");
    
    platonic_abacus_free(dodeca);
    
    TEST("Icosahedron with Abacus (golden ratio)");
    
    PlatonicSolidAbacus* icosa = platonic_generate_icosahedron_abacus(12, 10);
    ASSERT_TRUE(icosa != NULL);
    ASSERT_EQ(icosa->dimension, 3);
    ASSERT_EQ(icosa->num_vertices, 12);
    ASSERT_EQ(icosa->num_edges, 30);
    ASSERT_EQ(icosa->num_faces, 20);
    ASSERT_TRUE(icosa->has_golden_ratio);
    
    printf("Icosahedron (Abacus with golden ratio):\n");
    printf("  Vertices: %lu\n", (unsigned long)icosa->num_vertices);
    printf("  Edges: %lu\n", (unsigned long)icosa->num_edges);
    printf("  Faces: %lu\n", (unsigned long)icosa->num_faces);
    printf("  Has golden ratio: %s\n", icosa->has_golden_ratio ? "YES" : "NO");
    
    platonic_abacus_free(icosa);
}

int main(void) {
    printf("=================================================\n");
    printf("Abacus-Based Platonic Solid Generator Tests\n");
    printf("=================================================\n");
    
    test_simplex_abacus();
    test_hypercube_abacus();
    test_conversion();
    test_high_dimension();
    test_golden_ratio_solids();
    
    printf("\n=================================================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("=================================================\n");
    
    return (tests_failed == 0) ? 0 : 1;
}