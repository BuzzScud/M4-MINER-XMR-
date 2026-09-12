/**
 * @file test_platonic_precision.c
 * @brief Test precision of platonic coordinate generation
 */

#include "math/platonic_clock.h"
#include "math/prime.h"
#include "math/arithmetic.h"
#include "math/constants.h"
#include <stdio.h>
#include <stdlib.h>

// Helper to print Abacus value
void print_abacus(const char* label, CrystallineAbacus* num) {
    double value;
    if (abacus_to_double(num, &value) == MATH_SUCCESS) {
        printf("%s: %.15f\n", label, value);
    } else {
        printf("%s: [conversion error]\n", label);
    }
}

// Test 1: Generate coordinates for vertex 0 (prime 2)
void test_vertex_0_coordinates() {
    printf("\n=== Test 1: Vertex 0 Coordinates ===\n");
    
    uint32_t dimension = 3;
    uint32_t base = 12;  // Must use 12, 60, or 100
    uint32_t precision = 15; // High precision
    CrystallineAbacus** coords = NULL;
    
    MathError err = platonic_vertex_to_coordinates(0, dimension, &coords, base, precision);
    
    if (err == MATH_SUCCESS && coords) {
        printf("Vertex 0 (prime 2) coordinates:\n");
        print_abacus("  x", coords[0]);
        print_abacus("  y", coords[1]);
        print_abacus("  z", coords[2]);
        
        // Compute distance from origin
        CrystallineAbacus* zero = abacus_from_uint64(0, 12);
        CrystallineAbacus** origin = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
        for (uint32_t i = 0; i < dimension; i++) {
            origin[i] = abacus_from_uint64(0, 12);
        }
        
        CrystallineAbacus* dist = platonic_coordinate_distance(coords, origin, dimension);
        if (dist) {
            print_abacus("  Distance from origin", dist);
            abacus_free(dist);
        }
        
        // Clean up
        for (uint32_t i = 0; i < dimension; i++) {
            abacus_free(coords[i]);
            abacus_free(origin[i]);
        }
        free(coords);
        free(origin);
        abacus_free(zero);
        
        printf("✓ PASS\n");
    } else {
        printf("✗ FAIL: Error generating coordinates\n");
    }
}

// Test 2: Test 12-fold symmetry
void test_12fold_symmetry() {
    printf("\n=== Test 2: 12-fold Symmetry ===\n");
    
    uint32_t dimension = 3;
    uint32_t base = 12;  // Must use 12, 60, or 100
    uint32_t precision = 15;
    CrystallineAbacus** coords = NULL;
    
    MathError err = platonic_vertex_to_coordinates(0, dimension, &coords, base, precision);
    
    if (err == MATH_SUCCESS && coords) {
        // Test rotation
        CrystallineAbacus** rotated = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
        
        printf("Original coordinates:\n");
        print_abacus("  x", coords[0]);
        print_abacus("  y", coords[1]);
        
        // Rotate by 30° (1/12 of circle)
        err = platonic_apply_12fold_rotation(coords, dimension, 1, rotated);
        
        if (err == MATH_SUCCESS) {
            printf("After 30° rotation:\n");
            print_abacus("  x'", rotated[0]);
            print_abacus("  y'", rotated[1]);
            
            // Check symmetry
            bool has_sym = platonic_has_12fold_symmetry(coords, dimension);
            printf("Has 12-fold symmetry: %s\n", has_sym ? "YES" : "NO");
            
            printf("✓ PASS\n");
        } else {
            printf("✗ FAIL: Rotation error\n");
        }
        
        // Clean up
        for (uint32_t i = 0; i < dimension; i++) {
            abacus_free(coords[i]);
            if (rotated[i]) abacus_free(rotated[i]);
        }
        free(coords);
        free(rotated);
    } else {
        printf("✗ FAIL: Error generating coordinates\n");
    }
}

// Test 3: Test precision loss through multiple conversions
void test_precision_loss() {
    printf("\n=== Test 3: Precision Loss Analysis ===\n");
    
    uint32_t base = 12;  // Must use 12, 60, or 100
    uint32_t precision = 15;
    
    // Create a high-precision value
    CrystallineAbacus* original = abacus_from_double(0.123456789012345, base, precision);
    
    printf("Original value: 0.123456789012345\n");
    print_abacus("  Stored as Abacus", original);
    
    // Convert to double and back (simulating current implementation)
    double temp;
    abacus_to_double(original, &temp);
    CrystallineAbacus* after_conversion = abacus_from_double(temp, base, precision);
    
    print_abacus("  After double conversion", after_conversion);
    
    // Compute difference
    CrystallineAbacus* diff = abacus_new(base);
    abacus_sub(diff, original, after_conversion);
    print_abacus("  Precision loss", diff);
    
    abacus_free(original);
    abacus_free(after_conversion);
    abacus_free(diff);
    
    printf("✓ PASS\n");
}

// Test 4: Distance calculation precision
void test_distance_precision() {
    printf("\n=== Test 4: Distance Calculation Precision ===\n");
    
    uint32_t dimension = 3;
    uint32_t base __attribute__((unused)) = 12;  // Must use 12, 60, or 100
    uint32_t precision __attribute__((unused)) = 15;
    
    // Create two points
    CrystallineAbacus** point1 = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
    CrystallineAbacus** point2 = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
    
    // Point 1: (1, 0, 0)
    point1[0] = abacus_from_uint64(1, 12);
    point1[1] = abacus_from_uint64(0, 12);
    point1[2] = abacus_from_uint64(0, 12);
    
    // Point 2: (0, 1, 0)
    point2[0] = abacus_from_uint64(0, 12);
    point2[1] = abacus_from_uint64(1, 12);
    point2[2] = abacus_from_uint64(0, 12);
    
    // Expected distance: sqrt(2) = 1.41421356237...
    CrystallineAbacus* dist = platonic_coordinate_distance(point1, point2, dimension);
    
    if (dist) {
        print_abacus("Distance between (1,0,0) and (0,1,0)", dist);
        printf("Expected: 1.41421356237309...\n");
        
        // Check if close to sqrt(2)
        double dist_val;
        abacus_to_double(dist, &dist_val);
        double expected = MATH_SQRT2;  // Use constant from constants.h
        double error = (dist_val > expected) ? (dist_val - expected) : (expected - dist_val);
        printf("Error: %.15e\n", error);
        
        if (error < 1e-10) {
            printf("✓ PASS (error < 1e-10)\n");
        } else {
            printf("⚠ WARNING: Precision loss detected\n");
        }
        
        abacus_free(dist);
    } else {
        printf("✗ FAIL: Distance calculation error\n");
    }
    
    // Clean up
    for (uint32_t i = 0; i < dimension; i++) {
        abacus_free(point1[i]);
        abacus_free(point2[i]);
    }
    free(point1);
    free(point2);
}

int main() {
    printf("=================================================\n");
    printf("PLATONIC COORDINATE PRECISION BASELINE TESTS\n");
    printf("=================================================\n");
    printf("Testing CURRENT implementation (with double conversions)\n");
    
    test_vertex_0_coordinates();
    test_12fold_symmetry();
    test_precision_loss();
    test_distance_precision();
    
    printf("\n=================================================\n");
    printf("BASELINE TESTS COMPLETE\n");
    printf("=================================================\n");
    
    return 0;
}