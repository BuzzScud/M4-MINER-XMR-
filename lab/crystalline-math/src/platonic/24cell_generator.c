/**
 * @file 24cell_generator.c
 * @brief 24-cell (icositetrachoron) generator
 * 
 * The 24-cell is a regular 4D polytope with 24 vertices.
 * It is UNIQUE to 4D - no analogue exists in other dimensions.
 * 
 * Properties:
 * - Vertices: 24
 * - Edges: 96
 * - Faces: 96 (triangular)
 * - Cells: 24 (octahedral)
 * - Schläfli symbol: {3,4,3}
 * 
 * CRITICAL IMPORTANCE:
 * - Perfect 12-fold symmetry (24 = 2×12)
 * - Self-dual (dual of itself)
 * - Used in anchor_grid.c for geometric recovery
 * - Central to Master Plan OBJECTIVE 2
 * 
 * COORDINATE SYSTEM:
 * The 24-cell can be constructed as the vertices of the 4D cross-polytope
 * plus the vertices of the tesseract, all normalized to unit distance from origin.
 * 
 * Vertices fall into two groups:
 * 1. 16 vertices from tesseract: all permutations of (±1, ±1, 0, 0)
 * 2. 8 vertices from cross-polytope: (±1, 0, 0, 0), (0, ±1, 0, 0), etc.
 * 
 * All vertices are at distance √2 from origin.
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
 * @brief Generate coordinates for 24-cell
 * 
 * Algorithm:
 *   The 24 vertices are all permutations and sign changes of:
 *   (±1, ±1, 0, 0)
 *   
 *   This gives us:
 *   - Choose 2 positions out of 4 for the non-zero coordinates: C(4,2) = 6 ways
 *   - For each choice, we have 4 sign combinations: (++), (+-), (-+), (--)
 *   - Total: 6 × 4 = 24 vertices
 * 
 * All vertices are at distance √2 from origin.
 */
static bool generate_24cell_coordinates(PlatonicSolid* solid) {
    if (!solid || solid->dimension != 4) {
        return false;
    }
    
    uint64_t num_vertices = 24;
    
    // Allocate coordinate array
    solid->vertex_coords = (double*)calloc(num_vertices * 4, sizeof(double));
    if (!solid->vertex_coords) {
        return false;
    }
    
    uint64_t idx = 0;
    
    // All permutations of (±1, ±1, 0, 0)
    // Choose 2 positions out of 4 for the non-zero coordinates
    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = i + 1; j < 4; j++) {
            // Four sign combinations: (++), (+-), (-+), (--)
            for (int si = -1; si <= 1; si += 2) {
                for (int sj = -1; sj <= 1; sj += 2) {
                    solid->vertex_coords[idx * 4 + i] = (double)si;
                    solid->vertex_coords[idx * 4 + j] = (double)sj;
                    // Other coordinates remain 0 (from calloc)
                    idx++;
                }
            }
        }
    }
    
    // Verify we generated exactly 24 vertices
    if (idx != 24) {
        fprintf(stderr, "Error: Generated %lu vertices instead of 24\n", 
                (unsigned long)idx);
        return false;
    }
    
    return true;
}

// ============================================================================
// EDGE GENERATION
// ============================================================================

/**
 * @brief Generate edges for 24-cell
 * 
 * Two vertices are connected if their Euclidean distance is √2.
 * Each vertex connects to 8 others, giving 24×8/2 = 96 edges.
 */
static bool generate_24cell_edges(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint64_t num_vertices = 24;
    
    // Allocate temporary edge list (max possible edges)
    uint32_t** temp_edges = (uint32_t**)calloc(num_vertices * num_vertices / 2, 
                                                sizeof(uint32_t*));
    if (!temp_edges) {
        return false;
    }
    
    uint64_t edge_count = 0;
    double edge_length_sq = 2.0;  // √2 squared = 2
    double tolerance = 0.0001;
    
    // Check all pairs of vertices
    for (uint64_t i = 0; i < num_vertices; i++) {
        for (uint64_t j = i + 1; j < num_vertices; j++) {
            // Compute squared distance
            double dist_sq = 0.0;
            for (uint32_t k = 0; k < 4; k++) {
                double diff = solid->vertex_coords[i * 4 + k] - 
                             solid->vertex_coords[j * 4 + k];
                dist_sq += diff * diff;
            }
            
            // Check if this is an edge (distance = √2)
            if (math_abs(dist_sq - edge_length_sq) < tolerance) {
                temp_edges[edge_count] = (uint32_t*)calloc(2, sizeof(uint32_t));
                if (!temp_edges[edge_count]) {
                    // Cleanup on error
                    for (uint64_t e = 0; e < edge_count; e++) {
                        free(temp_edges[e]);
                    }
                    free(temp_edges);
                    return false;
                }
                temp_edges[edge_count][0] = (uint32_t)i;
                temp_edges[edge_count][1] = (uint32_t)j;
                edge_count++;
            }
        }
    }
    
    // Verify we found exactly 96 edges
    if (edge_count != 96) {
        fprintf(stderr, "Warning: Found %lu edges instead of expected 96\n", 
                (unsigned long)edge_count);
    }
    
    // Allocate final edge array
    solid->num_edges = edge_count;
    solid->edge_indices = (uint32_t**)calloc(edge_count, sizeof(uint32_t*));
    if (!solid->edge_indices) {
        for (uint64_t e = 0; e < edge_count; e++) {
            free(temp_edges[e]);
        }
        free(temp_edges);
        return false;
    }
    
    // Copy edges to final array
    memcpy(solid->edge_indices, temp_edges, edge_count * sizeof(uint32_t*));
    free(temp_edges);
    
    return true;
}

// ============================================================================
// FACE GENERATION
// ============================================================================

/**
 * @brief Generate faces for 24-cell
 * 
 * The 24-cell has 96 triangular faces.
 * Each face is an equilateral triangle with vertices at distance √2 apart.
 */
static bool generate_24cell_faces(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    uint64_t num_vertices = 24;
    
    // Allocate temporary face list
    uint32_t** temp_faces = (uint32_t**)calloc(num_vertices * num_vertices, 
                                                sizeof(uint32_t*));
    uint32_t* temp_face_sizes = (uint32_t*)calloc(num_vertices * num_vertices, 
                                                   sizeof(uint32_t));
    if (!temp_faces || !temp_face_sizes) {
        free(temp_faces);
        free(temp_face_sizes);
        return false;
    }
    
    uint64_t face_count = 0;
    double edge_length_sq = 2.0;  // √2 squared = 2
    double tolerance = 0.0001;
    
    // Check all triplets of vertices
    for (uint64_t i = 0; i < num_vertices; i++) {
        for (uint64_t j = i + 1; j < num_vertices; j++) {
            for (uint64_t k = j + 1; k < num_vertices; k++) {
                // Check if all three pairs form edges (distance = √2)
                double dist_ij_sq = 0.0, dist_ik_sq = 0.0, dist_jk_sq = 0.0;
                
                for (uint32_t d = 0; d < 4; d++) {
                    double diff_ij = solid->vertex_coords[i * 4 + d] - 
                                    solid->vertex_coords[j * 4 + d];
                    double diff_ik = solid->vertex_coords[i * 4 + d] - 
                                    solid->vertex_coords[k * 4 + d];
                    double diff_jk = solid->vertex_coords[j * 4 + d] - 
                                    solid->vertex_coords[k * 4 + d];
                    
                    dist_ij_sq += diff_ij * diff_ij;
                    dist_ik_sq += diff_ik * diff_ik;
                    dist_jk_sq += diff_jk * diff_jk;
                }
                
                // Check if all three distances equal √2 (forms equilateral triangle)
                if (math_abs(dist_ij_sq - edge_length_sq) < tolerance &&
                    math_abs(dist_ik_sq - edge_length_sq) < tolerance &&
                    math_abs(dist_jk_sq - edge_length_sq) < tolerance) {
                    
                    temp_faces[face_count] = (uint32_t*)calloc(3, sizeof(uint32_t));
                    if (!temp_faces[face_count]) {
                        // Cleanup on error
                        for (uint64_t f = 0; f < face_count; f++) {
                            free(temp_faces[f]);
                        }
                        free(temp_faces);
                        free(temp_face_sizes);
                        return false;
                    }
                    
                    temp_faces[face_count][0] = (uint32_t)i;
                    temp_faces[face_count][1] = (uint32_t)j;
                    temp_faces[face_count][2] = (uint32_t)k;
                    temp_face_sizes[face_count] = 3;
                    face_count++;
                }
            }
        }
    }
    
    // Verify we found exactly 96 faces
    if (face_count != 96) {
        fprintf(stderr, "Warning: Found %lu faces instead of expected 96\n", 
                (unsigned long)face_count);
    }
    
    // Allocate final face arrays
    solid->num_faces = face_count;
    solid->face_indices = (uint32_t**)calloc(face_count, sizeof(uint32_t*));
    solid->face_sizes = (uint32_t*)calloc(face_count, sizeof(uint32_t));
    if (!solid->face_indices || !solid->face_sizes) {
        for (uint64_t f = 0; f < face_count; f++) {
            free(temp_faces[f]);
        }
        free(temp_faces);
        free(temp_face_sizes);
        return false;
    }
    
    // Copy faces to final arrays
    memcpy(solid->face_indices, temp_faces, face_count * sizeof(uint32_t*));
    memcpy(solid->face_sizes, temp_face_sizes, face_count * sizeof(uint32_t));
    
    free(temp_faces);
    free(temp_face_sizes);
    
    return true;
}

// ============================================================================
// CELL GENERATION
// ============================================================================

/**
 * @brief Generate cells for 24-cell
 * 
 * The 24-cell has 24 octahedral cells.
 * Each cell is a regular octahedron with 6 vertices.
 */
static bool generate_24cell_cells(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // For now, just set the count
    // Full cell connectivity can be added later if needed
    solid->num_cells = 24;
    
    return true;
}

// ============================================================================
// MAIN GENERATOR
// ============================================================================

PlatonicSolid* platonic_generate_24cell(void) {
    // Allocate solid
    PlatonicSolid* solid = platonic_alloc();
    if (!solid) {
        return NULL;
    }
    
    // Set basic properties
    solid->dimension = 4;
    solid->num_vertices = 24;
    
    // Create Schläfli symbol {3,4,3}
    uint32_t* schlafli = (uint32_t*)malloc(3 * sizeof(uint32_t));
    if (!schlafli) {
        platonic_free(solid);
        return NULL;
    }
    schlafli[0] = 3;
    schlafli[1] = 4;
    schlafli[2] = 3;
    solid->schlafli_symbol = schlafli;
    solid->symbol_length = 3;
    
    // Set name
    snprintf(solid->name, sizeof(solid->name), "24-cell");
    
    // Generate coordinates
    if (!generate_24cell_coordinates(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate edges
    if (!generate_24cell_edges(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate faces
    if (!generate_24cell_faces(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate cells
    if (!generate_24cell_cells(solid)) {
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