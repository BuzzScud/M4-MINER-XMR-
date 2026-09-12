/**
 * @file polytope_abacus.h
 * @brief Platonic solid structure using Crystalline Abacus (arbitrary precision)
 * 
 * This is the NEW structure that uses pure Crystalline mathematics.
 * Replaces double coordinates with Crystalline Abacus for arbitrary precision.
 */

#ifndef MATH_POLYTOPE_ABACUS_H
#define MATH_POLYTOPE_ABACUS_H

#include "math/types.h"
#include "math/abacus.h"
#include "math/polytope.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Platonic solid with Crystalline Abacus coordinates
 * 
 * This structure uses arbitrary precision arithmetic throughout.
 * All coordinates, lengths, and radii are represented as Abacus numbers.
 */
typedef struct {
    // Basic properties
    char name[64];                   /**< Name of the solid */
    uint32_t dimension;              /**< Dimension (3D, 4D, 5D, ...) */
    
    // Schläfli symbol
    uint32_t* schlafli_symbol;       /**< Schläfli symbol components */
    uint32_t symbol_length;          /**< Length of Schläfli symbol */
    
    // Vertex data (ABACUS - arbitrary precision)
    CrystallineAbacus** vertex_coords; /**< Vertex coordinates [num_vertices][dimension] */
    uint64_t num_vertices;           /**< Number of vertices */
    
    // Edge data
    uint32_t** edge_indices;         /**< Edge vertex indices [num_edges][2] */
    uint64_t num_edges;              /**< Number of edges */
    
    // Face data
    uint32_t** face_indices;         /**< Face vertex indices [num_faces][face_size] */
    uint32_t* face_sizes;            /**< Size of each face */
    uint64_t num_faces;              /**< Number of 2-faces */
    
    // Cell data (4D+)
    uint64_t num_cells;              /**< Number of 3-cells (4D+) */
    
    // Higher-dimensional faces (5D+)
    uint64_t* higher_faces;          /**< Counts of k-faces for k > 3 */
    uint32_t num_face_types;         /**< Number of face types */
    
    // Geometric properties (ABACUS - arbitrary precision)
    CrystallineAbacus* edge_length;  /**< Edge length */
    CrystallineAbacus* circumradius; /**< Radius of circumscribed hypersphere */
    CrystallineAbacus* inradius;     /**< Radius of inscribed hypersphere */
    CrystallineAbacus* volume;       /**< Volume (or hypervolume) */
    
    // Topological properties
    int64_t euler_characteristic;    /**< Euler characteristic χ */
    
    // Symmetry properties
    char symmetry_group[32];         /**< Symmetry group notation */
    uint32_t symmetry_order;         /**< Order of symmetry group */
    
    // Special properties
    bool has_golden_ratio;           /**< Contains golden ratio φ */
    bool is_regular;                 /**< All faces/edges/angles equal */
    bool is_convex;                  /**< Convex polytope */
    
    // CLLM integration properties
    uint32_t embedding_dim;          /**< Embedding dimension (V × 12) */
    uint32_t hidden_dim;             /**< Hidden dimension (E × 12) */
    uint32_t num_layers;             /**< Number of layers (F or C) */
    uint32_t num_heads;              /**< Number of attention heads (always 12) */
    
    // Abacus base (for all coordinates)
    uint32_t abacus_base;            /**< Base for Abacus (12, 60, or 100) */
    int32_t abacus_precision;        /**< Fractional precision */
    
} PlatonicSolidAbacus;

/**
 * @brief Allocate a new Platonic solid with Abacus
 * @param base Abacus base (12, 60, or 100)
 * @param precision Fractional precision
 * @return Pointer to new solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_abacus_alloc(uint32_t base, int32_t precision);

/**
 * @brief Free a Platonic solid with Abacus
 * @param solid Solid to free
 */
void platonic_abacus_free(PlatonicSolidAbacus* solid);

/**
 * @brief Convert double-based solid to Abacus-based solid
 * @param solid_double Double-based solid
 * @param base Abacus base
 * @param precision Fractional precision
 * @return Abacus-based solid, or NULL on error
 */
PlatonicSolidAbacus* platonic_double_to_abacus(const PlatonicSolid* solid_double,
                                                uint32_t base, int32_t precision);

/**
 * @brief Convert Abacus-based solid to double-based solid (for compatibility)
 * @param solid_abacus Abacus-based solid
 * @return Double-based solid, or NULL on error
 */
PlatonicSolid* platonic_abacus_to_double(const PlatonicSolidAbacus* solid_abacus);

#ifdef __cplusplus
}
#endif

#endif /* MATH_POLYTOPE_ABACUS_H */