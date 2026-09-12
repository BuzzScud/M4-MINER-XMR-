#include "math/ntt.h"
#include "math/abacus.h"
#include <stdio.h>

int main() {
    printf("Step 1: Creating prime abacus...\n");
    CrystallineAbacus* prime = abacus_from_uint64(65537, 60);
    if (!prime) {
        printf("FAIL at step 1\n");
        return 1;
    }
    printf("SUCCESS: Prime created\n");
    
    printf("\nStep 2: Creating root abacus...\n");
    CrystallineAbacus* root = abacus_new(60);
    if (!root) {
        printf("FAIL at step 2\n");
        abacus_free(prime);
        return 1;
    }
    printf("SUCCESS: Root created\n");
    
    printf("\nStep 3: Calling ntt_find_primitive_root...\n");
    fflush(stdout);
    
    MathError err = ntt_find_primitive_root(root, 256, prime);
    
    printf("ntt_find_primitive_root returned: %d\n", err);
    
    abacus_free(root);
    abacus_free(prime);
    
    printf("\nAll steps completed\n");
    return 0;
}