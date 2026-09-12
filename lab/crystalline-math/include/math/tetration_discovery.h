#ifndef MATH_TETRATION_DISCOVERY_H
#define MATH_TETRATION_DISCOVERY_H

/**
 * @file tetration_discovery.h
 * @brief Tetration-Based Polytope Discovery System
 * 
 * Discovers new regular polytopes in arbitrary dimensions using:
 * 1. Prime triadic sets (non-overlapping constraint)
 * 2. Tetration towers (bases^bases^...^bases)
 * 3. Attractor analysis (convergence points)
 * 4. Geometric verification (regularity conditions)
 * 
 * Based on Plato's discovery that prime relationships encode
 * geometric structure at all scales.
 */

#include "math/prime_triadic_sets.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Tetration tower for discovery
 */
typedef struct {
    uint32_t base;              /**< Prime base */
    uint32_t depth;             /**< Tetration depth */
    double log_value;           /**< log(base^base^...^base) */
    double* position;           /**< Position in nD space */
    uint32_t dimension;         /**< Dimension of position */
    double convergence_rate;    /**< Convergence rate */
    bool is_attractor;          /**< Is this an attractor point? */
} DiscoveryTower;

/**
 * @brief Tetration discovery system
 */
typedef struct {
    DiscoveryTower* towers;     /**< Array of towers */
    uint32_t num_towers;        /**< Number of towers */
    
    uint32_t* bases;            /**< Prime bases */
    uint32_t num_bases;         /**< Number of bases */
    
    uint32_t min_depth;         /**< Minimum depth */
    uint32_t max_depth;         /**< Maximum depth */
    uint32_t num_depths;        /**< Number of depths */
    
    uint32_t dimension;         /**< Target dimension */
    
    PrimeTriadicSets* triadic_sets; /**< Source triadic sets */
} TetrationDiscoverySystem;

/**
 * @brief Attractor cluster
 */
typedef struct {
    double* centroid;           /**< Cluster centroid */
    uint32_t* tower_indices;    /**< Indices of towers in cluster */
    uint32_t num_towers;        /**< Number of towers in cluster */
    uint32_t dimension;         /**< Dimension */
    double radius;              /**< Cluster radius */
} AttractorCluster;

/**
 * @brief Collection of attractor clusters
 */
typedef struct {
    AttractorCluster* clusters; /**< Array of clusters */
    uint32_t num_clusters;      /**< Number of clusters */
    uint32_t dimension;         /**< Dimension */
} AttractorClusters;

/**
 * @brief Triadic pattern (3 attractors forming a triangle)
 */
typedef struct {
    uint32_t attractor_indices[3]; /**< Indices of 3 attractors */
    double distances[3];        /**< Distances between attractors */
    double angles[3];           /**< Angles of triangle */
    bool is_equilateral;        /**< Is triangle equilateral? */
    double regularity_score;    /**< How regular is this pattern? */
} TriadicPattern;

/**
 * @brief Collection of triadic patterns
 */
typedef struct {
    TriadicPattern* patterns;   /**< Array of patterns */
    uint32_t num_patterns;      /**< Number of patterns */
    uint32_t dimension;         /**< Dimension */
} TriadicPatterns;

/**
 * @brief Candidate polytope
 */
typedef struct {
    uint32_t dimension;         /**< Dimension */
    uint32_t num_vertices;      /**< Number of vertices */
    double** vertices;          /**< Vertex positions */
    
    char schlafli_symbol[64];   /**< Schläfli symbol {p,q,r,...} */
    
    bool is_regular;            /**< Is it regular? */
    bool is_convex;             /**< Is it convex? */
    
    int64_t euler_characteristic; /**< Euler characteristic */
    int64_t expected_euler;     /**< Expected Euler characteristic */
    
    double regularity_score;    /**< Overall regularity score */
    
    char validation_notes[256]; /**< Validation notes */
} PolytopeCandidate;

/**
 * @brief Discovery configuration
 */
typedef struct {
    uint32_t dimension;         /**< Target dimension */
    uint32_t num_sets;          /**< Number of triadic sets */
    uint32_t min_depth;         /**< Minimum tetration depth */
    uint32_t max_depth;         /**< Maximum tetration depth */
    double cluster_threshold;   /**< Clustering threshold */
    double regularity_threshold; /**< Regularity threshold */
} DiscoveryConfig;

// ============================================================================
// DISCOVERY SYSTEM
// ============================================================================

/**
 * @brief Create tetration discovery system
 * 
 * @param triadic_sets Prime triadic sets
 * @param min_depth Minimum tetration depth
 * @param max_depth Maximum tetration depth
 * @param dimension Target dimension
 * @return Discovery system, or NULL on error
 */
TetrationDiscoverySystem* tetration_discovery_create(
    PrimeTriadicSets* triadic_sets,
    uint32_t min_depth,
    uint32_t max_depth,
    uint32_t dimension
);

/**
 * @brief Compute all tetration towers
 * 
 * @param system Discovery system
 * @return true on success, false on error
 */
bool tetration_discovery_compute_towers(TetrationDiscoverySystem* system);

/**
 * @brief Find attractor clusters
 * 
 * @param system Discovery system
 * @param threshold Clustering threshold
 * @return Attractor clusters, or NULL on error
 */
AttractorClusters* tetration_discovery_find_clusters(
    TetrationDiscoverySystem* system,
    double threshold
);

/**
 * @brief Detect triadic patterns
 * 
 * @param clusters Attractor clusters
 * @return Triadic patterns, or NULL on error
 */
TriadicPatterns* tetration_discovery_detect_patterns(
    AttractorClusters* clusters
);

/**
 * @brief Verify polytope candidate
 * 
 * @param patterns Triadic patterns
 * @param dimension Target dimension
 * @return Polytope candidate, or NULL if not valid
 */
PolytopeCandidate* tetration_discovery_verify_polytope(
    TriadicPatterns* patterns,
    uint32_t dimension
);

/**
 * @brief Run complete discovery pipeline
 * 
 * @param config Discovery configuration
 * @return Polytope candidate, or NULL if none found
 */
PolytopeCandidate* tetration_discovery_run(const DiscoveryConfig* config);

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * @brief Print discovery system info
 * 
 * @param system Discovery system
 */
void tetration_discovery_print(const TetrationDiscoverySystem* system);

/**
 * @brief Print attractor clusters
 * 
 * @param clusters Attractor clusters
 */
void tetration_discovery_print_clusters(const AttractorClusters* clusters);

/**
 * @brief Print triadic patterns
 * 
 * @param patterns Triadic patterns
 */
void tetration_discovery_print_patterns(const TriadicPatterns* patterns);

/**
 * @brief Print polytope candidate
 * 
 * @param candidate Polytope candidate
 */
void tetration_discovery_print_candidate(const PolytopeCandidate* candidate);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Free discovery system
 * 
 * @param system Discovery system
 */
void tetration_discovery_free(TetrationDiscoverySystem* system);

/**
 * @brief Free attractor clusters
 * 
 * @param clusters Attractor clusters
 */
void tetration_discovery_free_clusters(AttractorClusters* clusters);

/**
 * @brief Free triadic patterns
 * 
 * @param patterns Triadic patterns
 */
void tetration_discovery_free_patterns(TriadicPatterns* patterns);

/**
 * @brief Free polytope candidate
 * 
 * @param candidate Polytope candidate
 */
void tetration_discovery_free_candidate(PolytopeCandidate* candidate);

// ============================================================================
// PRESETS
// ============================================================================

/**
 * @brief Create discovery config for 5D
 * 
 * @return Discovery config for 5D
 */
DiscoveryConfig tetration_discovery_config_5d(void);

/**
 * @brief Create discovery config for 6D
 * 
 * @return Discovery config for 6D
 */
DiscoveryConfig tetration_discovery_config_6d(void);

/**
 * @brief Create discovery config for 7D
 * 
 * @return Discovery config for 7D
 */
DiscoveryConfig tetration_discovery_config_7d(void);

/**
 * @brief Create discovery config for 8D
 * 
 * @return Discovery config for 8D
 */
DiscoveryConfig tetration_discovery_config_8d(void);

/**
 * @brief Create discovery config for 10D
 * 
 * @return Discovery config for 10D
 */
DiscoveryConfig tetration_discovery_config_10d(void);

/**
 * @brief Create discovery config for 20D
 * 
 * @return Discovery config for 20D
 */
DiscoveryConfig tetration_discovery_config_20d(void);

/**
 * @brief Create discovery config for 100D
 * 
 * @return Discovery config for 100D
 */
DiscoveryConfig tetration_discovery_config_100d(void);

#ifdef __cplusplus
}
#endif

#endif /* MATH_TETRATION_DISCOVERY_H */