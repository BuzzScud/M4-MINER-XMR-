#include "math/schlafli.h"
#include "math/schlafli_validator.h"
#include <stdio.h>

int main() {
    printf("Testing comprehensive validation...\n");
    
    SchlafliSymbol* sym = schlafli_parse("{3,3}");
    if (!sym) {
        printf("ERROR: Failed to parse\n");
        return 1;
    }
    
    printf("Calling schlafli_validate_comprehensive...\n");
    SchlafliValidationResult result = schlafli_validate_comprehensive(sym);
    
    printf("Result: %s\n", result.is_valid ? "VALID" : "INVALID");
    printf("Angle sum valid: %s\n", result.angle_sum_valid ? "YES" : "NO");
    printf("Euler valid: %s\n", result.euler_valid ? "YES" : "NO");
    
    printf("\nCalling schlafli_print_validation...\n");
    schlafli_print_validation(&result);
    
    schlafli_free(sym);
    
    printf("\nTest passed!\n");
    return 0;
}