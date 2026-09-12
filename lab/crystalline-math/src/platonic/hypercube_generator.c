/**
 * @file hypercube_generator.c
 * @brief nD hypercube (generalized cube) generator
 * 
 * The hypercube is the generalization of the square and cube.
 * It has 2^n vertices in n dimensions.
 * 
 * Properties:
 * - Vertices: 2^n
 * - Edges: n × 2^(n-1)
 * - Schläfli symbol: {4,3,...,3} (n-2 threes)
 * 
 * Examples:
 * - 2D: Square (4 vertices)
 * - 3D: Cube (8 vertices)
 * - 4D: Tesseract (16 vertices)
 * - 5D: 5-cube (32 vertices)
 */

#include "math/platonic_generator.h"
#include "math/constants.h"
#include "math/polytope.h"
#include "math/schlafli.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// COORDINATE GENERATION
// ============================================================================

/**
 * @brief Generate coordinates for nD hypercube
 * 
 * Algorithm:
 *   Vertices are all combinations of {-1, +1} in n dimensions
 *   Total vertices: 2^n
 * 
 * Example (3D cube):
 *   (-1,-1,-1), (+1,-1,-1), (-1,+1,-1), (+1,+1,-1),
 *   (-1,-1,+1), (+1,-1,+1), (-1,+1,+1), (+1,+1,+1)
 */
static bool generate_hypercube_coordinates(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint32_t n = solid->dimension;
    uint64_t num_vertices = 1ULL << n;  // 2^n
    
    // Allocate coordinate array
    solid->vertex_coords = (double*)calloc(num_vertices * n, sizeof(double));
    if (!solid->vertex_coords) {
        return false;
    }
    
    // Generate all binary combinations
    for (uint64_t i = 0; i < num_vertices; i++) {
        for (uint32_t j = 0; j < n; j++) {
            // Check if bit j is set in i
            solid->vertex_coords[i * n + j] = (i & (1ULL << j)) ? 1.0 : -1.0;
        }
    }
    
    return true;
}

// ============================================================================
// EDGE GENERATION
// ============================================================================

/**
 * @brief Generate edges for hypercube
 * 
 * Two vertices are connected if they differ in exactly one coordinate.
 * Number of edges = n × 2^(n-1)
 */
static bool generate_hypercube_edges(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint32_t n = solid->dimension;
    uint64_t num_vertices = solid->num_vertices;
    uint64_t num_edges = n * (1ULL << (n - 1));  // n × 2^(n-1)
    
    solid->num_edges = num_edges;
    
    // Allocate edge array
    solid->edge_indices = (uint32_t**)calloc(num_edges, sizeof(uint32_t*));
    if (!solid->edge_indices) {
        return false;
    }
    
    // Generate edges
    uint64_t edge_idx = 0;
    for (uint64_t i = 0; i < num_vertices; i++) {
        for (uint32_t dim = 0; dim < n; dim++) {
            // Flip bit 'dim' to get adjacent vertex
            uint64_t j = i ^ (1ULL << dim);
            
            // Only add edge once (i < j)
            if (i < j) {
                solid->edge_indices[edge_idx] = (uint32_t*)calloc(2, sizeof(uint32_t));
                if (!solid->edge_indices[edge_idx]) {
                    return false;
                }
                solid->edge_indices[edge_idx][0] = (uint32_t)i;
                solid->edge_indices[edge_idx][1] = (uint32_t)j;
                edge_idx++;
            }
        }
    }
    
    return true;
}

// ============================================================================
// FACE GENERATION
// ============================================================================

/**
 * @brief Generate faces for hypercube
 * 
 * Faces are squares (4 vertices).
 * Number of 2-faces = n(n-1) × 2^(n-2)
 */
static bool generate_hypercube_faces(PlatonicSolid* solid) {
    if (!solid || solid->dimension < 3) {
        return true;  // No 2-faces for dimension < 3
    }
    
    uint32_t n = solid->dimension;
    uint64_t num_vertices __attribute__((unused)) = solid->num_vertices;
    
    // Number of 2-faces (square faces)
    solid->num_faces = (n * (n - 1) / 2) * (1ULL << (n - 2));
    
    // Allocate face arrays
    solid->face_indices = (uint32_t**)calloc(solid->num_faces, sizeof(uint32_t*));
    solid->face_sizes = (uint32_t*)calloc(solid->num_faces, sizeof(uint32_t));
    if (!solid->face_indices || !solid->face_sizes) {
        return false;
    }
    
    // Generate square faces
    // A face is defined by fixing n-2 coordinates and varying 2
    uint64_t face_idx = 0;
    
    // For each pair of dimensions
    for (uint32_t dim1 = 0; dim1 < n; dim1++) {
        for (uint32_t dim2 = dim1 + 1; dim2 < n; dim2++) {
            // For each combination of fixed coordinates
            uint64_t num_fixed_combos = 1ULL << (n - 2);
            for (uint64_t fixed = 0; fixed < num_fixed_combos; fixed++) {
                // Build the 4 vertices of this square face
                solid->face_indices[face_idx] = (uint32_t*)calloc(4, sizeof(uint32_t));
                if (!solid->face_indices[face_idx]) {
                    return false;
                }
                
                // Generate base vertex with fixed coordinates
                uint64_t base = 0;
                uint32_t fixed_bit = 0;
                for (uint32_t d = 0; d < n; d++) {
                    if (d != dim1 && d != dim2) {
                        if (fixed & (1ULL << fixed_bit)) {
                            base |= (1ULL << d);
                        }
                        fixed_bit++;
                    }
                }
                
                // Four corners of the square
                solid->face_indices[face_idx][0] = (uint32_t)base;
                solid->face_indices[face_idx][1] = (uint32_t)(base | (1ULL << dim1));
                solid->face_indices[face_idx][2] = (uint32_t)(base | (1ULL << dim2));
                solid->face_indices[face_idx][3] = (uint32_t)(base | (1ULL << dim1) | (1ULL << dim2));
                
                solid->face_sizes[face_idx] = 4;
                face_idx++;
            }
        }
    }
    
    return true;
}

// ============================================================================
// CELL GENERATION (4D+)
// ============================================================================

/**
 * @brief Generate cells for 4D+ hypercube
 * 
 * Cells are cubes (8 vertices).
 * Number of 3-cells = n(n-1)(n-2)/6 × 2^(n-3)
 */
static bool generate_hypercube_cells(PlatonicSolid* solid) {
    if (!solid || solid->dimension < 4) {
        return true;  // No cells for dimension < 4
    }
    
    uint32_t n = solid->dimension;
    
    // Number of 3-cells (cubic cells)
    solid->num_cells = (n * (n - 1) * (n - 2) / 6) * (1ULL << (n - 3));
    
    // For now, we just store the count
    // Full cell connectivity can be added later if needed
    
    return true;
}

// ============================================================================
// MAIN GENERATOR
// ============================================================================

PlatonicSolid* platonic_generate_hypercube(uint32_t dimension) {
    if (dimension < 2) {
        fprintf(stderr, "Error: Hypercube dimension must be >= 2\n");
        return NULL;
    }
    
    // Check for overflow (2^n vertices)
    if (dimension > 20) {
        fprintf(stderr, "Error: Hypercube dimension too large (max 20)\n");
        return NULL;
    }
    
    // Allocate solid
    PlatonicSolid* solid = platonic_alloc();
    if (!solid) {
        return NULL;
    }
    
    // Set basic properties
    solid->dimension = dimension;
    solid->num_vertices = 1ULL << dimension;  // 2^n
    
    // Create Schläfli symbol {4,3,...,3}
    if (dimension >= 3) {
        uint32_t* schlafli = (uint32_t*)malloc((dimension - 1) * sizeof(uint32_t));
        if (!schlafli) {
            platonic_free(solid);
            return NULL;
        }
        schlafli[0] = 4;
        for (uint32_t i = 1; i < dimension - 1; i++) {
            schlafli[i] = 3;
        }
        solid->schlafli_symbol = schlafli;
        solid->symbol_length = dimension - 1;
    }
    
    // Set name
    if (dimension == 2) {
        snprintf(solid->name, sizeof(solid->name), "Square");
    } else if (dimension == 3) {
        snprintf(solid->name, sizeof(solid->name), "Cube");
    } else if (dimension == 4) {
        snprintf(solid->name, sizeof(solid->name), "Tesseract");
    } else {
        snprintf(solid->name, sizeof(solid->name), "%u-cube", dimension);
    }
    
    // Generate coordinates
    if (!generate_hypercube_coordinates(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate edges
    if (!generate_hypercube_edges(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate faces
    if (!generate_hypercube_faces(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate cells (4D+)
    if (!generate_hypercube_cells(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Compute remaining properties
    if (!platonic_compute_properties(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    return solid;
}

// ============================================================================
// SPECIALIZED 3D CUBE
// ============================================================================

PlatonicSolid* platonic_generate_cube(void) {
    return platonic_generate_hypercube(3);
}

// ============================================================================
// SPECIALIZED 4D TESSERACT
// ============================================================================

PlatonicSolid* platonic_generate_tesseract(void) {
    return platonic_generate_hypercube(4);
}