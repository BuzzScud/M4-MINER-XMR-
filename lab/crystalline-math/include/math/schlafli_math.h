#ifndef MATH_SCHLAFLI_MATH_ENHANCED_H
#define MATH_SCHLAFLI_MATH_ENHANCED_H

/**
 * @file schlafli_math.h
 * @brief Enhanced mathematical validation using general formulas
 * 
 * This module provides general mathematical formulas for polytope
 * properties without hardcoded values, enabling discovery of new
 * polytopes.
 */

#include "math/schlafli.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// GENERAL PROPERTY CALCULATION
// ============================================================================

/**
 * @brief Calculate vertices using general recursive formula
 * 
 * Uses mathematical formulas based on Schläfli symbol structure:
 * - Simplex {3,3,...,3}: V = n+1
 * - Hypercube {4,3,...,3}: V = 2^n
 * - Cross-polytope {3,3,...,4}: V = 2n
 * - General polyhedra: V = 4p / (4 - (p-2)(q-2))
 * 
 * @param symbol Schläfli symbol
 * @return Number of vertices, or 0 if cannot calculate
 */
uint64_t schlafli_calculate_vertices_general(const SchlafliSymbol* symbol);

/**
 * @brief Calculate edges using general formula
 * 
 * Uses vertex count and degree:
 * - Simplex: E = n(n+1)/2
 * - Hypercube: E = n * 2^(n-1)
 * - Cross-polytope: E = n(n-1)
 * - General: E = (V * degree) / 2
 * 
 * @param symbol Schläfli symbol
 * @return Number of edges, or 0 if cannot calculate
 */
uint64_t schlafli_calculate_edges_general(const SchlafliSymbol* symbol);

/**
 * @brief Calculate 2-faces using general formula
 * 
 * Uses Euler characteristic or recursive formulas.
 * 
 * @param symbol Schläfli symbol
 * @return Number of 2-faces, or 0 if cannot calculate
 */
uint64_t schlafli_calculate_faces_general(const SchlafliSymbol* symbol);

/**
 * @brief Calculate 3-faces (cells) for 4D+ polytopes
 * 
 * Uses Euler characteristic or recursive formulas.
 * 
 * @param symbol Schläfli symbol
 * @return Number of cells, or 0 if not applicable
 */
uint64_t schlafli_calculate_cells_general(const SchlafliSymbol* symbol);

/**
 * @brief Calculate number of k-faces for any dimension k
 * 
 * General formula for k-faces:
 * - Simplex: f_k = C(n+1, k+1)
 * - Hypercube: f_k = C(n, k+1) * 2^(n-k-1)
 * - Cross-polytope: f_k = C(n, k+1) * 2^(k+1)
 * 
 * @param symbol Schläfli symbol
 * @param k Dimension of faces (0=vertices, 1=edges, 2=faces, etc.)
 * @return Number of k-faces, or 0 if cannot calculate
 */
uint64_t schlafli_calculate_k_faces(const SchlafliSymbol* symbol, uint32_t k);

// ============================================================================
// ENHANCED VALIDATION
// ============================================================================

/**
 * @brief Validate using generalized Euler characteristic
 * 
 * For dimension n:
 * χ = Σ(-1)^k * f_k for k=0 to n-1
 * 
 * Expected values:
 * - 3D: χ = 2
 * - 4D: χ = 0
 * - 5D: χ = 2
 * - Generally: χ = 1 + (-1)^n
 * 
 * @param symbol Schläfli symbol
 * @return true if Euler characteristic is correct
 */
bool schlafli_validate_euler_general(const SchlafliSymbol* symbol);

/**
 * @brief Check if Schläfli symbol represents a valid regular polytope
 * 
 * Uses mathematical constraints:
 * 1. All components >= 3
 * 2. Angle sum < 2π at each vertex
 * 3. Euler characteristic matches dimension
 * 4. Nested Schläfli symbols are valid (for 4D+)
 * 5. Schläfli condition: (p-2)(q-2) < 4 for 3D
 * 
 * Does NOT use hardcoded polytope lists.
 * 
 * @param symbol Schläfli symbol
 * @return true if valid regular polytope
 */
bool schlafli_is_valid_regular_polytope(const SchlafliSymbol* symbol);

// ============================================================================
// FACE ENUMERATION
// ============================================================================

/**
 * @brief Face vector structure
 * 
 * Stores the number of k-faces for all dimensions.
 */
typedef struct {
    uint32_t dimension;           /**< Dimension of polytope */
    uint64_t* f_vector;          /**< Array of f_k values (length = dimension) */
    bool is_complete;            /**< All values calculated successfully */
} FaceVector;

/**
 * @brief Calculate complete f-vector
 * 
 * The f-vector is (f_0, f_1, ..., f_{n-1}) where f_k is the
 * number of k-faces.
 * 
 * @param symbol Schläfli symbol
 * @return Face vector, or NULL on error
 */
FaceVector* schlafli_calculate_f_vector(const SchlafliSymbol* symbol);

/**
 * @brief Free face vector
 * 
 * @param fv Face vector to free
 */
void schlafli_free_f_vector(FaceVector* fv);

/**
 * @brief Print face vector
 * 
 * @param fv Face vector to print
 */
void schlafli_print_f_vector(const FaceVector* fv);

#ifdef __cplusplus
}
#endif

#endif /* MATH_SCHLAFLI_MATH_ENHANCED_H */