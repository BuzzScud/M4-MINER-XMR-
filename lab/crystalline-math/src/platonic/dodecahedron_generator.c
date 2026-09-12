/**
 * @file dodecahedron_generator.c
 * @brief 3D dodecahedron generator
 * 
 * The dodecahedron is one of the 5 Platonic solids.
 * It has 12 pentagonal faces and uses the golden ratio.
 * 
 * Properties:
 * - Vertices: 20
 * - Edges: 30
 * - Faces: 12 (pentagons)
 * - Schläfli symbol: {5,3}
 * - Contains golden ratio φ = (1+√5)/2
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

// Golden ratio: φ = (1 + √5) / 2

// ============================================================================
// COORDINATE GENERATION
// ============================================================================

/**
 * @brief Generate coordinates for dodecahedron
 * 
 * The dodecahedron has 20 vertices that can be constructed from:
 * - 8 vertices of a cube: (±1, ±1, ±1)
 * - 4 vertices on xy plane: (0, ±1/φ, ±φ)
 * - 4 vertices on yz plane: (±1/φ, 0, ±φ)
 * - 4 vertices on xz plane: (±φ, ±1/φ, 0)
 */
static bool generate_dodecahedron_coordinates(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    double phi = MATH_PHI;
    double inv_phi = 1.0 / phi;
    
    // Allocate coordinate array (20 vertices × 3 dimensions)
    solid->vertex_coords = (double*)calloc(20 * 3, sizeof(double));
    if (!solid->vertex_coords) {
        return false;
    }
    
    int idx = 0;
    
    // 8 vertices of a cube: (±1, ±1, ±1)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                solid->vertex_coords[idx * 3 + 0] = (i == 0) ? -1.0 : 1.0;
                solid->vertex_coords[idx * 3 + 1] = (j == 0) ? -1.0 : 1.0;
                solid->vertex_coords[idx * 3 + 2] = (k == 0) ? -1.0 : 1.0;
                idx++;
            }
        }
    }
    
    // 4 vertices on xy plane: (0, ±1/φ, ±φ)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            solid->vertex_coords[idx * 3 + 0] = 0.0;
            solid->vertex_coords[idx * 3 + 1] = (i == 0) ? -inv_phi : inv_phi;
            solid->vertex_coords[idx * 3 + 2] = (j == 0) ? -phi : phi;
            idx++;
        }
    }
    
    // 4 vertices on yz plane: (±1/φ, 0, ±φ)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            solid->vertex_coords[idx * 3 + 0] = (i == 0) ? -inv_phi : inv_phi;
            solid->vertex_coords[idx * 3 + 1] = 0.0;
            solid->vertex_coords[idx * 3 + 2] = (j == 0) ? -phi : phi;
            idx++;
        }
    }
    
    // 4 vertices on xz plane: (±φ, ±1/φ, 0)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            solid->vertex_coords[idx * 3 + 0] = (i == 0) ? -phi : phi;
            solid->vertex_coords[idx * 3 + 1] = (j == 0) ? -inv_phi : inv_phi;
            solid->vertex_coords[idx * 3 + 2] = 0.0;
            idx++;
        }
    }
    
    return true;
}

// ============================================================================
// EDGE GENERATION
// ============================================================================

/**
 * @brief Generate edges for dodecahedron
 * 
 * The dodecahedron has 30 edges.
 * Two vertices are connected if their distance is approximately 2/φ.
 */
static bool generate_dodecahedron_edges(PlatonicSolid* solid) {
    if (!solid || !solid->vertex_coords) {
        return false;
    }
    
    // Edge length for dodecahedron with these coordinates
    double edge_length_sq = 4.0 / (MATH_PHI * MATH_PHI);  // (2/φ)²
    double tolerance = 0.01;
    
    // Count edges first
    uint64_t num_edges = 0;
    for (uint64_t i = 0; i < 20; i++) {
        for (uint64_t j = i + 1; j < 20; j++) {
            double dx = solid->vertex_coords[i * 3 + 0] - solid->vertex_coords[j * 3 + 0];
            double dy = solid->vertex_coords[i * 3 + 1] - solid->vertex_coords[j * 3 + 1];
            double dz = solid->vertex_coords[i * 3 + 2] - solid->vertex_coords[j * 3 + 2];
            double dist_sq = dx * dx + dy * dy + dz * dz;
            
            if (math_abs(dist_sq - edge_length_sq) < tolerance) {
                num_edges++;
            }
        }
    }
    
    solid->num_edges = num_edges;
    
    // Allocate edge array
    solid->edge_indices = (uint32_t**)calloc(num_edges, sizeof(uint32_t*));
    if (!solid->edge_indices) {
        return false;
    }
    
    // Generate edges
    uint64_t edge_idx = 0;
    for (uint64_t i = 0; i < 20; i++) {
        for (uint64_t j = i + 1; j < 20; j++) {
            double dx = solid->vertex_coords[i * 3 + 0] - solid->vertex_coords[j * 3 + 0];
            double dy = solid->vertex_coords[i * 3 + 1] - solid->vertex_coords[j * 3 + 1];
            double dz = solid->vertex_coords[i * 3 + 2] - solid->vertex_coords[j * 3 + 2];
            double dist_sq = dx * dx + dy * dy + dz * dz;
            
            if (math_abs(dist_sq - edge_length_sq) < tolerance) {
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
 * @brief Generate faces for dodecahedron
 * 
 * The dodecahedron has 12 pentagonal faces.
 * This is a simplified implementation that just sets the count.
 */
static bool generate_dodecahedron_faces(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // The dodecahedron has 12 pentagonal faces
    solid->num_faces = 12;
    
    // For now, we just set the count
    // Full face connectivity would require more complex geometry
    // This can be added later if needed
    
    return true;
}

// ============================================================================
// MAIN GENERATOR
// ============================================================================

PlatonicSolid* platonic_generate_dodecahedron(void) {
    // Allocate solid
    PlatonicSolid* solid = platonic_alloc();
    if (!solid) {
        return NULL;
    }
    
    // Set basic properties
    solid->dimension = 3;
    solid->num_vertices = 20;
    
    // Create Schläfli symbol {5,3}
    uint32_t* schlafli = (uint32_t*)calloc(2, sizeof(uint32_t));
    if (!schlafli) {
        platonic_free(solid);
        return NULL;
    }
    schlafli[0] = 5;
    schlafli[1] = 3;
    solid->schlafli_symbol = schlafli;
    solid->symbol_length = 2;
    
    // Set name
    snprintf(solid->name, sizeof(solid->name), "Dodecahedron");
    
    // Mark as having golden ratio
    solid->has_golden_ratio = true;
    
    // Generate coordinates
    if (!generate_dodecahedron_coordinates(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate edges
    if (!generate_dodecahedron_edges(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate faces
    if (!generate_dodecahedron_faces(solid)) {
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