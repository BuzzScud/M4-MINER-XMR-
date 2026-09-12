/**
 * @file test_ntt_integration.c
 * @brief Test proper NTT integration after removing workarounds
 */

#include "math/ntt.h"
#include "math/polytope_ntt.h"
#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_PASS printf("✓ PASS: %s\n", __func__)
#define TEST_FAIL printf("✗ FAIL: %s at line %d\n", __func__, __LINE__)

void test_ntt_init_with_prime() {
    printf("\n=== Test: ntt_init_with_prime ===\n");
    
    /* Allocate context */
    NTTContext* ctx = (NTTContext*)calloc(1, sizeof(NTTContext));
    if (!ctx) {
        TEST_FAIL;
        return;
    }
    
    /* Create prime */
    CrystallineAbacus* prime = abacus_from_uint64(65537, 60);
    if (!prime) {
        free(ctx);
        TEST_FAIL;
        return;
    }
    
    printf("Prime created: 65537\n");
    
    /* Initialize with prime */
    bool success = ntt_init_with_prime(ctx, 256, prime);
    
    printf("ntt_init_with_prime returned: %s\n", success ? "true" : "false");
    printf("ctx->initialized: %s\n", ctx->initialized ? "true" : "false");
    printf("ctx->n: %zu\n", ctx->n);
    printf("ctx->log_n: %zu\n", ctx->log_n);
    printf("ctx->prime: %p\n", (void*)ctx->prime);
    printf("ctx->root: %p\n", (void*)ctx->root);
    
    if (!success) {
        abacus_free(prime);
        free(ctx);
        TEST_FAIL;
        return;
    }
    
    abacus_free(prime);
    ntt_free(ctx);
    
    TEST_PASS;
}

void test_polytope_ntt_create_context_custom() {
    printf("\n=== Test: polytope_ntt_create_context_custom ===\n");
    
    NTTContext* ctx = polytope_ntt_create_context_custom(256, 65537);
    
    if (!ctx) {
        printf("Context creation failed\n");
        TEST_FAIL;
        return;
    }
    
    printf("Context created successfully\n");
    printf("ctx->initialized: %s\n", ctx->initialized ? "true" : "false");
    printf("ctx->n: %zu\n", ctx->n);
    printf("ctx->log_n: %zu\n", ctx->log_n);
    printf("ctx->prime: %p\n", (void*)ctx->prime);
    printf("ctx->root: %p\n", (void*)ctx->root);
    printf("ctx->roots_forward: %p\n", (void*)ctx->roots_forward);
    printf("ctx->roots_inverse: %p\n", (void*)ctx->roots_inverse);
    
    if (ctx->roots_forward) {
        printf("ctx->roots_forward[0]: %p\n", (void*)ctx->roots_forward[0]);
        printf("ctx->roots_forward[1]: %p\n", (void*)ctx->roots_forward[1]);
    }
    
    ntt_free(ctx);
    
    TEST_PASS;
}

int main() {
    printf("=================================\n");
    printf("NTT Integration Test Suite\n");
    printf("=================================\n");
    
    test_ntt_init_with_prime();
    test_polytope_ntt_create_context_custom();
    
    printf("\n=================================\n");
    printf("All tests completed\n");
    printf("=================================\n");
    
    return 0;
}