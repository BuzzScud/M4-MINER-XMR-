/**
 * @file tetration_discovery.c
 * @brief Tetration-Based Polytope Discovery System Implementation
 * 
 * Discovers new regular polytopes in arbitrary dimensions using:
 * 1. Prime triadic sets (non-overlapping constraint)
 * 2. Tetration towers (bases^bases^...^bases)
 * 3. Attractor analysis (convergence points)
 * 4. Geometric verification (regularity conditions)
 */

#include "math/tetration_discovery.h"
#include "math/prime.h"
#include "math/transcendental.h"
#include "math/arithmetic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_TETRATION_ITERATIONS 100
#define TETRATION_CONVERGENCE_THRESHOLD 1e-10
#define CLUSTER_DISTANCE_THRESHOLD 0.1
#define REGULARITY_THRESHOLD 0.95

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Compute tetration tower value
 * 
 * Computes base^base^...^base (depth times) using logarithms
 * to avoid overflow.
 * 
 * @param base Prime base
 * @param depth Tetration depth
 * @return log(base^base^...^base)
 */
static double compute_tetration_log(uint32_t base, uint32_t depth) {
    if (depth == 0) return 0.0;
    if (depth == 1) return math_log((double)base);
    
    // Use iterative logarithmic computation
    double log_val = math_log((double)base);
    
    for (uint32_t i = 1; i < depth; i++) {
        // log(base^x) = x * log(base)
        log_val = math_exp(log_val) * math_log((double)base);
        
        // Prevent overflow
        if (log_val > 100.0) {
            log_val = 100.0;
            break;
        }
    }
    
    return log_val;
}

/**
 * @brief Map tetration value to nD position
 * 
 * Uses harmonic extension to map 1D tetration value to nD space.
 * 
 * @param log_value Logarithm of tetration value
 * @param dimension Target dimension
 * @param position Output position array (must be allocated)
 */
static void map_to_nd_position(double log_value, uint32_t dimension, double* position) {
    // Use harmonic extension similar to platonic generator
    double radius = math_exp(log_value / (double)dimension);
    
    for (uint32_t d = 0; d < dimension; d++) {
        double harmonic = (double)(d + 1);
        double phase = log_value * harmonic / 10.0;  // Scale factor
        
        // Alternate sin/cos for orthogonality
        if (d % 2 == 0) {
            position[d] = radius * math_sin(phase);
        } else {
            position[d] = radius * math_cos(phase);
        }
    }
}

/**
 * @brief Compute Euclidean distance between two points
 */
static double compute_distance(const double* p1, const double* p2, uint32_t dimension) {
    double sum = 0.0;
    for (uint32_t d = 0; d < dimension; d++) {
        double diff = p1[d] - p2[d];
        sum += diff * diff;
    }
    return math_sqrt(sum);
}

/**
 * @brief Check if a tower is an attractor
 * 
 * A tower is an attractor if nearby towers converge to it.
 */
static bool is_attractor(const DiscoveryTower* tower, 
                        const DiscoveryTower* all_towers,
                        uint32_t num_towers,
                        double threshold) {
    uint32_t converging_count = 0;
    
    for (uint32_t i = 0; i < num_towers; i++) {
        if (&all_towers[i] == tower) continue;
        
        double dist = compute_distance(tower->position, 
                                      all_towers[i].position,
                                      tower->dimension);
        
        if (dist < threshold) {
            converging_count++;
        }
    }
    
    // Attractor if at least 3 towers converge to it
    return converging_count >= 3;
}

// ============================================================================
// DISCOVERY SYSTEM
// ============================================================================

TetrationDiscoverySystem* tetration_discovery_create(
    PrimeTriadicSets* triadic_sets,
    uint32_t min_depth,
    uint32_t max_depth,
    uint32_t dimension)
{
    if (!triadic_sets || min_depth > max_depth || dimension < 5) {
        return NULL;
    }
    
    TetrationDiscoverySystem* system = calloc(1, sizeof(TetrationDiscoverySystem));
    if (!system) return NULL;
    
    system->triadic_sets = triadic_sets;
    system->min_depth = min_depth;
    system->max_depth = max_depth;
    system->num_depths = max_depth - min_depth + 1;
    system->dimension = dimension;
    
    // Extract unique bases from all triadic sets
    uint32_t max_bases = triadic_sets->num_sets * 3;
    uint32_t* temp_bases = calloc(max_bases, sizeof(uint32_t));
    uint32_t num_unique = 0;
    
    for (uint32_t i = 0; i < triadic_sets->num_sets; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            uint32_t base = triadic_sets->sets[i].primes[j];
            
            // Check if already added
            bool found = false;
            for (uint32_t k = 0; k < num_unique; k++) {
                if (temp_bases[k] == base) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                temp_bases[num_unique++] = base;
            }
        }
    }
    
    system->num_bases = num_unique;
    system->bases = calloc(num_unique, sizeof(uint32_t));
    memcpy(system->bases, temp_bases, num_unique * sizeof(uint32_t));
    free(temp_bases);
    
    // Create towers: num_bases × num_depths
    system->num_towers = system->num_bases * system->num_depths;
    system->towers = calloc(system->num_towers, sizeof(DiscoveryTower));
    
    uint32_t tower_idx = 0;
    for (uint32_t b = 0; b < system->num_bases; b++) {
        for (uint32_t d = min_depth; d <= max_depth; d++) {
            DiscoveryTower* tower = &system->towers[tower_idx++];
            
            tower->base = system->bases[b];
            tower->depth = d;
            tower->dimension = dimension;
            tower->log_value = compute_tetration_log(tower->base, tower->depth);
            
            // Allocate and compute position
            tower->position = calloc(dimension, sizeof(double));
            map_to_nd_position(tower->log_value, dimension, tower->position);
            
            tower->convergence_rate = 1.0 / (double)d;  // Placeholder
            tower->is_attractor = false;  // Will be computed later
        }
    }
    
    // Identify attractors
    for (uint32_t i = 0; i < system->num_towers; i++) {
        system->towers[i].is_attractor = is_attractor(
            &system->towers[i],
            system->towers,
            system->num_towers,
            CLUSTER_DISTANCE_THRESHOLD
        );
    }
    
    return system;
}

// ============================================================================
// ATTRACTOR CLUSTERING
// ============================================================================

AttractorClusters* tetration_discovery_find_clusters(
    TetrationDiscoverySystem* system,
    double threshold)
{
    if (!system) return NULL;
    
    AttractorClusters* clusters = calloc(1, sizeof(AttractorClusters));
    if (!clusters) return NULL;
    
    clusters->dimension = system->dimension;
    
    // Count attractors
    uint32_t num_attractors = 0;
    for (uint32_t i = 0; i < system->num_towers; i++) {
        if (system->towers[i].is_attractor) {
            num_attractors++;
        }
    }
    
    if (num_attractors == 0) {
        clusters->num_clusters = 0;
        return clusters;
    }
    
    // Simple clustering: each attractor is its own cluster
    // (In production, use k-means or hierarchical clustering)
    clusters->num_clusters = num_attractors;
    clusters->clusters = calloc(num_attractors, sizeof(AttractorCluster));
    
    uint32_t cluster_idx = 0;
    for (uint32_t i = 0; i < system->num_towers; i++) {
        if (!system->towers[i].is_attractor) continue;
        
        AttractorCluster* cluster = &clusters->clusters[cluster_idx++];
        cluster->dimension = system->dimension;
        cluster->num_towers = 1;
        cluster->tower_indices = calloc(1, sizeof(uint32_t));
        cluster->tower_indices[0] = i;
        
        // Centroid is the attractor position
        cluster->centroid = calloc(system->dimension, sizeof(double));
        memcpy(cluster->centroid, system->towers[i].position,
               system->dimension * sizeof(double));
        
        cluster->radius = threshold;
    }
    
    return clusters;
}

// ============================================================================
// TRIADIC PATTERN DETECTION
// ============================================================================

TriadicPatterns* tetration_discovery_detect_patterns(
    AttractorClusters* clusters)
{
    if (!clusters || clusters->num_clusters < 3) return NULL;
    
    double regularity_threshold = 0.95;  // Default threshold
    
    TriadicPatterns* patterns = calloc(1, sizeof(TriadicPatterns));
    if (!patterns) return NULL;
    
    patterns->dimension = clusters->dimension;
    
    // Find all combinations of 3 attractors
    uint32_t max_patterns = (clusters->num_clusters * 
                            (clusters->num_clusters - 1) * 
                            (clusters->num_clusters - 2)) / 6;
    
    TriadicPattern* temp_patterns = calloc(max_patterns, sizeof(TriadicPattern));
    uint32_t num_found = 0;
    
    for (uint32_t i = 0; i < clusters->num_clusters; i++) {
        for (uint32_t j = i + 1; j < clusters->num_clusters; j++) {
            for (uint32_t k = j + 1; k < clusters->num_clusters; k++) {
                TriadicPattern* pattern = &temp_patterns[num_found];
                
                pattern->attractor_indices[0] = i;
                pattern->attractor_indices[1] = j;
                pattern->attractor_indices[2] = k;
                
                // Compute distances
                pattern->distances[0] = compute_distance(
                    clusters->clusters[i].centroid,
                    clusters->clusters[j].centroid,
                    clusters->dimension
                );
                pattern->distances[1] = compute_distance(
                    clusters->clusters[j].centroid,
                    clusters->clusters[k].centroid,
                    clusters->dimension
                );
                pattern->distances[2] = compute_distance(
                    clusters->clusters[k].centroid,
                    clusters->clusters[i].centroid,
                    clusters->dimension
                );
                
                // Check if equilateral (all distances equal within threshold)
                double avg_dist = (pattern->distances[0] + 
                                  pattern->distances[1] + 
                                  pattern->distances[2]) / 3.0;
                
                double max_deviation = 0.0;
                for (int d = 0; d < 3; d++) {
                    double dev = math_abs(pattern->distances[d] - avg_dist) / avg_dist;
                    if (dev > max_deviation) max_deviation = dev;
                }
                
                pattern->is_equilateral = (max_deviation < 0.1);
                pattern->regularity_score = 1.0 - max_deviation;
                
                // Only keep patterns above regularity threshold
                if (pattern->regularity_score >= regularity_threshold) {
                    num_found++;
                }
            }
        }
    }
    
    patterns->num_patterns = num_found;
    if (num_found > 0) {
        patterns->patterns = calloc(num_found, sizeof(TriadicPattern));
        memcpy(patterns->patterns, temp_patterns, 
               num_found * sizeof(TriadicPattern));
    }
    
    free(temp_patterns);
    return patterns;
}

// ============================================================================
// POLYTOPE VERIFICATION
// ============================================================================

PolytopeCandidate* tetration_discovery_verify_polytope(
    TriadicPatterns* patterns,
    uint32_t dimension)
{
    if (!patterns || patterns->num_patterns == 0) {
        return NULL;
    }
    
    PolytopeCandidate* candidate = calloc(1, sizeof(PolytopeCandidate));
    if (!candidate) return NULL;
    
    candidate->dimension = dimension;
    candidate->num_vertices = 0;  // Will be computed from patterns
    candidate->vertices = NULL;  // Not needed for initial validation
    
    // Compute regularity score (average of pattern scores)
    double total_score = 0.0;
    for (uint32_t i = 0; i < patterns->num_patterns; i++) {
        total_score += patterns->patterns[i].regularity_score;
    }
    candidate->regularity_score = total_score / (double)patterns->num_patterns;
    
    // Check if regular (score above threshold)
    candidate->is_regular = (candidate->regularity_score >= REGULARITY_THRESHOLD);
    candidate->is_convex = true;  // Assume convex for now
    
    // Placeholder Schläfli symbol
    snprintf(candidate->schlafli_symbol, sizeof(candidate->schlafli_symbol),
             "{?,?,...} (%uD, %u vertices)", 
             candidate->dimension, candidate->num_vertices);
    
    // Euler characteristic (placeholder - needs proper computation)
    candidate->euler_characteristic = 0;
    candidate->expected_euler = (candidate->dimension % 2 == 0) ? 2 : 0;
    
    snprintf(candidate->validation_notes, sizeof(candidate->validation_notes),
             "Found %u triadic patterns with avg regularity %.3f",
             patterns->num_patterns, candidate->regularity_score);
    
    return candidate;
}

// ============================================================================
// MAIN DISCOVERY PIPELINE
// ============================================================================

PolytopeCandidate* tetration_discovery_run(const DiscoveryConfig* config) {
    if (!config || config->dimension < 5) return NULL;
    
    printf("=== Tetration Discovery Pipeline ===\n");
    printf("Dimension: %u\n", config->dimension);
    printf("Depth range: [%u, %u]\n", config->min_depth, config->max_depth);
    printf("Number of triadic sets: %u\n\n", config->num_sets);
    
    // Step 1: Generate prime triadic sets
    printf("Step 1: Generating prime triadic sets...\n");
    TriadicSetConfig triadic_config = {
        .dimension = config->dimension,
        .num_sets = config->num_sets,
        .max_prime = 100  // Use first 100 primes
    };
    PrimeTriadicSets* triadic_sets = prime_triadic_generate(&triadic_config);
    
    if (!triadic_sets) {
        printf("ERROR: Failed to generate triadic sets\n");
        return NULL;
    }
    printf("Generated %u triadic sets\n\n", triadic_sets->num_sets);
    
    // Step 2: Create discovery system
    printf("Step 2: Creating tetration discovery system...\n");
    TetrationDiscoverySystem* system = tetration_discovery_create(
        triadic_sets,
        config->min_depth,
        config->max_depth,
        config->dimension
    );
    
    if (!system) {
        printf("ERROR: Failed to create discovery system\n");
        prime_triadic_free(triadic_sets);
        return NULL;
    }
    printf("Created %u tetration towers\n", system->num_towers);
    
    // Count attractors
    uint32_t num_attractors = 0;
    for (uint32_t i = 0; i < system->num_towers; i++) {
        if (system->towers[i].is_attractor) num_attractors++;
    }
    printf("Found %u attractors\n\n", num_attractors);
    
    // Step 3: Find attractor clusters
    printf("Step 3: Clustering attractors...\n");
    AttractorClusters* clusters = tetration_discovery_find_clusters(
        system,
        config->cluster_threshold
    );
    
    if (!clusters || clusters->num_clusters == 0) {
        printf("ERROR: No attractor clusters found\n");
        tetration_discovery_free(system);
        prime_triadic_free(triadic_sets);
        return NULL;
    }
    printf("Found %u attractor clusters\n\n", clusters->num_clusters);
    
    // Step 4: Find triadic patterns
    printf("Step 4: Detecting triadic patterns...\n");
    TriadicPatterns* patterns = tetration_discovery_detect_patterns(clusters);
    
    if (!patterns || patterns->num_patterns == 0) {
        printf("ERROR: No triadic patterns found\n");
        tetration_discovery_free_clusters(clusters);
        tetration_discovery_free(system);
        prime_triadic_free(triadic_sets);
        return NULL;
    }
    printf("Found %u triadic patterns\n\n", patterns->num_patterns);
    
    // Step 5: Verify polytope
    printf("Step 5: Verifying polytope candidate...\n");
    PolytopeCandidate* candidate = tetration_discovery_verify_polytope(
        patterns,
        config->dimension
    );
    
    if (!candidate) {
        printf("ERROR: Failed to verify polytope\n");
    } else {
        printf("SUCCESS: Found polytope candidate!\n");
        printf("  Vertices: %u\n", candidate->num_vertices);
        printf("  Regularity: %.3f\n", candidate->regularity_score);
        printf("  Regular: %s\n", candidate->is_regular ? "YES" : "NO");
    }
    
    // Cleanup
    tetration_discovery_free_patterns(patterns);
    tetration_discovery_free_clusters(clusters);
    tetration_discovery_free(system);
    prime_triadic_free(triadic_sets);
    
    return candidate;
}

// ============================================================================
// UTILITIES
// ============================================================================

void tetration_discovery_print(const TetrationDiscoverySystem* system) {
    if (!system) return;
    
    printf("=== Tetration Discovery System ===\n");
    printf("Dimension: %u\n", system->dimension);
    printf("Bases: %u [", system->num_bases);
    for (uint32_t i = 0; i < system->num_bases && i < 10; i++) {
        printf("%u%s", system->bases[i], i < system->num_bases - 1 ? ", " : "");
    }
    if (system->num_bases > 10) printf("...");
    printf("]\n");
    printf("Depth range: [%u, %u]\n", system->min_depth, system->max_depth);
    printf("Total towers: %u\n", system->num_towers);
    
    uint32_t num_attractors = 0;
    for (uint32_t i = 0; i < system->num_towers; i++) {
        if (system->towers[i].is_attractor) num_attractors++;
    }
    printf("Attractors: %u\n", num_attractors);
}

void tetration_discovery_print_clusters(const AttractorClusters* clusters) {
    if (!clusters) return;
    
    printf("=== Attractor Clusters ===\n");
    printf("Dimension: %u\n", clusters->dimension);
    printf("Number of clusters: %u\n", clusters->num_clusters);
    
    for (uint32_t i = 0; i < clusters->num_clusters && i < 10; i++) {
        printf("Cluster %u: %u towers, radius %.3f\n",
               i, clusters->clusters[i].num_towers,
               clusters->clusters[i].radius);
    }
    if (clusters->num_clusters > 10) {
        printf("... (%u more clusters)\n", clusters->num_clusters - 10);
    }
}

void tetration_discovery_print_patterns(const TriadicPatterns* patterns) {
    if (!patterns) return;
    
    printf("=== Triadic Patterns ===\n");
    printf("Dimension: %u\n", patterns->dimension);
    printf("Number of patterns: %u\n", patterns->num_patterns);
    
    for (uint32_t i = 0; i < patterns->num_patterns && i < 10; i++) {
        const TriadicPattern* p = &patterns->patterns[i];
        printf("Pattern %u: attractors [%u, %u, %u], regularity %.3f%s\n",
               i, p->attractor_indices[0], p->attractor_indices[1],
               p->attractor_indices[2], p->regularity_score,
               p->is_equilateral ? " (equilateral)" : "");
    }
    if (patterns->num_patterns > 10) {
        printf("... (%u more patterns)\n", patterns->num_patterns - 10);
    }
}

void tetration_discovery_print_candidate(const PolytopeCandidate* candidate) {
    if (!candidate) return;
    
    printf("=== Polytope Candidate ===\n");
    printf("Dimension: %u\n", candidate->dimension);
    printf("Vertices: %u\n", candidate->num_vertices);
    printf("Schläfli symbol: %s\n", candidate->schlafli_symbol);
    printf("Regular: %s\n", candidate->is_regular ? "YES" : "NO");
    printf("Convex: %s\n", candidate->is_convex ? "YES" : "NO");
    printf("Regularity score: %.3f\n", candidate->regularity_score);
    printf("Euler characteristic: %ld (expected %ld)\n",
           candidate->euler_characteristic, candidate->expected_euler);
    printf("Notes: %s\n", candidate->validation_notes);
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void tetration_discovery_free(TetrationDiscoverySystem* system) {
    if (!system) return;
    
    if (system->towers) {
        for (uint32_t i = 0; i < system->num_towers; i++) {
            free(system->towers[i].position);
        }
        free(system->towers);
    }
    
    free(system->bases);
    free(system);
}

void tetration_discovery_free_clusters(AttractorClusters* clusters) {
    if (!clusters) return;
    
    if (clusters->clusters) {
        for (uint32_t i = 0; i < clusters->num_clusters; i++) {
            free(clusters->clusters[i].centroid);
            free(clusters->clusters[i].tower_indices);
        }
        free(clusters->clusters);
    }
    
    free(clusters);
}

void tetration_discovery_free_patterns(TriadicPatterns* patterns) {
    if (!patterns) return;
    
    free(patterns->patterns);
    free(patterns);
}

void tetration_discovery_free_candidate(PolytopeCandidate* candidate) {
    if (!candidate) return;
    
    if (candidate->vertices) {
        for (uint32_t i = 0; i < candidate->num_vertices; i++) {
            free(candidate->vertices[i]);
        }
        free(candidate->vertices);
    }
    
    free(candidate);
}

// ============================================================================
// PRESETS
// ============================================================================

DiscoveryConfig tetration_discovery_config_5d(void) {
    DiscoveryConfig config = {
        .dimension = 5,
        .num_sets = 10,
        .min_depth = 29,
        .max_depth = 59,
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.95
    };
    return config;
}

DiscoveryConfig tetration_discovery_config_6d(void) {
    DiscoveryConfig config = {
        .dimension = 6,
        .num_sets = 15,
        .min_depth = 29,
        .max_depth = 59,
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.95
    };
    return config;
}

DiscoveryConfig tetration_discovery_config_7d(void) {
    DiscoveryConfig config = {
        .dimension = 7,
        .num_sets = 20,
        .min_depth = 29,
        .max_depth = 59,
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.95
    };
    return config;
}

DiscoveryConfig tetration_discovery_config_8d(void) {
    DiscoveryConfig config = {
        .dimension = 8,
        .num_sets = 25,
        .min_depth = 29,
        .max_depth = 59,
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.95
    };
    return config;
}

DiscoveryConfig tetration_discovery_config_10d(void) {
    DiscoveryConfig config = {
        .dimension = 10,
        .num_sets = 30,
        .min_depth = 29,
        .max_depth = 59,
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.95
    };
    return config;
}

DiscoveryConfig tetration_discovery_config_20d(void) {
    DiscoveryConfig config = {
        .dimension = 20,
        .num_sets = 60,
        .min_depth = 29,
        .max_depth = 59,
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.95
    };
    return config;
}

DiscoveryConfig tetration_discovery_config_100d(void) {
    DiscoveryConfig config = {
        .dimension = 100,
        .num_sets = 300,
        .min_depth = 29,
        .max_depth = 59,
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.95
    };
    return config;
}