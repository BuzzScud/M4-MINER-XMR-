#ifndef MATH_POLYTOPE_DISCOVERY_H
#define MATH_POLYTOPE_DISCOVERY_H

/**
 * @file polytope_discovery.h
 * @brief Systematic discovery of regular polytopes
 * 
 * This module implements algorithms to systematically search for and
 * discover regular polytopes in any dimension using mathematical
 * constraints and validation.
 * 
 * Discovery Strategy:
 * 1. Generate candidate Schläfli symbols
 * 2. Apply mathematical constraints
 * 3. Validate using enhanced validation
 * 4. Classify by polytope family
 * 5. Store and report discoveries
 */

#include "math/schlafli.h"
#include "math/schlafli_math.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DISCOVERY CONFIGURATION
// ============================================================================

/**
 * @brief Discovery search parameters
 */
typedef struct {
    uint32_t min_dimension;        /**< Minimum dimension to search (e.g., 3) */
    uint32_t max_dimension;        /**< Maximum dimension to search (e.g., 10) */
    uint32_t min_component;        /**< Minimum Schläfli component (usually 3) */
    uint32_t max_component;        /**< Maximum Schläfli component (e.g., 10) */
    bool search_simplices;         /**< Search for simplices {3,3,...,3} */
    bool search_hypercubes;        /**< Search for hypercubes {4,3,...,3} */
    bool search_cross_polytopes;   /**< Search for cross-polytopes {3,3,...,4} */
    bool search_exceptional;       /**< Search for exceptional polytopes */
    uint32_t max_candidates;       /**< Maximum candidates to test (0 = unlimited) */
} DiscoveryConfig;

/**
 * @brief Polytope family classification
 */
typedef enum {
    FAMILY_UNKNOWN = 0,
    FAMILY_SIMPLEX,           /**< {3,3,...,3} */
    FAMILY_HYPERCUBE,         /**< {4,3,...,3} */
    FAMILY_CROSS_POLYTOPE,    /**< {3,3,...,4} */
    FAMILY_EXCEPTIONAL_3D,    /**< Dodecahedron, Icosahedron */
    FAMILY_EXCEPTIONAL_4D,    /**< 24-cell, 120-cell, 600-cell */
    FAMILY_EXCEPTIONAL_OTHER  /**< Other exceptional polytopes */
} PolytopeFamily;

/**
 * @brief Discovered polytope information
 */
typedef struct {
    SchlafliSymbol* symbol;        /**< Schläfli symbol */
    PolytopeFamily family;         /**< Polytope family */
    uint32_t dimension;            /**< Dimension */
    
    // Computed properties
    uint64_t vertices;             /**< Number of vertices */
    uint64_t edges;                /**< Number of edges */
    uint64_t faces;                /**< Number of 2-faces */
    uint64_t cells;                /**< Number of 3-faces (4D+) */
    
    FaceVector* f_vector;          /**< Complete f-vector */
    
    bool is_self_dual;             /**< Self-dual polytope */
    bool is_regular;               /**< Regular polytope */
    
    char name[64];                 /**< Common name (if known) */
} DiscoveredPolytope;

/**
 * @brief Discovery results
 */
typedef struct {
    DiscoveredPolytope** polytopes;  /**< Array of discovered polytopes */
    uint32_t count;                  /**< Number of polytopes discovered */
    uint32_t capacity;               /**< Array capacity */
    
    uint32_t candidates_tested;      /**< Total candidates tested */
    uint32_t candidates_valid;       /**< Valid candidates found */
    uint32_t candidates_invalid;     /**< Invalid candidates rejected */
    
    double search_time_seconds;      /**< Time taken for search */
} DiscoveryResults;

// ============================================================================
// DISCOVERY FUNCTIONS
// ============================================================================

/**
 * @brief Create default discovery configuration
 * 
 * @return Default configuration for 3D-5D search
 */
DiscoveryConfig discovery_config_default(void);

/**
 * @brief Create discovery configuration for specific dimension
 * 
 * @param dimension Dimension to search
 * @return Configuration for single dimension
 */
DiscoveryConfig discovery_config_for_dimension(uint32_t dimension);

/**
 * @brief Discover all regular polytopes in given dimension range
 * 
 * Systematically searches for all regular polytopes within the
 * specified dimension range using mathematical constraints.
 * 
 * @param config Discovery configuration
 * @return Discovery results (must be freed with discovery_results_free)
 */
DiscoveryResults* discovery_search(const DiscoveryConfig* config);

/**
 * @brief Discover all regular polytopes in specific dimension
 * 
 * Convenience function for single dimension search.
 * 
 * @param dimension Dimension to search
 * @return Discovery results (must be freed with discovery_results_free)
 */
DiscoveryResults* discovery_search_dimension(uint32_t dimension);

/**
 * @brief Verify known polytopes can be discovered
 * 
 * Tests the discovery system by searching for all known regular
 * polytopes and verifying they are found.
 * 
 * @return true if all known polytopes discovered
 */
bool discovery_verify_known_polytopes(void);

// ============================================================================
// CANDIDATE GENERATION
// ============================================================================

/**
 * @brief Generate candidate Schläfli symbols for dimension
 * 
 * Generates all possible Schläfli symbols for the given dimension
 * within the component range.
 * 
 * @param dimension Dimension
 * @param min_component Minimum component value
 * @param max_component Maximum component value
 * @param count Output: number of candidates generated
 * @return Array of candidate symbols (must be freed)
 */
SchlafliSymbol** discovery_generate_candidates(uint32_t dimension,
                                                uint32_t min_component,
                                                uint32_t max_component,
                                                uint32_t* count);

/**
 * @brief Filter candidates using mathematical constraints
 * 
 * Applies quick mathematical tests to filter out impossible
 * polytopes before full validation.
 * 
 * @param candidates Array of candidate symbols
 * @param count Number of candidates
 * @return Number of candidates remaining after filtering
 */
uint32_t discovery_filter_candidates(SchlafliSymbol** candidates,
                                      uint32_t count);

// ============================================================================
// CLASSIFICATION
// ============================================================================

/**
 * @brief Classify polytope by family
 * 
 * Determines which family a polytope belongs to based on its
 * Schläfli symbol.
 * 
 * @param symbol Schläfli symbol
 * @return Polytope family
 */
PolytopeFamily discovery_classify_family(const SchlafliSymbol* symbol);

/**
 * @brief Get common name for polytope
 * 
 * Returns the common name (e.g., "Tetrahedron") if known,
 * otherwise generates a descriptive name.
 * 
 * @param symbol Schläfli symbol
 * @param name Output buffer for name
 * @param name_size Size of output buffer
 */
void discovery_get_name(const SchlafliSymbol* symbol, char* name, size_t name_size);

/**
 * @brief Check if polytope is self-dual
 * 
 * A polytope is self-dual if its dual has the same Schläfli symbol.
 * 
 * @param symbol Schläfli symbol
 * @return true if self-dual
 */
bool discovery_is_self_dual(const SchlafliSymbol* symbol);

// ============================================================================
// RESULTS MANAGEMENT
// ============================================================================

/**
 * @brief Create discovery results structure
 * 
 * @return Empty results structure
 */
DiscoveryResults* discovery_results_create(void);

/**
 * @brief Add discovered polytope to results
 * 
 * @param results Results structure
 * @param polytope Discovered polytope (ownership transferred)
 */
void discovery_results_add(DiscoveryResults* results, DiscoveredPolytope* polytope);

/**
 * @brief Free discovery results
 * 
 * @param results Results to free
 */
void discovery_results_free(DiscoveryResults* results);

/**
 * @brief Print discovery results
 * 
 * @param results Results to print
 */
void discovery_results_print(const DiscoveryResults* results);

/**
 * @brief Export discovery results to file
 * 
 * Supported formats: "txt", "json", "csv"
 * 
 * @param results Results to export
 * @param filename Output filename
 * @param format Format string
 * @return true on success
 */
bool discovery_results_export(const DiscoveryResults* results,
                               const char* filename,
                               const char* format);

// ============================================================================
// STATISTICS
// ============================================================================

/**
 * @brief Get discovery statistics by dimension
 * 
 * @param results Discovery results
 * @param dimension Dimension to query
 * @return Number of polytopes found in dimension
 */
uint32_t discovery_count_by_dimension(const DiscoveryResults* results,
                                       uint32_t dimension);

/**
 * @brief Get discovery statistics by family
 * 
 * @param results Discovery results
 * @param family Family to query
 * @return Number of polytopes in family
 */
uint32_t discovery_count_by_family(const DiscoveryResults* results,
                                    PolytopeFamily family);

#ifdef __cplusplus
}
#endif

#endif /* MATH_POLYTOPE_DISCOVERY_H */