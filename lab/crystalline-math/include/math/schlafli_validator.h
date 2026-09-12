#ifndef MATH_SCHLAFLI_VALIDATOR_H
#define MATH_SCHLAFLI_VALIDATOR_H

/**
 * @file schlafli_validator.h
 * @brief Mathematical validation for Schläfli symbols
 * 
 * This module provides mathematical proofs for Schläfli symbol validity,
 * replacing hardcoded polytope lists with rigorous mathematical validation.
 * 
 * Key Theorems:
 * 1. Angle Sum Theorem: Sum of face angles at vertex < 2π
 * 2. Euler Characteristic: V - E + F = χ (dimension-dependent)
 * 3. Dihedral Angle: Must be < π for convexity
 * 4. Regularity: All faces congruent, all vertices equivalent
 */

#include "math/schlafli.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MATHEMATICAL VALIDATION
// ============================================================================

/**
 * @brief Validate Schläfli symbol using mathematical proofs
 * 
 * Uses angle sum theorem, Euler characteristic, and dihedral angle
 * constraints to determine if a Schläfli symbol represents a valid
 * regular polytope.
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return true if mathematically valid, false otherwise
 */
bool schlafli_validate_mathematical(const SchlafliSymbol* symbol);

/**
 * @brief Validate using angle sum theorem
 * 
 * For a vertex to be valid, the sum of face angles meeting at that
 * vertex must be less than 2π (360°).
 * 
 * For {p,q}: q faces of p sides meet at each vertex
 * Face angle = (p-2)π/p
 * Vertex angle sum = q × (p-2)π/p < 2π
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return true if angle sum constraint satisfied
 */
bool schlafli_validate_angle_sum(const SchlafliSymbol* symbol);

/**
 * @brief Validate using Euler characteristic
 * 
 * For 3D: V - E + F = 2
 * For 4D: V - E + F - C = 0
 * For nD: Alternating sum of k-faces
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return true if Euler characteristic satisfied
 */
bool schlafli_validate_euler(const SchlafliSymbol* symbol);

/**
 * @brief Validate dihedral angle
 * 
 * The dihedral angle (angle between adjacent faces) must be < π
 * for the polytope to be convex.
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return true if dihedral angle valid
 */
bool schlafli_validate_dihedral(const SchlafliSymbol* symbol);

// ============================================================================
// POLYTOPE PROPERTY DISCOVERY
// ============================================================================

/**
 * @brief Calculate number of vertices from Schläfli symbol
 * 
 * Uses recursive formulas based on dimension and symbol components.
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Number of vertices, or 0 if cannot be calculated
 */
uint64_t schlafli_calculate_vertices(const SchlafliSymbol* symbol);

/**
 * @brief Calculate number of edges from Schläfli symbol
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Number of edges, or 0 if cannot be calculated
 */
uint64_t schlafli_calculate_edges(const SchlafliSymbol* symbol);

/**
 * @brief Calculate number of faces from Schläfli symbol
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Number of faces, or 0 if cannot be calculated
 */
uint64_t schlafli_calculate_faces(const SchlafliSymbol* symbol);

/**
 * @brief Calculate number of cells from Schläfli symbol (4D+)
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Number of cells, or 0 if not applicable
 */
uint64_t schlafli_calculate_cells(const SchlafliSymbol* symbol);

/**
 * @brief Calculate circumradius (radius of circumscribed sphere)
 * 
 * Normalized to edge length = 1.
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Circumradius, or 0.0 if cannot be calculated
 */
double schlafli_calculate_circumradius(const SchlafliSymbol* symbol);

/**
 * @brief Calculate inradius (radius of inscribed sphere)
 * 
 * Normalized to edge length = 1.
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Inradius, or 0.0 if cannot be calculated
 */
double schlafli_calculate_inradius(const SchlafliSymbol* symbol);

/**
 * @brief Calculate dihedral angle between adjacent faces
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Dihedral angle in radians, or 0.0 if cannot be calculated
 */
double schlafli_calculate_dihedral_angle(const SchlafliSymbol* symbol);

// ============================================================================
// VALIDATION DIAGNOSTICS
// ============================================================================

/**
 * @brief Validation result with detailed diagnostics
 */
typedef struct {
    bool is_valid;                    /**< Overall validity */
    bool angle_sum_valid;             /**< Angle sum constraint satisfied */
    bool euler_valid;                 /**< Euler characteristic satisfied */
    bool dihedral_valid;              /**< Dihedral angle valid */
    
    double angle_sum;                 /**< Actual angle sum at vertex */
    double angle_sum_limit;           /**< Maximum allowed (2π) */
    
    int64_t euler_characteristic;     /**< Calculated Euler characteristic */
    int64_t expected_euler;           /**< Expected value for dimension */
    
    double dihedral_angle;            /**< Dihedral angle in radians */
    
    char error_message[256];          /**< Error description if invalid */
} SchlafliValidationResult;

/**
 * @brief Perform comprehensive validation with diagnostics
 * 
 * @param symbol Pointer to Schläfli symbol
 * @return Validation result with detailed diagnostics
 */
SchlafliValidationResult schlafli_validate_comprehensive(const SchlafliSymbol* symbol);

/**
 * @brief Print validation diagnostics
 * 
 * @param result Pointer to validation result
 */
void schlafli_print_validation(const SchlafliValidationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* MATH_SCHLAFLI_VALIDATOR_H */