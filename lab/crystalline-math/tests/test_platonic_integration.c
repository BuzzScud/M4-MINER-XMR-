/**
 * @file test_platonic_integration.c
 * @brief Tests for Platonic Solids Integration
 */

#include "math/compact_vector.h"
#include "math/platonic_generator.h"
#include "math/polytope.h"
#include <stdio.h>
#include <stdlib.h>

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("Testing %s... ", name); \
    fflush(stdout);

#define PASS() \
    printf("PASS\n"); \
    tests_passed++;

#define FAIL(msg) \
    printf("FAIL: %s\n", msg); \
    tests_failed++;

/* ============================================================================
 * SOLID SELECTION TESTS
 * ============================================================================
 */

void test_solid_selection_small() {
    TEST("select_solid_for_magnitude - small");
    
    // Small magnitude should give tetrahedron (4 vertices)
    PlatonicSolid* solid = select_solid_for_magnitude(2);
    
    if (!solid) {
        FAIL("Failed to select solid");
        return;
    }
    
    if (solid->num_vertices != 4) {
        platonic_free(solid);
        FAIL("Expected tetrahedron (4 vertices)");
        return;
    }
    
    printf("(tetrahedron: %lu vertices) ", solid->num_vertices);
    platonic_free(solid);
    PASS();
}

void test_solid_selection_medium() {
    TEST("select_solid_for_magnitude - medium");
    
    // Medium magnitude should give icosahedron (12 vertices)
    PlatonicSolid* solid = select_solid_for_magnitude(15);
    
    if (!solid) {
        FAIL("Failed to select solid");
        return;
    }
    
    if (solid->num_vertices != 12) {
        platonic_free(solid);
        FAIL("Expected icosahedron (12 vertices)");
        return;
    }
    
    printf("(icosahedron: %lu vertices) ", solid->num_vertices);
    platonic_free(solid);
    PASS();
}

void test_solid_selection_large() {
    TEST("select_solid_for_magnitude - large");
    
    // Large magnitude should give 4D+ polytope
    PlatonicSolid* solid = select_solid_for_magnitude(500);
    
    if (!solid) {
        FAIL("Failed to select solid");
        return;
    }
    
    // Should have more vertices than 3D solids
    if (solid->num_vertices <= 20) {
        platonic_free(solid);
        FAIL("Expected 4D+ polytope");
        return;
    }
    
    printf("(%uD: %lu vertices) ", solid->dimension, (unsigned long)solid->num_vertices);
    platonic_free(solid);
    PASS();
}

void test_solid_selection_progression() {
    TEST("solid_selection - progression");
    
    // Test that vertex count increases with magnitude
    uint64_t magnitudes[] = {2, 10, 50, 200, 1000};
    uint64_t prev_vertices = 0;
    
    for (size_t i = 0; i < 5; i++) {
        PlatonicSolid* solid = select_solid_for_magnitude(magnitudes[i]);
        if (!solid) {
            FAIL("Failed to select solid");
            return;
        }
        
        if (solid->num_vertices < prev_vertices) {
            platonic_free(solid);
            FAIL("Vertex count should increase with magnitude");
            return;
        }
        
        prev_vertices = solid->num_vertices;
        platonic_free(solid);
    }
    
    PASS();
}

/* ============================================================================
 * VERTEX MAPPING TESTS
 * ============================================================================
 */

void test_angle_to_vertex_basic() {
    TEST("map_angle_to_vertex - basic");
    
    // Create a simple solid (tetrahedron)
    PlatonicSolid* solid = platonic_generate_simplex(3);
    if (!solid) {
        FAIL("Failed to create solid");
        return;
    }
    
    // Map angle to vertex
    uint32_t vertex = map_angle_to_vertex(45.0, solid);
    
    // Should be within valid range
    if (vertex >= solid->num_vertices) {
        platonic_free(solid);
        FAIL("Vertex out of range");
        return;
    }
    
    printf("(angle 45° → vertex %u) ", vertex);
    platonic_free(solid);
    PASS();
}

void test_angle_to_vertex_coverage() {
    TEST("map_angle_to_vertex - full coverage");
    
    PlatonicSolid* solid = platonic_generate_simplex(3);
    if (!solid) {
        FAIL("Failed to create solid");
        return;
    }
    
    // Test that all angles map to valid vertices
    for (double angle = 0.0; angle < 360.0; angle += 30.0) {
        uint32_t vertex = map_angle_to_vertex(angle, solid);
        if (vertex >= solid->num_vertices) {
            platonic_free(solid);
            FAIL("Vertex out of range");
            return;
        }
    }
    
    platonic_free(solid);
    PASS();
}

void test_number_to_solid() {
    TEST("map_number_to_solid");
    
    PlatonicSolid* solid;
    uint32_t vertex;
    
    MathError err = map_number_to_solid(157, &solid, &vertex);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to map number to solid");
        return;
    }
    
    if (!solid) {
        FAIL("Solid is NULL");
        return;
    }
    
    if (vertex >= solid->num_vertices) {
        platonic_free(solid);
        FAIL("Vertex out of range");
        return;
    }
    
    printf("(157 → %lu-vertex solid, vertex %u) ", solid->num_vertices, vertex);
    platonic_free(solid);
    PASS();
}

/* ============================================================================
 * TRAJECTORY TESTS
 * ============================================================================
 */

void test_trajectory_creation() {
    TEST("create_trajectory_for_operation");
    
    GeometricTrajectory* traj;
    MathError err = create_trajectory_for_operation(
        OP_ADD,
        5,
        7,
        &traj
    );
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to create trajectory");
        return;
    }
    
    if (!traj) {
        FAIL("Trajectory is NULL");
        return;
    }
    
    if (!traj->solid) {
        trajectory_free(traj);
        FAIL("Solid is NULL");
        return;
    }
    
    if (traj->path_length < 2) {
        trajectory_free(traj);
        FAIL("Path too short");
        return;
    }
    
    printf("(path length: %zu) ", traj->path_length);
    trajectory_free(traj);
    PASS();
}

/* ============================================================================
 * MULTI-VALUE MAPPING TESTS
 * ============================================================================
 */

void test_multi_value_mapping() {
    TEST("create_multi_value_mapping");
    
    uint64_t values[] = {5, 7, 11, 13};
    MultiValueMapping* mapping;
    
    MathError err = create_multi_value_mapping(values, 4, &mapping);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to create mapping");
        return;
    }
    
    if (!mapping) {
        FAIL("Mapping is NULL");
        return;
    }
    
    if (!mapping->solid) {
        multi_value_mapping_free(mapping);
        FAIL("Solid is NULL");
        return;
    }
    
    if (mapping->num_values != 4) {
        multi_value_mapping_free(mapping);
        FAIL("Wrong number of values");
        return;
    }
    
    // Check all vertices are valid
    for (size_t i = 0; i < mapping->num_values; i++) {
        if (mapping->vertex_ids[i] >= mapping->solid->num_vertices) {
            multi_value_mapping_free(mapping);
            FAIL("Vertex out of range");
            return;
        }
    }
    
    printf("(%lu vertices mapped) ", mapping->num_values);
    multi_value_mapping_free(mapping);
    PASS();
}

/* ============================================================================
 * EXTENDED VECTOR TESTS
 * ============================================================================
 */

void test_extended_vector_creation() {
    TEST("create_extended_vector");
    
    ExtendedCompactVector vec;
    MathError err = create_extended_vector(157, &vec);
    
    if (err != MATH_SUCCESS) {
        FAIL("Failed to create extended vector");
        return;
    }
    
    // Check base vector
    if (vec.base.sphere_id != 0) {
        FAIL("Sphere ID incorrect");
        return;
    }
    
    // Check solid type is set
    if (vec.solid == 0) {
        FAIL("Solid type not set");
        return;
    }
    
    // Check ring is set
    if (vec.ring > 3) {
        FAIL("Ring out of range");
        return;
    }
    
    printf("(solid: %d, vertex: %u, ring: %u) ", vec.solid, vec.vertex_id, vec.ring);
    PASS();
}

void test_extended_vector_consistency() {
    TEST("extended_vector - consistency");
    
    // Create multiple extended vectors and check consistency
    for (uint64_t num = 10; num < 100; num += 10) {
        ExtendedCompactVector vec;
        MathError err = create_extended_vector(num, &vec);
        
        if (err != MATH_SUCCESS) {
            FAIL("Failed to create extended vector");
            return;
        }
        
        // Verify all fields are reasonable
        if (vec.base.phase_angle < 0.0f || vec.base.phase_angle >= 360.0f) {
            FAIL("Phase angle out of range");
            return;
        }
        
        if (vec.ring > 3) {
            FAIL("Ring out of range");
            return;
        }
    }
    
    PASS();
}

/* ============================================================================
 * MAIN
 * ============================================================================
 */

int main() {
    printf("=== Platonic Solids Integration Tests ===\n\n");
    
    // Solid selection tests
    test_solid_selection_small();
    test_solid_selection_medium();
    test_solid_selection_large();
    test_solid_selection_progression();
    
    // Vertex mapping tests
    test_angle_to_vertex_basic();
    test_angle_to_vertex_coverage();
    test_number_to_solid();
    
    // Trajectory tests
    test_trajectory_creation();
    
    // Multi-value mapping tests
    test_multi_value_mapping();
    
    // Extended vector tests
    test_extended_vector_creation();
    test_extended_vector_consistency();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}