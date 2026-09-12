/**
 * @file simplex_generator.c
 * @brief nD simplex (generalized tetrahedron) generator
 * 
 * The simplex is the simplest regular polytope in any dimension.
 * It has n+1 vertices in n dimensions.
 * 
 * Properties:
 * - Vertices: n+1
 * - Edges: (n+1)n/2
 * - Schläfli symbol: {3,3,...,3} (n-1 threes)
 * 
 * Examples:
 * - 2D: Triangle (3 vertices)
 * - 3D: Tetrahedron (4 vertices)
 * - 4D: 5-cell (5 vertices)
 * - 5D: 5-simplex (6 vertices)
 */

#include "math/platonic_generator.h"
#include "math/constants.h"
#include "math/polytope.h"
#include "math/schlafli.h"
#include "math/transcendental.h"
#include "math/arithmetic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// COORDINATE GENERATION
// ============================================================================

/**
 * @brief Generate coordinates for nD simplex
 * 
 * Algorithm:
 *   1. Place first vertex at origin
 *   2. Place second vertex at (1, 0, 0, ..., 0)
 *   3. Place third vertex at (1/2, √3/2, 0, ..., 0)
 *   4. Continue pattern for higher dimensions
 * 
 * This creates a regular simplex with edge length 1.
 */
static bool generate_simplex_coordinates(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint32_t n = solid->dimension;
    uint64_t num_vertices = n + 1;
    
    // Allocate coordinate array
    solid->vertex_coords = (double*)calloc(num_vertices * n, sizeof(double));
    if (!solid->vertex_coords) {
        return false;
    }
    
    // First vertex at origin (all zeros - already done by calloc)
    
    // Generate subsequent vertices
    for (uint64_t i = 1; i < num_vertices; i++) {
        // For each dimension up to i-1
        for (uint32_t j = 0; j < i && j < n; j++) {
            if (j == i - 1) {
                // New dimension component
                // Calculate using Pythagorean theorem to maintain unit edge length
                double sum_sq = 0.0;
                for (uint32_t k = 0; k < j; k++) {
                    double coord = solid->vertex_coords[i * n + k];
                    sum_sq += coord * coord;
                }
                solid->vertex_coords[i * n + j] = math_sqrt(1.0 - sum_sq);
            } else {
                // Copy and adjust from previous vertex
                solid->vertex_coords[i * n + j] = 
                    solid->vertex_coords[(i-1) * n + j] / (double)i;
            }
        }
    }
    
    return true;
}

// ============================================================================
// EDGE GENERATION
// ============================================================================

/**
 * @brief Generate edges for simplex
 * 
 * In a simplex, every vertex is connected to every other vertex.
 * Number of edges = (n+1)n/2
 */
static bool generate_simplex_edges(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint64_t num_vertices = solid->num_vertices;
    uint64_t num_edges = (num_vertices * (num_vertices - 1)) / 2;
    
    solid->num_edges = num_edges;
    
    // Allocate edge array
    solid->edge_indices = (uint32_t**)calloc(num_edges, sizeof(uint32_t*));
    if (!solid->edge_indices) {
        return false;
    }
    
    // Generate all pairs of vertices
    uint64_t edge_idx = 0;
    for (uint64_t i = 0; i < num_vertices; i++) {
        for (uint64_t j = i + 1; j < num_vertices; j++) {
            solid->edge_indices[edge_idx] = (uint32_t*)calloc(2, sizeof(uint32_t));
            if (!solid->edge_indices[edge_idx]) {
                return false;
            }
            solid->edge_indices[edge_idx][0] = (uint32_t)i;
            solid->edge_indices[edge_idx][1] = (uint32_t)j;
            edge_idx++;
        }
    }
    
    return true;
}

// ============================================================================
// FACE GENERATION
// ============================================================================

/**
 * @brief Generate faces for simplex
 * 
 * In a simplex, faces are (n-1)-simplices.
 * For 3D tetrahedron: 4 triangular faces
 * For 4D 5-cell: 5 tetrahedral faces
 */
static bool generate_simplex_faces(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint64_t num_vertices = solid->num_vertices;
    
    // Number of 2-faces (triangular faces)
    // For nD simplex: C(n+1, 3) = (n+1)n(n-1)/6
    if (solid->dimension >= 3) {
        solid->num_faces = (num_vertices * (num_vertices - 1) * (num_vertices - 2)) / 6;
    } else {
        solid->num_faces = 0;
    }
    
    if (solid->num_faces == 0) {
        return true;  // No 2-faces for dimension < 3
    }
    
    // Allocate face arrays
    solid->face_indices = (uint32_t**)calloc(solid->num_faces, sizeof(uint32_t*));
    solid->face_sizes = (uint32_t*)calloc(solid->num_faces, sizeof(uint32_t));
    if (!solid->face_indices || !solid->face_sizes) {
        return false;
    }
    
    // Generate all triplets of vertices (triangular faces)
    uint64_t face_idx = 0;
    for (uint64_t i = 0; i < num_vertices; i++) {
        for (uint64_t j = i + 1; j < num_vertices; j++) {
            for (uint64_t k = j + 1; k < num_vertices; k++) {
                solid->face_indices[face_idx] = (uint32_t*)calloc(3, sizeof(uint32_t));
                if (!solid->face_indices[face_idx]) {
                    return false;
                }
                solid->face_indices[face_idx][0] = (uint32_t)i;
                solid->face_indices[face_idx][1] = (uint32_t)j;
                solid->face_indices[face_idx][2] = (uint32_t)k;
                solid->face_sizes[face_idx] = 3;
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
 * @brief Generate cells for 4D+ simplex
 * 
 * In a 4D simplex, cells are tetrahedra.
 * Number of cells = C(n+1, 4) = (n+1)n(n-1)(n-2)/24
 */
static bool generate_simplex_cells(PlatonicSolid* solid) {
    if (!solid || solid->dimension < 4) {
        return true;  // No cells for dimension < 4
    }
    
    uint64_t num_vertices = solid->num_vertices;
    
    // Number of 3-cells (tetrahedral cells)
    solid->num_cells = (num_vertices * (num_vertices - 1) * 
                       (num_vertices - 2) * (num_vertices - 3)) / 24;
    
    // For now, we just store the count
    // Full cell connectivity can be added later if needed
    
    return true;
}

// ============================================================================
// MAIN GENERATOR
// ============================================================================

PlatonicSolid* platonic_generate_simplex(uint32_t dimension) {
    if (dimension < 2) {
        fprintf(stderr, "Error: Simplex dimension must be >= 2\n");
        return NULL;
    }
    
    // Allocate solid
    PlatonicSolid* solid = platonic_alloc();
    if (!solid) {
        return NULL;
    }
    
    // Set basic properties
    solid->dimension = dimension;
    solid->num_vertices = dimension + 1;
    
    // Create Schläfli symbol {3,3,...,3}
    uint32_t* schlafli = (uint32_t*)malloc((dimension - 1) * sizeof(uint32_t));
    if (!schlafli) {
        platonic_free(solid);
        return NULL;
    }
    for (uint32_t i = 0; i < dimension - 1; i++) {
        schlafli[i] = 3;
    }
    solid->schlafli_symbol = schlafli;
    solid->symbol_length = dimension - 1;
    
    // Set name
    if (dimension == 3) {
        snprintf(solid->name, sizeof(solid->name), "Tetrahedron");
    } else if (dimension == 4) {
        snprintf(solid->name, sizeof(solid->name), "5-cell");
    } else {
        snprintf(solid->name, sizeof(solid->name), "%u-simplex", dimension);
    }
    
    // Generate coordinates
    if (!generate_simplex_coordinates(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate edges
    if (!generate_simplex_edges(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate faces
    if (!generate_simplex_faces(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate cells (4D+)
    if (!generate_simplex_cells(solid)) {
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
// SPECIALIZED 3D TETRAHEDRON
// ============================================================================

PlatonicSolid* platonic_generate_tetrahedron(void) {
    return platonic_generate_simplex(3);
}

// ============================================================================
// SPECIALIZED 4D 5-CELL
// ============================================================================

PlatonicSolid* platonic_generate_5cell(void) {
    return platonic_generate_simplex(4);
}