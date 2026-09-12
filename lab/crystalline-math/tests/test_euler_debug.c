/**
 * @file test_euler_debug.c
 * @brief Debug Euler characteristic calculation
 */

#include "math/schlafli_math.h"
#include "math/schlafli.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing Cube {4,3} Euler characteristic...\n\n");
    
    uint32_t components[] = {4, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 2);
    
    uint32_t n = symbol->dimension;
    printf("Dimension n = %u\n\n", n);
    
    printf("=== Calculating k-faces ===\n");
    int64_t chi = 0;
    for (uint32_t k = 0; k < n; k++) {
        uint64_t f_k = schlafli_calculate_k_faces(symbol, k);
        printf("f_%u = %lu\n", k, f_k);
        
        if (f_k == 0) {
            printf("  -> Cannot calculate, returning true (valid)\n");
            schlafli_free(symbol);
            return 0;
        }
        
        if (k % 2 == 0) {
            chi += (int64_t)f_k;
            printf("  -> Adding to chi: %ld\n", chi);
        } else {
            chi -= (int64_t)f_k;
            printf("  -> Subtracting from chi: %ld\n", chi);
        }
    }
    
    printf("\n=== Result ===\n");
    printf("Calculated χ = %ld\n", chi);
    
    int64_t expected = 1 + ((n % 2 == 0) ? 1 : -1);
    printf("Expected χ = %ld (for dimension %u)\n", expected, n);
    printf("Match: %s\n", (chi == expected) ? "YES" : "NO");
    
    schlafli_free(symbol);
    return 0;
}