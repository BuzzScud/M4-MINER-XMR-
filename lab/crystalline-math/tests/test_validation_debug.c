/**
 * @file test_validation_debug.c
 * @brief Debug validation to see why it's failing
 */

#include "math/schlafli_math.h"
#include "math/schlafli_validator.h"
#include "math/schlafli.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing Cube {4,3} validation...\n\n");
    
    uint32_t components[] = {4, 3};
    SchlafliSymbol* symbol = schlafli_create(components, 2);
    
    if (!symbol) {
        printf("Failed to create symbol\n");
        return 1;
    }
    
    printf("Symbol created: {%u,%u}\n", symbol->components[0], symbol->components[1]);
    printf("Dimension: %u\n", symbol->dimension);
    printf("Length: %u\n\n", symbol->length);
    
    // Test each validation step
    printf("=== Validation Steps ===\n\n");
    
    // Step 1: Check components >= 3
    printf("1. Components >= 3: ");
    bool comp_ok = true;
    for (uint32_t i = 0; i < symbol->length; i++) {
        if (symbol->components[i] < 3) {
            comp_ok = false;
            break;
        }
    }
    printf("%s\n", comp_ok ? "PASS" : "FAIL");
    
    // Step 2: Angle sum
    printf("2. Angle sum constraint: ");
    bool angle_ok = schlafli_validate_angle_sum(symbol);
    printf("%s\n", angle_ok ? "PASS" : "FAIL");
    
    // Step 3: Euler characteristic (general)
    printf("3. Euler characteristic (general): ");
    bool euler_ok = schlafli_validate_euler_general(symbol);
    printf("%s\n", euler_ok ? "PASS" : "FAIL");
    
    // Step 4: Schläfli condition for 3D
    printf("4. Schläfli condition (p-2)(q-2) < 4: ");
    uint32_t p = symbol->components[0];
    uint32_t q = symbol->components[1];
    uint32_t product = (p - 2) * (q - 2);
    printf("(%u-2)(%u-2) = %u < 4? %s\n", 
           p, q, product, (product < 4) ? "PASS" : "FAIL");
    
    // Overall validation
    printf("\n=== Overall Validation ===\n");
    bool valid = schlafli_is_valid_regular_polytope(symbol);
    printf("Result: %s\n", valid ? "VALID" : "INVALID");
    
    schlafli_free(symbol);
    return 0;
}