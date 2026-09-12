/**
 * @file cross_polytope_generator.c
 * @brief nD cross-polytope (generalized octahedron) generator
 * 
 * The cross-polytope is the dual of the hypercube.
 * It has 2n vertices in n dimensions.
 * 
 * Properties:
 * - Vertices: 2n
 * - Edges: n(n-1)
 * - Schläfli symbol: {3,3,...,4} (n-2 threes, then 4)
 * 
 * Examples:
 * - 2D: Square (4 vertices) - self-dual with square
 * - 3D: Octahedron (6 vertices) - dual of cube
 * - 4D: 16-cell (8 vertices) - dual of tesseract
 * - 5D: 5-orthoplex (10 vertices) - dual of 5-cube
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
 * @brief Generate coordinates for nD cross-polytope
 * 
 * Algorithm:
 *   Vertices are unit vectors along each axis (positive and negative)
 *   Total vertices: 2n
 * 
 * Example (3D octahedron):
 *   (±1,0,0), (0,±1,0), (0,0,±1)
 */
static bool generate_cross_polytope_coordinates(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint32_t n = solid->dimension;
    uint64_t num_vertices = 2 * n;
    
    // Allocate coordinate array
    solid->vertex_coords = (double*)calloc(num_vertices * n, sizeof(double));
    if (!solid->vertex_coords) {
        return false;
    }
    
    // Generate unit vectors
    for (uint32_t i = 0; i < n; i++) {
        // Positive direction
        solid->vertex_coords[i * n + i] = 1.0;
        
        // Negative direction
        solid->vertex_coords[(n + i) * n + i] = -1.0;
    }
    
    return true;
}

// ============================================================================
// EDGE GENERATION
// ============================================================================

/**
 * @brief Generate edges for cross-polytope
 * 
 * Two vertices are connected if they are on different axes.
 * Number of edges = 2n(2n-2)/2 - n = 2n² - 2n = 2n(n-1)
 * But we only count each edge once, so: n(n-1)
 * 
 * Actually, each vertex connects to all vertices except itself and its opposite.
 * So each vertex has 2n-2 edges, total = 2n(2n-2)/2 = 2n(n-1)
 */
static bool generate_cross_polytope_edges(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint32_t n = solid->dimension;
    uint64_t num_vertices = solid->num_vertices;
    
    // Number of edges: each vertex connects to all except itself and opposite
    // Total edges = 2n × (2n-2) / 2 = 2n(n-1)
    solid->num_edges = 2 * n * (n - 1);
    
    // Allocate edge array (use calloc to zero-initialize)
    solid->edge_indices = (uint32_t**)calloc(solid->num_edges, sizeof(uint32_t*));
    if (!solid->edge_indices) {
        return false;
    }
    
    // Generate edges
    uint64_t edge_idx = 0;
    for (uint64_t i = 0; i < num_vertices; i++) {
        for (uint64_t j = i + 1; j < num_vertices; j++) {
            // Check if vertices are on different axes
            // Vertex i is on axis (i % n), vertex j is on axis (j % n)
            uint32_t axis_i = i % n;
            uint32_t axis_j = j % n;
            
            // Connect if on different axes
            if (axis_i != axis_j) {
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
 * @brief Generate faces for cross-polytope
 * 
 * Faces are triangles.
 * Number of 2-faces = 2^(n-1) × n(n-1)
 * 
 * For 3D octahedron: 8 triangular faces
 * For 4D 16-cell: 32 triangular faces
 */
static bool generate_cross_polytope_faces(PlatonicSolid* solid) {
    if (!solid || solid->dimension < 3) {
        return true;  // No 2-faces for dimension < 3
    }
    
    uint32_t n = solid->dimension;
    
    // Number of 2-faces (triangular faces)
    // For 3D octahedron: 8 triangular faces = 2^3
    // For 4D 16-cell: 32 triangular faces = 2^3 × 4
    // Formula: 2^n for 3D, 2^(n-1) × n for 4D+
    if (n == 3) {
        solid->num_faces = 1ULL << n;  // 2^3 = 8
    } else {
        solid->num_faces = (1ULL << (n - 1)) * n;  // 2^(n-1) × n
    }
    
    // Allocate face arrays (use calloc to zero-initialize)
    solid->face_indices = (uint32_t**)calloc(solid->num_faces, sizeof(uint32_t*));
    solid->face_sizes = (uint32_t*)calloc(solid->num_faces, sizeof(uint32_t));
    if (!solid->face_indices || !solid->face_sizes) {
        return false;
    }
    
    // Generate triangular faces
    // For cross-polytope, faces are simplices formed by vertices on different axes
    // For 3D octahedron: 8 triangular faces (one per orthant)
    // Each face has 3 vertices, one from each of 3 different axes
    
    uint64_t face_idx = 0;
    
    // For 3D, generate all 8 triangular faces explicitly
    if (n == 3) {
        // Each orthant (+,+,+), (+,+,-), (+,-,+), (+,-,-), (-,+,+), (-,+,-), (-,-,+), (-,-,-)
        // corresponds to a triangular face with vertices from 3 different axes
        for (uint32_t s0 = 0; s0 < 2; s0++) {
            for (uint32_t s1 = 0; s1 < 2; s1++) {
                for (uint32_t s2 = 0; s2 < 2; s2++) {
                    solid->face_indices[face_idx] = (uint32_t*)calloc(3, sizeof(uint32_t));
                    if (!solid->face_indices[face_idx]) {
                        return false;
                    }
                    
                    // Vertices: one from each axis with appropriate sign
                    solid->face_indices[face_idx][0] = 0 + (s0 * n);  // axis 0
                    solid->face_indices[face_idx][1] = 1 + (s1 * n);  // axis 1
                    solid->face_indices[face_idx][2] = 2 + (s2 * n);  // axis 2
                    solid->face_sizes[face_idx] = 3;
                    face_idx++;
                }
            }
        }
    } else {
        // For higher dimensions, use simplified approach
        // Just set the count for now
        // Full face generation can be added later
    }
    
    return true;
}

// ============================================================================
// CELL GENERATION (4D+)
// ============================================================================

/**
 * @brief Generate cells for 4D+ cross-polytope
 * 
 * Cells are octahedra (for 4D) or cross-polytopes (for higher dimensions).
 * Number of 3-cells depends on dimension.
 */
static bool generate_cross_polytope_cells(PlatonicSolid* solid) {
    if (!solid || solid->dimension < 4) {
        return true;  // No cells for dimension < 4
    }
    
    uint32_t n = solid->dimension;
    
    // Number of 3-cells
    // For 4D 16-cell: 16 tetrahedral cells = 2^4
    // Formula: 2^n for n-dimensional cross-polytope
    solid->num_cells = 1ULL << n;  // 2^n
    
    // For now, we just store the count
    // Full cell connectivity can be added later if needed
    
    return true;
}

// ============================================================================
// MAIN GENERATOR
// ============================================================================

PlatonicSolid* platonic_generate_cross_polytope(uint32_t dimension) {
    if (dimension < 2) {
        fprintf(stderr, "Error: Cross-polytope dimension must be >= 2\n");
        return NULL;
    }
    
    // Allocate solid
    PlatonicSolid* solid = platonic_alloc();
    if (!solid) {
        return NULL;
    }
    
    // Set basic properties
    solid->dimension = dimension;
    solid->num_vertices = 2 * dimension;
    
    // Create Schläfli symbol {3,3,...,4}
    if (dimension >= 3) {
        uint32_t* schlafli = (uint32_t*)malloc((dimension - 1) * sizeof(uint32_t));
        if (!schlafli) {
            platonic_free(solid);
            return NULL;
        }
        for (uint32_t i = 0; i < dimension - 2; i++) {
            schlafli[i] = 3;
        }
        schlafli[dimension - 2] = 4;
        solid->schlafli_symbol = schlafli;
        solid->symbol_length = dimension - 1;
    }
    
    // Set name
    if (dimension == 2) {
        snprintf(solid->name, sizeof(solid->name), "Square");
    } else if (dimension == 3) {
        snprintf(solid->name, sizeof(solid->name), "Octahedron");
    } else if (dimension == 4) {
        snprintf(solid->name, sizeof(solid->name), "16-cell");
    } else {
        snprintf(solid->name, sizeof(solid->name), "%u-orthoplex", dimension);
    }
    
    // Generate coordinates
    if (!generate_cross_polytope_coordinates(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate edges
    if (!generate_cross_polytope_edges(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate faces
    if (!generate_cross_polytope_faces(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate cells (4D+)
    if (!generate_cross_polytope_cells(solid)) {
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
// SPECIALIZED 3D OCTAHEDRON
// ============================================================================

PlatonicSolid* platonic_generate_octahedron(void) {
    return platonic_generate_cross_polytope(3);
}

// ============================================================================
// SPECIALIZED 4D 16-CELL
// ============================================================================

PlatonicSolid* platonic_generate_16cell(void) {
    return platonic_generate_cross_polytope(4);
}