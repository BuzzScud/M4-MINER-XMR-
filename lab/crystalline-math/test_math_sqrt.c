#include "math/transcendental.h"
#include "math/arithmetic.h"
#include <stdio.h>

int main(void) {
    printf("Testing math_sqrt directly...\n");
    
    double result = math_sqrt(5.0);
    printf("math_sqrt(5.0) = %.10f\n", result);
    printf("Expected: 2.2360679775\n");
    
    double result2 = math_sqrt(2.0);
    printf("math_sqrt(2.0) = %.10f\n", result2);
    printf("Expected: 1.4142135624\n");
    
    return 0;
}