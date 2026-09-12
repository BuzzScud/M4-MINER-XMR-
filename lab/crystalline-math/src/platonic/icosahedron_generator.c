/**
 * @file icosahedron_generator.c
 * @brief 3D icosahedron generator
 * 
 * The icosahedron is one of the 5 Platonic solids.
 * It has 20 triangular faces and uses the golden ratio.
 * It is the dual of the dodecahedron.
 * 
 * Properties:
 * - Vertices: 12
 * - Edges: 30
 * - Faces: 20 (triangles)
 * - Schläfli symbol: {3,5}
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
 * @brief Generate coordinates for icosahedron
 * 
 * The icosahedron has 12 vertices at:
 * - (±1, ±φ, 0) and cyclic permutations
 * 
 * This gives the correct edge length of 2.
 */
static bool generate_icosahedron_coordinates(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    double phi = MATH_PHI;
    
    // Allocate coordinate array (12 vertices × 3 dimensions)
    solid->vertex_coords = (double*)calloc(12 * 3, sizeof(double));
    if (!solid->vertex_coords) {
        return false;
    }
    
    int idx = 0;
    
    // 4 vertices: (±1, ±φ, 0)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            solid->vertex_coords[idx * 3 + 0] = (i == 0) ? -1.0 : 1.0;
            solid->vertex_coords[idx * 3 + 1] = (j == 0) ? -phi : phi;
            solid->vertex_coords[idx * 3 + 2] = 0.0;
            idx++;
        }
    }
    
    // 4 vertices: (0, ±1, ±φ)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            solid->vertex_coords[idx * 3 + 0] = 0.0;
            solid->vertex_coords[idx * 3 + 1] = (i == 0) ? -1.0 : 1.0;
            solid->vertex_coords[idx * 3 + 2] = (j == 0) ? -phi : phi;
            idx++;
        }
    }
    
    // 4 vertices: (±φ, 0, ±1)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            solid->vertex_coords[idx * 3 + 0] = (i == 0) ? -phi : phi;
            solid->vertex_coords[idx * 3 + 1] = 0.0;
            solid->vertex_coords[idx * 3 + 2] = (j == 0) ? -1.0 : 1.0;
            idx++;
        }
    }
    
    return true;
}

// ============================================================================
// EDGE GENERATION
// ============================================================================

/**
 * @brief Generate edges for icosahedron
 * 
 * The icosahedron has 30 edges.
 * Two vertices are connected if their distance is approximately 2.
 */
static bool generate_icosahedron_edges(PlatonicSolid* solid) {
    if (!solid || !solid->vertex_coords) {
        return false;
    }
    
    // Edge length for icosahedron with these coordinates
    // Distance between adjacent vertices = 2
    double edge_length_sq = 4.0;  // 2²
    double tolerance = 0.01;
    
    // Count edges first
    uint64_t num_edges = 0;
    for (uint64_t i = 0; i < 12; i++) {
        for (uint64_t j = i + 1; j < 12; j++) {
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
    for (uint64_t i = 0; i < 12; i++) {
        for (uint64_t j = i + 1; j < 12; j++) {
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
 * @brief Generate faces for icosahedron
 * 
 * The icosahedron has 20 triangular faces.
 * This is a simplified implementation that just sets the count.
 */
static bool generate_icosahedron_faces(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // The icosahedron has 20 triangular faces
    solid->num_faces = 20;
    
    // For now, we just set the count
    // Full face connectivity would require more complex geometry
    // This can be added later if needed
    
    return true;
}

// ============================================================================
// MAIN GENERATOR
// ============================================================================

PlatonicSolid* platonic_generate_icosahedron(void) {
    // Allocate solid
    PlatonicSolid* solid = platonic_alloc();
    if (!solid) {
        return NULL;
    }
    
    // Set basic properties
    solid->dimension = 3;
    solid->num_vertices = 12;
    
    // Create Schläfli symbol {3,5}
    uint32_t* schlafli = (uint32_t*)calloc(2, sizeof(uint32_t));
    if (!schlafli) {
        platonic_free(solid);
        return NULL;
    }
    schlafli[0] = 3;
    schlafli[1] = 5;
    solid->schlafli_symbol = schlafli;
    solid->symbol_length = 2;
    
    // Set name
    snprintf(solid->name, sizeof(solid->name), "Icosahedron");
    
    // Mark as having golden ratio
    solid->has_golden_ratio = true;
    
    // Generate coordinates
    if (!generate_icosahedron_coordinates(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate edges
    if (!generate_icosahedron_edges(solid)) {
        platonic_free(solid);
        return NULL;
    }
    
    // Generate faces
    if (!generate_icosahedron_faces(solid)) {
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