/**
 * @file test_platonic_simple.c
 * @brief Simple test to debug platonic coordinate generation
 */

#include "math/platonic_clock.h"
#include "math/prime.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Simple Platonic Test ===\n\n");
    
    // Test 1: Get prime for vertex 0
    printf("Test 1: Get prime for vertex 0\n");
    uint64_t prime = platonic_vertex_to_prime(0);
    printf("  Vertex 0 -> Prime %lu\n", prime);
    
    // Test 2: Get clock position for vertex 0
    printf("\nTest 2: Get clock position for vertex 0\n");
    ClockPosition pos;
    MathError err = platonic_vertex_to_clock_position(0, &pos);
    printf("  Error code: %d\n", err);
    if (err == MATH_SUCCESS) {
        printf("  Angle: %f\n", pos.angle);
        printf("  Radius: %f\n", pos.radius);
    }
    
    // Test 3: Generate coordinates
    printf("\nTest 3: Generate coordinates for vertex 0\n");
    CrystallineAbacus** coords = NULL;
    err = platonic_vertex_to_coordinates(0, 3, &coords, 12, 6);
    printf("  Error code: %d\n", err);
    
    if (err == MATH_SUCCESS && coords) {
        printf("  Coordinates generated successfully!\n");
        
        // Try to convert to double
        for (int i = 0; i < 3; i++) {
            if (coords[i]) {
                double val;
                MathError conv_err = abacus_to_double(coords[i], &val);
                if (conv_err == MATH_SUCCESS) {
                    printf("    coord[%d] = %f\n", i, val);
                } else {
                    printf("    coord[%d] = [conversion error %d]\n", i, conv_err);
                }
                abacus_free(coords[i]);
            } else {
                printf("    coord[%d] = NULL\n", i);
            }
        }
        free(coords);
    } else {
        printf("  Failed to generate coordinates\n");
    }
    
    return 0;
}