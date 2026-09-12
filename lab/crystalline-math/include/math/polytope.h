#ifndef MATH_POLYTOPE_H
#define MATH_POLYTOPE_H

/**
 * @file polytope.h
 * @brief Core data structures for nD Platonic solids (polytopes)
 * 
 * This file defines the fundamental structures for representing
 * regular polytopes in any dimension (3D, 4D, 5D, ..., nD).
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CORE STRUCTURES
// ============================================================================

/**
 * @brief Represents a regular polytope (Platonic solid) in nD space
 * 
 * This structure can represent:
 * - 3D Platonic solids (tetrahedron, cube, octahedron, dodecahedron, icosahedron)
 * - 4D regular polychora (5-cell, tesseract, 16-cell, 24-cell, 120-cell, 600-cell)
 * - nD regular polytopes (simplex, hypercube, cross-polytope)
 */
typedef struct {
    // ========================================================================
    // DIMENSION
    // ========================================================================
    uint32_t dimension;              /**< Dimension of polytope (3, 4, 5, ..., n) */
    
    // ========================================================================
    // SCHLÄFLI SYMBOL
    // ========================================================================
    uint32_t* schlafli_symbol;       /**< Schläfli symbol {p,q,r,...} */
    uint32_t symbol_length;          /**< Length of Schläfli symbol */
    
    // ========================================================================
    // COMBINATORIAL STRUCTURE (f-vector)
    // ========================================================================
    uint64_t num_vertices;           /**< Number of 0-faces (vertices) */
    uint64_t num_edges;              /**< Number of 1-faces (edges) */
    uint64_t num_faces;              /**< Number of 2-faces (faces) */
    uint64_t num_cells;              /**< Number of 3-faces (cells, 4D+) */
    uint64_t* higher_faces;          /**< Number of k-faces for k > 3 (5D+) */
    uint32_t num_face_types;         /**< Number of face types */
    
    // ========================================================================
    // GEOMETRIC STRUCTURE
    // ========================================================================
    double* vertex_coords;           /**< Vertex coordinates [num_vertices * dimension] */
    uint32_t** edge_indices;         /**< Edge connectivity [num_edges][2] */
    uint32_t** face_indices;         /**< Face connectivity [num_faces][vertices_per_face] */
    uint32_t* face_sizes;            /**< Number of vertices per face [num_faces] */
    
    // ========================================================================
    // SYMMETRY PROPERTIES
    // ========================================================================
    uint32_t symmetry_order;         /**< Order of symmetry group */
    char symmetry_group[32];         /**< Name of symmetry group (e.g., "T_d", "O_h", "I_h") */
    
    // ========================================================================
    // METRIC PROPERTIES
    // ========================================================================
    double edge_length;              /**< Edge length (normalized to 1.0) */
    double circumradius;             /**< Radius of circumscribed hypersphere */
    double inradius;                 /**< Radius of inscribed hypersphere */
    double volume;                   /**< Volume (or hypervolume) */
    
    // ========================================================================
    // 12-FOLD PROPERTIES (for CLLM integration)
    // ========================================================================
    uint32_t embedding_dim;          /**< num_vertices × 12 */
    uint32_t hidden_dim;             /**< num_edges × 12 */
    uint32_t num_layers;             /**< num_faces (or highest face count) */
    uint32_t num_heads;              /**< Always 12 (12-fold symmetry) */
    
    // ========================================================================
    // EULER CHARACTERISTIC
    // ========================================================================
    int64_t euler_characteristic;    /**< χ = Σ(-1)^i * f_i */
    
    // ========================================================================
    // VALIDATION FLAGS
    // ========================================================================
    bool is_valid;                   /**< Passed all validation checks */
    bool is_regular;                 /**< All faces/edges/angles equal */
    bool is_convex;                  /**< Convex polytope */
    bool has_golden_ratio;           /**< Contains golden ratio (φ) */
    
    // ========================================================================
    // METADATA
    // ========================================================================
    char name[64];                   /**< Name of polytope (e.g., "Tetrahedron", "Tesseract") */
    
} PlatonicSolid;

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Allocate a new PlatonicSolid structure
 * @return Pointer to allocated structure, or NULL on error
 */
PlatonicSolid* platonic_alloc(void);

/**
 * @brief Free all memory associated with a PlatonicSolid
 * @param solid Pointer to solid to free
 */
void platonic_free(PlatonicSolid* solid);

/**
 * @brief Clone a PlatonicSolid (deep copy)
 * @param solid Pointer to solid to clone
 * @return Pointer to cloned solid, or NULL on error
 */
PlatonicSolid* platonic_clone(const PlatonicSolid* solid);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Get name of solid
 * @param solid Pointer to solid
 * @return Name string (e.g., "Tetrahedron", "Tesseract", "5-simplex")
 */
const char* platonic_get_name(const PlatonicSolid* solid);

/**
 * @brief Print solid properties to stdout
 * @param solid Pointer to solid
 */
void platonic_print(const PlatonicSolid* solid);

/**
 * @brief Print detailed solid properties to stdout
 * @param solid Pointer to solid
 */
void platonic_print_detailed(const PlatonicSolid* solid);

#ifdef __cplusplus
}
#endif

#endif /* MATH_POLYTOPE_H */