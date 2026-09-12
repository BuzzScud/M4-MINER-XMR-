#ifndef MATH_POLYTOPE_UNIFIED_H
#define MATH_POLYTOPE_UNIFIED_H

/**
 * @file polytope_unified.h
 * @brief THE unified polytope API - One interface for all polytope operations
 * 
 * This is THE way to create, query, and manipulate polytopes.
 * Not an alternative. Not an enhancement. THE solution.
 * 
 * Features:
 * - Automatic face hierarchy generation
 * - Automatic vertex-prime mapping
 * - Automatic clock lattice mapping
 * - Optional hierarchical nesting
 * - NTT optimization for large polytopes
 * - CrystallineAbacus precision throughout
 */

#include "math/polytope.h"
#include "math/polytope_discovery.h"
#include "math/higher_faces.h"
#include "math/nested_polytope.h"
#include "math/platonic_clock.h"
#include "math/schlafli.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// UNIFIED POLYTOPE SPECIFICATION
// ============================================================================

/**
 * @brief Complete specification for polytope creation
 * 
 * This structure defines EVERYTHING about how a polytope should be created.
 * All features are enabled by default - this IS how polytopes work.
 */
typedef struct {
    // ========================================================================
    // BASIC SPECIFICATION (Required)
    // ========================================================================
    const char* schlafli_symbol;  /**< Schläfli symbol, e.g., "{3,3}", "{4,3,3}" */
    uint32_t dimension;           /**< Dimension (derived from symbol if not set) */
    
    // ========================================================================
    // PRECISION CONFIGURATION
    // ========================================================================
    uint32_t abacus_base;         /**< Base for Abacus (default: 60 Babylonian) */
    int32_t abacus_precision;     /**< Precision (default: 100) */
    
    // ========================================================================
    // CORE FEATURES (All enabled by default)
    // ========================================================================
    bool generate_faces;          /**< Generate complete face hierarchy (default: true) */
    bool map_to_primes;           /**< Map vertices to primes (default: true) */
    bool map_to_clock;            /**< Map to clock lattice (default: true) */
    bool use_ntt;                 /**< Use NTT for large polytopes (default: auto) */
    
    // ========================================================================
    // NTT CONFIGURATION (Advanced)
    // ========================================================================
    uint32_t ntt_threshold;       /**< Vertex count threshold for NTT (default: 100) */
    uint64_t ntt_prime;           /**< Specific NTT prime (0 = auto-select) */
    bool ntt_force_enable;        /**< Force NTT even for small polytopes (default: false) */
    bool ntt_force_disable;       /**< Force disable NTT (default: false) */
    
    // ========================================================================
    // NESTING CONFIGURATION (Optional)
    // ========================================================================
    bool enable_nesting;          /**< Enable hierarchical nesting (default: false) */
    NestingStrategy nesting_strategy; /**< How to nest children */
    uint32_t nesting_depth;       /**< Depth of nesting (0 = no nesting) */
    double scale_factor;          /**< Scale factor for nested children */
    
    // ========================================================================
    // ADVANCED OPTIONS
    // ========================================================================
    bool validate_on_create;      /**< Validate polytope after creation (default: true) */
    bool compute_metrics;         /**< Compute geometric metrics (default: true) */
    
} PolytopeSpec;

/**
 * @brief Complete information about a polytope
 * 
 * This structure contains EVERYTHING you need to know about a polytope.
 * Not partial information. Not a subset. EVERYTHING.
 */
typedef struct {
    // ========================================================================
    // BASIC PROPERTIES
    // ========================================================================
    uint32_t dimension;           /**< Dimension of polytope */
    uint64_t num_vertices;        /**< Number of vertices (0-faces) */
    uint64_t num_edges;           /**< Number of edges (1-faces) */
    uint64_t num_faces;           /**< Number of faces (2-faces) */
    uint64_t num_cells;           /**< Number of cells (3-faces, 4D+) */
    
    // ========================================================================
    // SCHLÄFLI SYMBOL
    // ========================================================================
    SchlafliSymbol* schlafli;     /**< Complete Schläfli symbol */
    
    // ========================================================================
    // FACE HIERARCHY (Always present)
    // ========================================================================
    FaceHierarchy* faces;         /**< Complete face hierarchy (0 to n-1 faces) */
    
    // ========================================================================
    // VERTEX MAPPINGS (Always present)
    // ========================================================================
    uint64_t* vertex_primes;      /**< Prime number for each vertex */
    ClockPosition* vertex_clock_positions; /**< Clock position for each vertex */
    
    // ========================================================================
    // GEOMETRIC PROPERTIES
    // ========================================================================
    double edge_length;           /**< Edge length (normalized) */
    double circumradius;          /**< Circumradius */
    double inradius;              /**< Inradius */
    double volume;                /**< Volume/hypervolume */
    
    // ========================================================================
    // NESTING (If applicable)
    // ========================================================================
    NestedPolytopeTree* nesting_tree; /**< Hierarchical nesting tree (NULL if not nested) */
    
    // ========================================================================
    // NTT STATUS (Performance information)
    // ========================================================================
    bool ntt_enabled;             /**< Whether NTT is being used */
    uint64_t ntt_prime;           /**< NTT prime in use (0 if not using NTT) */
    size_t ntt_transform_size;    /**< NTT transform size (0 if not using NTT) */
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    bool is_valid;                /**< Passed all validation checks */
    bool is_regular;              /**< Is regular polytope */
    int64_t euler_characteristic; /**< Euler characteristic */
    
} PolytopeInfo;

// ============================================================================
// THE UNIFIED API - One way to do everything
// ============================================================================

/**
 * @brief Create a polytope with full specification
 * 
 * This is THE function to create polytopes.
 * 
 * Features automatically enabled:
 * - Complete face hierarchy generation
 * - Vertex-to-prime mapping
 * - Clock lattice mapping
 * - NTT optimization (for large polytopes)
 * - Validation
 * - Metric computation
 * 
 * @param spec Complete polytope specification
 * @return Nested polytope tree (even if not nested, for consistency)
 */
NestedPolytopeTree* polytope_create(const PolytopeSpec* spec);

/**
 * @brief Create a simple polytope from Schläfli symbol
 * 
 * Convenience function with all defaults.
 * Equivalent to polytope_create() with default spec.
 * 
 * @param schlafli_symbol Schläfli symbol, e.g., "{3,3}", "{4,3,3}"
 * @return Nested polytope tree
 */
NestedPolytopeTree* polytope_create_simple(const char* schlafli_symbol);

/**
 * @brief Create a nested polytope hierarchy
 * 
 * Convenience function for creating nested structures.
 * 
 * @param schlafli_symbol Schläfli symbol for root polytope
 * @param strategy Nesting strategy
 * @param depth Nesting depth
 * @param scale Scale factor for children
 * @return Nested polytope tree
 */
NestedPolytopeTree* polytope_create_nested(
    const char* schlafli_symbol,
    NestingStrategy strategy,
    uint32_t depth,
    double scale
);

/**
 * @brief Get complete information about a polytope
 * 
 * This is THE function to query polytopes.
 * Returns EVERYTHING you need to know.
 * 
 * @param solid Polytope to query
 * @return Complete polytope information
 */
PolytopeInfo* polytope_get_info(const PlatonicSolid* solid);

/**
 * @brief Free polytope information
 * 
 * @param info Information to free
 */
void polytope_free_info(PolytopeInfo* info);

// ============================================================================
// VERTEX OPERATIONS - Unified vertex access
// ============================================================================

/**
 * @brief Complete vertex information
 */
typedef struct {
    uint64_t index;               /**< Vertex index (0-based) */
    uint64_t prime;               /**< Prime number for this vertex */
    ClockPosition clock_pos;      /**< Clock lattice position */
    CrystallineAbacus** coords;   /**< nD coordinates (Abacus precision) */
    uint32_t dimension;           /**< Dimension of coordinates */
} PolytopeVertex;

/**
 * @brief Get complete vertex information
 * 
 * This is THE function to access vertices.
 * Returns EVERYTHING about a vertex.
 * 
 * @param solid Polytope
 * @param vertex_index Vertex index (0-based)
 * @return Complete vertex information
 */
PolytopeVertex* polytope_get_vertex(const PlatonicSolid* solid, uint64_t vertex_index);

/**
 * @brief Free vertex information
 * 
 * @param vertex Vertex to free
 */
void polytope_free_vertex(PolytopeVertex* vertex);

// ============================================================================
// FACE OPERATIONS - Unified face access
// ============================================================================

/**
 * @brief Get k-faces of a polytope
 * 
 * @param solid Polytope
 * @param k Dimension of faces (0 = vertices, 1 = edges, 2 = faces, etc.)
 * @return Set of k-faces
 */
KFaceSet* polytope_get_k_faces(const PlatonicSolid* solid, uint32_t k);

/**
 * @brief Get complete face hierarchy
 * 
 * @param solid Polytope
 * @return Complete face hierarchy
 */
FaceHierarchy* polytope_get_face_hierarchy(const PlatonicSolid* solid);

// ============================================================================
// VALIDATION - Unified validation
// ============================================================================

/**
 * @brief Validate a polytope
 * 
 * Checks:
 * - Schläfli symbol validity
 * - Euler characteristic
 * - Face counts
 * - Geometric properties
 * - Vertex-prime mapping
 * - Clock lattice mapping
 * 
 * @param solid Polytope to validate
 * @return true if valid
 */
bool polytope_validate(const PlatonicSolid* solid);

/**
 * @brief Get validation report
 * 
 * @param solid Polytope to validate
 * @param report Output buffer for report
 * @param max_length Maximum length of report
 * @return true if valid
 */
bool polytope_validate_detailed(const PlatonicSolid* solid, char* report, size_t max_length);

// ============================================================================
// UTILITIES - Unified utilities
// ============================================================================

/**
 * @brief Print polytope information
 * 
 * @param solid Polytope to print
 */
void polytope_print(const PlatonicSolid* solid);

/**
 * @brief Print polytope statistics
 * 
 * @param solid Polytope to analyze
 */
void polytope_print_stats(const PlatonicSolid* solid);

/**
 * @brief Export polytope to file
 * 
 * @param solid Polytope to export
 * @param filename Output filename
 * @param format Export format ("json", "obj", "ply", etc.)
 * @return true if successful
 */
bool polytope_export(const PlatonicSolid* solid, const char* filename, const char* format);

// ============================================================================
// DEFAULT SPECIFICATION - Sensible defaults
// ============================================================================

/**
 * @brief Get default polytope specification
 * 
 * Returns a spec with all sensible defaults:
 * - Babylonian base (60)
 * - High precision (100)
 * - All features enabled
 * - No nesting
 * 
 * @return Default specification
 */
PolytopeSpec polytope_default_spec(void);

/**
 * @brief Initialize a polytope specification
 * 
 * @param spec Specification to initialize
 * @param schlafli_symbol Schläfli symbol
 */
void polytope_init_spec(PolytopeSpec* spec, const char* schlafli_symbol);

#ifdef __cplusplus
}
#endif

#endif /* MATH_POLYTOPE_UNIFIED_H */