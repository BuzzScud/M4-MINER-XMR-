#include "math/ntt.h"
#include "math/abacus.h"
#include <stdio.h>

int main() {
    printf("=== Test ntt_find_prime value ===\n\n");
    
    /* Create prime abacus */
    CrystallineAbacus* prime = abacus_new(60);
    printf("Created prime abacus (should be 0): is_zero=%s\n", 
           abacus_is_zero(prime) ? "yes" : "no");
    
    /* Call ntt_find_prime */
    printf("\nCalling ntt_find_prime(prime, 256, 64)...\n");
    MathError err = ntt_find_prime(prime, 256, 64);
    printf("Returned: %d\n", err);
    
    /* Check if prime is still zero */
    printf("After ntt_find_prime: is_zero=%s\n", 
           abacus_is_zero(prime) ? "yes" : "no");
    
    /* Compare with expected value */
    CrystallineAbacus* expected = abacus_from_uint64(65537, 60);
    int cmp = abacus_compare(prime, expected);
    printf("Compare with 65537: %d (0=equal, <0=less, >0=greater)\n", cmp);
    
    abacus_free(expected);
    abacus_free(prime);
    
    return 0;
}