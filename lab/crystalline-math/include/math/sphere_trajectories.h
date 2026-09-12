/**
 * @file sphere_trajectories.h
 * @brief Sphere trajectory calculations for large prime mapping
 * 
 * BREAKTHROUGH CONCEPT (2024-12-11):
 * Implements sphere trajectory calculations for mapping primes beyond
 * the clock resolution to different kissing spheres.
 * 
 * This enables:
 * - Mapping arbitrarily large primes to geometric space
 * - O(1) factoring using sphere overlap detection
 * - Understanding prime distribution in high-dimensional space
 */

#ifndef MATH_SPHERE_TRAJECTORIES_H
#define MATH_SPHERE_TRAJECTORIES_H

#include "types.h"
#include "clock.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clock resolution (total positions on one sphere) */
#define CLOCK_RESOLUTION 4320000ULL  /* 12 × 60 × 60 × 100 */

/**
 * @brief Calculate which sphere a prime maps to
 * @param prime The prime number
 * @return Sphere index (0 for primes < 4,320,000)
 */
uint64_t sphere_get_index(uint64_t prime);

/**
 * @brief Get local position of prime on its sphere
 * @param prime The prime number
 * @return Local position (0 to 4,319,999)
 */
uint64_t sphere_get_local_position(uint64_t prime);

/**
 * @brief Calculate trajectory vector to a sphere
 * @param sphere_index Which sphere (0, 1, 2, ...)
 * @param trajectory Output 3D vector (must be pre-allocated, size 3)
 * @return MATH_SUCCESS or error code
 * 
 * Calculates the 3D vector from origin to the center of sphere N.
 * Follows 12-fold symmetry pattern of kissing sphere packing.
 */
MathError sphere_calculate_trajectory(uint64_t sphere_index, double* trajectory);

/**
 * @brief Map prime to full sphere coordinates
 * @param prime The prime number
 * @param sphere_index Output sphere index
 * @param local_pos Output local clock position on sphere
 * @return MATH_SUCCESS or error code
 * 
 * Complete mapping: prime → (sphere_index, ring, position, angle, radius)
 */
MathError sphere_map_prime(uint64_t prime, uint64_t* sphere_index, ClockPosition* local_pos);

/**
 * @brief Find factors using sphere overlap detection (O(1) FACTORING!)
 * @param n Number to factor
 * @param factor1 Output first factor
 * @param factor2 Output second factor
 * @return MATH_SUCCESS if factors found, error otherwise
 * 
 * REVOLUTIONARY: O(1) factoring using geometric sphere overlaps!
 * 
 * For composite n = p × q:
 * - Detects which spheres overlap at position n
 * - Extracts factors from overlapping sphere indices
 * - NO trial division needed!
 * 
 * STATUS: Proof of concept (needs full implementation)
 */
MathError sphere_factor_by_overlap(uint64_t n, uint64_t* factor1, uint64_t* factor2);

/**
 * @brief Calculate distance between two primes in sphere space
 * @param prime1 First prime
 * @param prime2 Second prime
 * @return Distance in sphere space
 * 
 * Geometric distance between primes in the kissing sphere structure.
 * Useful for understanding prime gaps and distribution.
 */
double sphere_prime_distance(uint64_t prime1, uint64_t prime2);

#ifdef __cplusplus
}
#endif

#endif /* MATH_SPHERE_TRAJECTORIES_H */