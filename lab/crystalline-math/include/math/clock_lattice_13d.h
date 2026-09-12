/**
 * @file clock_lattice_13d.h
 * @brief 13-Dimensional Clock Lattice Operations
 * 
 * Fundamental geometric operations for mapping values to 13D clock lattice.
 */

#ifndef MATH_CLOCK_LATTICE_13D_H
#define MATH_CLOCK_LATTICE_13D_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dimensional frequencies for 13D clock lattice
 * Based on prime and Babylonian structure
 */
extern const uint64_t CLOCK_LATTICE_FREQUENCIES_13D[13];

/**
 * Map single value to 13D clock lattice position
 * 
 * @param value Input value (k, index, symbol_id, etc.)
 * @param position Output 13D position
 */
void clock_map_value_to_lattice_13d(uint64_t value, double position[13]);

/**
 * Map (x,y) pair to 13D clock lattice position
 * 
 * @param x First value
 * @param y Second value
 * @param position Output 13D position
 */
void clock_map_pair_to_lattice_13d(uint64_t x, uint64_t y, double position[13]);

/**
 * Compute Euclidean distance in 13D space
 * 
 * @param pos1 First position
 * @param pos2 Second position
 * @return Distance between positions
 */
double math_distance_13d(const double pos1[13], const double pos2[13]);

/**
 * Compute dot product in 13D space
 * 
 * @param pos1 First position
 * @param pos2 Second position
 * @return Dot product
 */
double math_dot_product_13d(const double pos1[13], const double pos2[13]);

/**
 * Normalize a 13D position vector
 * 
 * @param position Position to normalize (modified in place)
 */
void math_normalize_13d(double position[13]);

#ifdef __cplusplus
}
#endif

#endif /* MATH_CLOCK_LATTICE_13D_H */
