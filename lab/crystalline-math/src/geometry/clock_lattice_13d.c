/**
 * @file clock_lattice_13d.c
 * @brief 13-Dimensional Clock Lattice Operations
 * 
 * Fundamental geometric operations for mapping values to 13D clock lattice.
 * Uses π×φ metric and dimensional frequencies.
 * 
 * These are PURE MATHEMATICAL operations - no algorithm-specific logic.
 * Used by ANY system that needs to map values to geometric positions.
 */

#include "math/clock_lattice_13d.h"
#include "math/transcendental.h"
#include <string.h>

// Get constants from types.h
#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846
#endif

#ifndef MATH_PHI
#define MATH_PHI 1.61803398874989484820
#endif

#ifndef MATH_TWO_PI
#define MATH_TWO_PI (2.0 * MATH_PI)
#endif

// Dimensional frequencies for 13D clock lattice
// Based on prime and Babylonian structure
const uint64_t CLOCK_LATTICE_FREQUENCIES_13D[13] = {
    3, 7, 31, 12, 19, 5, 11, 13, 17, 23, 29, 37, 41
};

/**
 * Map single value to 13D clock lattice position
 */
void clock_map_value_to_lattice_13d(uint64_t value, double position[13]) {
    // Compute base angle using π×φ metric
    double base_angle = (double)value * MATH_PI * MATH_PHI;
    
    // Normalize to [0, 2π)
    while (base_angle < 0.0) base_angle += MATH_TWO_PI;
    while (base_angle >= MATH_TWO_PI) base_angle -= MATH_TWO_PI;
    
    // Map to each dimension using frequency and φ powers
    for (int d = 0; d < 13; d++) {
        double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
        position[d] = math_cos(base_angle * freq) * math_pow(MATH_PHI, d % 5);
    }
}

/**
 * Map (x,y) pair to 13D clock lattice position
 */
void clock_map_pair_to_lattice_13d(
    uint64_t x,
    uint64_t y,
    double position[13]
) {
    for (int d = 0; d < 13; d++) {
        double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
        
        // X component (cosine)
        double x_angle = (double)(x % 360) * MATH_PI / 180.0;
        position[d] = math_cos(x_angle * freq) * math_pow(MATH_PHI, d % 5);
        
        // Y component (sine) - orthogonal to X
        double y_angle = (double)(y % 360) * MATH_PI / 180.0;
        position[d] += math_sin(y_angle * freq) * math_pow(MATH_PHI, (d + 1) % 5) * 0.5;
    }
}

/**
 * Compute Euclidean distance in 13D space
 */
double math_distance_13d(const double pos1[13], const double pos2[13]) {
    double sum = 0.0;
    for (int d = 0; d < 13; d++) {
        double diff = pos1[d] - pos2[d];
        sum += diff * diff;
    }
    return math_sqrt(sum);
}

/**
 * Compute dot product in 13D space
 */
double math_dot_product_13d(const double pos1[13], const double pos2[13]) {
    double sum = 0.0;
    for (int d = 0; d < 13; d++) {
        sum += pos1[d] * pos2[d];
    }
    return sum;
}

/**
 * Normalize a 13D position vector
 */
void math_normalize_13d(double position[13]) {
    double magnitude = 0.0;
    for (int d = 0; d < 13; d++) {
        magnitude += position[d] * position[d];
    }
    magnitude = math_sqrt(magnitude);
    
    if (magnitude > 1e-10) {
        for (int d = 0; d < 13; d++) {
            position[d] /= magnitude;
        }
    }
}
