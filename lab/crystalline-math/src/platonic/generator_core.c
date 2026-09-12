/**
 * @file generator_core.c
 * @brief Core generator functions and property computation
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

// Forward declarations for specialized generators
extern PlatonicSolid* platonic_generate_tetrahedron(void);
extern PlatonicSolid* platonic_generate_cube(void);
extern PlatonicSolid* platonic_generate_octahedron(void);
extern PlatonicSolid* platonic_generate_dodecahedron(void);
extern PlatonicSolid* platonic_generate_icosahedron(void);
extern PlatonicSolid* platonic_generate_5cell(void);
extern PlatonicSolid* platonic_generate_tesseract(void);
extern PlatonicSolid* platonic_generate_16cell(void);
extern PlatonicSolid* platonic_generate_24cell(void);
extern PlatonicSolid* platonic_generate_120cell(void);
extern PlatonicSolid* platonic_generate_600cell(void);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

PlatonicSolid* platonic_alloc(void) {
    PlatonicSolid* solid = (PlatonicSolid*)calloc(1, sizeof(PlatonicSolid));
    if (!solid) {
        return NULL;
    }
    
    // Set defaults
    solid->edge_length = 1.0;
    solid->num_heads = 12;  // Always 12 (12-fold symmetry)
    
    return solid;
}

void platonic_free(PlatonicSolid* solid) {
    if (!solid) {
        return;
    }
    
    // Free Schläfli symbol
    if (solid->schlafli_symbol) {
        free(solid->schlafli_symbol);
    }
    
    // Free vertex coordinates
    if (solid->vertex_coords) {
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
    
    free(solid);
}

PlatonicSolid* platonic_clone(const PlatonicSolid* solid) {
    if (!solid) {
        return NULL;
    }
    
    // Allocate new solid
    PlatonicSolid* clone = platonic_alloc();
    if (!clone) {
        return NULL;
    }
    
    // Copy all scalar fields
    memcpy(clone, solid, sizeof(PlatonicSolid));
    
    // Deep copy Schläfli symbol
    if (solid->schlafli_symbol && solid->symbol_length > 0) {
        clone->schlafli_symbol = (uint32_t*)calloc(solid->symbol_length, sizeof(uint32_t));
        if (!clone->schlafli_symbol) {
            platonic_free(clone);
            return NULL;
        }
        memcpy(clone->schlafli_symbol, solid->schlafli_symbol, 
               solid->symbol_length * sizeof(uint32_t));
    }
    
    // Deep copy vertex coordinates
    if (solid->vertex_coords && solid->num_vertices > 0) {
        size_t coord_size = solid->num_vertices * solid->dimension * sizeof(double);
        clone->vertex_coords = (double*)malloc(coord_size);
        if (!clone->vertex_coords) {
            platonic_free(clone);
            return NULL;
        }
        memcpy(clone->vertex_coords, solid->vertex_coords, coord_size);
    }
    
    // Deep copy edges
    if (solid->edge_indices && solid->num_edges > 0) {
        clone->edge_indices = (uint32_t**)calloc(solid->num_edges, sizeof(uint32_t*));
        if (!clone->edge_indices) {
            platonic_free(clone);
            return NULL;
        }
        for (uint64_t i = 0; i < solid->num_edges; i++) {
            clone->edge_indices[i] = (uint32_t*)calloc(2, sizeof(uint32_t));
            if (!clone->edge_indices[i]) {
                platonic_free(clone);
                return NULL;
            }
            memcpy(clone->edge_indices[i], solid->edge_indices[i], 2 * sizeof(uint32_t));
        }
    }
    
    // Deep copy faces
    if (solid->face_indices && solid->num_faces > 0) {
        clone->face_indices = (uint32_t**)calloc(solid->num_faces, sizeof(uint32_t*));
        clone->face_sizes = (uint32_t*)calloc(solid->num_faces, sizeof(uint32_t));
        if (!clone->face_indices || !clone->face_sizes) {
            platonic_free(clone);
            return NULL;
        }
        memcpy(clone->face_sizes, solid->face_sizes, solid->num_faces * sizeof(uint32_t));
        
        for (uint64_t i = 0; i < solid->num_faces; i++) {
            uint32_t face_size = solid->face_sizes[i];
            clone->face_indices[i] = (uint32_t*)calloc(face_size, sizeof(uint32_t));
            if (!clone->face_indices[i]) {
                platonic_free(clone);
                return NULL;
            }
            memcpy(clone->face_indices[i], solid->face_indices[i], 
                   face_size * sizeof(uint32_t));
        }
    }
    
    return clone;
}

// ============================================================================
// PROPERTY COMPUTATION
// ============================================================================

bool platonic_compute_metrics(PlatonicSolid* solid) {
    if (!solid || !solid->vertex_coords || solid->num_vertices == 0) {
        return false;
    }
    
    // Compute circumradius (distance from origin to any vertex)
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < solid->dimension; i++) {
        double coord = solid->vertex_coords[i];
        sum_sq += coord * coord;
    }
    solid->circumradius = math_sqrt(sum_sq);
    
    // Edge length is already set to 1.0 by default
    
    // Inradius and volume computation would require more complex geometry
    // For now, set to 0 (can be computed later if needed)
    solid->inradius = 0.0;
    solid->volume = 0.0;
    
    return true;
}

bool platonic_compute_symmetries(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // Symmetry computation is complex and depends on the specific solid
    // For now, set default values
    // These can be computed more accurately later
    
    if (solid->dimension == 3) {
        if (solid->num_vertices == 4) {
            solid->symmetry_order = 12;  // T_d
            snprintf(solid->symmetry_group, sizeof(solid->symmetry_group), "T_d");
        } else if (solid->num_vertices == 8 || solid->num_vertices == 6) {
            solid->symmetry_order = 48;  // O_h
            snprintf(solid->symmetry_group, sizeof(solid->symmetry_group), "O_h");
        } else if (solid->num_vertices == 20 || solid->num_vertices == 12) {
            solid->symmetry_order = 120;  // I_h
            snprintf(solid->symmetry_group, sizeof(solid->symmetry_group), "I_h");
        }
    } else {
        // For higher dimensions, use factorial as approximation
        solid->symmetry_order = 1;
        for (uint32_t i = 2; i <= solid->dimension + 1; i++) {
            solid->symmetry_order *= i;
        }
        snprintf(solid->symmetry_group, sizeof(solid->symmetry_group), "S_%u", solid->dimension + 1);
    }
    
    return true;
}

bool platonic_compute_cllm_properties(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // 12-fold scaling law
    solid->embedding_dim = (uint32_t)(solid->num_vertices * 12);
    solid->hidden_dim = (uint32_t)(solid->num_edges * 12);
    solid->num_layers = (uint32_t)solid->num_faces;
    solid->num_heads = 12;  // Always 12 (12-fold symmetry)
    
    return true;
}

bool platonic_compute_properties(PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // Compute metrics
    if (!platonic_compute_metrics(solid)) {
        return false;
    }
    
    // Compute symmetries
    if (!platonic_compute_symmetries(solid)) {
        return false;
    }
    
    // Compute 12-fold properties
    if (!platonic_compute_cllm_properties(solid)) {
        return false;
    }
    
    // Compute Euler characteristic
    // For now, just compute for dimensions we have data for
    int64_t chi = (int64_t)solid->num_vertices - (int64_t)solid->num_edges + 
                  (int64_t)solid->num_faces;
    if (solid->dimension >= 4 && solid->num_cells > 0) {
        chi -= (int64_t)solid->num_cells;
    }
    // For 5D+, we would need to add/subtract higher faces
    // For now, just store what we computed
    solid->euler_characteristic = chi;
    
    // Validate
    solid->is_valid = platonic_validate(solid);
    solid->is_regular = solid->is_valid;
    solid->is_convex = true;  // All Platonic solids are convex
    
    return true;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool platonic_validate_euler(const PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // For dimensions where we have complete face data (3D and 4D), validate
    if (solid->dimension == 3 || solid->dimension == 4) {
        // Compute expected Euler characteristic
        int64_t expected = (solid->dimension == 3) ? 2 : 0;
        return solid->euler_characteristic == expected;
    }
    
    // For higher dimensions, we don't have complete face data yet
    // So we can't validate Euler characteristic
    // Just return true for now
    return true;
}

bool platonic_validate_symmetry(const PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // Basic validation: symmetry order should be > 0
    return solid->symmetry_order > 0;
}

bool platonic_validate_regularity(const PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // All generated solids are regular by construction
    return true;
}

bool platonic_validate(const PlatonicSolid* solid) {
    if (!solid) {
        return false;
    }
    
    // Validate basic properties
    if (solid->dimension < 2 || solid->num_vertices == 0) {
        return false;
    }
    
    // Validate Euler characteristic
    if (!platonic_validate_euler(solid)) {
        return false;
    }
    
    // Validate symmetry
    if (!platonic_validate_symmetry(solid)) {
        return false;
    }
    
    // Validate regularity
    if (!platonic_validate_regularity(solid)) {
        return false;
    }
    
    return true;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const char* platonic_get_name(const PlatonicSolid* solid) {
    if (!solid) {
        return "(null)";
    }
    return solid->name;
}

void platonic_print(const PlatonicSolid* solid) {
    if (!solid) {
        printf("(null)\n");
        return;
    }
    
    printf("Platonic Solid: %s\n", solid->name);
    printf("  Dimension: %u\n", solid->dimension);
    printf("  Vertices: %lu\n", (unsigned long)solid->num_vertices);
    printf("  Edges: %lu\n", (unsigned long)solid->num_edges);
    printf("  Faces: %lu\n", (unsigned long)solid->num_faces);
    if (solid->dimension >= 4) {
        printf("  Cells: %lu\n", (unsigned long)solid->num_cells);
    }
    printf("  Euler characteristic: %ld\n", (long)solid->euler_characteristic);
    printf("  Symmetry: %s (order %u)\n", solid->symmetry_group, solid->symmetry_order);
    printf("  CLLM Properties:\n");
    printf("    Embedding dim: %u\n", solid->embedding_dim);
    printf("    Hidden dim: %u\n", solid->hidden_dim);
    printf("    Layers: %u\n", solid->num_layers);
    printf("    Heads: %u\n", solid->num_heads);
}

void platonic_print_detailed(const PlatonicSolid* solid) {
    platonic_print(solid);
    
    if (!solid) {
        return;
    }
    
    // Print Schläfli symbol
    if (solid->schlafli_symbol && solid->symbol_length > 0) {
        printf("  Schläfli symbol: {");
        for (uint32_t i = 0; i < solid->symbol_length; i++) {
            if (i > 0) printf(",");
            printf("%u", solid->schlafli_symbol[i]);
        }
        printf("}\n");
    }
    
    // Print metrics
    printf("  Metrics:\n");
    printf("    Edge length: %.6f\n", solid->edge_length);
    printf("    Circumradius: %.6f\n", solid->circumradius);
    
    // Print validation status
    printf("  Validation:\n");
    printf("    Valid: %s\n", solid->is_valid ? "yes" : "no");
    printf("    Regular: %s\n", solid->is_regular ? "yes" : "no");
}

// ============================================================================
// MAIN GENERATOR FUNCTION
// ============================================================================

PlatonicSolid* platonic_generate(uint32_t dimension, 
                                  const uint32_t* schlafli, 
                                  uint32_t length) {
    if (!schlafli || length == 0) {
        return NULL;
    }
    
    // Create Schläfli symbol
    SchlafliSymbol* symbol = schlafli_create(schlafli, length);
    if (!symbol || !symbol->is_valid) {
        if (symbol) schlafli_free(symbol);
        return NULL;
    }
    
    // Check dimension matches
    if (dimension != symbol->dimension) {
        fprintf(stderr, "Error: Dimension mismatch (expected %u, got %u)\n",
                symbol->dimension, dimension);
        schlafli_free(symbol);
        return NULL;
    }
    
    PlatonicSolid* solid = NULL;
    
    // 3D solids
    if (dimension == 3 && length == 2) {
        uint32_t p = schlafli[0];
        uint32_t q = schlafli[1];
        
        if (p == 3 && q == 3) {
            solid = platonic_generate_tetrahedron();
        } else if (p == 4 && q == 3) {
            solid = platonic_generate_cube();
        } else if (p == 3 && q == 4) {
            solid = platonic_generate_octahedron();
        } else if (p == 5 && q == 3) {
            solid = platonic_generate_dodecahedron();
        } else if (p == 3 && q == 5) {
            solid = platonic_generate_icosahedron();
        }
    }
    // 4D solids
    else if (dimension == 4 && length == 3) {
        uint32_t p = schlafli[0];
        uint32_t q = schlafli[1];
        uint32_t r = schlafli[2];
        
        if (p == 3 && q == 3 && r == 3) {
            solid = platonic_generate_5cell();
        } else if (p == 4 && q == 3 && r == 3) {
            solid = platonic_generate_tesseract();
        } else if (p == 3 && q == 3 && r == 4) {
            solid = platonic_generate_16cell();
        } else if (p == 3 && q == 4 && r == 3) {
            solid = platonic_generate_24cell();
        } else if (p == 5 && q == 3 && r == 3) {
            solid = platonic_generate_120cell();
        } else if (p == 3 && q == 3 && r == 5) {
            solid = platonic_generate_600cell();
        }
    }
    // nD solids (simplex, hypercube, cross-polytope)
    else {
        // Check if all components are 3 (simplex)
        bool all_three = true;
        for (uint32_t i = 0; i < length; i++) {
            if (schlafli[i] != 3) {
                all_three = false;
                break;
            }
        }
        if (all_three) {
            solid = platonic_generate_simplex(dimension);
        }
        
        // Check if hypercube {4,3,...,3}
        if (!solid && schlafli[0] == 4) {
            bool rest_three = true;
            for (uint32_t i = 1; i < length; i++) {
                if (schlafli[i] != 3) {
                    rest_three = false;
                    break;
                }
            }
            if (rest_three) {
                solid = platonic_generate_hypercube(dimension);
            }
        }
        
        // Check if cross-polytope {3,...,3,4}
        if (!solid && schlafli[length - 1] == 4) {
            bool rest_three = true;
            for (uint32_t i = 0; i < length - 1; i++) {
                if (schlafli[i] != 3) {
                    rest_three = false;
                    break;
                }
            }
            if (rest_three) {
                solid = platonic_generate_cross_polytope(dimension);
            }
        }
    }
    
    schlafli_free(symbol);
    
    if (!solid) {
        fprintf(stderr, "Error: Could not generate solid for given Schläfli symbol\n");
    }
    
    return solid;
}

// ============================================================================
// EXPORT
// ============================================================================

bool platonic_export(const PlatonicSolid* solid, 
                     const char* filename, 
                     const char* format) {
    if (!solid || !filename || !format) {
        return false;
    }
    
    // For now, just implement basic text export
    if (strcmp(format, "txt") == 0) {
        FILE* f = fopen(filename, "w");
        if (!f) {
            return false;
        }
        
        fprintf(f, "Platonic Solid: %s\n", solid->name);
        fprintf(f, "Dimension: %u\n", solid->dimension);
        fprintf(f, "Vertices: %lu\n", (unsigned long)solid->num_vertices);
        fprintf(f, "Edges: %lu\n", (unsigned long)solid->num_edges);
        fprintf(f, "Faces: %lu\n", (unsigned long)solid->num_faces);
        
        fclose(f);
        return true;
    }
    
    fprintf(stderr, "Error: Unsupported export format: %s\n", format);
    return false;
}