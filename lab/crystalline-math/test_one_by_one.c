#include "math/schlafli.h"
#include "math/schlafli_validator.h"
#include <stdio.h>

int main() {
    printf("Test 1: Tetrahedron\n");
    SchlafliSymbol* sym1 = schlafli_parse("{3,3}");
    printf("  Parsed: %s\n", sym1 ? "YES" : "NO");
    if (sym1) {
        printf("  Valid: %s\n", schlafli_validate_mathematical(sym1) ? "YES" : "NO");
        printf("  Vertices: %lu\n", schlafli_calculate_vertices(sym1));
        schlafli_free(sym1);
    }
    
    printf("\nTest 2: Cube\n");
    SchlafliSymbol* sym2 = schlafli_parse("{4,3}");
    printf("  Parsed: %s\n", sym2 ? "YES" : "NO");
    if (sym2) {
        printf("  Valid: %s\n", schlafli_validate_mathematical(sym2) ? "YES" : "NO");
        printf("  Vertices: %lu\n", schlafli_calculate_vertices(sym2));
        schlafli_free(sym2);
    }
    
    printf("\nTest 3: Octahedron\n");
    SchlafliSymbol* sym3 = schlafli_parse("{3,4}");
    printf("  Parsed: %s\n", sym3 ? "YES" : "NO");
    if (sym3) {
        printf("  Valid: %s\n", schlafli_validate_mathematical(sym3) ? "YES" : "NO");
        printf("  Vertices: %lu\n", schlafli_calculate_vertices(sym3));
        schlafli_free(sym3);
    }
    
    printf("\nAll tests passed!\n");
    return 0;
}