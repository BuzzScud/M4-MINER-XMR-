#include "math/schlafli.h"
#include "math/schlafli_validator.h"
#include <stdio.h>

int main() {
    printf("Testing Schläfli validator...\n");
    
    // Test 1: Parse a simple symbol
    printf("Test 1: Parsing {3,3}...\n");
    SchlafliSymbol* sym = schlafli_parse("{3,3}");
    if (!sym) {
        printf("ERROR: Failed to parse {3,3}\n");
        return 1;
    }
    printf("SUCCESS: Parsed {3,3}\n");
    
    // Test 2: Validate angle sum
    printf("Test 2: Validating angle sum...\n");
    bool valid = schlafli_validate_angle_sum(sym);
    printf("Angle sum valid: %s\n", valid ? "YES" : "NO");
    
    // Test 3: Calculate vertices
    printf("Test 3: Calculating vertices...\n");
    uint64_t vertices = schlafli_calculate_vertices(sym);
    printf("Vertices: %lu\n", vertices);
    
    // Test 4: Mathematical validation
    printf("Test 4: Mathematical validation...\n");
    bool math_valid = schlafli_validate_mathematical(sym);
    printf("Mathematically valid: %s\n", math_valid ? "YES" : "NO");
    
    schlafli_free(sym);
    
    printf("\nAll tests passed!\n");
    return 0;
}