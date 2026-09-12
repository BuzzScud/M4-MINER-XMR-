/**
 * @file platonic_clock.h
 * @brief Clock lattice integration for Platonic solid generators
 * 
 * This module integrates the Babylonian clock lattice with Platonic solid
 * generators to enable deterministic vertex positioning based on prime numbers.
 * 
 * Key Principle: Each vertex maps to a prime, each prime maps to a clock position,
 * and each clock position maps to geometric coordinates in nD space.
 * 
 * Vertex → Prime → Clock Position → nD Coordinates
 */

#ifndef MATH_PLATONIC_CLOCK_H
#define MATH_PLATONIC_CLOCK_H

#include "types.h"
#include "clock.h"
#include "abacus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * VERTEX TO PRIME MAPPING
 * ============================================================================
 */

/**
 * @brief Map vertex index to prime number
 * @param vertex_idx Vertex index (0-based)
 * @return Prime number for this vertex (nth prime where n = vertex_idx + 1)
 * 
 * Example:
 * - vertex_idx 0 → 2 (1st prime)
 * - vertex_idx 1 → 3 (2nd prime)
 * - vertex_idx 2 → 5 (3rd prime)
 * - vertex_idx 3 → 7 (4th prime)
 */
uint64_t platonic_vertex_to_prime(uint64_t vertex_idx);

/**
 * @brief Map prime number to vertex index
 * @param prime Prime number
 * @return Vertex index (0-based), or UINT64_MAX if not a prime
 * 
 * Inverse of platonic_vertex_to_prime.
 */
uint64_t platonic_prime_to_vertex(uint64_t prime);

/* ============================================================================
 * PRIME TO CLOCK POSITION MAPPING
 * ============================================================================
 */

/**
 * @brief Map prime to clock position
 * @param prime Prime number
 * @param pos Output clock position (must be pre-allocated)
 * @return MATH_SUCCESS or error code
 * 
 * Uses the clock lattice to map a prime to its unique position.
 */
MathError platonic_prime_to_clock_position(uint64_t prime, ClockPosition* pos);

/**
 * @brief Map vertex index to clock position
 * @param vertex_idx Vertex index (0-based)
 * @param pos Output clock position (must be pre-allocated)
 * @return MATH_SUCCESS or error code
 * 
 * Convenience function: vertex → prime → clock position
 */
MathError platonic_vertex_to_clock_position(uint64_t vertex_idx, ClockPosition* pos);

/* ============================================================================
 * CLOCK POSITION TO COORDINATES
 * ============================================================================
 */

/**
 * @brief Generate nD coordinates from clock position
 * @param pos Clock position
 * @param dimension Target dimension (3, 4, 5, ..., n)
 * @param coords Pointer to output coordinate array (will be allocated by function)
 * @param base Abacus base (12, 60, 100, or any base >= 2)
 * @param precision Fractional precision (number of fractional digits)
 * @return MATH_SUCCESS or error code
 * 
 * Uses stereographic projection to map clock position to nD space.
 * Maintains 12-fold symmetry through projection.
 * 
 * The function allocates an array of dimension Abacus pointers.
 * Caller must free each coordinate and the array itself.
 * 
 * Algorithm:
 * 1. Project clock position to 3D sphere (stereographic projection)
 * 2. Extend to nD using harmonic functions
 * 3. Normalize to maintain unit sphere constraint
 * 4. Convert to Abacus representation
 */
MathError platonic_clock_to_coordinates(const ClockPosition* pos,
                                       uint32_t dimension,
                                       CrystallineAbacus*** coords,
                                       uint32_t base,
                                       uint32_t precision);

/**
 * @brief Generate nD coordinates from vertex index
 * @param vertex_idx Vertex index (0-based)
 * @param dimension Target dimension
 * @param coords Pointer to output coordinate array (will be allocated by function)
 * @param base Abacus base
 * @param precision Fractional precision
 * @return MATH_SUCCESS or error code
 * 
 * Convenience function: vertex → prime → clock → coordinates
 * The function allocates an array of dimension Abacus pointers.
 * Caller must free each coordinate and the array itself.
 */
MathError platonic_vertex_to_coordinates(uint64_t vertex_idx,
                                        uint32_t dimension,
                                        CrystallineAbacus*** coords,
                                        uint32_t base,
                                        uint32_t precision);

/* ============================================================================
 * COORDINATE VALIDATION
 * ============================================================================
 */

/**
 * @brief Validate that coordinates maintain geometric properties
 * @param coords Coordinates (array of dimension Abacus values)
 * @param dimension Dimension
 * @return true if coordinates are valid
 * 
 * Checks:
 * - All coordinates are finite
 * - Magnitude is reasonable (not too large/small)
 * - 12-fold symmetry is maintained (where applicable)
 */
bool platonic_validate_coordinates(CrystallineAbacus** coords, uint32_t dimension);

/**
 * @brief Compute distance between two coordinate sets
 * @param coords1 First coordinates
 * @param coords2 Second coordinates
 * @param dimension Dimension
 * @return Distance (as Abacus value)
 * 
 * Computes Euclidean distance in nD space.
 */
CrystallineAbacus* platonic_coordinate_distance(CrystallineAbacus** coords1,
                                               CrystallineAbacus** coords2,
                                               uint32_t dimension);

/* ============================================================================
 * SYMMETRY OPERATIONS
 * ============================================================================
 */

/**
 * @brief Apply 12-fold rotational symmetry
 * @param coords Input coordinates
 * @param dimension Dimension
 * @param rotation Rotation index (0-11)
 * @param result Output coordinates (must be pre-allocated)
 * @return MATH_SUCCESS or error code
 * 
 * Rotates coordinates by (rotation * 30°) around primary axis.
 * Maintains 12-fold symmetry structure.
 */
MathError platonic_apply_12fold_rotation(CrystallineAbacus** coords,
                                         uint32_t dimension,
                                         uint32_t rotation,
                                         CrystallineAbacus** result);

/**
 * @brief Check if coordinates exhibit 12-fold symmetry
 * @param coords Coordinates to check
 * @param dimension Dimension
 * @return true if 12-fold symmetry is present
 */
bool platonic_has_12fold_symmetry(CrystallineAbacus** coords, uint32_t dimension);

#ifdef __cplusplus
}
#endif

#endif /* MATH_PLATONIC_CLOCK_H */