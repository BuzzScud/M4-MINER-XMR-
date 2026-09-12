/**
 * @file test_sphere_hopping.c
 * @brief Tests for Sphere Hopping System
 */

#include "math/compact_vector.h"
#include "math/clock.h"
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
 * SPHERE HIERARCHY TESTS
 * ============================================================================
 */

void test_hierarchy_creation() {
    TEST("create_sphere_hierarchy");
    
    CompactSphere* root = create_sphere_hierarchy(3);
    
    if (!root) {
        FAIL("Failed to create hierarchy");
        return;
    }
    
    if (root->level != 0) {
        free_sphere_hierarchy(root);
        FAIL("Root level should be 0");
        return;
    }
    
    if (root->id != 0) {
        free_sphere_hierarchy(root);
        FAIL("Root ID should be 0");
        return;
    }
    
    printf("(root created) ");
    free_sphere_hierarchy(root);
    PASS();
}

void test_hierarchy_children() {
    TEST("hierarchy_children - 12-fold");
    
    CompactSphere* root = create_sphere_hierarchy(2);
    
    if (!root) {
        FAIL("Failed to create hierarchy");
        return;
    }
    
    // Check root has 12 children
    int child_count = 0;
    for (int i = 0; i < 12; i++) {
        if (root->children[i]) {
            child_count++;
        }
    }
    
    if (child_count != 12) {
        free_sphere_hierarchy(root);
        FAIL("Root should have 12 children");
        return;
    }
    
    printf("(12 children) ");
    free_sphere_hierarchy(root);
    PASS();
}

void test_hierarchy_depth() {
    TEST("hierarchy_depth");
    
    CompactSphere* root = create_sphere_hierarchy(3);
    
    if (!root) {
        FAIL("Failed to create hierarchy");
        return;
    }
    
    // Check depth 1
    if (!root->children[0]) {
        free_sphere_hierarchy(root);
        FAIL("Missing level 1");
        return;
    }
    
    if (root->children[0]->level != 1) {
        free_sphere_hierarchy(root);
        FAIL("Level 1 incorrect");
        return;
    }
    
    // Check depth 2
    if (!root->children[0]->children[0]) {
        free_sphere_hierarchy(root);
        FAIL("Missing level 2");
        return;
    }
    
    if (root->children[0]->children[0]->level != 2) {
        free_sphere_hierarchy(root);
        FAIL("Level 2 incorrect");
        return;
    }
    
    printf("(3 levels) ");
    free_sphere_hierarchy(root);
    PASS();
}

void test_sphere_count() {
    TEST("count_spheres");
    
    CompactSphere* root = create_sphere_hierarchy(2);
    
    if (!root) {
        FAIL("Failed to create hierarchy");
        return;
    }
    
    uint32_t count = count_spheres(root);
    
    // Level 0: 1 sphere
    // Level 1: 12 spheres
    // Level 2: 144 spheres
    // Total: 1 + 12 + 144 = 157
    // But max_depth=2 means we create levels 0, 1 only (not 2)
    // So: 1 + 12 = 13
    if (count != 13) {
        printf("(expected 13, got %u) ", count);
        free_sphere_hierarchy(root);
        FAIL("Sphere count incorrect");
        return;
    }
    
    printf("(13 spheres) ");
    free_sphere_hierarchy(root);
    PASS();
}

/* ============================================================================
 * SPHERE HOPPING TESTS
 * ============================================================================
 */

void test_phase_difference() {
    TEST("calculate_sphere_phase_difference");
    
    double phase_diff = calculate_sphere_phase_difference(0, 1);
    
    // Sphere 1 is 30° from sphere 0
    if (phase_diff < 25.0 || phase_diff > 35.0) {
        FAIL("Phase difference incorrect");
        return;
    }
    
    printf("(0→1: %.1f°) ", phase_diff);
    PASS();
}

void test_magnitude_scale() {
    TEST("calculate_magnitude_scale");
    
    int32_t scale = calculate_magnitude_scale(0, 1);
    
    // Moving to deeper level should scale up
    if (scale <= 1) {
        FAIL("Scale should be > 1 for deeper level");
        return;
    }
    
    printf("(scale: %d) ", scale);
    PASS();
}

void test_sphere_hop_basic() {
    TEST("sphere_hop - basic");
    
    ClockContext ctx;
    MathError err = clock_init(&ctx);
    if (err != MATH_SUCCESS) {
        FAIL("Failed to initialize clock");
        return;
    }
    
    // Create number with vector at sphere 0
    CompactNumber* num;
    err = compact_number_from_uint64(157, 12, 50, &ctx, &num);
    
    if (err != MATH_SUCCESS) {
        clock_cleanup(&ctx);
        FAIL("Failed to create number");
        return;
    }
    
    // Hop from sphere 0 to sphere 1
    CompactVector result;
    err = sphere_hop(num, 0, 1, &result);
    
    if (err != MATH_SUCCESS) {
        compact_number_free(num);
        clock_cleanup(&ctx);
        FAIL("Failed to hop");
        return;
    }
    
    // Result should be at sphere 1
    if (result.sphere_id != 1) {
        compact_number_free(num);
        clock_cleanup(&ctx);
        FAIL("Result sphere incorrect");
        return;
    }
    
    printf("(0→1 successful) ");
    compact_number_free(num);
    clock_cleanup(&ctx);
    PASS();
}

/* ============================================================================
 * MAGNITUDE SCALING TESTS
 * ============================================================================
 */

void test_sphere_radius() {
    TEST("calculate_sphere_radius");
    
    double r0 = calculate_sphere_radius(0);
    double r10 = calculate_sphere_radius(10);
    double r100 = calculate_sphere_radius(100);
    
    // Radius should increase with magnitude
    if (r10 <= r0 || r100 <= r10) {
        FAIL("Radius should increase with magnitude");
        return;
    }
    
    printf("(r0: %.2f, r10: %.2f, r100: %.2f) ", r0, r10, r100);
    PASS();
}

void test_magnitude_to_level() {
    TEST("magnitude_to_level");
    
    if (magnitude_to_level(5) != 0) {
        FAIL("Level 0 incorrect");
        return;
    }
    
    if (magnitude_to_level(50) != 1) {
        FAIL("Level 1 incorrect");
        return;
    }
    
    if (magnitude_to_level(500) != 2) {
        FAIL("Level 2 incorrect");
        return;
    }
    
    PASS();
}

void test_scale_factor() {
    TEST("get_scale_factor");
    
    double s0 = get_scale_factor(0);
    double s1 = get_scale_factor(1);
    double s2 = get_scale_factor(2);
    
    // Each level is 1/12 of previous
    if (s0 != 1.0) {
        FAIL("Level 0 scale should be 1.0");
        return;
    }
    
    if (s1 < 0.08 || s1 > 0.09) {  // ~1/12
        FAIL("Level 1 scale incorrect");
        return;
    }
    
    if (s2 < 0.006 || s2 > 0.008) {  // ~1/144
        FAIL("Level 2 scale incorrect");
        return;
    }
    
    printf("(s0: %.3f, s1: %.3f, s2: %.4f) ", s0, s1, s2);
    PASS();
}

/* ============================================================================
 * MAIN
 * ============================================================================
 */

int main() {
    printf("=== Sphere Hopping System Tests ===\n\n");
    
    // Hierarchy tests
    test_hierarchy_creation();
    test_hierarchy_children();
    test_hierarchy_depth();
    test_sphere_count();
    
    // Hopping tests
    test_phase_difference();
    test_magnitude_scale();
    test_sphere_hop_basic();
    
    // Scaling tests
    test_sphere_radius();
    test_magnitude_to_level();
    test_scale_factor();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}