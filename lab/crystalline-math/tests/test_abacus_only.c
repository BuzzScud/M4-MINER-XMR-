#include "math/abacus.h"
#include <stdio.h>

int main() {
    printf("Creating abacus...\n");
    CrystallineAbacus* a = abacus_from_uint64(12345, 60);
    if (!a) {
        printf("FAIL: Could not create abacus\n");
        return 1;
    }
    printf("SUCCESS: Abacus created\n");
    abacus_free(a);
    return 0;
}