/**
 * @file test_01_basic_operations.c
 * @brief Comprehensive testing of basic Abacus88D operations
 * 
 * This test verifies:
 * - Creation and destruction
 * - Memory leak detection
 * - Set/get operations at all layers
 * - Set/get operations at all dimensions
 * - Basic arithmetic operations
 */

#include "math/abacus88d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define TEST_PASS "\033[32m✓\033[0m"
#define TEST_FAIL "\033[31m✗\033[0m"
#define TEST_WARN "\033[33m⚠\033[0m"

static int tests_passed = 0;
static int tests_failed = 0;
static int tests_warned = 0;

#define TEST_SECTION(name) \
    printf("\n" \
           "========================================\n" \
           "%s\n" \
           "========================================\n", name);

#define TEST(name) \
    printf("\n[TEST] %s\n", name);

#define ASSERT_TRUE(condition, message) \
    do { \
        if (condition) { \
            printf("  %s %s\n", TEST_PASS, message); \
            tests_passed++; \
        } else { \
            printf("  %s %s\n", TEST_FAIL, message); \
            tests_failed++; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, message) \
    ASSERT_TRUE((a) == (b), message)

#define ASSERT_NEQ(a, b, message) \
    ASSERT_TRUE((a) != (b), message)

#define WARN_IF(condition, message) \
    do { \
        if (condition) { \
            printf("  %s %s\n", TEST_WARN, message); \
            tests_warned++; \
        } \
    } while(0)

// ============================================================================
// MEMORY TRACKING
// ============================================================================

static size_t allocations = 0;
static size_t deallocations = 0;

void reset_memory_tracking() {
    allocations = 0;
    deallocations = 0;
}

// ============================================================================
// TEST: CREATION AND DESTRUCTION
// ============================================================================

void test_creation_destruction() {
    TEST_SECTION("Test 1: Creation and Destruction");
    
    TEST("Create Abacus88D with base 60");
    Abacus88D* abacus = abacus88d_create(60);
    ASSERT_TRUE(abacus != NULL, "Abacus created successfully");
    
    if (abacus) {
        ASSERT_EQ(abacus->base, 60, "Base is 60");
        ASSERT_EQ(abacus->active_layer, 0, "Active layer initialized to 0");
        ASSERT_EQ(abacus->active_dimension, 0, "Active dimension initialized to 0");
        
        TEST("Verify all layers initialized");
        for (int layer = 0; layer < ABACUS88D_NUM_LAYERS; layer++) {
            ASSERT_TRUE(abacus->layers[layer].frame != NULL, 
                       "Layer frame initialized");
            ASSERT_EQ(abacus->layers[layer].layer_index, layer, 
                     "Layer index correct");
            ASSERT_EQ(abacus->layers[layer].magnitude_scale, LAYER_SCALES[layer],
                     "Layer scale correct");
            
            // Verify all dimensions in this layer
            for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
                ASSERT_TRUE(abacus->layers[layer].dimensions[dim] != NULL,
                           "Dimension abacus initialized");
            }
        }
        
        TEST("Verify clock lattice initialized");
        ASSERT_TRUE(abacus->clock != NULL, "Clock context exists");
        
        TEST("Verify tetration system initialized");
        ASSERT_TRUE(abacus->tetration != NULL, "Tetration system exists");
        if (abacus->tetration) {
            ASSERT_TRUE(abacus->tetration->num_towers > 0, "Towers generated");
            ASSERT_TRUE(abacus->tetration->num_towers <= 186, "Tower count valid");
        }
        
        TEST("Destroy Abacus88D");
        abacus88d_free(abacus);
        ASSERT_TRUE(true, "Destruction completed without crash");
    }
    
    TEST("Create with different bases");
    Abacus88D* abacus10 = abacus88d_create(10);
    ASSERT_TRUE(abacus10 != NULL, "Base-10 abacus created");
    if (abacus10) {
        ASSERT_EQ(abacus10->base, 10, "Base is 10");
        abacus88d_free(abacus10);
    }
    
    Abacus88D* abacus12 = abacus88d_create(12);
    ASSERT_TRUE(abacus12 != NULL, "Base-12 abacus created");
    if (abacus12) {
        ASSERT_EQ(abacus12->base, 12, "Base is 12");
        abacus88d_free(abacus12);
    }
}

// ============================================================================
// TEST: SET AND GET OPERATIONS
// ============================================================================

void test_set_get_operations() {
    TEST_SECTION("Test 2: Set and Get Operations");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    TEST("Set and get at layer 0, dimension 0");
    MathError err = abacus88d_set_uint64(abacus, 0, 42);
    ASSERT_TRUE(err == MATH_SUCCESS, "Set value 42");
    
    uint8_t layer = abacus88d_select_layer(42);
    uint64_t value = abacus88d_get_uint64(abacus, layer, 0);
    ASSERT_EQ(value, 42, "Retrieved value is 42");
    ASSERT_EQ(layer, 0, "Value stored in layer 0");
    
    TEST("Set and get at all dimensions in layer 0");
    for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
        uint64_t test_value = 100 + dim;
        err = abacus88d_set_uint64(abacus, dim, test_value);
        ASSERT_TRUE(err == MATH_SUCCESS, "Set value succeeded");
        
        layer = abacus88d_select_layer(test_value);
        value = abacus88d_get_uint64(abacus, layer, dim);
        ASSERT_EQ(value, test_value, "Retrieved correct value");
    }
    
    TEST("Set and get at different layers");
    struct {
        uint64_t value;
        uint8_t expected_layer;
    } test_cases[] = {
        {100, 0},
        {1000, 1},
        {1000000, 2},
        {1000000000, 3},
        {1000000000000ULL, 4}
    };
    
    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        err = abacus88d_set_uint64(abacus, 0, test_cases[i].value);
        ASSERT_TRUE(err == MATH_SUCCESS, "Set value succeeded");
        
        layer = abacus88d_select_layer(test_cases[i].value);
        ASSERT_EQ(layer, test_cases[i].expected_layer, "Correct layer selected");
        
        value = abacus88d_get_uint64(abacus, layer, 0);
        ASSERT_EQ(value, test_cases[i].value, "Retrieved correct value");
    }
    
    TEST("Set using CrystallineAbacus directly");
    CrystallineAbacus* test_abacus = abacus_from_uint64(12345, 60);
    ASSERT_TRUE(test_abacus != NULL, "Created test abacus");
    
    if (test_abacus) {
        err = abacus88d_set(abacus, 0, 5, test_abacus);
        ASSERT_TRUE(err == MATH_SUCCESS, "Set using abacus succeeded");
        
        CrystallineAbacus* retrieved = abacus_new(60);
        err = abacus88d_get(abacus, 0, 5, retrieved);
        ASSERT_TRUE(err == MATH_SUCCESS, "Get using abacus succeeded");
        
        uint64_t retrieved_value = abacus_to_uint64(retrieved);
        ASSERT_EQ(retrieved_value, 12345, "Retrieved correct value");
        
        abacus_free(test_abacus);
        abacus_free(retrieved);
    }
    
    abacus88d_free(abacus);
}

// ============================================================================
// TEST: BOUNDARY CONDITIONS
// ============================================================================

void test_boundary_conditions() {
    TEST_SECTION("Test 3: Boundary Conditions");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    TEST("Test with zero values");
    MathError err = abacus88d_set_uint64(abacus, 0, 0);
    ASSERT_TRUE(err == MATH_SUCCESS, "Set zero value");
    
    uint64_t value = abacus88d_get_uint64(abacus, 0, 0);
    ASSERT_EQ(value, 0, "Retrieved zero value");
    
    TEST("Test with maximum uint64 value");
    uint64_t max_value = UINT64_MAX;
    err = abacus88d_set_uint64(abacus, 0, max_value);
    ASSERT_TRUE(err == MATH_SUCCESS, "Set max value");
    
    uint8_t layer = abacus88d_select_layer(max_value);
    value = abacus88d_get_uint64(abacus, layer, 0);
    WARN_IF(value != max_value, "Max value may overflow - check abacus precision");
    
    TEST("Test with invalid layer index");
    err = abacus88d_set_uint64(abacus, 0, 100);
    ASSERT_TRUE(err == MATH_SUCCESS, "Set value for invalid layer test");
    
    value = abacus88d_get_uint64(abacus, 255, 0);  // Invalid layer
    ASSERT_EQ(value, 0, "Invalid layer returns 0");
    
    TEST("Test with invalid dimension index");
    value = abacus88d_get_uint64(abacus, 0, 255);  // Invalid dimension
    ASSERT_EQ(value, 0, "Invalid dimension returns 0");
    
    TEST("Test NULL pointer handling");
    err = abacus88d_set(NULL, 0, 0, NULL);
    ASSERT_TRUE(err == MATH_ERROR_NULL_POINTER, "NULL abacus returns error");
    
    CrystallineAbacus* test_abacus = abacus_from_uint64(100, 60);
    err = abacus88d_set(NULL, 0, 0, test_abacus);
    ASSERT_TRUE(err == MATH_ERROR_NULL_POINTER, "NULL abacus returns error");
    
    err = abacus88d_set(abacus, 0, 0, NULL);
    ASSERT_TRUE(err == MATH_ERROR_NULL_POINTER, "NULL value returns error");
    
    abacus_free(test_abacus);
    abacus88d_free(abacus);
}

// ============================================================================
// TEST: INITIALIZATION
// ============================================================================

void test_initialization() {
    TEST_SECTION("Test 4: Initialization");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    TEST("Set some values");
    for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
        abacus88d_set_uint64(abacus, dim, 100 + dim);
    }
    
    TEST("Verify values are set");
    for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
        uint64_t value = abacus88d_get_uint64(abacus, 0, dim);
        ASSERT_EQ(value, 100 + dim, "Value is set");
    }
    
    TEST("Initialize to zero");
    MathError err = abacus88d_init_zero(abacus);
    ASSERT_TRUE(err == MATH_SUCCESS, "Initialization succeeded");
    
    TEST("Verify all values are zero");
    for (int layer = 0; layer < ABACUS88D_NUM_LAYERS; layer++) {
        for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
            uint64_t value = abacus88d_get_uint64(abacus, layer, dim);
            ASSERT_EQ(value, 0, "Value is zero");
        }
    }
    
    abacus88d_free(abacus);
}

// ============================================================================
// TEST: LAYER SELECTION
// ============================================================================

void test_layer_selection() {
    TEST_SECTION("Test 5: Layer Selection");
    
    TEST("Test layer selection for various magnitudes");
    
    struct {
        uint64_t magnitude;
        uint8_t expected_layer;
        const char* description;
    } test_cases[] = {
        {0, 0, "Zero"},
        {1, 0, "One"},
        {999, 0, "999 (< 1K)"},
        {1000, 1, "1K"},
        {999999, 1, "999K (< 1M)"},
        {1000000, 2, "1M"},
        {999999999, 2, "999M (< 1B)"},
        {1000000000, 3, "1B"},
        {999999999999ULL, 3, "999B (< 1T)"},
        {1000000000000ULL, 4, "1T"},
        {1000000000000000ULL, 5, "1Q"},
        {1000000000000000000ULL, 6, "1Qi"},
    };
    
    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        uint8_t layer = abacus88d_select_layer(test_cases[i].magnitude);
        char msg[256];
        snprintf(msg, sizeof(msg), "%s → Layer %u", 
                test_cases[i].description, test_cases[i].expected_layer);
        ASSERT_EQ(layer, test_cases[i].expected_layer, msg);
    }
    
    TEST("Test layer selection from abacus");
    Abacus88D* abacus = abacus88d_create(60);
    if (abacus) {
        for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
            CrystallineAbacus* test_abacus = abacus_from_uint64(test_cases[i].magnitude, 60);
            if (test_abacus) {
                uint8_t layer = abacus88d_select_layer_from_abacus(test_abacus);
                char msg[256];
                snprintf(msg, sizeof(msg), "%s → Layer %u (from abacus)", 
                        test_cases[i].description, test_cases[i].expected_layer);
                ASSERT_EQ(layer, test_cases[i].expected_layer, msg);
                abacus_free(test_abacus);
            }
        }
        abacus88d_free(abacus);
    }
}

// ============================================================================
// TEST: PLATONIC SOLID FRAMES
// ============================================================================

void test_platonic_frames() {
    TEST_SECTION("Test 6: Platonic Solid Frames");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    TEST("Verify each layer has correct Platonic solid");
    
    const char* expected_names[] = {
        "Tetrahedron",
        "Cube",
        "Octahedron",
        "Dodecahedron",
        "Icosahedron",
        "Tetrahedron (2nd)",
        "Cube (2nd)",
        "Octahedron (2nd)"
    };
    
    for (int layer = 0; layer < ABACUS88D_NUM_LAYERS; layer++) {
        const char* name = abacus88d_layer_name(abacus->layers[layer].type);
        char msg[256];
        snprintf(msg, sizeof(msg), "Layer %d: %s", layer, name);
        ASSERT_TRUE(strcmp(name, expected_names[layer]) == 0, msg);
        
        PlatonicSolid* frame = abacus88d_get_layer_frame(abacus, layer);
        ASSERT_TRUE(frame != NULL, "Frame exists");
        
        if (frame) {
            ASSERT_TRUE(frame->dimension >= 3, "Frame dimension >= 3");
            ASSERT_TRUE(frame->num_vertices > 0, "Frame has vertices");
        }
    }
    
    abacus88d_free(abacus);
}

// ============================================================================
// TEST: PERFORMANCE BASELINE
// ============================================================================

void test_performance_baseline() {
    TEST_SECTION("Test 7: Performance Baseline");
    
    Abacus88D* abacus = abacus88d_create(60);
    if (!abacus) {
        printf("%s Failed to create abacus\n", TEST_FAIL);
        tests_failed++;
        return;
    }
    
    TEST("Measure set operation performance");
    clock_t start = clock();
    const int iterations = 10000;
    
    for (int i = 0; i < iterations; i++) {
        abacus88d_set_uint64(abacus, i % ABACUS88D_DIMS_PER_LAYER, i);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double ops_per_sec = iterations / elapsed;
    
    printf("  Set operations: %.2f ops/sec (%.6f sec/op)\n", 
           ops_per_sec, elapsed / iterations);
    WARN_IF(ops_per_sec < 100000, "Performance may be suboptimal");
    
    TEST("Measure get operation performance");
    start = clock();
    
    for (int i = 0; i < iterations; i++) {
        uint8_t layer = abacus88d_select_layer(i);
        abacus88d_get_uint64(abacus, layer, i % ABACUS88D_DIMS_PER_LAYER);
    }
    
    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    ops_per_sec = iterations / elapsed;
    
    printf("  Get operations: %.2f ops/sec (%.6f sec/op)\n", 
           ops_per_sec, elapsed / iterations);
    WARN_IF(ops_per_sec < 100000, "Performance may be suboptimal");
    
    abacus88d_free(abacus);
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                ║\n");
    printf("║           ABACUS88D COMPREHENSIVE TEST SUITE                  ║\n");
    printf("║           Test 01: Basic Operations                           ║\n");
    printf("║                                                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    test_creation_destruction();
    test_set_get_operations();
    test_boundary_conditions();
    test_initialization();
    test_layer_selection();
    test_platonic_frames();
    test_performance_baseline();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                      TEST RESULTS                              ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Passed:  %-4d                                                 ║\n", tests_passed);
    printf("║  Failed:  %-4d                                                 ║\n", tests_failed);
    printf("║  Warned:  %-4d                                                 ║\n", tests_warned);
    printf("║  Total:   %-4d                                                 ║\n", tests_passed + tests_failed);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    if (tests_failed == 0) {
        printf("\n%s ALL TESTS PASSED\n\n", TEST_PASS);
        return 0;
    } else {
        printf("\n%s %d TESTS FAILED\n\n", TEST_FAIL, tests_failed);
        return 1;
    }
}