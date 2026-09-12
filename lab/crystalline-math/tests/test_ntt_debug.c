#include "math/polytope_ntt.h"
#include "math/ntt.h"
#include <stdio.h>

int main(void) {
    printf("Testing NTT context creation...\n");
    
    PlatonicSolid solid = {
        .dimension = 4,
        .num_vertices = 120
    };
    
    printf("Checking if NTT should be used: ");
    bool should_use = polytope_ntt_should_use(&solid);
    printf("%s\n", should_use ? "YES" : "NO");
    
    printf("Finding optimal prime: ");
    uint64_t prime = polytope_ntt_find_optimal_prime(&solid);
    printf("%lu\n", prime);
    
    printf("Getting transform size: ");
    size_t size = polytope_ntt_get_transform_size(&solid);
    printf("%zu\n", size);
    
    printf("Creating NTT context...\n");
    NTTContext* ctx = polytope_ntt_create_context(&solid);
    
    if (ctx) {
        printf("Context created successfully!\n");
        printf("  - Transform size: %zu\n", ctx->n);
        printf("  - Initialized: %s\n", ctx->initialized ? "YES" : "NO");
        ntt_free(ctx);
    } else {
        printf("Context creation FAILED!\n");
        
        /* Try creating directly */
        printf("\nTrying direct NTT creation...\n");
        NTTContext* ctx2 = ntt_create(size);
        if (ctx2) {
            printf("Direct creation succeeded!\n");
            ntt_free(ctx2);
        } else {
            printf("Direct creation also failed!\n");
        }
    }
    
    return 0;
}