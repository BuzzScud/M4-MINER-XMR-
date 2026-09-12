/**
 * @file polytope_abacus.c
 * @brief Platonic solid operations using Crystalline Abacus
 */

#include "math/polytope_abacus.h"
#include "math/polytope.h"
#include "math/abacus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// ALLOCATION AND DEALLOCATION
// ============================================================================

PlatonicSolidAbacus* platonic_abacus_alloc(uint32_t base, int32_t precision) {
    PlatonicSolidAbacus* solid = (PlatonicSolidAbacus*)calloc(1, sizeof(PlatonicSolidAbacus));
    if (!solid) {
        return NULL;
    }
    
    // Set Abacus parameters
    solid->abacus_base = base;
    solid->abacus_precision = precision;
    
    // Initialize geometric properties with Abacus
    solid->edge_length = abacus_from_double(1.0, base, precision);
    solid->circumradius = abacus_from_double(0.0, base, precision);
    solid->inradius = abacus_from_double(0.0, base, precision);
    solid->volume = abacus_from_double(0.0, base, precision);
    
    if (!solid->edge_length || !solid->circumradius || 
        !solid->inradius || !solid->volume) {
        platonic_abacus_free(solid);
        return NULL;
    }
    
    // Set defaults
    solid->num_heads = 12;  // Always 12 (12-fold symmetry)
    
    return solid;
}

void platonic_abacus_free(PlatonicSolidAbacus* solid) {
    if (!solid) {
        return;
    }
    
    // Free Schläfli symbol
    if (solid->schlafli_symbol) {
        free(solid->schlafli_symbol);
    }
    
    // Free vertex coordinates (Abacus)
    if (solid->vertex_coords) {
        for (uint64_t i = 0; i < solid->num_vertices * solid->dimension; i++) {
            if (solid->vertex_coords[i]) {
                abacus_free(solid->vertex_coords[i]);
            }
        }
        free(solid->vertex_coords);
    }
    
    // Free edge indices
    if (solid->edge_indices) {
        for (uint64_t i = 0; i < solid->num_edges; i++) {
            if (solid->edge_indices[i]) {
                free(solid->edge_indices[i]);
            }
        }
        free(solid->edge_indices);
    }
    
    // Free face indices
    if (solid->face_indices) {
        for (uint64_t i = 0; i < solid->num_faces; i++) {
            if (solid->face_indices[i]) {
                free(solid->face_indices[i]);
            }
        }
        free(solid->face_indices);
    }
    
    // Free face sizes
    if (solid->face_sizes) {
        free(solid->face_sizes);
    }
    
    // Free higher faces
    if (solid->higher_faces) {
        free(solid->higher_faces);
    }
    
    // Free geometric properties (Abacus)
    if (solid->edge_length) {
        abacus_free(solid->edge_length);
    }
    if (solid->circumradius) {
        abacus_free(solid->circumradius);
    }
    if (solid->inradius) {
        abacus_free(solid->inradius);
    }
    if (solid->volume) {
        abacus_free(solid->volume);
    }
    
    free(solid);
}

// ============================================================================
// CONVERSION FUNCTIONS
// ============================================================================

PlatonicSolidAbacus* platonic_double_to_abacus(const PlatonicSolid* solid_double,
                                                uint32_t base, int32_t precision) {
    if (!solid_double) {
        return NULL;
    }
    
    // Allocate new Abacus-based solid
    PlatonicSolidAbacus* solid = platonic_abacus_alloc(base, precision);
    if (!solid) {
        return NULL;
    }
    
    // Copy basic properties
    strncpy(solid->name, solid_double->name, sizeof(solid->name) - 1);
    solid->name[sizeof(solid->name) - 1] = '\0';  // Ensure null termination
    solid->dimension = solid_double->dimension;
    solid->num_vertices = solid_double->num_vertices;
    solid->num_edges = solid_double->num_edges;
    solid->num_faces = solid_double->num_faces;
    solid->num_cells = solid_double->num_cells;
    solid->euler_characteristic = solid_double->euler_characteristic;
    solid->symmetry_order = solid_double->symmetry_order;
    solid->has_golden_ratio = solid_double->has_golden_ratio;
    solid->is_regular = solid_double->is_regular;
    solid->is_convex = solid_double->is_convex;
    solid->embedding_dim = solid_double->embedding_dim;
    solid->hidden_dim = solid_double->hidden_dim;
    solid->num_layers = solid_double->num_layers;
    solid->num_heads = solid_double->num_heads;
    
    strncpy(solid->symmetry_group, solid_double->symmetry_group, 
            sizeof(solid->symmetry_group) - 1);
    solid->symmetry_group[sizeof(solid->symmetry_group) - 1] = '\0';  // Ensure null termination
    
    // Copy Schläfli symbol
    if (solid_double->schlafli_symbol && solid_double->symbol_length > 0) {
        solid->schlafli_symbol = (uint32_t*)malloc(solid_double->symbol_length * sizeof(uint32_t));
        if (!solid->schlafli_symbol) {
            platonic_abacus_free(solid);
            return NULL;
        }
        memcpy(solid->schlafli_symbol, solid_double->schlafli_symbol,
               solid_double->symbol_length * sizeof(uint32_t));
        solid->symbol_length = solid_double->symbol_length;
    }
    
    // Convert vertex coordinates to Abacus
    if (solid_double->vertex_coords && solid_double->num_vertices > 0) {
        uint64_t total_coords = solid_double->num_vertices * solid_double->dimension;
        solid->vertex_coords = (CrystallineAbacus**)calloc(total_coords, 
                                                            sizeof(CrystallineAbacus*));
        if (!solid->vertex_coords) {
            platonic_abacus_free(solid);
            return NULL;
        }
        
        for (uint64_t i = 0; i < total_coords; i++) {
            solid->vertex_coords[i] = abacus_from_double(solid_double->vertex_coords[i],
                                                          base, precision);
            if (!solid->vertex_coords[i]) {
                platonic_abacus_free(solid);
                return NULL;
            }
        }
    }
    
    // Copy edge indices
    if (solid_double->edge_indices && solid_double->num_edges > 0) {
        solid->edge_indices = (uint32_t**)calloc(solid_double->num_edges, sizeof(uint32_t*));
        if (!solid->edge_indices) {
            platonic_abacus_free(solid);
            return NULL;
        }
        
        for (uint64_t i = 0; i < solid_double->num_edges; i++) {
            if (solid_double->edge_indices[i]) {
                solid->edge_indices[i] = (uint32_t*)malloc(2 * sizeof(uint32_t));
                if (!solid->edge_indices[i]) {
                    platonic_abacus_free(solid);
                    return NULL;
                }
                solid->edge_indices[i][0] = solid_double->edge_indices[i][0];
                solid->edge_indices[i][1] = solid_double->edge_indices[i][1];
            }
        }
    }
    
    // Copy face indices and sizes
    if (solid_double->face_indices && solid_double->num_faces > 0) {
        solid->face_indices = (uint32_t**)calloc(solid_double->num_faces, sizeof(uint32_t*));
        solid->face_sizes = (uint32_t*)calloc(solid_double->num_faces, sizeof(uint32_t));
        
        if (!solid->face_indices || !solid->face_sizes) {
            platonic_abacus_free(solid);
            return NULL;
        }
        
        for (uint64_t i = 0; i < solid_double->num_faces; i++) {
            uint32_t face_size = solid_double->face_sizes[i];
            solid->face_sizes[i] = face_size;
            
            if (solid_double->face_indices[i] && face_size > 0) {
                solid->face_indices[i] = (uint32_t*)malloc(face_size * sizeof(uint32_t));
                if (!solid->face_indices[i]) {
                    platonic_abacus_free(solid);
                    return NULL;
                }
                memcpy(solid->face_indices[i], solid_double->face_indices[i],
                       face_size * sizeof(uint32_t));
            }
        }
    }
    
    // Convert geometric properties to Abacus
    abacus_free(solid->edge_length);
    abacus_free(solid->circumradius);
    abacus_free(solid->inradius);
    abacus_free(solid->volume);
    
    solid->edge_length = abacus_from_double(solid_double->edge_length, base, precision);
    solid->circumradius = abacus_from_double(solid_double->circumradius, base, precision);
    solid->inradius = abacus_from_double(solid_double->inradius, base, precision);
    solid->volume = abacus_from_double(solid_double->volume, base, precision);
    
    if (!solid->edge_length || !solid->circumradius || 
        !solid->inradius || !solid->volume) {
        platonic_abacus_free(solid);
        return NULL;
    }
    
    return solid;
}

PlatonicSolid* platonic_abacus_to_double(const PlatonicSolidAbacus* solid_abacus) {
    if (!solid_abacus) {
        return NULL;
    }
    
    // Allocate new double-based solid
    PlatonicSolid* solid = (PlatonicSolid*)calloc(1, sizeof(PlatonicSolid));
    if (!solid) {
        return NULL;
    }
    
    // Copy basic properties
    strncpy(solid->name, solid_abacus->name, sizeof(solid->name) - 1);
    solid->name[sizeof(solid->name) - 1] = '\0';  // Ensure null termination
    solid->dimension = solid_abacus->dimension;
    solid->num_vertices = solid_abacus->num_vertices;
    solid->num_edges = solid_abacus->num_edges;
    solid->num_faces = solid_abacus->num_faces;
    solid->num_cells = solid_abacus->num_cells;
    solid->euler_characteristic = solid_abacus->euler_characteristic;
    solid->symmetry_order = solid_abacus->symmetry_order;
    solid->has_golden_ratio = solid_abacus->has_golden_ratio;
    solid->is_regular = solid_abacus->is_regular;
    solid->is_convex = solid_abacus->is_convex;
    solid->embedding_dim = solid_abacus->embedding_dim;
    solid->hidden_dim = solid_abacus->hidden_dim;
    solid->num_layers = solid_abacus->num_layers;
    solid->num_heads = solid_abacus->num_heads;
    
    strncpy(solid->symmetry_group, solid_abacus->symmetry_group,
            sizeof(solid->symmetry_group) - 1);
    solid->symmetry_group[sizeof(solid->symmetry_group) - 1] = '\0';  // Ensure null termination
    
    // Copy Schläfli symbol
    if (solid_abacus->schlafli_symbol && solid_abacus->symbol_length > 0) {
        solid->schlafli_symbol = (uint32_t*)malloc(solid_abacus->symbol_length * sizeof(uint32_t));
        if (!solid->schlafli_symbol) {
            free(solid);
            return NULL;
        }
        memcpy(solid->schlafli_symbol, solid_abacus->schlafli_symbol,
               solid_abacus->symbol_length * sizeof(uint32_t));
        solid->symbol_length = solid_abacus->symbol_length;
    }
    
    // Convert vertex coordinates from Abacus to double
    if (solid_abacus->vertex_coords && solid_abacus->num_vertices > 0) {
        uint64_t total_coords = solid_abacus->num_vertices * solid_abacus->dimension;
        solid->vertex_coords = (double*)calloc(total_coords, sizeof(double));
        if (!solid->vertex_coords) {
            free(solid->schlafli_symbol);
            free(solid);
            return NULL;
        }
        
        for (uint64_t i = 0; i < total_coords; i++) {
            if (solid_abacus->vertex_coords[i]) {
                MathError err = abacus_to_double(solid_abacus->vertex_coords[i],
                                                  &solid->vertex_coords[i]);
                if (err != MATH_SUCCESS) {
                    free(solid->vertex_coords);
                    free(solid->schlafli_symbol);
                    free(solid);
                    return NULL;
                }
            }
        }
    }
    
    // Copy edge indices (same structure)
    if (solid_abacus->edge_indices && solid_abacus->num_edges > 0) {
        solid->edge_indices = (uint32_t**)calloc(solid_abacus->num_edges, sizeof(uint32_t*));
        if (!solid->edge_indices) {
            free(solid->vertex_coords);
            free(solid->schlafli_symbol);
            free(solid);
            return NULL;
        }
        
        for (uint64_t i = 0; i < solid_abacus->num_edges; i++) {
            if (solid_abacus->edge_indices[i]) {
                solid->edge_indices[i] = (uint32_t*)malloc(2 * sizeof(uint32_t));
                if (!solid->edge_indices[i]) {
                    // Cleanup
                    for (uint64_t j = 0; j < i; j++) {
                        free(solid->edge_indices[j]);
                    }
                    free(solid->edge_indices);
                    free(solid->vertex_coords);
                    free(solid->schlafli_symbol);
                    free(solid);
                    return NULL;
                }
                solid->edge_indices[i][0] = solid_abacus->edge_indices[i][0];
                solid->edge_indices[i][1] = solid_abacus->edge_indices[i][1];
            }
        }
    }
    
    // Copy face indices and sizes (same structure)
    if (solid_abacus->face_indices && solid_abacus->num_faces > 0) {
        solid->face_indices = (uint32_t**)calloc(solid_abacus->num_faces, sizeof(uint32_t*));
        solid->face_sizes = (uint32_t*)calloc(solid_abacus->num_faces, sizeof(uint32_t));
        
        if (!solid->face_indices || !solid->face_sizes) {
            // Cleanup edges
            if (solid->edge_indices) {
                for (uint64_t i = 0; i < solid->num_edges; i++) {
                    free(solid->edge_indices[i]);
                }
                free(solid->edge_indices);
            }
            free(solid->vertex_coords);
            free(solid->schlafli_symbol);
            free(solid);
            return NULL;
        }
        
        for (uint64_t i = 0; i < solid_abacus->num_faces; i++) {
            uint32_t face_size = solid_abacus->face_sizes[i];
            solid->face_sizes[i] = face_size;
            
            if (solid_abacus->face_indices[i] && face_size > 0) {
                solid->face_indices[i] = (uint32_t*)malloc(face_size * sizeof(uint32_t));
                if (!solid->face_indices[i]) {
                    // Cleanup
                    for (uint64_t j = 0; j < i; j++) {
                        free(solid->face_indices[j]);
                    }
                    free(solid->face_indices);
                    free(solid->face_sizes);
                    if (solid->edge_indices) {
                        for (uint64_t j = 0; j < solid->num_edges; j++) {
                            free(solid->edge_indices[j]);
                        }
                        free(solid->edge_indices);
                    }
                    free(solid->vertex_coords);
                    free(solid->schlafli_symbol);
                    free(solid);
                    return NULL;
                }
                memcpy(solid->face_indices[i], solid_abacus->face_indices[i],
                       face_size * sizeof(uint32_t));
            }
        }
    }
    
    // Convert geometric properties from Abacus to double
    if (solid_abacus->edge_length) {
        abacus_to_double(solid_abacus->edge_length, &solid->edge_length);
    }
    if (solid_abacus->circumradius) {
        abacus_to_double(solid_abacus->circumradius, &solid->circumradius);
    }
    if (solid_abacus->inradius) {
        abacus_to_double(solid_abacus->inradius, &solid->inradius);
    }
    if (solid_abacus->volume) {
        abacus_to_double(solid_abacus->volume, &solid->volume);
    }
    
    return solid;
}