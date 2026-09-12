/**
 * @file platonic_generator_abacus.h
 * @brief Platonic solid generators using Crystalline Abacus (arbitrary precision)
 * 
 * Pure Crystalline mathematics - NO math.h, uses Abacus throughout
 */

#ifndef MATH_PLATONIC_GENERATOR_ABACUS_H
#define MATH_PLATONIC_GENERATOR_ABACUS_H

#include "math/polytope_abacus.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// GENERAL GENERATORS
// ============================================================================

/**
 * @brief Generate nD simplex with Abacus
 * @param dimension Dimension (2D, 3D, 4D, ...)
 * @param base Abacus base (12, 60, 100, or custom)
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_simplex_abacus(uint32_t dimension,
                                                       uint32_t base,
                                                       int32_t precision);

/**
 * @brief Generate nD hypercube with Abacus
 * @param dimension Dimension (2D, 3D, 4D, ...)
 * @param base Abacus base (12, 60, 100, or custom)
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_hypercube_abacus(uint32_t dimension,
                                                         uint32_t base,
                                                         int32_t precision);

/**
 * @brief Generate nD cross-polytope with Abacus
 * @param dimension Dimension (2D, 3D, 4D, ...)
 * @param base Abacus base (12, 60, 100, or custom)
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_cross_polytope_abacus(uint32_t dimension,
                                                              uint32_t base,
                                                              int32_t precision);

// ============================================================================
// 3D PLATONIC SOLIDS WITH ABACUS
// ============================================================================

/**
 * @brief Generate tetrahedron with Abacus
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_tetrahedron_abacus(uint32_t base, int32_t precision);

/**
 * @brief Generate cube with Abacus
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_cube_abacus(uint32_t base, int32_t precision);

/**
 * @brief Generate octahedron with Abacus
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_octahedron_abacus(uint32_t base, int32_t precision);

/**
 * @brief Generate dodecahedron with Abacus (golden ratio)
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_dodecahedron_abacus(uint32_t base, int32_t precision);

/**
 * @brief Generate icosahedron with Abacus (golden ratio)
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_icosahedron_abacus(uint32_t base, int32_t precision);

// ============================================================================
// 4D PLATONIC SOLIDS WITH ABACUS
// ============================================================================

/**
 * @brief Generate 5-cell with Abacus
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_5cell_abacus(uint32_t base, int32_t precision);

/**
 * @brief Generate tesseract with Abacus
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_tesseract_abacus(uint32_t base, int32_t precision);

/**
 * @brief Generate 16-cell with Abacus
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_generate_16cell_abacus(uint32_t base, int32_t precision);

// ============================================================================
// GOLDEN RATIO COMPUTATION WITH ABACUS
// ============================================================================

/**
 * @brief Compute golden ratio φ = (1 + √5) / 2 using Abacus
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Pointer to Abacus with golden ratio, or NULL on error
 */

#ifdef __cplusplus
}
#endif

#endif /* MATH_PLATONIC_GENERATOR_ABACUS_H */