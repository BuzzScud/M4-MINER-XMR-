/**
 * @file polytope_discovery.c
 * @brief Implementation of systematic polytope discovery
 */

#include "math/polytope_discovery.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

DiscoveryConfig discovery_config_default(void) {
    DiscoveryConfig config;
    config.min_dimension = 3;
    config.max_dimension = 5;
    config.min_component = 3;
    config.max_component = 6;
    config.search_simplices = true;
    config.search_hypercubes = true;
    config.search_cross_polytopes = true;
    config.search_exceptional = true;
    config.max_candidates = 0;  // Unlimited
    return config;
}

DiscoveryConfig discovery_config_for_dimension(uint32_t dimension) {
    DiscoveryConfig config;
    config.min_dimension = dimension;
    config.max_dimension = dimension;
    config.min_component = 3;
    config.max_component = 10;
    config.search_simplices = true;
    config.search_hypercubes = true;
    config.search_cross_polytopes = true;
    config.search_exceptional = true;
    config.max_candidates = 0;
    return config;
}

// ============================================================================
// CANDIDATE GENERATION
// ============================================================================

/**
 * @brief Generate all combinations of components for given length
 */
static void generate_combinations_recursive(uint32_t* components,
                                             uint32_t length,
                                             uint32_t current_pos,
                                             uint32_t min_val,
                                             uint32_t max_val,
                                             SchlafliSymbol*** results,
                                             uint32_t* count,
                                             uint32_t* capacity) {
    if (current_pos == length) {
        // Create symbol from components
        SchlafliSymbol* symbol = schlafli_create(components, length);
        if (symbol) {
            // Expand array if needed
            if (*count >= *capacity) {
                *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
                *results = (SchlafliSymbol**)realloc(*results, 
                                                      *capacity * sizeof(SchlafliSymbol*));
            }
            (*results)[*count] = symbol;
            (*count)++;
        }
        return;
    }
    
    // Try each value for current position
    for (uint32_t val = min_val; val <= max_val; val++) {
        components[current_pos] = val;
        generate_combinations_recursive(components, length, current_pos + 1,
                                        min_val, max_val, results, count, capacity);
    }
}

SchlafliSymbol** discovery_generate_candidates(uint32_t dimension,
                                                uint32_t min_component,
                                                uint32_t max_component,
                                                uint32_t* count) {
    if (dimension < 2 || !count) {
        return NULL;
    }
    
    // For dimension d, Schläfli symbol has length d-1
    // But we only want d >= 3 for polytopes (not polygons)
    if (dimension < 3) {
        *count = 0;
        return NULL;
    }
    
    uint32_t length = dimension - 1;  // Schläfli symbol length for d-dimensional polytope
    uint32_t* components = (uint32_t*)malloc(length * sizeof(uint32_t));
    if (!components) {
        return NULL;
    }
    
    SchlafliSymbol** results = NULL;
    uint32_t capacity = 0;
    *count = 0;
    
    generate_combinations_recursive(components, length, 0,
                                    min_component, max_component,
                                    &results, count, &capacity);
    
    free(components);
    return results;
}

/**
 * @brief Check if 4D Schläfli symbol is one of the 6 known regular polychora
 */
static bool is_valid_4d_polychoron(uint32_t p, uint32_t q, uint32_t r) {
    // The 6 regular 4D polychora
    if (p == 3 && q == 3 && r == 3) return true;  // 5-cell
    if (p == 4 && q == 3 && r == 3) return true;  // Tesseract
    if (p == 3 && q == 3 && r == 4) return true;  // 16-cell
    if (p == 3 && q == 4 && r == 3) return true;  // 24-cell
    if (p == 5 && q == 3 && r == 3) return true;  // 120-cell
    if (p == 3 && q == 3 && r == 5) return true;  // 600-cell
    return false;
}

/**
 * @brief Check if nD Schläfli symbol is one of the 3 infinite families
 */
static bool is_infinite_family(const SchlafliSymbol* symbol) {
    if (!symbol || symbol->length == 0) return false;
    
    // Simplex {3,3,...,3}
    bool is_simplex = true;
    for (uint32_t i = 0; i < symbol->length; i++) {
        if (symbol->components[i] != 3) {
            is_simplex = false;
            break;
        }
    }
    if (is_simplex) return true;
    
    // Hypercube {4,3,...,3}
    if (symbol->components[0] == 4) {
        bool is_hypercube = true;
        for (uint32_t i = 1; i < symbol->length; i++) {
            if (symbol->components[i] != 3) {
                is_hypercube = false;
                break;
            }
        }
        if (is_hypercube) return true;
    }
    
    // Cross-polytope {3,3,...,4}
    if (symbol->length > 0 && symbol->components[symbol->length - 1] == 4) {
        bool is_cross = true;
        for (uint32_t i = 0; i < symbol->length - 1; i++) {
            if (symbol->components[i] != 3) {
                is_cross = false;
                break;
            }
        }
        if (is_cross) return true;
    }
    
    return false;
}

uint32_t discovery_filter_candidates(SchlafliSymbol** candidates, uint32_t count) {
    if (!candidates || count == 0) {
        return 0;
    }
    
    uint32_t write_pos = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        SchlafliSymbol* symbol = candidates[i];
        if (!symbol) continue;
        
        bool keep = false;
        
        // 3D: Use angle sum constraint (p-2)(q-2) < 4
        if (symbol->length == 2) {
            uint32_t p = symbol->components[0];
            uint32_t q = symbol->components[1];
            keep = ((p - 2) * (q - 2) < 4);
        }
        // 4D: Only the 6 known regular polychora
        else if (symbol->length == 3) {
            uint32_t p = symbol->components[0];
            uint32_t q = symbol->components[1];
            uint32_t r = symbol->components[2];
            keep = is_valid_4d_polychoron(p, q, r);
        }
        // 5D+: Only the 3 infinite families
        else if (symbol->length >= 4) {
            keep = is_infinite_family(symbol);
        }
        
        if (!keep) {
            schlafli_free(symbol);
            continue;
        }
        
        // Keep this candidate
        if (write_pos != i) {
            candidates[write_pos] = candidates[i];
        }
        write_pos++;
    }
    
    return write_pos;
}

// ============================================================================
// CLASSIFICATION
// ============================================================================

PolytopeFamily discovery_classify_family(const SchlafliSymbol* symbol) {
    if (!symbol || symbol->length == 0) {
        return FAMILY_UNKNOWN;
    }
    
    // Check for simplex {3,3,...,3}
    bool is_simplex = true;
    for (uint32_t i = 0; i < symbol->length; i++) {
        if (symbol->components[i] != 3) {
            is_simplex = false;
            break;
        }
    }
    if (is_simplex) {
        return FAMILY_SIMPLEX;
    }
    
    // Check for hypercube {4,3,...,3}
    if (symbol->components[0] == 4) {
        bool is_hypercube = true;
        for (uint32_t i = 1; i < symbol->length; i++) {
            if (symbol->components[i] != 3) {
                is_hypercube = false;
                break;
            }
        }
        if (is_hypercube) {
            return FAMILY_HYPERCUBE;
        }
    }
    
    // Check for cross-polytope {3,3,...,4}
    if (symbol->length > 0 && symbol->components[symbol->length - 1] == 4) {
        bool is_cross = true;
        for (uint32_t i = 0; i < symbol->length - 1; i++) {
            if (symbol->components[i] != 3) {
                is_cross = false;
                break;
            }
        }
        if (is_cross) {
            return FAMILY_CROSS_POLYTOPE;
        }
    }
    
    // Check for exceptional 3D
    if (symbol->length == 2) {
        uint32_t p = symbol->components[0];
        uint32_t q = symbol->components[1];
        if ((p == 5 && q == 3) || (p == 3 && q == 5)) {
            return FAMILY_EXCEPTIONAL_3D;
        }
    }
    
    // Check for exceptional 4D
    if (symbol->length == 3) {
        uint32_t p = symbol->components[0];
        uint32_t q = symbol->components[1];
        uint32_t r = symbol->components[2];
        
        // 24-cell {3,4,3}
        if (p == 3 && q == 4 && r == 3) {
            return FAMILY_EXCEPTIONAL_4D;
        }
        
        // 120-cell {5,3,3} or 600-cell {3,3,5}
        if ((p == 5 && q == 3 && r == 3) || (p == 3 && q == 3 && r == 5)) {
            return FAMILY_EXCEPTIONAL_4D;
        }
    }
    
    return FAMILY_EXCEPTIONAL_OTHER;
}

void discovery_get_name(const SchlafliSymbol* symbol, char* name, size_t name_size) {
    if (!symbol || !name || name_size == 0) {
        return;
    }
    
    // 3D Platonic solids
    if (symbol->length == 2) {
        uint32_t p = symbol->components[0];
        uint32_t q = symbol->components[1];
        
        if (p == 3 && q == 3) {
            snprintf(name, name_size, "Tetrahedron");
            return;
        }
        if (p == 4 && q == 3) {
            snprintf(name, name_size, "Cube");
            return;
        }
        if (p == 3 && q == 4) {
            snprintf(name, name_size, "Octahedron");
            return;
        }
        if (p == 5 && q == 3) {
            snprintf(name, name_size, "Dodecahedron");
            return;
        }
        if (p == 3 && q == 5) {
            snprintf(name, name_size, "Icosahedron");
            return;
        }
    }
    
    // 4D polychora
    if (symbol->length == 3) {
        uint32_t p = symbol->components[0];
        uint32_t q = symbol->components[1];
        uint32_t r = symbol->components[2];
        
        if (p == 3 && q == 3 && r == 3) {
            snprintf(name, name_size, "5-cell");
            return;
        }
        if (p == 4 && q == 3 && r == 3) {
            snprintf(name, name_size, "Tesseract");
            return;
        }
        if (p == 3 && q == 3 && r == 4) {
            snprintf(name, name_size, "16-cell");
            return;
        }
        if (p == 3 && q == 4 && r == 3) {
            snprintf(name, name_size, "24-cell");
            return;
        }
        if (p == 5 && q == 3 && r == 3) {
            snprintf(name, name_size, "120-cell");
            return;
        }
        if (p == 3 && q == 3 && r == 5) {
            snprintf(name, name_size, "600-cell");
            return;
        }
    }
    
    // Generic name based on family
    PolytopeFamily family = discovery_classify_family(symbol);
    switch (family) {
        case FAMILY_SIMPLEX:
            snprintf(name, name_size, "%uD-simplex", symbol->dimension);
            break;
        case FAMILY_HYPERCUBE:
            snprintf(name, name_size, "%uD-hypercube", symbol->dimension);
            break;
        case FAMILY_CROSS_POLYTOPE:
            snprintf(name, name_size, "%uD-cross-polytope", symbol->dimension);
            break;
        default:
            snprintf(name, name_size, "Unknown polytope");
            break;
    }
}

bool discovery_is_self_dual(const SchlafliSymbol* symbol) {
    if (!symbol || symbol->length == 0) {
        return false;
    }
    
    // Simplex is always self-dual
    bool is_simplex = true;
    for (uint32_t i = 0; i < symbol->length; i++) {
        if (symbol->components[i] != 3) {
            is_simplex = false;
            break;
        }
    }
    if (is_simplex) {
        return true;
    }
    
    // Check if symbol is palindrome (dual has reversed components)
    for (uint32_t i = 0; i < symbol->length / 2; i++) {
        if (symbol->components[i] != symbol->components[symbol->length - 1 - i]) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// DISCOVERY
// ============================================================================

DiscoveryResults* discovery_search(const DiscoveryConfig* config) {
    if (!config) {
        return NULL;
    }
    
    DiscoveryResults* results = discovery_results_create();
    if (!results) {
        return NULL;
    }
    
    clock_t start_time = clock();
    
    // Search each dimension
    for (uint32_t dim = config->min_dimension; dim <= config->max_dimension; dim++) {
        // Generate candidates
        uint32_t candidate_count = 0;
        SchlafliSymbol** candidates = discovery_generate_candidates(dim,
                                                                     config->min_component,
                                                                     config->max_component,
                                                                     &candidate_count);
        
        if (!candidates) continue;
        
        // Filter candidates
        candidate_count = discovery_filter_candidates(candidates, candidate_count);
        
        // Validate each candidate
        for (uint32_t i = 0; i < candidate_count; i++) {
            SchlafliSymbol* symbol = candidates[i];
            results->candidates_tested++;
            
            // Check if we've hit the limit
            if (config->max_candidates > 0 && 
                results->candidates_tested >= config->max_candidates) {
                schlafli_free(symbol);
                break;
            }
            
            // Validate using enhanced validation
            if (schlafli_is_valid_regular_polytope(symbol)) {
                results->candidates_valid++;
                
                // Create discovered polytope
                DiscoveredPolytope* polytope = (DiscoveredPolytope*)malloc(sizeof(DiscoveredPolytope));
                if (polytope) {
                    polytope->symbol = symbol;
                    polytope->family = discovery_classify_family(symbol);
                    polytope->dimension = symbol->dimension;
                    
                    // Compute properties
                    polytope->vertices = schlafli_calculate_vertices_general(symbol);
                    polytope->edges = schlafli_calculate_edges_general(symbol);
                    polytope->faces = schlafli_calculate_faces_general(symbol);
                    polytope->cells = (dim >= 4) ? schlafli_calculate_cells_general(symbol) : 0;
                    
                    polytope->f_vector = schlafli_calculate_f_vector(symbol);
                    polytope->is_self_dual = discovery_is_self_dual(symbol);
                    polytope->is_regular = true;
                    
                    discovery_get_name(symbol, polytope->name, sizeof(polytope->name));
                    
                    discovery_results_add(results, polytope);
                }
            } else {
                results->candidates_invalid++;
                schlafli_free(symbol);
            }
        }
        
        free(candidates);
        
        if (config->max_candidates > 0 && 
            results->candidates_tested >= config->max_candidates) {
            break;
        }
    }
    
    clock_t end_time = clock();
    results->search_time_seconds = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    return results;
}

DiscoveryResults* discovery_search_dimension(uint32_t dimension) {
    DiscoveryConfig config = discovery_config_for_dimension(dimension);
    return discovery_search(&config);
}

bool discovery_verify_known_polytopes(void) {
    // Search 3D and 4D
    DiscoveryConfig config = discovery_config_default();
    config.max_dimension = 4;
    
    DiscoveryResults* results = discovery_search(&config);
    if (!results) {
        return false;
    }
    
    // Expected counts:
    // 3D: 5 Platonic solids
    // 4D: 6 regular polychora
    uint32_t count_3d = discovery_count_by_dimension(results, 3);
    uint32_t count_4d = discovery_count_by_dimension(results, 4);
    
    bool success = (count_3d == 5 && count_4d == 6);
    
    discovery_results_free(results);
    return success;
}

// ============================================================================
// RESULTS MANAGEMENT
// ============================================================================

DiscoveryResults* discovery_results_create(void) {
    DiscoveryResults* results = (DiscoveryResults*)malloc(sizeof(DiscoveryResults));
    if (!results) {
        return NULL;
    }
    
    results->polytopes = NULL;
    results->count = 0;
    results->capacity = 0;
    results->candidates_tested = 0;
    results->candidates_valid = 0;
    results->candidates_invalid = 0;
    results->search_time_seconds = 0.0;
    
    return results;
}

void discovery_results_add(DiscoveryResults* results, DiscoveredPolytope* polytope) {
    if (!results || !polytope) {
        return;
    }
    
    // Expand array if needed
    if (results->count >= results->capacity) {
        results->capacity = (results->capacity == 0) ? 16 : (results->capacity * 2);
        results->polytopes = (DiscoveredPolytope**)realloc(results->polytopes,
                                                            results->capacity * sizeof(DiscoveredPolytope*));
    }
    
    results->polytopes[results->count] = polytope;
    results->count++;
}

void discovery_results_free(DiscoveryResults* results) {
    if (!results) {
        return;
    }
    
    if (results->polytopes) {
        for (uint32_t i = 0; i < results->count; i++) {
            DiscoveredPolytope* p = results->polytopes[i];
            if (p) {
                if (p->symbol) schlafli_free(p->symbol);
                if (p->f_vector) schlafli_free_f_vector(p->f_vector);
                free(p);
            }
        }
        free(results->polytopes);
    }
    
    free(results);
}

void discovery_results_print(const DiscoveryResults* results) {
    if (!results) {
        printf("(null)\n");
        return;
    }
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Polytope Discovery Results                                ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Polytopes Discovered: %-3u                                 ║\n", results->count);
    printf("║  Candidates Tested:    %-6u                             ║\n", results->candidates_tested);
    printf("║  Valid:                %-6u                             ║\n", results->candidates_valid);
    printf("║  Invalid:              %-6u                             ║\n", results->candidates_invalid);
    printf("║  Search Time:          %.3f seconds                       ║\n", results->search_time_seconds);
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    if (results->count == 0) {
        printf("No polytopes discovered.\n");
        return;
    }
    
    printf("Discovered Polytopes:\n\n");
    
    for (uint32_t i = 0; i < results->count; i++) {
        DiscoveredPolytope* p = results->polytopes[i];
        if (!p) continue;
        
        printf("%2u. %s ", i + 1, p->name);
        
        // Print Schläfli symbol
        char* symbol_str = schlafli_to_string(p->symbol);
        if (symbol_str) {
            printf("%s\n", symbol_str);
            free(symbol_str);
        }
        
        printf("    Dimension: %uD\n", p->dimension);
        printf("    Vertices: %lu, Edges: %lu", p->vertices, p->edges);
        if (p->dimension >= 3) {
            printf(", Faces: %lu", p->faces);
        }
        if (p->dimension >= 4 && p->cells > 0) {
            printf(", Cells: %lu", p->cells);
        }
        printf("\n");
        
        if (p->is_self_dual) {
            printf("    Self-dual: Yes\n");
        }
        
        printf("\n");
    }
}

bool discovery_results_export(const DiscoveryResults* results,
                               const char* filename,
                               const char* format) {
    // TODO: Implement export functionality
    (void)results;
    (void)filename;
    (void)format;
    return false;
}

uint32_t discovery_count_by_dimension(const DiscoveryResults* results,
                                       uint32_t dimension) {
    if (!results) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->polytopes[i] && results->polytopes[i]->dimension == dimension) {
            count++;
        }
    }
    
    return count;
}

uint32_t discovery_count_by_family(const DiscoveryResults* results,
                                    PolytopeFamily family) {
    if (!results) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->polytopes[i] && results->polytopes[i]->family == family) {
            count++;
        }
    }
    
    return count;
}