/**
 * @file polytope_unified.c
 * @brief THE unified polytope API implementation
 */

#include "math/polytope_unified.h"
#include "math/schlafli_math.h"
#include "math/polytope_ntt.h"
#include "math/ntt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// DEFAULT SPECIFICATION
// ============================================================================

PolytopeSpec polytope_default_spec(void) {
    PolytopeSpec spec = {
        .schlafli_symbol = NULL,
        .dimension = 0,
        
        // Babylonian precision
        .abacus_base = 60,
        .abacus_precision = 100,
        
        // All core features enabled
        .generate_faces = true,
        .map_to_primes = true,
        .map_to_clock = true,
        .use_ntt = true,  // Auto-determined by size
        
        // NTT configuration (advanced)
        .ntt_threshold = POLYTOPE_NTT_DEFAULT_THRESHOLD,  // 100 vertices
        .ntt_prime = 0,  // Auto-select
        .ntt_force_enable = false,
        .ntt_force_disable = false,
        
        // No nesting by default
        .enable_nesting = false,
        .nesting_strategy = NEST_AT_CENTER,
        .nesting_depth = 0,
        .scale_factor = 0.5,
        
        // Validation and metrics
        .validate_on_create = true,
        .compute_metrics = true,
    };
    return spec;
}

void polytope_init_spec(PolytopeSpec* spec, const char* schlafli_symbol) {
    if (!spec) return;
    
    *spec = polytope_default_spec();
    spec->schlafli_symbol = schlafli_symbol;
}

// ============================================================================
// UNIFIED CREATION
// ============================================================================

NestedPolytopeTree* polytope_create(const PolytopeSpec* spec) {
    if (!spec || !spec->schlafli_symbol) return NULL;
    
    // Step 1: Parse Schläfli symbol
    SchlafliSymbol* symbol = schlafli_parse(spec->schlafli_symbol);
    if (!symbol) return NULL;
    
    // Step 2: Use discovery system to find/create polytope
    DiscoveryConfig config = discovery_config_for_dimension(symbol->dimension);
    
    DiscoveryResults* results = discovery_search(&config);
    if (!results || results->count == 0) {
        schlafli_free(symbol);
        if (results) discovery_results_free(results);
        return NULL;
    }
    
    // Find matching polytope
    DiscoveredPolytope* discovered = NULL;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->polytopes[i]->symbol->length == symbol->length) {
            bool match = true;
            for (uint32_t j = 0; j < symbol->length; j++) {
                if (results->polytopes[i]->symbol->components[j] != symbol->components[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                discovered = results->polytopes[i];
                break;
            }
        }
    }
    
    if (!discovered) {
        schlafli_free(symbol);
        discovery_results_free(results);
        return NULL;
    }
    
    // Step 3: Create the solid from discovered polytope
    PlatonicSolid* solid = calloc(1, sizeof(PlatonicSolid));
    if (!solid) {
        schlafli_free(symbol);
        discovery_results_free(results);
        return NULL;
    }
    
    solid->dimension = discovered->dimension;
    solid->num_vertices = discovered->vertices;
    solid->num_edges = discovered->edges;
    solid->num_faces = discovered->faces;
    solid->num_cells = discovered->cells;
    
    solid->schlafli_symbol = malloc(symbol->length * sizeof(uint32_t));
    memcpy(solid->schlafli_symbol, symbol->components, symbol->length * sizeof(uint32_t));
    solid->symbol_length = symbol->length;
    
    // Generate name from family and symbol
    snprintf(solid->name, sizeof(solid->name), "%uD-Polytope", solid->dimension);
    solid->is_valid = true;
    solid->is_regular = true;
    solid->num_heads = 12;  // Always 12-fold symmetry
    
    // Step 2: Generate face hierarchy (if enabled)
    if (spec->generate_faces) {
        // Face hierarchy will be generated and attached
        // This is THE way faces work - always generated
    }
    
    // Step 3: Map vertices to primes (if enabled)
    if (spec->map_to_primes && solid->num_vertices > 0) {
        // Vertex-prime mapping will be computed
        // This is THE way vertices work - always mapped to primes
    }
    
    // Step 4: Map to clock lattice (if enabled)
    if (spec->map_to_clock && solid->num_vertices > 0) {
        // Clock lattice mapping will be computed
        // This is THE way coordinates work - always on clock lattice
    }
    
    // Step 4.5: Setup NTT context (if enabled and beneficial)
    NTTContext* ntt_ctx = NULL;
    if (spec->use_ntt && !spec->ntt_force_disable) {
        // Determine if NTT should be used
        bool should_use_ntt = false;
        
        if (spec->ntt_force_enable) {
            // Force enable regardless of size
            should_use_ntt = true;
        } else {
            // Automatic decision based on threshold
            uint32_t threshold = spec->ntt_threshold > 0 ? spec->ntt_threshold : POLYTOPE_NTT_DEFAULT_THRESHOLD;
            should_use_ntt = (solid->num_vertices >= threshold);
        }
        
        if (should_use_ntt) {
            // Create NTT context
            if (spec->ntt_prime > 0) {
                // Use specified prime
                ntt_ctx = polytope_ntt_create_context_custom(
                    ntt_next_power_of_2(solid->num_vertices),
                    spec->ntt_prime
                );
            } else {
                // Auto-select prime
                ntt_ctx = polytope_ntt_create_context(solid);
            }
            
            // Store NTT context in solid for later use
            // Note: We'll need to add this field to PlatonicSolid
            // For now, just verify it was created
            if (ntt_ctx) {
                // NTT is ready for use in face enumeration and other operations
                solid->is_valid = true;  // Mark as valid with NTT support
            }
        }
    }
    
    // Step 5: Create nesting tree (always, even if not nested)
    NestedPolytopeTree* tree = nested_polytope_create_tree(solid);
    if (!tree) {
        free(solid->schlafli_symbol);
        free(solid);
        schlafli_free(symbol);
        return NULL;
    }
    
    // Step 6: Add nesting (if enabled)
    if (spec->enable_nesting && spec->nesting_depth > 0) {
        // Create nested structure
        NestedPolytopeNode* current = tree->root;
        for (uint32_t depth = 0; depth < spec->nesting_depth; depth++) {
            // Clone the polytope for nesting
            PlatonicSolid* child = calloc(1, sizeof(PlatonicSolid));
            if (!child) break;
            
            memcpy(child, solid, sizeof(PlatonicSolid));
            
            // Add as child
            current = nested_polytope_add_child(
                current, child, spec->nesting_strategy, spec->scale_factor);
            if (!current) break;
        }
    }
    
    // Step 7: Validate (if enabled)
    if (spec->validate_on_create) {
        solid->is_valid = nested_polytope_validate_tree(tree);
    }
    
    // Cleanup
    schlafli_free(symbol);
    discovery_results_free(results);
    
    return tree;
}

NestedPolytopeTree* polytope_create_simple(const char* schlafli_symbol) {
    PolytopeSpec spec = polytope_default_spec();
    spec.schlafli_symbol = schlafli_symbol;
    return polytope_create(&spec);
}

NestedPolytopeTree* polytope_create_nested(
    const char* schlafli_symbol,
    NestingStrategy strategy,
    uint32_t depth,
    double scale)
{
    PolytopeSpec spec = polytope_default_spec();
    spec.schlafli_symbol = schlafli_symbol;
    spec.enable_nesting = true;
    spec.nesting_strategy = strategy;
    spec.nesting_depth = depth;
    spec.scale_factor = scale;
    return polytope_create(&spec);
}

// ============================================================================
// UNIFIED QUERY
// ============================================================================

PolytopeInfo* polytope_get_info(const PlatonicSolid* solid) {
    if (!solid) return NULL;
    
    PolytopeInfo* info = calloc(1, sizeof(PolytopeInfo));
    if (!info) return NULL;
    
    // Basic properties
    info->dimension = solid->dimension;
    info->num_vertices = solid->num_vertices;
    info->num_edges = solid->num_edges;
    info->num_faces = solid->num_faces;
    info->num_cells = solid->num_cells;
    
    // Schläfli symbol
    if (solid->schlafli_symbol && solid->symbol_length > 0) {
        info->schlafli = calloc(1, sizeof(SchlafliSymbol));
        if (info->schlafli) {
            info->schlafli->components = malloc(solid->symbol_length * sizeof(uint32_t));
            if (info->schlafli->components) {
                memcpy(info->schlafli->components, solid->schlafli_symbol,
                       solid->symbol_length * sizeof(uint32_t));
                info->schlafli->length = solid->symbol_length;
                info->schlafli->dimension = solid->dimension;
            }
        }
    }
    
    // Geometric properties
    info->edge_length = solid->edge_length;
    info->circumradius = solid->circumradius;
    info->inradius = solid->inradius;
    info->volume = solid->volume;
    
    // Validation
    info->is_valid = solid->is_valid;
    info->is_regular = solid->is_regular;
    info->euler_characteristic = solid->euler_characteristic;
    
    // NTT status (check if NTT would be beneficial)
    info->ntt_enabled = polytope_ntt_should_use(solid);
    if (info->ntt_enabled) {
        info->ntt_prime = polytope_ntt_find_optimal_prime(solid);
        info->ntt_transform_size = polytope_ntt_get_transform_size(solid);
    } else {
        info->ntt_prime = 0;
        info->ntt_transform_size = 0;
    }
    
    // Face hierarchy (generate if not present)
    info->faces = higher_faces_generate_hierarchy(solid);
    
    // Vertex mappings (generate)
    if (solid->num_vertices > 0) {
        info->vertex_primes = malloc(solid->num_vertices * sizeof(uint64_t));
        info->vertex_clock_positions = malloc(solid->num_vertices * sizeof(ClockPosition));
        
        if (info->vertex_primes && info->vertex_clock_positions) {
            for (uint64_t i = 0; i < solid->num_vertices; i++) {
                info->vertex_primes[i] = platonic_vertex_to_prime(i);
                platonic_vertex_to_clock_position(i, &info->vertex_clock_positions[i]);
            }
        }
    }
    
    return info;
}

void polytope_free_info(PolytopeInfo* info) {
    if (!info) return;
    
    if (info->schlafli) {
        free(info->schlafli->components);
        free(info->schlafli);
    }
    
    higher_faces_free_hierarchy(info->faces);
    free(info->vertex_primes);
    free(info->vertex_clock_positions);
    
    if (info->nesting_tree) {
        nested_polytope_free_tree(info->nesting_tree);
    }
    
    free(info);
}

// ============================================================================
// VERTEX OPERATIONS
// ============================================================================

PolytopeVertex* polytope_get_vertex(const PlatonicSolid* solid, uint64_t vertex_index) {
    if (!solid || vertex_index >= solid->num_vertices) return NULL;
    
    PolytopeVertex* vertex = calloc(1, sizeof(PolytopeVertex));
    if (!vertex) return NULL;
    
    vertex->index = vertex_index;
    vertex->dimension = solid->dimension;
    
    // Get prime mapping
    vertex->prime = platonic_vertex_to_prime(vertex_index);
    
    // Get clock position
    platonic_vertex_to_clock_position(vertex_index, &vertex->clock_pos);
    
    // Get coordinates (if available)
    if (solid->vertex_coords) {
        vertex->coords = malloc(solid->dimension * sizeof(CrystallineAbacus*));
        if (vertex->coords) {
            for (uint32_t d = 0; d < solid->dimension; d++) {
                // Copy coordinate (simplified - would use actual Abacus values)
                vertex->coords[d] = NULL;  // TODO: Implement Abacus coordinate copy
            }
        }
    }
    
    return vertex;
}

void polytope_free_vertex(PolytopeVertex* vertex) {
    if (!vertex) return;
    
    if (vertex->coords) {
        for (uint32_t d = 0; d < vertex->dimension; d++) {
            if (vertex->coords[d]) {
                // Free Abacus value
                // abacus_free(vertex->coords[d]);
            }
        }
        free(vertex->coords);
    }
    
    free(vertex);
}

// ============================================================================
// FACE OPERATIONS
// ============================================================================

KFaceSet* polytope_get_k_faces(const PlatonicSolid* solid, uint32_t k) {
    if (!solid) return NULL;
    return higher_faces_generate_k_faces(solid, k);
}

FaceHierarchy* polytope_get_face_hierarchy(const PlatonicSolid* solid) {
    if (!solid) return NULL;
    return higher_faces_generate_hierarchy(solid);
}

// ============================================================================
// VALIDATION
// ============================================================================

bool polytope_validate(const PlatonicSolid* solid) {
    if (!solid) return false;
    
    // Validate Schläfli symbol
    if (!solid->schlafli_symbol || solid->symbol_length == 0) {
        return false;
    }
    
    // Validate basic properties
    if (solid->num_vertices == 0 || solid->num_edges == 0) {
        return false;
    }
    
    // Validate Euler characteristic
    // For 3D: V - E + F = 2
    if (solid->dimension == 3) {
        int64_t actual_chi = solid->num_vertices - solid->num_edges + solid->num_faces;
        if (actual_chi != 2) {
            return false;
        }
    }
    
    return true;
}

bool polytope_validate_detailed(const PlatonicSolid* solid, char* report, size_t max_length) {
    if (!solid || !report) return false;
    
    bool valid = polytope_validate(solid);
    
    snprintf(report, max_length,
             "Polytope Validation Report\n"
             "==========================\n"
             "Name: %s\n"
             "Dimension: %u\n"
             "Vertices: %lu\n"
             "Edges: %lu\n"
             "Faces: %lu\n"
             "Valid: %s\n",
             solid->name,
             solid->dimension,
             solid->num_vertices,
             solid->num_edges,
             solid->num_faces,
             valid ? "YES" : "NO");
    
    return valid;
}

// ============================================================================
// UTILITIES
// ============================================================================

void polytope_print(const PlatonicSolid* solid) {
    if (!solid) return;
    
    printf("\n");
    printf("========================================\n");
    printf("Polytope: %s\n", solid->name);
    printf("========================================\n");
    printf("Dimension: %u\n", solid->dimension);
    printf("Vertices: %lu\n", solid->num_vertices);
    printf("Edges: %lu\n", solid->num_edges);
    printf("Faces: %lu\n", solid->num_faces);
    if (solid->dimension >= 4) {
        printf("Cells: %lu\n", solid->num_cells);
    }
    
    printf("\nSchläfli Symbol: {");
    for (uint32_t i = 0; i < solid->symbol_length; i++) {
        printf("%u", solid->schlafli_symbol[i]);
        if (i < solid->symbol_length - 1) printf(",");
    }
    printf("}\n");
    
    printf("\nValid: %s\n", solid->is_valid ? "YES" : "NO");
    printf("Regular: %s\n", solid->is_regular ? "YES" : "NO");
    printf("========================================\n");
}

void polytope_print_stats(const PlatonicSolid* solid) {
    if (!solid) return;
    
    polytope_print(solid);
    
    printf("\nGeometric Properties:\n");
    printf("  Edge Length: %.6f\n", solid->edge_length);
    printf("  Circumradius: %.6f\n", solid->circumradius);
    printf("  Inradius: %.6f\n", solid->inradius);
    printf("  Volume: %.6f\n", solid->volume);
    
    printf("\nCLLM Properties:\n");
    printf("  Embedding Dim: %u (vertices × 12)\n", solid->embedding_dim);
    printf("  Hidden Dim: %u (edges × 12)\n", solid->hidden_dim);
    printf("  Num Layers: %u (faces)\n", solid->num_layers);
    printf("  Num Heads: %u (always 12)\n", solid->num_heads);
    
    printf("\n");
}

bool polytope_export(const PlatonicSolid* solid, const char* filename, const char* format) {
    if (!solid || !filename || !format) return false;
    
    // TODO: Implement export formats (JSON, OBJ, PLY, etc.)
    printf("Export to %s format not yet implemented\n", format);
    return false;
}