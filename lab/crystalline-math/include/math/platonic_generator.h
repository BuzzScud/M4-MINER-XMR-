#ifndef MATH_PLATONIC_GENERATOR_H
#define MATH_PLATONIC_GENERATOR_H

/**
 * @file platonic_generator.h
 * @brief Infinite Platonic Solid Generator - Main API
 * 
 * This is the core API for generating regular polytopes (Platonic solids)
 * in any dimension. The generator can create:
 * 
 * - 3D Platonic solids (5 classical)
 * - 4D regular polychora (6 regular)
 * - nD regular polytopes (3 per dimension for n ≥ 5)
 * 
 * The generator enables:
 * - Infinite scalability (any dimension)
 * - Dynamic model scaling
 * - Hierarchical threading
 * - Multi-model training
 * - Live capacity scaling
 */

#include "polytope.h"
#include "schlafli.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MAIN GENERATOR FUNCTIONS
// ============================================================================

/**
 * @brief Generate a Platonic solid from Schläfli symbol
 * 
 * @param dimension Dimension of polytope (3, 4, 5, ..., n)
 * @param schlafli Schläfli symbol components {p, q, r, ...}
 * @param length Length of Schläfli symbol
 * @return Pointer to generated solid, or NULL on error
 * 
 * Examples:
 *   platonic_generate(3, (uint32_t[]){3,3}, 2)  → Tetrahedron
 *   platonic_generate(3, (uint32_t[]){4,3}, 2)  → Cube
 *   platonic_generate(3, (uint32_t[]){3,4}, 2)  → Octahedron
 *   platonic_generate(3, (uint32_t[]){5,3}, 2)  → Dodecahedron
 *   platonic_generate(3, (uint32_t[]){3,5}, 2)  → Icosahedron
 *   platonic_generate(4, (uint32_t[]){3,3,3}, 3) → 5-cell
 *   platonic_generate(4, (uint32_t[]){4,3,3}, 3) → Tesseract
 */
PlatonicSolid* platonic_generate(uint32_t dimension, 
                                  const uint32_t* schlafli, 
                                  uint32_t length);

/**
 * @brief Generate nD simplex (generalization of tetrahedron)
 * 
 * The simplex is the simplest regular polytope in any dimension.
 * 
 * Properties:
 *   - Vertices: n+1
 *   - Edges: (n+1)n/2
 *   - Schläfli: {3,3,...,3} (n-1 threes)
 * 
 * @param dimension Dimension of simplex
 * @return Pointer to generated simplex, or NULL on error
 * 
 * Examples:
 *   platonic_generate_simplex(3) → Tetrahedron (4 vertices)
 *   platonic_generate_simplex(4) → 5-cell (5 vertices)
 *   platonic_generate_simplex(5) → 5-simplex (6 vertices)
 *   platonic_generate_simplex(10) → 10-simplex (11 vertices)
 */
PlatonicSolid* platonic_generate_simplex(uint32_t dimension);

/**
 * @brief Generate nD hypercube (generalization of cube)
 * 
 * The hypercube is the generalization of the square and cube.
 * 
 * Properties:
 *   - Vertices: 2^n
 *   - Edges: n × 2^(n-1)
 *   - Schläfli: {4,3,...,3} (n-2 threes)
 * 
 * @param dimension Dimension of hypercube
 * @return Pointer to generated hypercube, or NULL on error
 * 
 * Examples:
 *   platonic_generate_hypercube(3) → Cube (8 vertices)
 *   platonic_generate_hypercube(4) → Tesseract (16 vertices)
 *   platonic_generate_hypercube(5) → 5-cube (32 vertices)
 *   platonic_generate_hypercube(10) → 10-cube (1024 vertices)
 */
PlatonicSolid* platonic_generate_hypercube(uint32_t dimension);

/**
 * @brief Generate nD cross-polytope (generalization of octahedron)
 * 
 * The cross-polytope is the dual of the hypercube.
 * 
 * Properties:
 *   - Vertices: 2n
 *   - Edges: n(n-1)
 *   - Schläfli: {3,3,...,4} (n-2 threes, then 4)
 * 
 * @param dimension Dimension of cross-polytope
 * @return Pointer to generated cross-polytope, or NULL on error
 * 
 * Examples:
 *   platonic_generate_cross_polytope(3) → Octahedron (6 vertices)
 *   platonic_generate_cross_polytope(4) → 16-cell (8 vertices)
 *   platonic_generate_cross_polytope(5) → 5-orthoplex (10 vertices)
 *   platonic_generate_cross_polytope(10) → 10-orthoplex (20 vertices)
 */
PlatonicSolid* platonic_generate_cross_polytope(uint32_t dimension);

// ============================================================================
// SPECIALIZED 3D GENERATORS
// ============================================================================

/**
 * @brief Generate 3D tetrahedron
 * @return Pointer to tetrahedron, or NULL on error
 */
PlatonicSolid* platonic_generate_tetrahedron(void);

/**
 * @brief Generate 3D cube
 * @return Pointer to cube, or NULL on error
 */
PlatonicSolid* platonic_generate_cube(void);

/**
 * @brief Generate 3D octahedron
 * @return Pointer to octahedron, or NULL on error
 */
PlatonicSolid* platonic_generate_octahedron(void);

/**
 * @brief Generate 3D dodecahedron
 * @return Pointer to dodecahedron, or NULL on error
 */
PlatonicSolid* platonic_generate_dodecahedron(void);

/**
 * @brief Generate 3D icosahedron
 * @return Pointer to icosahedron, or NULL on error
 */
PlatonicSolid* platonic_generate_icosahedron(void);

// ============================================================================
// SPECIALIZED 4D GENERATORS
// ============================================================================

/**
 * @brief Generate 4D 5-cell (4-simplex)
 * @return Pointer to 5-cell, or NULL on error
 */
PlatonicSolid* platonic_generate_5cell(void);

/**
 * @brief Generate 4D tesseract (4-cube)
 * @return Pointer to tesseract, or NULL on error
 */
PlatonicSolid* platonic_generate_tesseract(void);

/**
 * @brief Generate 4D 16-cell (4-orthoplex)
 * @return Pointer to 16-cell, or NULL on error
 */
PlatonicSolid* platonic_generate_16cell(void);

/**
 * @brief Generate 4D 24-cell
 * @return Pointer to 24-cell, or NULL on error
 */
PlatonicSolid* platonic_generate_24cell(void);

/**
 * @brief Generate 4D 120-cell
 * @return Pointer to 120-cell, or NULL on error
 */
PlatonicSolid* platonic_generate_120cell(void);

/**
 * @brief Generate 4D 600-cell
 * @return Pointer to 600-cell, or NULL on error
 */
PlatonicSolid* platonic_generate_600cell(void);

// ============================================================================
// ADAPTIVE GENERATORS
// ============================================================================

/**
 * @brief Generate solid by target vertex count
 * 
 * Finds the closest valid regular polytope with approximately
 * the target number of vertices.
 * 
 * @param dimension Dimension of polytope
 * @param target_vertices Target number of vertices
 * @return Pointer to generated solid, or NULL on error
 */
PlatonicSolid* platonic_generate_by_vertices(uint32_t dimension, 
                                              uint64_t target_vertices);

/**
 * @brief Generate solid for CLLM model
 * 
 * Automatically selects appropriate solid based on model requirements.
 * Uses the 12-fold scaling law:
 *   - Embedding dimension = V × 12
 *   - Hidden dimension = E × 12
 *   - Number of layers = F
 * 
 * @param vocab_size Vocabulary size
 * @param target_params Target number of parameters
 * @return Pointer to generated solid, or NULL on error
 */
PlatonicSolid* platonic_generate_for_model(uint64_t vocab_size, 
                                            uint64_t target_params);

// ============================================================================
// PROPERTY COMPUTATION
// ============================================================================

/**
 * @brief Compute all properties of a solid
 * 
 * Fills in all fields of PlatonicSolid structure:
 * - Vertex coordinates
 * - Edge connectivity
 * - Face connectivity
 * - Symmetry properties
 * - Metric properties
 * - 12-fold properties
 * - Euler characteristic
 * 
 * @param solid Pointer to solid
 * @return true on success, false on error
 */
bool platonic_compute_properties(PlatonicSolid* solid);

/**
 * @brief Compute vertex coordinates in nD space
 * @param solid Pointer to solid
 * @return true on success, false on error
 */
bool platonic_compute_coordinates(PlatonicSolid* solid);

/**
 * @brief Compute edge connectivity
 * @param solid Pointer to solid
 * @return true on success, false on error
 */
bool platonic_compute_edges(PlatonicSolid* solid);

/**
 * @brief Compute face connectivity
 * @param solid Pointer to solid
 * @return true on success, false on error
 */
bool platonic_compute_faces(PlatonicSolid* solid);

/**
 * @brief Compute symmetry group and order
 * @param solid Pointer to solid
 * @return true on success, false on error
 */
bool platonic_compute_symmetries(PlatonicSolid* solid);

/**
 * @brief Compute metric properties (radii, volume)
 * @param solid Pointer to solid
 * @return true on success, false on error
 */
bool platonic_compute_metrics(PlatonicSolid* solid);

/**
 * @brief Compute 12-fold properties for CLLM
 * @param solid Pointer to solid
 * @return true on success, false on error
 */
bool platonic_compute_cllm_properties(PlatonicSolid* solid);

// ============================================================================
// PRINTING & DEBUGGING
// ============================================================================

/**
 * @brief Print solid properties to stdout
 * @param solid Platonic solid to print
 */
void platonic_print(const PlatonicSolid* solid);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * @brief Validate solid using generalized Euler characteristic
 * 
 * For dimension d:
 *   χ(d) = Σ(-1)^i * f_i
 * 
 * Expected values:
 *   3D: χ = 2
 *   4D: χ = 0
 *   5D: χ = 2
 *   Generally: χ(d) = 1 + (-1)^d
 * 
 * @param solid Pointer to solid
 * @return true if valid, false otherwise
 */
bool platonic_validate_euler(const PlatonicSolid* solid);

/**
 * @brief Validate symmetry properties
 * @param solid Pointer to solid
 * @return true if valid, false otherwise
 */
bool platonic_validate_symmetry(const PlatonicSolid* solid);

/**
 * @brief Validate regularity (all faces/edges/angles equal)
 * @param solid Pointer to solid
 * @return true if regular, false otherwise
 */
bool platonic_validate_regularity(const PlatonicSolid* solid);

/**
 * @brief Validate all properties
 * @param solid Pointer to solid
 * @return true if all validations pass, false otherwise
 */
bool platonic_validate(const PlatonicSolid* solid);

// ============================================================================
// EXPORT
// ============================================================================

/**
 * @brief Export solid to file
 * 
 * Supported formats:
 * - "obj" - Wavefront OBJ (3D only)
 * - "ply" - Stanford PLY (3D only)
 * - "json" - JSON format (all dimensions)
 * - "txt" - Plain text (all dimensions)
 * 
 * @param solid Pointer to solid
 * @param filename Output filename
 * @param format Format string ("obj", "ply", "json", "txt")
 * @return true on success, false on error
 */
bool platonic_export(const PlatonicSolid* solid, 
                     const char* filename, 
                     const char* format);

#ifdef __cplusplus
}
#endif

#endif /* MATH_PLATONIC_GENERATOR_H */