/**
 * @file test_polytope_discovery.c
 * @brief Test suite for polytope discovery system
 */

#include "math/polytope_discovery.h"
#include <stdio.h>
#include <stdlib.h>

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
            printf("✓ PASS: %s (got %u)\n", message, (uint32_t)(actual)); \
            tests_passed++; \
        } else { \
            printf("✗ FAIL: %s (expected %u, got %u)\n", message, \
                   (uint32_t)(expected), (uint32_t)(actual)); \
        } \
    } while(0)

// ============================================================================
// TESTS
// ============================================================================

void test_discover_3d_platonic_solids() {
    TEST("Discover 3D Platonic Solids");
    
    DiscoveryResults* results = discovery_search_dimension(3);
    
    ASSERT(results != NULL, "Discovery results created");
    
    if (results) {
        printf("  Discovered %u polytopes in 3D\n", results->count);
        printf("  Tested %u candidates\n", results->candidates_tested);
        printf("  Search time: %.3f seconds\n", results->search_time_seconds);
        
        ASSERT_EQ(results->count, 5, "Found all 5 Platonic solids");
        
        // Check for specific polytopes
        bool found_tetrahedron = false;
        bool found_cube = false;
        bool found_octahedron = false;
        bool found_dodecahedron = false;
        bool found_icosahedron = false;
        
        for (uint32_t i = 0; i < results->count; i++) {
            DiscoveredPolytope* p = results->polytopes[i];
            if (p->symbol->length == 2) {
                uint32_t p_val = p->symbol->components[0];
                uint32_t q = p->symbol->components[1];
                
                if (p_val == 3 && q == 3) found_tetrahedron = true;
                if (p_val == 4 && q == 3) found_cube = true;
                if (p_val == 3 && q == 4) found_octahedron = true;
                if (p_val == 5 && q == 3) found_dodecahedron = true;
                if (p_val == 3 && q == 5) found_icosahedron = true;
            }
        }
        
        ASSERT(found_tetrahedron, "Found tetrahedron {3,3}");
        ASSERT(found_cube, "Found cube {4,3}");
        ASSERT(found_octahedron, "Found octahedron {3,4}");
        ASSERT(found_dodecahedron, "Found dodecahedron {5,3}");
        ASSERT(found_icosahedron, "Found icosahedron {3,5}");
        
        discovery_results_free(results);
    }
}

void test_discover_4d_polychora() {
    TEST("Discover 4D Regular Polychora");
    
    DiscoveryResults* results = discovery_search_dimension(4);
    
    ASSERT(results != NULL, "Discovery results created");
    
    if (results) {
        printf("  Discovered %u polytopes in 4D\n", results->count);
        printf("  Tested %u candidates\n", results->candidates_tested);
        printf("  Search time: %.3f seconds\n", results->search_time_seconds);
        
        ASSERT_EQ(results->count, 6, "Found all 6 regular polychora");
        
        // Check for specific polytopes
        bool found_5cell = false;
        bool found_tesseract = false;
        bool found_16cell = false;
        bool found_24cell = false;
        bool found_120cell = false;
        bool found_600cell = false;
        
        for (uint32_t i = 0; i < results->count; i++) {
            DiscoveredPolytope* p = results->polytopes[i];
            if (p->symbol->length == 3) {
                uint32_t p_val = p->symbol->components[0];
                uint32_t q = p->symbol->components[1];
                uint32_t r = p->symbol->components[2];
                
                if (p_val == 3 && q == 3 && r == 3) found_5cell = true;
                if (p_val == 4 && q == 3 && r == 3) found_tesseract = true;
                if (p_val == 3 && q == 3 && r == 4) found_16cell = true;
                if (p_val == 3 && q == 4 && r == 3) found_24cell = true;
                if (p_val == 5 && q == 3 && r == 3) found_120cell = true;
                if (p_val == 3 && q == 3 && r == 5) found_600cell = true;
            }
        }
        
        ASSERT(found_5cell, "Found 5-cell {3,3,3}");
        ASSERT(found_tesseract, "Found tesseract {4,3,3}");
        ASSERT(found_16cell, "Found 16-cell {3,3,4}");
        ASSERT(found_24cell, "Found 24-cell {3,4,3}");
        ASSERT(found_120cell, "Found 120-cell {5,3,3}");
        ASSERT(found_600cell, "Found 600-cell {3,3,5}");
        
        discovery_results_free(results);
    }
}

void test_discover_5d_polytopes() {
    TEST("Discover 5D Regular Polytopes");
    
    DiscoveryResults* results = discovery_search_dimension(5);
    
    ASSERT(results != NULL, "Discovery results created");
    
    if (results) {
        printf("  Discovered %u polytopes in 5D\n", results->count);
        printf("  Tested %u candidates\n", results->candidates_tested);
        printf("  Search time: %.3f seconds\n", results->search_time_seconds);
        
        // In 5D+, only 3 regular polytopes exist: simplex, hypercube, cross-polytope
        ASSERT_EQ(results->count, 3, "Found all 3 regular 5D polytopes");
        
        discovery_results_free(results);
    }
}

void test_family_classification() {
    TEST("Family Classification");
    
    // Test simplex classification
    uint32_t simplex_comp[] = {3, 3, 3};
    SchlafliSymbol* simplex = schlafli_create(simplex_comp, 3);
    PolytopeFamily family = discovery_classify_family(simplex);
    ASSERT(family == FAMILY_SIMPLEX, "Simplex classified correctly");
    schlafli_free(simplex);
    
    // Test hypercube classification
    uint32_t cube_comp[] = {4, 3, 3};
    SchlafliSymbol* cube = schlafli_create(cube_comp, 3);
    family = discovery_classify_family(cube);
    ASSERT(family == FAMILY_HYPERCUBE, "Hypercube classified correctly");
    schlafli_free(cube);
    
    // Test cross-polytope classification
    uint32_t cross_comp[] = {3, 3, 4};
    SchlafliSymbol* cross = schlafli_create(cross_comp, 3);
    family = discovery_classify_family(cross);
    ASSERT(family == FAMILY_CROSS_POLYTOPE, "Cross-polytope classified correctly");
    schlafli_free(cross);
    
    // Test exceptional 3D
    uint32_t dodeca_comp[] = {5, 3};
    SchlafliSymbol* dodeca = schlafli_create(dodeca_comp, 2);
    family = discovery_classify_family(dodeca);
    ASSERT(family == FAMILY_EXCEPTIONAL_3D, "Dodecahedron classified as exceptional 3D");
    schlafli_free(dodeca);
    
    // Test exceptional 4D
    uint32_t cell24_comp[] = {3, 4, 3};
    SchlafliSymbol* cell24 = schlafli_create(cell24_comp, 3);
    family = discovery_classify_family(cell24);
    ASSERT(family == FAMILY_EXCEPTIONAL_4D, "24-cell classified as exceptional 4D");
    schlafli_free(cell24);
}

void test_self_duality() {
    TEST("Self-Duality Detection");
    
    // Simplex is self-dual
    uint32_t simplex_comp[] = {3, 3};
    SchlafliSymbol* simplex = schlafli_create(simplex_comp, 2);
    bool is_self_dual = discovery_is_self_dual(simplex);
    ASSERT(is_self_dual, "Tetrahedron is self-dual");
    schlafli_free(simplex);
    
    // 24-cell is self-dual {3,4,3}
    uint32_t cell24_comp[] = {3, 4, 3};
    SchlafliSymbol* cell24 = schlafli_create(cell24_comp, 3);
    is_self_dual = discovery_is_self_dual(cell24);
    ASSERT(is_self_dual, "24-cell is self-dual");
    schlafli_free(cell24);
    
    // Cube is NOT self-dual (dual is octahedron)
    uint32_t cube_comp[] = {4, 3};
    SchlafliSymbol* cube = schlafli_create(cube_comp, 2);
    is_self_dual = discovery_is_self_dual(cube);
    ASSERT(!is_self_dual, "Cube is not self-dual");
    schlafli_free(cube);
}

void test_verify_known_polytopes() {
    TEST("Verify Known Polytopes");
    
    bool verified = discovery_verify_known_polytopes();
    ASSERT(verified, "All known polytopes discovered correctly");
}

void test_multi_dimension_search() {
    TEST("Multi-Dimension Search (3D-5D)");
    
    DiscoveryConfig config = discovery_config_default();
    DiscoveryResults* results = discovery_search(&config);
    
    ASSERT(results != NULL, "Discovery results created");
    
    if (results) {
        printf("  Total discovered: %u polytopes\n", results->count);
        printf("  Tested %u candidates\n", results->candidates_tested);
        printf("  Search time: %.3f seconds\n", results->search_time_seconds);
        
        uint32_t count_3d = discovery_count_by_dimension(results, 3);
        uint32_t count_4d = discovery_count_by_dimension(results, 4);
        uint32_t count_5d = discovery_count_by_dimension(results, 5);
        
        printf("  3D: %u polytopes\n", count_3d);
        printf("  4D: %u polytopes\n", count_4d);
        printf("  5D: %u polytopes\n", count_5d);
        
        ASSERT_EQ(count_3d, 5, "Found 5 3D polytopes");
        ASSERT_EQ(count_4d, 6, "Found 6 4D polytopes");
        ASSERT_EQ(count_5d, 3, "Found 3 5D polytopes");
        
        // Total should be 14
        ASSERT_EQ(results->count, 14, "Total of 14 polytopes found");
        
        discovery_results_free(results);
    }
}

void test_print_results() {
    TEST("Print Discovery Results");
    
    DiscoveryResults* results = discovery_search_dimension(3);
    
    if (results) {
        printf("\n");
        discovery_results_print(results);
        ASSERT(true, "Results printed successfully");
        discovery_results_free(results);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Polytope Discovery System Test Suite                     ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_discover_3d_platonic_solids();
    test_discover_4d_polychora();
    test_discover_5d_polytopes();
    test_family_classification();
    test_self_duality();
    test_verify_known_polytopes();
    test_multi_dimension_search();
    test_print_results();
    
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