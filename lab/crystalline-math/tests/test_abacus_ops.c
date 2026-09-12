#include "math/abacus.h"
#include "math/arithmetic.h"
#include <stdio.h>

int main() {
    printf("Test: Abacus operations\n\n");
    
    /* Test 1: Create new abacus */
    CrystallineAbacus* a = abacus_new(60);
    printf("Created abacus: %p\n", (void*)a);
    printf("Is zero: %s\n", abacus_is_zero(a) ? "yes" : "no");
    
    /* Test 2: Create from uint64 */
    CrystallineAbacus* b = abacus_from_uint64(12345, 60);
    printf("Created from 12345: %p\n", (void*)b);
    
    /* Test 3: Clear a to zero and add b */
    printf("\nTest: a = a - a (should be 0)\n");
    abacus_sub(a, a, a);
    printf("Is zero after sub: %s\n", abacus_is_zero(a) ? "yes" : "no");
    
    printf("\nTest: a = a + b (should be 12345)\n");
    abacus_add(a, a, b);
    
    /* Test 4: Compare */
    printf("Compare a and b: %d (should be 0 if equal)\n", abacus_compare(a, b));
    
    abacus_free(a);
    abacus_free(b);
    
    return 0;
}