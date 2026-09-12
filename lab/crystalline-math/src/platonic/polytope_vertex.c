/**
 * @file polytope_vertex.c
 * @brief THE vertex system implementation
 */

#include "math/polytope_vertex.h"
#include "math/platonic_clock.h"
#include "math/constants.h"
#include "math/prime.h"
#include "math/transcendental.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// AUTOMATIC MAPPING
// ============================================================================

bool polytope_vertex_initialize(PlatonicSolid* solid) {
    if (!solid || solid->num_vertices == 0) return false;
    
    // This function sets up the vertex system
    // In practice, mappings are computed on-demand
    // This just validates the polytope is ready
    
    return true;
}

PolytopeVertexComplete* polytope_vertex_get(const PlatonicSolid* solid, uint64_t vertex_index) {
    if (!solid || vertex_index >= solid->num_vertices) return NULL;
    
    PolytopeVertexComplete* vertex = calloc(1, sizeof(PolytopeVertexComplete));
    if (!vertex) return NULL;
    
    // Basic identification
    vertex->index = vertex_index;
    vertex->dimension = solid->dimension;
    
    // Prime mapping (THE mapping)
    vertex->prime = polytope_vertex_to_prime(vertex_index);
    
    // Clock lattice mapping (THE coordinate system)
    polytope_vertex_to_clock(solid, vertex_index, &vertex->clock_pos);
    
    // Coordinates (if available)
    if (solid->vertex_coords) {
        vertex->coords_double = malloc(solid->dimension * sizeof(double));
        if (vertex->coords_double) {
            for (uint32_t d = 0; d < solid->dimension; d++) {
                vertex->coords_double[d] = solid->vertex_coords[vertex_index * solid->dimension + d];
            }
        }
    }
    
    // Connectivity (compute from edges)
    if (solid->edge_indices && solid->num_edges > 0) {
        // Count adjacent vertices
        uint32_t count = 0;
        for (uint64_t e = 0; e < solid->num_edges; e++) {
            if (solid->edge_indices[e][0] == vertex_index || 
                solid->edge_indices[e][1] == vertex_index) {
                count++;
            }
        }
        
        if (count > 0) {
            vertex->adjacent_vertices = malloc(count * sizeof(uint64_t));
            if (vertex->adjacent_vertices) {
                vertex->num_adjacent = 0;
                for (uint64_t e = 0; e < solid->num_edges; e++) {
                    if (solid->edge_indices[e][0] == vertex_index) {
                        vertex->adjacent_vertices[vertex->num_adjacent++] = solid->edge_indices[e][1];
                    } else if (solid->edge_indices[e][1] == vertex_index) {
                        vertex->adjacent_vertices[vertex->num_adjacent++] = solid->edge_indices[e][0];
                    }
                }
            }
        }
    }
    
    return vertex;
}

void polytope_vertex_free(PolytopeVertexComplete* vertex) {
    if (!vertex) return;
    
    if (vertex->coords) {
        for (uint32_t d = 0; d < vertex->dimension; d++) {
            if (vertex->coords[d]) {
                abacus_free(vertex->coords[d]);
            }
        }
        free(vertex->coords);
    }
    
    free(vertex->coords_double);
    free(vertex->adjacent_vertices);
    free(vertex);
}

// ============================================================================
// VERTEX-PRIME MAPPING
// ============================================================================

uint64_t polytope_vertex_to_prime(uint64_t vertex_index) {
    // THE mapping: vertex → prime
    // vertex_index is 0-based, prime_nth expects 1-based
    return prime_nth(vertex_index + 1);
}

uint64_t polytope_prime_to_vertex(uint64_t prime) {
    // THE inverse mapping: prime → vertex
    if (!prime_is_prime(prime)) {
        return UINT64_MAX;
    }
    
    // Get prime index (1-based), convert to 0-based vertex index
    uint64_t index = prime_index(prime);
    if (index == 0) return UINT64_MAX;
    
    return index - 1;
}

uint32_t polytope_vertex_get_all_primes(const PlatonicSolid* solid, 
                                        uint64_t* primes, 
                                        uint32_t max_primes) {
    if (!solid || !primes) return 0;
    
    uint32_t count = solid->num_vertices < max_primes ? solid->num_vertices : max_primes;
    
    for (uint32_t i = 0; i < count; i++) {
        primes[i] = polytope_vertex_to_prime(i);
    }
    
    return count;
}

// ============================================================================
// CLOCK LATTICE MAPPING
// ============================================================================

bool polytope_vertex_to_clock(const PlatonicSolid* solid,
                              uint64_t vertex_index,
                              ClockPosition* position) {
    if (!solid || !position || vertex_index >= solid->num_vertices) {
        return false;
    }
    
    // Use THE mapping: vertex → prime → clock position
    return platonic_vertex_to_clock_position(vertex_index, position) == MATH_SUCCESS;
}

uint32_t polytope_vertex_get_all_clock_positions(const PlatonicSolid* solid,
                                                 ClockPosition* positions,
                                                 uint32_t max_positions) {
    if (!solid || !positions) return 0;
    
    uint32_t count = solid->num_vertices < max_positions ? solid->num_vertices : max_positions;
    
    for (uint32_t i = 0; i < count; i++) {
        polytope_vertex_to_clock(solid, i, &positions[i]);
    }
    
    return count;
}

// ============================================================================
// COORDINATE COMPUTATION
// ============================================================================

bool polytope_vertex_compute_coords(const PlatonicSolid* solid,
                                    uint64_t vertex_index,
                                    CrystallineAbacus** coords,
                                    uint32_t base,
                                    int32_t precision) {
    if (!solid || !coords || vertex_index >= solid->num_vertices) {
        return false;
    }
    
    // Get clock position
    ClockPosition clock_pos;
    if (!polytope_vertex_to_clock(solid, vertex_index, &clock_pos)) {
        return false;
    }
    
    // Convert clock position to coordinates using platonic_clock
    // Note: platonic_clock_to_coordinates expects CrystallineAbacus***
    CrystallineAbacus** coords_ptr = coords;
    return platonic_clock_to_coordinates(&clock_pos, solid->dimension, 
                                        &coords_ptr, base, precision) == MATH_SUCCESS;
}

// ============================================================================
// VERTEX CONNECTIVITY
// ============================================================================

uint32_t polytope_vertex_get_adjacent(const PlatonicSolid* solid,
                                      uint64_t vertex_index,
                                      uint64_t* adjacent,
                                      uint32_t max_adjacent) {
    if (!solid || !adjacent || vertex_index >= solid->num_vertices) {
        return 0;
    }
    
    if (!solid->edge_indices || solid->num_edges == 0) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint64_t e = 0; e < solid->num_edges && count < max_adjacent; e++) {
        if (solid->edge_indices[e][0] == vertex_index) {
            adjacent[count++] = solid->edge_indices[e][1];
        } else if (solid->edge_indices[e][1] == vertex_index) {
            adjacent[count++] = solid->edge_indices[e][0];
        }
    }
    
    return count;
}

bool polytope_vertex_are_adjacent(const PlatonicSolid* solid,
                                  uint64_t vertex1,
                                  uint64_t vertex2) {
    if (!solid || vertex1 >= solid->num_vertices || vertex2 >= solid->num_vertices) {
        return false;
    }
    
    if (!solid->edge_indices || solid->num_edges == 0) {
        return false;
    }
    
    for (uint64_t e = 0; e < solid->num_edges; e++) {
        if ((solid->edge_indices[e][0] == vertex1 && solid->edge_indices[e][1] == vertex2) ||
            (solid->edge_indices[e][0] == vertex2 && solid->edge_indices[e][1] == vertex1)) {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// VERTEX OPERATIONS
// ============================================================================

double polytope_vertex_distance(const PlatonicSolid* solid,
                                uint64_t vertex1,
                                uint64_t vertex2) {
    if (!solid || vertex1 >= solid->num_vertices || vertex2 >= solid->num_vertices) {
        return -1.0;
    }
    
    if (!solid->vertex_coords) {
        return -1.0;
    }
    
    // Compute Euclidean distance
    double sum = 0.0;
    for (uint32_t d = 0; d < solid->dimension; d++) {
        double diff = solid->vertex_coords[vertex1 * solid->dimension + d] -
                     solid->vertex_coords[vertex2 * solid->dimension + d];
        sum += diff * diff;
    }
    
    return math_sqrt(sum);
}

double polytope_vertex_angle(const PlatonicSolid* solid,
                             uint64_t vertex1,
                             uint64_t vertex2,
                             uint64_t vertex3) {
    if (!solid || vertex1 >= solid->num_vertices || 
        vertex2 >= solid->num_vertices || vertex3 >= solid->num_vertices) {
        return -1.0;
    }
    
    if (!solid->vertex_coords) {
        return -1.0;
    }
    
    // Compute vectors v1 = vertex1 - vertex2, v2 = vertex3 - vertex2
    double v1[12] = {0};  // Max dimension
    double v2[12] = {0};
    
    for (uint32_t d = 0; d < solid->dimension && d < 12; d++) {
        v1[d] = solid->vertex_coords[vertex1 * solid->dimension + d] -
                solid->vertex_coords[vertex2 * solid->dimension + d];
        v2[d] = solid->vertex_coords[vertex3 * solid->dimension + d] -
                solid->vertex_coords[vertex2 * solid->dimension + d];
    }
    
    // Compute dot product and magnitudes
    double dot = 0.0, mag1 = 0.0, mag2 = 0.0;
    for (uint32_t d = 0; d < solid->dimension && d < 12; d++) {
        dot += v1[d] * v2[d];
        mag1 += v1[d] * v1[d];
        mag2 += v2[d] * v2[d];
    }
    
    mag1 = math_sqrt(mag1);
    mag2 = math_sqrt(mag2);
    
    if (mag1 == 0.0 || mag2 == 0.0) {
        return -1.0;
    }
    
    // Compute angle using arccos
    double cos_angle = dot / (mag1 * mag2);
    
    // Clamp to [-1, 1] to avoid numerical errors
    if (cos_angle > 1.0) cos_angle = 1.0;
    if (cos_angle < -1.0) cos_angle = -1.0;
    
    return math_acos(cos_angle);
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

bool polytope_vertex_initialize_all(PlatonicSolid* solid) {
    if (!solid || solid->num_vertices == 0) return false;
    
    // Initialize all vertex mappings
    // In practice, this is a no-op since mappings are computed on-demand
    // But we can pre-validate everything
    
    for (uint64_t i = 0; i < solid->num_vertices; i++) {
        uint64_t prime = polytope_vertex_to_prime(i);
        if (!prime_is_prime(prime)) {
            return false;
        }
    }
    
    return true;
}

uint32_t polytope_vertex_get_all(const PlatonicSolid* solid,
                                 PolytopeVertexComplete** vertices,
                                 uint32_t max_vertices) {
    if (!solid || !vertices) return 0;
    
    uint32_t count = solid->num_vertices < max_vertices ? solid->num_vertices : max_vertices;
    
    for (uint32_t i = 0; i < count; i++) {
        vertices[i] = polytope_vertex_get(solid, i);
        if (!vertices[i]) {
            // Cleanup on error
            for (uint32_t j = 0; j < i; j++) {
                polytope_vertex_free(vertices[j]);
            }
            return 0;
        }
    }
    
    return count;
}

// ============================================================================
// CACHING
// ============================================================================

bool polytope_vertex_cache_mappings(PlatonicSolid* solid) {
    if (!solid || solid->num_vertices == 0) return false;
    
    // Pre-compute all mappings for fast access
    // This is an optimization for frequently accessed polytopes
    
    // For now, this is a placeholder
    // In a full implementation, we would cache:
    // - All vertex primes
    // - All clock positions
    // - All coordinates
    
    return true;
}

void polytope_vertex_clear_cache(PlatonicSolid* solid) {
    if (!solid) return;
    
    // Clear cached mappings
    // Placeholder for now
}

// ============================================================================
// VALIDATION
// ============================================================================

bool polytope_vertex_validate_mappings(const PlatonicSolid* solid) {
    if (!solid || solid->num_vertices == 0) return false;
    
    // Validate all vertex mappings
    for (uint64_t i = 0; i < solid->num_vertices; i++) {
        // Check prime mapping
        uint64_t prime = polytope_vertex_to_prime(i);
        if (!prime_is_prime(prime)) {
            return false;
        }
        
        // Check inverse mapping
        uint64_t vertex = polytope_prime_to_vertex(prime);
        if (vertex != i) {
            return false;
        }
        
        // Check clock mapping
        ClockPosition pos;
        if (!polytope_vertex_to_clock(solid, i, &pos)) {
            return false;
        }
    }
    
    return true;
}

void polytope_vertex_print(const PolytopeVertexComplete* vertex) {
    if (!vertex) return;
    
    printf("\nVertex %lu:\n", vertex->index);
    printf("  Prime: %lu\n", vertex->prime);
    printf("  Clock Position: (angle=%.6f, radius=%.6f)\n",
           vertex->clock_pos.angle, vertex->clock_pos.radius);
    
    if (vertex->coords_double) {
        printf("  Coordinates: [");
        for (uint32_t d = 0; d < vertex->dimension; d++) {
            printf("%.6f", vertex->coords_double[d]);
            if (d < vertex->dimension - 1) printf(", ");
        }
        printf("]\n");
    }
    
    if (vertex->num_adjacent > 0) {
        printf("  Adjacent vertices: [");
        for (uint32_t i = 0; i < vertex->num_adjacent; i++) {
            printf("%lu", vertex->adjacent_vertices[i]);
            if (i < vertex->num_adjacent - 1) printf(", ");
        }
        printf("]\n");
    }
}

void polytope_vertex_print_all(const PlatonicSolid* solid) {
    if (!solid) return;
    
    printf("\n========================================\n");
    printf("All Vertices for %s\n", solid->name);
    printf("========================================\n");
    
    for (uint64_t i = 0; i < solid->num_vertices; i++) {
        PolytopeVertexComplete* vertex = polytope_vertex_get(solid, i);
        if (vertex) {
            polytope_vertex_print(vertex);
            polytope_vertex_free(vertex);
        }
    }
    
    printf("========================================\n");
}