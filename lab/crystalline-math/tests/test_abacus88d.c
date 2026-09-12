/**
 * @file test_abacus88d.c
 * @brief Test suite for 88D geometric abacus
 */

#include "math/abacus88d.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS "\033[32m✓\033[0m"
#define TEST_FAIL "\033[31m✗\033[0m"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("\nTest: %s\n", name); \
    printf("----------------------------------------\n");

#define ASSERT_TRUE(condition, message) \
    if (condition) { \
        printf("%s %s\n", TEST_PASS, message); \
        tests_passed++; \
    } else { \
        printf("%s %s\n", TEST_FAIL, message); \
        tests_failed++; \
    }

#define ASSERT_EQ(a, b, message) \
    ASSERT_TRUE((a) == (b), message)

void test_creation() {
    TEST("88D Abacus Creation");
    
    Abacus88D* abacus = abacus88d_create(60);
    ASSERT_TRUE(abacus != NULL, "Abacus created successfully");
    
    if (abacus) {
        ASSERT_EQ(abacus->base, 60, "Base is 60");
        ASSERT_TRUE(abacus->clock != NULL, "Clock lattice initialized");
        ASSERT_TRUE(abacus->tetration != NULL, "Tetration system initialized");
        
        // Check all layers initialized
        for (int i = 0; i < ABACUS88D_NUM_LAYERS; i++) {
            ASSERT_TRUE(abacus->layers[i].frame != NULL, "Layer frame initialized");
            
            // Check all dimensions initialized
            for (int j = 0; j < ABACUS88D_DIMS_PER_LAYER; j++) {
                ASSERT_TRUE(abacus->layers[i].dimensions[j] != NULL, 
                           "Dimension initialized");
            }
        }
        
        abacus88d_free(abacus);
    }
}

void test_layer_selection() {
    TEST("Layer Selection");
    
    ASSERT_EQ(abacus88d_select_layer(100), 0, "100 → Layer 0");
    ASSERT_EQ(abacus88d_select_layer(1000), 1, "1000 → Layer 1");
    ASSERT_EQ(abacus88d_select_layer(1000000), 2, "1000000 → Layer 2");
    ASSERT_EQ(abacus88d_select_layer(1000000000), 3, "1000000000 → Layer 3");
    ASSERT_EQ(abacus88d_select_layer(1000000000000ULL), 4, "1 trillion → Layer 4");
}

void test_set_get() {
    TEST("Set and Get Operations");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    // Set value using uint64
    MathError err = abacus88d_set_uint64(abacus, 0, 42);
    ASSERT_TRUE(err == MATH_SUCCESS, "Set value 42 at dimension 0");
    
    // Get value back
    uint8_t layer = abacus88d_select_layer(42);
    uint64_t value = abacus88d_get_uint64(abacus, layer, 0);
    ASSERT_EQ(value, 42, "Retrieved value is 42");
    
    // Set larger value
    err = abacus88d_set_uint64(abacus, 5, 1000000);
    ASSERT_TRUE(err == MATH_SUCCESS, "Set value 1000000 at dimension 5");
    
    layer = abacus88d_select_layer(1000000);
    value = abacus88d_get_uint64(abacus, layer, 5);
    ASSERT_EQ(value, 1000000, "Retrieved value is 1000000");
    ASSERT_EQ(layer, 2, "Value is in layer 2");
    
    abacus88d_free(abacus);
}

void test_cross_dimensional_add() {
    TEST("Cross-Dimensional Addition");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    // Set values in different layers
    abacus88d_set_uint64(abacus, 0, 100);      // Layer 0
    abacus88d_set_uint64(abacus, 1, 1000);     // Layer 1
    
    // Add across dimensions (same layer)
    uint8_t layer0 = abacus88d_select_layer(100);
    MathError err = abacus88d_add_cross(abacus,
                                        layer0, 0,  // 100 at dim 0
                                        layer0, 1,  // 1000 at dim 1
                                        layer0, 2); // result at dim 2
    
    ASSERT_TRUE(err == MATH_SUCCESS, "Cross-dimensional addition succeeded");
    
    uint64_t result = abacus88d_get_uint64(abacus, layer0, 2);
    printf("  Result: %lu (expected ~1100)\n", result);
    
    abacus88d_free(abacus);
}

void test_prime_generation() {
    TEST("Prime Generation via Clock Lattice");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    // Generate primes at different dimensions
    CrystallineAbacus* prime = abacus_new(60);
    if (!prime) {
        printf("%s Failed to create prime abacus\n", TEST_FAIL);
        tests_failed++;
        abacus88d_free(abacus);
        return;
    }
    
    // Generate prime at dimension 0 (position 1), magnitude 0
    MathError err = abacus88d_generate_prime(abacus, 0, 0, 0, prime);
    ASSERT_TRUE(err == MATH_SUCCESS, "Generated prime at dim 0");
    
    uint64_t prime_value;
    abacus_to_uint64(prime, &prime_value);
    printf("  Prime at (layer=0, dim=0, mag=0): %lu\n", prime_value);
    
    // Generate prime at dimension 4 (position 5), magnitude 0
    err = abacus88d_generate_prime(abacus, 0, 4, 0, prime);
    ASSERT_TRUE(err == MATH_SUCCESS, "Generated prime at dim 4");
    
    abacus_to_uint64(prime, &prime_value);
    printf("  Prime at (layer=0, dim=4, mag=0): %lu\n", prime_value);
    
    // Map prime back to coordinates
    uint8_t layer, dimension;
    uint64_t magnitude;
    
    CrystallineAbacus* test_prime = abacus_from_uint64(17, 60);
    err = abacus88d_map_prime(abacus, test_prime, &layer, &dimension, &magnitude);
    ASSERT_TRUE(err == MATH_SUCCESS, "Mapped prime 17 to coordinates");
    printf("  Prime 17 maps to: layer=%u, dim=%u, mag=%lu\n", 
           layer, dimension, magnitude);
    
    abacus_free(test_prime);
    abacus_free(prime);
    abacus88d_free(abacus);
}

void test_layer_frames() {
    TEST("Platonic Solid Frames");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    // Check each layer has correct Platonic solid
    const char* expected_names[] = {
        "Tetrahedron",
        "Cube", 
        "Octahedron",
        "Dodecahedron",
        "Icosahedron",
        "Tetrahedron",
        "Cube",
        "Octahedron"
    };
    
    for (int i = 0; i < ABACUS88D_NUM_LAYERS; i++) {
        const char* name = abacus88d_layer_name(abacus->layers[i].type);
        printf("  Layer %d: %s (scale: %lu)\n", 
               i, name, abacus->layers[i].magnitude_scale);
        ASSERT_TRUE(strcmp(name, expected_names[i]) == 0, "Correct Platonic solid");
    }
    
    abacus88d_free(abacus);
}

void test_tetration_system() {
    TEST("Tetration Tower System");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    ASSERT_TRUE(abacus->tetration != NULL, "Tetration system exists");
    
    if (abacus->tetration) {
        printf("  Number of towers: %zu\n", abacus->tetration->num_towers);
        ASSERT_TRUE(abacus->tetration->num_towers > 0, "Towers generated");
        ASSERT_TRUE(abacus->tetration->num_towers <= 186, "Tower count <= 186");
        
        // Check first few towers
        for (size_t i = 0; i < 5 && i < abacus->tetration->num_towers; i++) {
            TetrationTower88D* tower = &abacus->tetration->towers[i];
            printf("  Tower %zu: base=%lu, depth=%u, layers=%u→%u\n",
                   i, tower->base, tower->depth, 
                   tower->source_layer, tower->target_layer);
            ASSERT_TRUE(tower->value != NULL, "Tower has value");
        }
    }
    
    abacus88d_free(abacus);
}

void test_print() {
    TEST("Print Functions");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    // Set some values
    abacus88d_set_uint64(abacus, 0, 42);
    abacus88d_set_uint64(abacus, 5, 1000000);
    
    printf("\n");
    abacus88d_print(abacus);
    
    ASSERT_TRUE(true, "Print completed without crash");
    
    abacus88d_free(abacus);
}

int main() {
    printf("========================================\n");
    printf("88D Geometric Abacus Test Suite\n");
    printf("========================================\n");
    
    test_creation();
    test_layer_selection();
    test_set_get();
    test_cross_dimensional_add();
    test_prime_generation();
    test_layer_frames();
    test_tetration_system();
    test_print();
    
    printf("\n========================================\n");
    printf("Test Results\n");
    printf("========================================\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\n%s ALL TESTS PASSED\n", TEST_PASS);
        return 0;
    } else {
        printf("\n%s SOME TESTS FAILED\n", TEST_FAIL);
        return 1;
    }
}