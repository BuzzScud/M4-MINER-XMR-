/**
 * @file test_ntt_basic.c
 * @brief Basic test to identify NTT issues
 */

#include "math/ntt.h"
#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Basic NTT Test ===\n\n");
    
    /* Test 1: Create abacus */
    printf("Test 1: Creating Abacus with value 65537...\n");
    CrystallineAbacus* prime = abacus_from_uint64(65537, 60);
    if (!prime) {
        printf("FAIL: Could not create prime abacus\n");
        return 1;
    }
    printf("PASS: Prime abacus created\n\n");
    
    /* Test 2: Find primitive root */
    printf("Test 2: Finding primitive root for n=256...\n");
    CrystallineAbacus* root = abacus_new(60);
    if (!root) {
        printf("FAIL: Could not create root abacus\n");
        abacus_free(prime);
        return 1;
    }
    
    MathError err = ntt_find_primitive_root(root, 256, prime);
    printf("ntt_find_primitive_root returned: %d\n", err);
    
    if (err != MATH_SUCCESS) {
        printf("FAIL: Could not find primitive root (error %d)\n", err);
        abacus_free(root);
        abacus_free(prime);
        return 1;
    }
    printf("PASS: Primitive root found\n\n");
    
    /* Test 3: Use ntt_create directly */
    printf("Test 3: Using ntt_create(256)...\n");
    NTTContext* ctx = ntt_create(256);
    if (!ctx) {
        printf("FAIL: ntt_create returned NULL\n");
        abacus_free(root);
        abacus_free(prime);
        return 1;
    }
    printf("PASS: ntt_create succeeded\n");
    printf("  ctx->n = %zu\n", ctx->n);
    printf("  ctx->log_n = %zu\n", ctx->log_n);
    printf("  ctx->initialized = %s\n", ctx->initialized ? "true" : "false");
    
    ntt_free(ctx);
    abacus_free(root);
    abacus_free(prime);
    
    printf("\n=== All tests passed ===\n");
    return 0;
}