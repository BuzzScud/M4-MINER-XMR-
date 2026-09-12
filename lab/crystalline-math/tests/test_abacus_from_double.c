#include "math/abacus.h"
#include <stdio.h>

int main() {
    printf("Testing abacus_from_double...\n");
    
    CrystallineAbacus* num = abacus_from_double(1.0, 12, 6);
    if (num) {
        printf("✓ Created Abacus from 1.0\n");
        double val;
        if (abacus_to_double(num, &val) == MATH_SUCCESS) {
            printf("  Value: %f\n", val);
        }
        abacus_free(num);
    } else {
        printf("✗ Failed to create Abacus from 1.0\n");
    }
    
    return 0;
}