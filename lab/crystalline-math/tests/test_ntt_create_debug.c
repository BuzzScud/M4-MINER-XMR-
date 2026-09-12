#include "math/ntt.h"
#include "math/abacus.h"
#include <stdio.h>

int main() {
    printf("=== Debug ntt_create ===\n\n");
    
    printf("Step 1: Calling ntt_create(256)...\n");
    fflush(stdout);
    
    NTTContext* ctx = ntt_create(256);
    
    if (!ctx) {
        printf("FAIL: ntt_create returned NULL\n");
        
        /* Try manually to see where it fails */
        printf("\nManual test:\n");
        
        printf("  Creating prime abacus...\n");
        CrystallineAbacus* prime = abacus_new(60);
        if (!prime) {
            printf("  FAIL: Could not create prime abacus\n");
            return 1;
        }
        printf("  SUCCESS: Prime abacus created\n");
        
        printf("  Calling ntt_find_prime...\n");
        MathError err = ntt_find_prime(prime, 256, 64);
        printf("  ntt_find_prime returned: %d\n", err);
        
        if (err == MATH_SUCCESS) {
            printf("  Prime found successfully\n");
            
            printf("  Creating root abacus...\n");
            CrystallineAbacus* root = abacus_new(60);
            if (!root) {
                printf("  FAIL: Could not create root abacus\n");
                abacus_free(prime);
                return 1;
            }
            
            printf("  Calling ntt_find_primitive_root...\n");
            err = ntt_find_primitive_root(root, 256, prime);
            printf("  ntt_find_primitive_root returned: %d\n", err);
            
            abacus_free(root);
        }
        
        abacus_free(prime);
        return 1;
    }
    
    printf("SUCCESS: ntt_create succeeded\n");
    printf("  ctx->n = %zu\n", ctx->n);
    printf("  ctx->initialized = %s\n", ctx->initialized ? "true" : "false");
    
    ntt_free(ctx);
    return 0;
}