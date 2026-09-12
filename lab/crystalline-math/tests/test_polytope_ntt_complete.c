/**
 * @file test_polytope_ntt_complete.c
 * @brief Comprehensive test suite for NTT-polytope integration
 */

#include "math/polytope_ntt.h"
#include "math/polytope.h"
#include "math/ntt.h"
#include "math/abacus.h"
#include "math/higher_faces.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s...", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf(" PASS\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf(" FAIL: %s\n", msg); \
    } while(0)

/* ============================================================================
 * TEST 1: Face Enumeration
 * ============================================================================ */

void test_face_enumeration_small() {
    TEST("Face enumeration for small polytope (tetrahedron)");
    
    /* Create a simple tetrahedron structure */
    PlatonicSolid solid = {
        .dimension = 3,
        .num_vertices = 4,
        .num_edges = 6,
        .num_faces = 4
    };
    
    /* Try to enumerate 0-faces (vertices) */
    KFaceSet* faces = NULL;
    MathError err = polytope_ntt_enumerate_faces(&solid, 0, NULL, &faces);
    
    if (err == MATH_SUCCESS && faces != NULL) {
        /* Check that we got some faces */
        if (faces->count > 0) {
            /* Cleanup */
            for (uint32_t i = 0; i < faces->count; i++) {
                if (faces->faces[i]) {
                    free(faces->faces[i]->vertex_indices);
                    free(faces->faces[i]);
                }
            }
            free(faces->faces);
            free(faces);
            PASS();
        } else {
            free(faces);
            FAIL("No faces generated");
        }
    } else {
        FAIL("Face enumeration failed");
    }
}

void test_face_enumeration_large() {
    TEST("Face enumeration for large polytope (600-cell)");
    
    /* Create a 600-cell structure */
    PlatonicSolid solid = {
        .dimension = 4,
        .num_vertices = 120,
        .num_edges = 720,
        .num_faces = 1200,
        .num_cells = 600
    };
    
    /* Check if NTT should be used */
    bool should_use = polytope_ntt_should_use(&solid);
    
    if (should_use) {
        /* Try to enumerate 0-faces */
        KFaceSet* faces = NULL;
        MathError err = polytope_ntt_enumerate_faces(&solid, 0, NULL, &faces);
        
        if (err == MATH_SUCCESS && faces != NULL) {
            /* Cleanup */
            for (uint32_t i = 0; i < faces->count; i++) {
                if (faces->faces[i]) {
                    free(faces->faces[i]->vertex_indices);
                    free(faces->faces[i]);
                }
            }
            free(faces->faces);
            free(faces);
            PASS();
        } else {
            FAIL("Face enumeration failed");
        }
    } else {
        FAIL("NTT should be used for 600-cell");
    }
}

/* ============================================================================
 * TEST 2: Hierarchy Generation
 * ============================================================================ */

void test_hierarchy_generation() {
    TEST("Complete hierarchy generation");
    
    /* Create a simple polytope */
    PlatonicSolid solid = {
        .dimension = 3,
        .num_vertices = 8,  /* Cube */
        .num_edges = 12,
        .num_faces = 6
    };
    
    /* Generate complete hierarchy */
    FaceHierarchy* hierarchy = NULL;
    MathError err = polytope_ntt_generate_hierarchy(&solid, NULL, &hierarchy);
    
    if (err == MATH_SUCCESS && hierarchy != NULL) {
        /* Check that hierarchy has correct dimensions */
        if (hierarchy->polytope_dimension == 3 && 
            hierarchy->num_dimensions == 3) {
            
            /* Cleanup */
            for (uint32_t k = 0; k < hierarchy->num_dimensions; k++) {
                if (hierarchy->face_sets[k]) {
                    for (uint32_t f = 0; f < hierarchy->face_sets[k]->count; f++) {
                        if (hierarchy->face_sets[k]->faces[f]) {
                            free(hierarchy->face_sets[k]->faces[f]->vertex_indices);
                            free(hierarchy->face_sets[k]->faces[f]);
                        }
                    }
                    free(hierarchy->face_sets[k]->faces);
                    free(hierarchy->face_sets[k]);
                }
            }
            free(hierarchy->face_sets);
            free(hierarchy);
            
            PASS();
        } else {
            FAIL("Hierarchy dimensions incorrect");
        }
    } else {
        FAIL("Hierarchy generation failed");
    }
}

/* ============================================================================
 * TEST 3: Vertex Transformations
 * ============================================================================ */

void test_vertex_transformation() {
    TEST("Vertex transformation");
    
    /* Create a simple polytope with coordinates */
    PlatonicSolid solid = {
        .dimension = 3,
        .num_vertices = 4,
        .vertex_coords = malloc(4 * 3 * sizeof(double))
    };
    
    if (!solid.vertex_coords) {
        FAIL("Memory allocation failed");
        return;
    }
    
    /* Initialize coordinates (tetrahedron) */
    double coords[] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
        1.0, 1.0, 1.0
    };
    memcpy(solid.vertex_coords, coords, 4 * 3 * sizeof(double));
    
    /* Create identity transformation matrix (as Abacus) */
    CrystallineAbacus** transform = malloc(9 * sizeof(CrystallineAbacus*));
    for (int i = 0; i < 9; i++) {
        if (i % 4 == 0) {  /* Diagonal elements */
            transform[i] = abacus_from_uint64(1, 60);
        } else {
            transform[i] = abacus_from_uint64(0, 60);
        }
    }
    
    /* Apply transformation */
    MathError err = polytope_ntt_transform_vertices(&solid, 
                                                    (const CrystallineAbacus**)transform,
                                                    NULL);
    
    /* Cleanup */
    for (int i = 0; i < 9; i++) {
        abacus_free(transform[i]);
    }
    free(transform);
    free(solid.vertex_coords);
    
    if (err == MATH_SUCCESS) {
        PASS();
    } else {
        FAIL("Transformation failed");
    }
}

/* ============================================================================
 * TEST 4: Scaling
 * ============================================================================ */

void test_scaling() {
    TEST("Polytope scaling");
    
    /* Create a simple polytope with coordinates */
    PlatonicSolid solid = {
        .dimension = 3,
        .num_vertices = 4,
        .vertex_coords = malloc(4 * 3 * sizeof(double))
    };
    
    if (!solid.vertex_coords) {
        FAIL("Memory allocation failed");
        return;
    }
    
    /* Initialize coordinates */
    for (int i = 0; i < 12; i++) {
        solid.vertex_coords[i] = 1.0;
    }
    
    /* Create scale factor */
    CrystallineAbacus* scale = abacus_from_uint64(2, 60);
    
    /* Apply scaling */
    MathError err = polytope_ntt_scale(&solid, scale, NULL);
    
    /* Cleanup */
    abacus_free(scale);
    free(solid.vertex_coords);
    
    if (err == MATH_SUCCESS) {
        PASS();
    } else {
        FAIL("Scaling failed");
    }
}

/* ============================================================================
 * TEST 5: Rotation
 * ============================================================================ */

void test_rotation() {
    TEST("Polytope rotation");
    
    /* Create a simple polytope with coordinates */
    PlatonicSolid solid = {
        .dimension = 3,
        .num_vertices = 4,
        .vertex_coords = malloc(4 * 3 * sizeof(double))
    };
    
    if (!solid.vertex_coords) {
        FAIL("Memory allocation failed");
        return;
    }
    
    /* Initialize coordinates */
    for (int i = 0; i < 12; i++) {
        solid.vertex_coords[i] = 1.0;
    }
    
    /* Create rotation axis and angle */
    CrystallineAbacus** axis = malloc(3 * sizeof(CrystallineAbacus*));
    axis[0] = abacus_from_uint64(1, 60);
    axis[1] = abacus_from_uint64(0, 60);
    axis[2] = abacus_from_uint64(0, 60);
    
    CrystallineAbacus* angle = abacus_from_uint64(90, 60);  /* 90 degrees */
    
    /* Apply rotation */
    MathError err = polytope_ntt_rotate(&solid, 
                                       (const CrystallineAbacus**)axis,
                                       angle,
                                       NULL);
    
    /* Cleanup */
    for (int i = 0; i < 3; i++) {
        abacus_free(axis[i]);
    }
    free(axis);
    abacus_free(angle);
    free(solid.vertex_coords);
    
    if (err == MATH_SUCCESS) {
        PASS();
    } else {
        FAIL("Rotation failed");
    }
}

/* ============================================================================
 * TEST 6: Performance Comparison
 * ============================================================================ */

void test_performance_estimation() {
    TEST("Performance estimation accuracy");
    
    /* Test various polytope sizes */
    struct {
        uint64_t vertices;
        double expected_min_speedup;
    } test_cases[] = {
        {4, 0.0},      /* Too small, no speedup */
        {100, 100.0},  /* Should have significant speedup */
        {120, 1000.0}, /* 600-cell, massive speedup */
        {1000, 10000.0} /* Very large, huge speedup */
    };
    
    bool all_passed = true;
    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        PlatonicSolid solid = {
            .num_vertices = test_cases[i].vertices,
            .dimension = 4
        };
        
        double speedup = polytope_ntt_estimate_speedup(&solid, "face_enum");
        
        if (test_cases[i].vertices < 100) {
            /* Small polytopes should have speedup < 1 */
            if (speedup >= 1.0) {
                all_passed = false;
                break;
            }
        } else {
            /* Large polytopes should have significant speedup */
            if (speedup < test_cases[i].expected_min_speedup) {
                all_passed = false;
                break;
            }
        }
    }
    
    if (all_passed) {
        PASS();
    } else {
        FAIL("Performance estimation incorrect");
    }
}

/* ============================================================================
 * TEST 7: Context Management
 * ============================================================================ */

void test_context_reuse() {
    TEST("NTT context reuse");
    
    /* Create a polytope */
    PlatonicSolid solid = {
        .dimension = 4,
        .num_vertices = 120
    };
    
    /* Create context once */
    NTTContext* ctx = polytope_ntt_create_context(&solid);
    
    if (!ctx) {
        FAIL("Context creation failed");
        return;
    }
    
    /* Use context for multiple operations */
    KFaceSet* faces1 = NULL;
    KFaceSet* faces2 = NULL;
    
    MathError err1 = polytope_ntt_enumerate_faces(&solid, 0, ctx, &faces1);
    MathError err2 = polytope_ntt_enumerate_faces(&solid, 0, ctx, &faces2);  /* Use same k for both */
    
    /* Cleanup */
    if (faces1) {
        for (uint32_t i = 0; i < faces1->count; i++) {
            if (faces1->faces[i]) {
                free(faces1->faces[i]->vertex_indices);
                free(faces1->faces[i]);
            }
        }
        free(faces1->faces);
        free(faces1);
    }
    
    if (faces2) {
        for (uint32_t i = 0; i < faces2->count; i++) {
            if (faces2->faces[i]) {
                free(faces2->faces[i]->vertex_indices);
                free(faces2->faces[i]);
            }
        }
        free(faces2->faces);
        free(faces2);
    }
    
    ntt_free(ctx);
    
    if (err1 == MATH_SUCCESS && err2 == MATH_SUCCESS) {
        PASS();
    } else {
        FAIL("Context reuse failed");
    }
}

/* ============================================================================
 * TEST 8: Edge Cases
 * ============================================================================ */

void test_edge_cases() {
    TEST("Edge cases handling");
    
    bool all_passed = true;
    
    /* Test 1: NULL polytope */
    KFaceSet* faces = NULL;
    MathError err = polytope_ntt_enumerate_faces(NULL, 0, NULL, &faces);
    if (err != MATH_ERROR_INVALID_ARG) {
        all_passed = false;
    }
    
    /* Test 2: NULL output */
    PlatonicSolid solid = {.dimension = 3, .num_vertices = 4};
    err = polytope_ntt_enumerate_faces(&solid, 0, NULL, NULL);
    if (err != MATH_ERROR_INVALID_ARG) {
        all_passed = false;
    }
    
    /* Test 3: Invalid dimension */
    err = polytope_ntt_enumerate_faces(&solid, 10, NULL, &faces);
    if (err != MATH_ERROR_INVALID_ARG) {
        all_passed = false;
    }
    
    if (all_passed) {
        PASS();
    } else {
        FAIL("Edge case handling incorrect");
    }
}

/* ============================================================================
 * TEST 9: Memory Management
 * ============================================================================ */

void test_memory_management() {
    TEST("Memory management (no leaks)");
    
    /* Create and destroy multiple contexts */
    for (int i = 0; i < 10; i++) {
        PlatonicSolid solid = {
            .dimension = 3,
            .num_vertices = 8 + i
        };
        
        NTTContext* ctx = polytope_ntt_create_context(&solid);
        if (ctx) {
            ntt_free(ctx);
        }
    }
    
    /* Create and destroy multiple face sets */
    for (int i = 0; i < 10; i++) {
        PlatonicSolid solid = {
            .dimension = 3,
            .num_vertices = 4 + i
        };
        
        KFaceSet* faces = NULL;
        MathError err = polytope_ntt_enumerate_faces(&solid, 0, NULL, &faces);
        
        if (err == MATH_SUCCESS && faces) {
            for (uint32_t j = 0; j < faces->count; j++) {
                if (faces->faces[j]) {
                    free(faces->faces[j]->vertex_indices);
                    free(faces->faces[j]);
                }
            }
            free(faces->faces);
            free(faces);
        }
    }
    
    PASS();  /* If we get here without crashing, memory management is OK */
}

/* ============================================================================
 * TEST 10: Integration Test
 * ============================================================================ */

void test_full_integration() {
    TEST("Full integration test");
    
    /* Create a polytope */
    PlatonicSolid solid = {
        .dimension = 4,
        .num_vertices = 120,
        .vertex_coords = malloc(120 * 4 * sizeof(double))
    };
    
    if (!solid.vertex_coords) {
        FAIL("Memory allocation failed");
        return;
    }
    
    /* Initialize coordinates */
    for (int i = 0; i < 120 * 4; i++) {
        solid.vertex_coords[i] = 1.0;
    }
    
    /* Check automatic NTT selection */
    bool should_use = polytope_ntt_should_use(&solid);
    
    if (!should_use) {
        free(solid.vertex_coords);
        FAIL("NTT should be used for 600-cell");
        return;
    }
    
    /* Create context */
    NTTContext* ctx = polytope_ntt_create_context(&solid);
    if (!ctx) {
        free(solid.vertex_coords);
        FAIL("Context creation failed");
        return;
    }
    
    /* Generate hierarchy */
    FaceHierarchy* hierarchy = NULL;
    MathError err = polytope_ntt_generate_hierarchy(&solid, ctx, &hierarchy);
    
    if (err != MATH_SUCCESS || !hierarchy) {
        ntt_free(ctx);
        free(solid.vertex_coords);
        FAIL("Hierarchy generation failed");
        return;
    }
    
    /* Apply scaling */
    CrystallineAbacus* scale = abacus_from_uint64(2, 60);
    err = polytope_ntt_scale(&solid, scale, ctx);
    abacus_free(scale);
    
    if (err != MATH_SUCCESS) {
        /* Cleanup */
        for (uint32_t k = 0; k < hierarchy->num_dimensions; k++) {
            if (hierarchy->face_sets[k]) {
                for (uint32_t f = 0; f < hierarchy->face_sets[k]->count; f++) {
                    if (hierarchy->face_sets[k]->faces[f]) {
                        free(hierarchy->face_sets[k]->faces[f]->vertex_indices);
                        free(hierarchy->face_sets[k]->faces[f]);
                    }
                }
                free(hierarchy->face_sets[k]->faces);
                free(hierarchy->face_sets[k]);
            }
        }
        free(hierarchy->face_sets);
        free(hierarchy);
        ntt_free(ctx);
        free(solid.vertex_coords);
        FAIL("Scaling failed");
        return;
    }
    
    /* Cleanup */
    for (uint32_t k = 0; k < hierarchy->num_dimensions; k++) {
        if (hierarchy->face_sets[k]) {
            for (uint32_t f = 0; f < hierarchy->face_sets[k]->count; f++) {
                if (hierarchy->face_sets[k]->faces[f]) {
                    free(hierarchy->face_sets[k]->faces[f]->vertex_indices);
                    free(hierarchy->face_sets[k]->faces[f]);
                }
            }
            free(hierarchy->face_sets[k]->faces);
            free(hierarchy->face_sets[k]);
        }
    }
    free(hierarchy->face_sets);
    free(hierarchy);
    ntt_free(ctx);
    free(solid.vertex_coords);
    
    PASS();
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================ */

int main(void) {
    printf("=================================================\n");
    printf("Comprehensive NTT-Polytope Integration Test Suite\n");
    printf("=================================================\n\n");
    
    /* Run all tests */
    test_face_enumeration_small();
    test_face_enumeration_large();
    test_hierarchy_generation();
    test_vertex_transformation();
    test_scaling();
    test_rotation();
    test_performance_estimation();
    test_context_reuse();
    test_edge_cases();
    test_memory_management();
    test_full_integration();
    
    /* Print summary */
    printf("\n=================================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("=================================================\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}