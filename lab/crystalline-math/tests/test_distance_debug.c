#include "math/platonic_clock.h"
#include "math/abacus.h"
#include "math/transcendental.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Distance Calculation Debug ===\n\n");
    
    uint32_t dimension = 3;
    uint32_t base = 12;
    
    // Create two points
    CrystallineAbacus** point1 = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
    CrystallineAbacus** point2 = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
    
    // Point 1: (1, 0, 0)
    point1[0] = abacus_from_uint64(1, base);
    point1[1] = abacus_from_uint64(0, base);
    point1[2] = abacus_from_uint64(0, base);
    
    // Point 2: (0, 1, 0)
    point2[0] = abacus_from_uint64(0, base);
    point2[1] = abacus_from_uint64(1, base);
    point2[2] = abacus_from_uint64(0, base);
    
    printf("Point 1: (1, 0, 0)\n");
    printf("Point 2: (0, 1, 0)\n\n");
    
    // Manual calculation
    printf("Manual calculation:\n");
    
    // diff[0] = 1 - 0 = 1
    CrystallineAbacus* diff0 = abacus_new(base);
    abacus_sub(diff0, point1[0], point2[0]);
    double d0;
    abacus_to_double(diff0, &d0);
    printf("  diff[0] = %f\n", d0);
    
    // diff_sq[0] = 1 * 1 = 1
    CrystallineAbacus* diff_sq0 = abacus_new(base);
    abacus_mul(diff_sq0, diff0, diff0);
    double ds0;
    abacus_to_double(diff_sq0, &ds0);
    printf("  diff_sq[0] = %f\n", ds0);
    
    // diff[1] = 0 - 1 = -1
    CrystallineAbacus* diff1 = abacus_new(base);
    abacus_sub(diff1, point1[1], point2[1]);
    double d1;
    abacus_to_double(diff1, &d1);
    printf("  diff[1] = %f\n", d1);
    
    // diff_sq[1] = -1 * -1 = 1
    CrystallineAbacus* diff_sq1 = abacus_new(base);
    abacus_mul(diff_sq1, diff1, diff1);
    double ds1;
    abacus_to_double(diff_sq1, &ds1);
    printf("  diff_sq[1] = %f\n", ds1);
    
    // sum = 1 + 1 = 2
    CrystallineAbacus* sum = abacus_new(base);
    abacus_add(sum, diff_sq0, diff_sq1);
    double sum_val;
    abacus_to_double(sum, &sum_val);
    printf("  sum = %f\n", sum_val);
    
    // sqrt(2) = 1.414...
    CrystallineAbacus* result = abacus_new(base);
    MathError err = math_sqrt_abacus(result, sum, 15);
    printf("  sqrt error code: %d\n", err);
    double result_val;
    abacus_to_double(result, &result_val);
    printf("  sqrt(sum) = %f\n", result_val);
    
    printf("\nUsing platonic_coordinate_distance:\n");
    CrystallineAbacus* dist = platonic_coordinate_distance(point1, point2, dimension);
    if (dist) {
        double dist_val;
        abacus_to_double(dist, &dist_val);
        printf("  distance = %f\n", dist_val);
        abacus_free(dist);
    } else {
        printf("  distance = NULL (error)\n");
    }
    
    // Clean up
    abacus_free(diff0);
    abacus_free(diff_sq0);
    abacus_free(diff1);
    abacus_free(diff_sq1);
    abacus_free(sum);
    abacus_free(result);
    
    for (uint32_t i = 0; i < dimension; i++) {
        abacus_free(point1[i]);
        abacus_free(point2[i]);
    }
    free(point1);
    free(point2);
    
    return 0;
}