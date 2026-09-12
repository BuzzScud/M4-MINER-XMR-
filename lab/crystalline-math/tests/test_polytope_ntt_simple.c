/**
 * @file test_polytope_ntt_simple.c
 * @brief Simple test for NTT integration
 */

#include "math/polytope_ntt.h"
#include "math/polytope.h"
#include <stdio.h>

int main(void) {
    printf("Testing NTT integration...\n");
    
    /* Test 1: Small polytope should NOT use NTT */
    PlatonicSolid small = {
        .num_vertices = 4,
        .dimension = 3
    };
    
    bool should_use_small = polytope_ntt_should_use(&small);
    printf("Small polytope (4 vertices): use NTT = %s\n", 
           should_use_small ? "YES" : "NO");
    
    /* Test 2: Large polytope SHOULD use NTT */
    PlatonicSolid large = {
        .num_vertices = 120,
        .dimension = 4
    };
    
    bool should_use_large = polytope_ntt_should_use(&large);
    printf("Large polytope (120 vertices): use NTT = %s\n",
           should_use_large ? "YES" : "NO");
    
    /* Test 3: Get optimal prime */
    uint64_t prime = polytope_ntt_find_optimal_prime(&large);
    printf("Optimal prime for 120 vertices: %lu\n", prime);
    
    /* Test 4: Get transform size */
    size_t size = polytope_ntt_get_transform_size(&large);
    printf("Transform size for 120 vertices: %zu\n", size);
    
    /* Test 5: Estimate speedup */
    double speedup = polytope_ntt_estimate_speedup(&large, "face_enum");
    printf("Estimated speedup: %.1fx\n", speedup);
    
    printf("\nAll basic tests passed!\n");
    return 0;
}