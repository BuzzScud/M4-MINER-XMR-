#ifndef MATH_POLYTOPE_VERTEX_H
#define MATH_POLYTOPE_VERTEX_H

/**
 * @file polytope_vertex.h
 * @brief THE vertex system - Automatic prime and clock lattice mapping
 * 
 * Every polytope vertex automatically has:
 * - A unique prime number
 * - A clock lattice position
 * - Precise coordinates (Abacus)
 * 
 * This IS how vertices work. Not optional. THE design.
 */

#include "math/polytope.h"
#include "math/clock_lattice_13d.h"
#include "math/abacus.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// VERTEX STRUCTURE - Complete vertex information
// ============================================================================

/**
 * @brief Complete vertex with all mappings
 * 
 * This structure contains EVERYTHING about a vertex:
 * - Index (position in polytope)
 * - Prime (unique prime number)
 * - Clock position (on Babylonian clock lattice)
 * - Coordinates (precise Abacus values)
 */
typedef struct {
    // Basic identification
    uint64_t index;               /**< Vertex index (0-based) */
    uint64_t prime;               /**< THE prime for this vertex */
    
    // Clock lattice mapping
    ClockPosition clock_pos;      /**< Position on clock lattice */
    
    // Coordinates (Abacus precision)
    CrystallineAbacus** coords;   /**< nD coordinates */
    uint32_t dimension;           /**< Dimension of coordinates */
    
    // Geometric properties
    double* coords_double;        /**< Double precision (for compatibility) */
    
    // Connectivity
    uint64_t* adjacent_vertices;  /**< Indices of adjacent vertices */
    uint32_t num_adjacent;        /**< Number of adjacent vertices */
    
} PolytopeVertexComplete;

// ============================================================================
// AUTOMATIC MAPPING - These happen automatically
// ============================================================================

/**
 * @brief Initialize vertex system for a polytope
 * 
 * This function sets up ALL vertex mappings automatically:
 * - Assigns prime to each vertex
 * - Maps each vertex to clock lattice
 * - Computes precise coordinates
 * 
 * Called automatically during polytope creation.
 * 
 * @param solid Polytope to initialize
 * @return true if successful
 */
bool polytope_vertex_initialize(PlatonicSolid* solid);

/**
 * @brief Get complete vertex information
 * 
 * Returns EVERYTHING about a vertex.
 * All mappings are computed on-demand if not cached.
 * 
 * @param solid Polytope
 * @param vertex_index Vertex index
 * @return Complete vertex information
 */
PolytopeVertexComplete* polytope_vertex_get(const PlatonicSolid* solid, uint64_t vertex_index);

/**
 * @brief Free vertex information
 * 
 * @param vertex Vertex to free
 */
void polytope_vertex_free(PolytopeVertexComplete* vertex);

// ============================================================================
// VERTEX-PRIME MAPPING - THE mapping
// ============================================================================

/**
 * @brief Get prime for vertex
 * 
 * This IS the mapping. Every vertex has a unique prime.
 * 
 * @param vertex_index Vertex index (0-based)
 * @return Prime number for this vertex
 */
uint64_t polytope_vertex_to_prime(uint64_t vertex_index);

/**
 * @brief Get vertex for prime
 * 
 * Inverse mapping: prime → vertex index
 * 
 * @param prime Prime number
 * @return Vertex index (0-based), or UINT64_MAX if not a prime
 */
uint64_t polytope_prime_to_vertex(uint64_t prime);

/**
 * @brief Get all vertex primes for a polytope
 * 
 * Returns array of primes, one per vertex.
 * 
 * @param solid Polytope
 * @param primes Output array (must be allocated)
 * @param max_primes Maximum primes to return
 * @return Number of primes returned
 */
uint32_t polytope_vertex_get_all_primes(const PlatonicSolid* solid, 
                                        uint64_t* primes, 
                                        uint32_t max_primes);

// ============================================================================
// CLOCK LATTICE MAPPING - THE coordinate system
// ============================================================================

/**
 * @brief Get clock position for vertex
 * 
 * This IS the coordinate system. Every vertex maps to clock lattice.
 * 
 * @param solid Polytope
 * @param vertex_index Vertex index
 * @param position Output clock position
 * @return true if successful
 */
bool polytope_vertex_to_clock(const PlatonicSolid* solid,
                              uint64_t vertex_index,
                              ClockPosition* position);

/**
 * @brief Get all clock positions for a polytope
 * 
 * Returns array of clock positions, one per vertex.
 * 
 * @param solid Polytope
 * @param positions Output array (must be allocated)
 * @param max_positions Maximum positions to return
 * @return Number of positions returned
 */
uint32_t polytope_vertex_get_all_clock_positions(const PlatonicSolid* solid,
                                                 ClockPosition* positions,
                                                 uint32_t max_positions);

// ============================================================================
// COORDINATE COMPUTATION - Precise Abacus coordinates
// ============================================================================

/**
 * @brief Compute precise coordinates for vertex
 * 
 * Uses clock lattice position to compute exact coordinates.
 * Returns Abacus values for arbitrary precision.
 * 
 * @param solid Polytope
 * @param vertex_index Vertex index
 * @param coords Output coordinates (must be allocated)
 * @param base Abacus base (default: 60 Babylonian)
 * @param precision Abacus precision (default: 100)
 * @return true if successful
 */
bool polytope_vertex_compute_coords(const PlatonicSolid* solid,
                                    uint64_t vertex_index,
                                    CrystallineAbacus** coords,
                                    uint32_t base,
                                    int32_t precision);

// ============================================================================
// VERTEX CONNECTIVITY - Adjacency information
// ============================================================================

/**
 * @brief Get adjacent vertices
 * 
 * Returns indices of vertices connected by edges.
 * 
 * @param solid Polytope
 * @param vertex_index Vertex index
 * @param adjacent Output array (must be allocated)
 * @param max_adjacent Maximum adjacent vertices to return
 * @return Number of adjacent vertices
 */
uint32_t polytope_vertex_get_adjacent(const PlatonicSolid* solid,
                                      uint64_t vertex_index,
                                      uint64_t* adjacent,
                                      uint32_t max_adjacent);

/**
 * @brief Check if two vertices are adjacent
 * 
 * @param solid Polytope
 * @param vertex1 First vertex index
 * @param vertex2 Second vertex index
 * @return true if vertices are connected by an edge
 */
bool polytope_vertex_are_adjacent(const PlatonicSolid* solid,
                                  uint64_t vertex1,
                                  uint64_t vertex2);

// ============================================================================
// VERTEX OPERATIONS - Geometric operations
// ============================================================================

/**
 * @brief Compute distance between vertices
 * 
 * Uses precise Abacus coordinates.
 * 
 * @param solid Polytope
 * @param vertex1 First vertex index
 * @param vertex2 Second vertex index
 * @return Distance (as double for convenience)
 */
double polytope_vertex_distance(const PlatonicSolid* solid,
                                uint64_t vertex1,
                                uint64_t vertex2);

/**
 * @brief Compute angle between three vertices
 * 
 * Angle at vertex2 formed by vertex1-vertex2-vertex3.
 * 
 * @param solid Polytope
 * @param vertex1 First vertex
 * @param vertex2 Center vertex
 * @param vertex3 Third vertex
 * @return Angle in radians
 */
double polytope_vertex_angle(const PlatonicSolid* solid,
                             uint64_t vertex1,
                             uint64_t vertex2,
                             uint64_t vertex3);

// ============================================================================
// BATCH OPERATIONS - Efficient bulk operations
// ============================================================================

/**
 * @brief Initialize all vertices at once
 * 
 * More efficient than initializing one at a time.
 * 
 * @param solid Polytope
 * @return true if successful
 */
bool polytope_vertex_initialize_all(PlatonicSolid* solid);

/**
 * @brief Get all vertices at once
 * 
 * Returns array of complete vertex information.
 * 
 * @param solid Polytope
 * @param vertices Output array (must be allocated)
 * @param max_vertices Maximum vertices to return
 * @return Number of vertices returned
 */
uint32_t polytope_vertex_get_all(const PlatonicSolid* solid,
                                 PolytopeVertexComplete** vertices,
                                 uint32_t max_vertices);

// ============================================================================
// CACHING - Performance optimization
// ============================================================================

/**
 * @brief Cache vertex mappings
 * 
 * Pre-computes and caches all mappings for fast access.
 * Called automatically for frequently accessed polytopes.
 * 
 * @param solid Polytope
 * @return true if successful
 */
bool polytope_vertex_cache_mappings(PlatonicSolid* solid);

/**
 * @brief Clear vertex cache
 * 
 * @param solid Polytope
 */
void polytope_vertex_clear_cache(PlatonicSolid* solid);

// ============================================================================
// VALIDATION - Ensure correctness
// ============================================================================

/**
 * @brief Validate vertex mappings
 * 
 * Checks:
 * - All vertices have unique primes
 * - All primes are actually prime
 * - Clock positions are valid
 * - Coordinates are consistent
 * 
 * @param solid Polytope
 * @return true if all mappings are valid
 */
bool polytope_vertex_validate_mappings(const PlatonicSolid* solid);

/**
 * @brief Print vertex information
 * 
 * @param vertex Vertex to print
 */
void polytope_vertex_print(const PolytopeVertexComplete* vertex);

/**
 * @brief Print all vertices
 * 
 * @param solid Polytope
 */
void polytope_vertex_print_all(const PlatonicSolid* solid);

#ifdef __cplusplus
}
#endif

#endif /* MATH_POLYTOPE_VERTEX_H */