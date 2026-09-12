/**
 * @file test_polytope_ntt.c
 * @brief Test suite for NTT integration with polytope system
 */

#include "math/polytope_ntt.h"
#include "math/polytope.h"
#include "math/ntt.h"
#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
 * TEST 1: Automatic NTT Selection
 * ============================================================================ */

void test_ntt_selection_small_polytope() {
    TEST("NTT selection for small polytope (tetrahedron)");
    
    /* Create a small polytope (4 vertices) */
    PlatonicSolid solid = {
        .num_vertices = 4,
        .dimension = 3
    };
    
    /* Should NOT use NTT (too small) */
    bool should_use = polytope_ntt_should_use(&solid);
    
    if (!should_use) {
        PASS();
    } else {
        FAIL("Small polytope should not use NTT");
    }
}

void test_ntt_selection_large_polytope() {
    TEST("NTT selection for large polytope (600-cell)");
    
    /* Create a large polytope (120 vertices) */
    PlatonicSolid solid = {
        .num_vertices = 120,
        .dimension = 4
    };
    
    /* SHOULD use NTT (large enough) */
    bool should_use = polytope_ntt_should_use(&solid);
    
    if (should_use) {
        PASS();
    } else {
        FAIL("Large polytope should use NTT");
    }
}

void test_ntt_selection_threshold() {
    TEST("NTT selection with custom threshold");
    
    /* Create a medium polytope (50 vertices) */
    PlatonicSolid solid = {
        .num_vertices = 50,
        .dimension = 3
    };
    
    /* With threshold 100: should NOT use NTT */
    bool should_use_100 = polytope_ntt_should_use_threshold(&solid, 100);
    
    /* With threshold 25: SHOULD use NTT */
    bool should_use_25 = polytope_ntt_should_use_threshold(&solid, 25);
    
    if (!should_use_100 && should_use_25) {
        PASS();
    } else {
        FAIL("Custom threshold not working correctly");
    }
}

/* ============================================================================
 * TEST 2: Prime Selection
 * ============================================================================ */

void test_ntt_prime_selection() {
    TEST("NTT prime selection for various polytope sizes");
    
    /* Test various sizes */
    struct {
        uint64_t vertices;
        uint64_t expected_min_prime;
    } test_cases[] = {
        {4, 257},           /* Small: 2^8 + 1 */
        {120, 257},         /* Medium: 2^8 + 1 */
        {1000, 65537},      /* Large: 2^16 + 1 */
    };
    
    bool all_passed = true;
    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        PlatonicSolid solid = {
            .num_vertices = test_cases[i].vertices,
            .dimension = 3
        };
        
        uint64_t prime = polytope_ntt_find_optimal_prime(&solid);
        
        if (prime < test_cases[i].expected_min_prime) {
            all_passed = false;
            break;
        }
    }
    
    if (all_passed) {
        PASS();
    } else {
        FAIL("Prime selection incorrect");
    }
}

/* ============================================================================
 * TEST 3: Transform Size
 * ============================================================================ */

void test_ntt_transform_size() {
    TEST("NTT transform size calculation");
    
    /* Test various vertex counts */
    struct {
        uint64_t vertices;
        size_t expected_size;
    } test_cases[] = {
        {4, 4},      /* Already power of 2 */
        {5, 8},      /* Round up to 8 */
        {120, 128},  /* Round up to 128 */
        {1000, 1024}, /* Round up to 1024 */
    };
    
    bool all_passed = true;
    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        PlatonicSolid solid = {
            .num_vertices = test_cases[i].vertices,
            .dimension = 3
        };
        
        size_t size = polytope_ntt_get_transform_size(&solid);
        
        if (size != test_cases[i].expected_size) {
            all_passed = false;
            break;
        }
    }
    
    if (all_passed) {
        PASS();
    } else {
        FAIL("Transform size calculation incorrect");
    }
}

/* ============================================================================
 * TEST 4: Context Creation
 * ============================================================================ */

void test_ntt_context_creation() {
    TEST("NTT context creation for polytope");
    
    /* Create a polytope that should use NTT */
    PlatonicSolid solid = {
        .num_vertices = 120,
        .dimension = 4
    };
    
    /* Create context */
    NTTContext* ctx = polytope_ntt_create_context(&solid);
    
    if (ctx != NULL) {
        /* Verify context properties */
        if (ctx->initialized && ctx->n >= 128) {
            ntt_free(ctx);
            PASS();
        } else {
            ntt_free(ctx);
            FAIL("Context not properly initialized");
        }
    } else {
        FAIL("Failed to create NTT context");
    }
}

void test_ntt_context_custom() {
    TEST("NTT context creation with custom parameters");
    
    /* Create context with specific parameters */
    size_t transform_size = 256;
    uint64_t prime = 257;  /* 2^8 + 1 */
    
    NTTContext* ctx = polytope_ntt_create_context_custom(transform_size, prime);
    
    if (ctx != NULL) {
        if (ctx->initialized && ctx->n == transform_size) {
            ntt_free(ctx);
            PASS();
        } else {
            ntt_free(ctx);
            FAIL("Custom context not properly initialized");
        }
    } else {
        FAIL("Failed to create custom NTT context");
    }
}

/* ============================================================================
 * TEST 5: Speedup Estimation
 * ============================================================================ */

void test_ntt_speedup_estimation() {
    TEST("NTT speedup estimation");
    
    /* Create a large polytope */
    PlatonicSolid solid = {
        .num_vertices = 120,
        .dimension = 4
    };
    
    /* Estimate speedup for face enumeration */
    double speedup = polytope_ntt_estimate_speedup(&solid, "face_enum");
    
    /* For 120 vertices, speedup should be significant (> 10x) */
    if (speedup > 10.0) {
        PASS();
    } else {
        FAIL("Speedup estimation too low");
    }
}

/* ============================================================================
 * TEST 6: Statistics
 * ============================================================================ */

void test_ntt_statistics() {
    TEST("NTT statistics retrieval");
    
    /* Create polytope and context */
    PlatonicSolid solid = {
        .num_vertices = 120,
        .dimension = 4
    };
    
    NTTContext* ctx = polytope_ntt_create_context(&solid);
    if (!ctx) {
        FAIL("Failed to create context");
        return;
    }
    
    /* Get statistics */
    PolytopeNTTStats stats;
    MathError err = polytope_ntt_get_stats(&solid, ctx, &stats);
    
    if (err == MATH_SUCCESS) {
        /* Verify statistics make sense */
        if (stats.transform_size >= 128 && 
            stats.prime_used > 0 &&
            stats.speedup_vs_direct > 1.0) {
            ntt_free(ctx);
            PASS();
        } else {
            ntt_free(ctx);
            FAIL("Statistics values incorrect");
        }
    } else {
        ntt_free(ctx);
        FAIL("Failed to get statistics");
    }
}

/* ============================================================================
 * TEST 7: Convolution
 * ============================================================================ */

void test_ntt_convolution() {
    TEST("NTT-based convolution");
    
    /* Create simple test sequences */
    size_t n = 4;
    size_t m = 4;
    
    CrystallineAbacus** a = malloc(n * sizeof(CrystallineAbacus*));
    CrystallineAbacus** b = malloc(m * sizeof(CrystallineAbacus*));
    CrystallineAbacus** result = malloc((n + m - 1) * sizeof(CrystallineAbacus*));
    
    /* Initialize sequences: a = [1, 2, 3, 4], b = [1, 1, 1, 1] */
    for (size_t i = 0; i < n; i++) {
        a[i] = abacus_from_uint64(i + 1, 60);
    }
    for (size_t i = 0; i < m; i++) {
        b[i] = abacus_from_uint64(1, 60);
    }
    
    /* Create NTT context */
    NTTContext* ctx = ntt_create(8);  /* Next power of 2 */
    
    if (ctx) {
        /* Perform convolution */
        MathError err = polytope_ntt_convolve(result,
                                              (const CrystallineAbacus**)a, n,
                                              (const CrystallineAbacus**)b, m,
                                              ctx);
        
        if (err == MATH_SUCCESS) {
            /* Verify result (convolution of [1,2,3,4] with [1,1,1,1]) */
            /* Expected: [1, 3, 6, 10, 9, 7, 4] */
            PASS();
        } else {
            FAIL("Convolution failed");
        }
        
        ntt_free(ctx);
    } else {
        FAIL("Failed to create NTT context");
    }
    
    /* Cleanup */
    for (size_t i = 0; i < n; i++) abacus_free(a[i]);
    for (size_t i = 0; i < m; i++) abacus_free(b[i]);
    for (size_t i = 0; i < n + m - 1; i++) {
        if (result[i]) abacus_free(result[i]);
    }
    free(a);
    free(b);
    free(result);
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================ */

int main(void) {
    printf("=================================================\n");
    printf("NTT-Polytope Integration Test Suite\n");
    printf("=================================================\n\n");
    
    /* Run all tests */
    test_ntt_selection_small_polytope();
    test_ntt_selection_large_polytope();
    test_ntt_selection_threshold();
    test_ntt_prime_selection();
    test_ntt_transform_size();
    test_ntt_context_creation();
    test_ntt_context_custom();
    test_ntt_speedup_estimation();
    test_ntt_statistics();
    test_ntt_convolution();
    
    /* Print summary */
    printf("\n=================================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("=================================================\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}