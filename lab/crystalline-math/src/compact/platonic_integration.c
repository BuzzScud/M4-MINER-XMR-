/**
 * @file platonic_integration.c
 * @brief Platonic Solids Integration with Compact Vector System
 * 
 * Integrates Platonic solid geometry with the compact vector memory hopping architecture.
 * Maps numbers to solid vertices, implements Schlafli trajectories, and enables
 * multi-dimensional computation.
 */

#include "math/polytope.h"
#include "math/platonic_generator.h"
#include "math/schlafli.h"
#include "math/compact_vector.h"
#include "math/transcendental.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * SOLID SELECTION BY MAGNITUDE
 * ============================================================================
 */

/**
 * @brief Select appropriate Platonic solid based on magnitude
 * 
 * Uses magnitude to determine which solid provides optimal representation:
 * - Small magnitudes: Simple solids (tetrahedron, cube)
 * - Medium magnitudes: Complex 3D solids (dodecahedron, icosahedron)
 * - Large magnitudes: 4D+ polytopes (tesseract, 5-cell, etc.)
 */
PlatonicSolid* select_solid_for_magnitude(uint64_t magnitude) {
    if (magnitude < 4) {
        // Tetrahedron: 4 vertices
        return platonic_generate_simplex(3);
    } else if (magnitude < 8) {
        // Octahedron: 6 vertices (cross-polytope)
        return platonic_generate_cross_polytope(3);
    } else if (magnitude < 12) {
        // Cube: 8 vertices
        return platonic_generate_hypercube(3);
    } else if (magnitude < 20) {
        // Icosahedron: 12 vertices
        uint32_t schlafli[] = {3, 5};
        return platonic_generate(3, schlafli, 2);
    } else if (magnitude < 100) {
        // Dodecahedron: 20 vertices
        uint32_t schlafli[] = {5, 3};
        return platonic_generate(3, schlafli, 2);
    } else if (magnitude < 1000) {
        // 4D: Tesseract (16 vertices) - but we need more than 20
        // Use 5D hypercube (32 vertices) instead
        return platonic_generate_hypercube(5);
    } else if (magnitude < 10000) {
        // 6D hypercube (64 vertices)
        return platonic_generate_hypercube(6);
    } else {
        // Higher dimensions: Use hypercube with appropriate dimension
        uint32_t dimension = 7;
        uint64_t temp = magnitude;
        while (temp > (1ULL << dimension)) {
            dimension++;
        }
        return platonic_generate_hypercube(dimension);
    }
}

/**
 * @brief Map angle to vertex on Platonic solid
 * 
 * Maps a phase angle (0-360°) to the nearest vertex on the solid.
 */
uint32_t map_angle_to_vertex(double angle, const PlatonicSolid* solid) {
    if (!solid || solid->num_vertices == 0) {
        return 0;
    }
    
    // Normalize angle to 0-360
    while (angle < 0.0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    
    // Simple mapping: divide 360° by number of vertices
    double angle_per_vertex = 360.0 / (double)solid->num_vertices;
    uint32_t vertex = (uint32_t)(angle / angle_per_vertex);
    
    // Ensure within bounds
    if (vertex >= solid->num_vertices) {
        vertex = solid->num_vertices - 1;
    }
    
    return vertex;
}

/**
 * @brief Map number to Platonic solid and vertex
 */
MathError map_number_to_solid(
    uint64_t number,
    PlatonicSolid** solid,
    uint32_t* vertex
) {
    if (!solid || !vertex) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Get precise clock position
    PreciseClockPosition pos;
    MathError err = get_precise_clock_position(number, &pos);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Select solid based on magnitude
    *solid = select_solid_for_magnitude(pos.magnitude);
    if (!*solid) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Map angle to vertex
    *vertex = map_angle_to_vertex(pos.precise_angle, *solid);
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * SCHLAFLI SYMBOL TRAJECTORIES
 * ============================================================================
 */

/* GeometricTrajectory structure defined in compact_vector.h */

/**
 * @brief Find shortest path between two vertices (BFS)
 */
static MathError find_shortest_path(
    const PlatonicSolid* solid,
    uint32_t start,
    uint32_t end,
    uint32_t** path,
    size_t* path_length
) {
    if (!solid || !path || !path_length) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Simple case: direct edge
    for (uint64_t i = 0; i < solid->num_edges; i++) {
        if ((solid->edge_indices[i][0] == start && solid->edge_indices[i][1] == end) ||
            (solid->edge_indices[i][0] == end && solid->edge_indices[i][1] == start)) {
            // Direct edge exists
            *path = (uint32_t*)malloc(2 * sizeof(uint32_t));
            if (!*path) return MATH_ERROR_OUT_OF_MEMORY;
            
            (*path)[0] = start;
            (*path)[1] = end;
            *path_length = 2;
            return MATH_SUCCESS;
        }
    }
    
    // For now, return simple 2-vertex path
    // TODO: Implement full BFS for multi-hop paths
    *path = (uint32_t*)malloc(2 * sizeof(uint32_t));
    if (!*path) return MATH_ERROR_OUT_OF_MEMORY;
    
    (*path)[0] = start;
    (*path)[1] = end;
    *path_length = 2;
    
    return MATH_SUCCESS;
}

/**
 * @brief Create trajectory for arithmetic operation
 */
MathError create_trajectory_for_operation(
    MathOperation op __attribute__((unused)),
    uint64_t operand_a,
    uint64_t operand_b,
    GeometricTrajectory** trajectory
) {
    if (!trajectory) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Allocate trajectory
    GeometricTrajectory* traj = (GeometricTrajectory*)malloc(sizeof(GeometricTrajectory));
    if (!traj) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Map operands to solid vertices
    PlatonicSolid* solid_a;
    PlatonicSolid* solid_b;
    uint32_t vertex_a, vertex_b;
    
    MathError err = map_number_to_solid(operand_a, &solid_a, &vertex_a);
    if (err != MATH_SUCCESS) {
        free(traj);
        return err;
    }
    
    err = map_number_to_solid(operand_b, &solid_b, &vertex_b);
    if (err != MATH_SUCCESS) {
        platonic_free(solid_a);
        free(traj);
        return err;
    }
    
    // Use the larger solid (more vertices = more precision)
    if (solid_a->num_vertices >= solid_b->num_vertices) {
        traj->solid = solid_a;
        platonic_free(solid_b);
    } else {
        traj->solid = solid_b;
        platonic_free(solid_a);
        // Remap vertex_a to solid_b
        vertex_a = vertex_a % solid_b->num_vertices;
    }
    
    traj->start_vertex = vertex_a;
    traj->end_vertex = vertex_b;
    
    // Find path
    err = find_shortest_path(
        traj->solid,
        vertex_a,
        vertex_b,
        &traj->path,
        &traj->path_length
    );
    
    if (err != MATH_SUCCESS) {
        platonic_free(traj->solid);
        free(traj);
        return err;
    }
    
    // Calculate total distance
    traj->total_distance = 0.0;
    for (size_t i = 0; i < traj->path_length - 1; i++) {
        traj->total_distance += traj->solid->edge_length;
    }
    
    *trajectory = traj;
    return MATH_SUCCESS;
}

/**
 * @brief Free geometric trajectory
 */
void trajectory_free(GeometricTrajectory* trajectory) {
    if (!trajectory) return;
    
    if (trajectory->solid) {
        platonic_free(trajectory->solid);
    }
    
    if (trajectory->path) {
        free(trajectory->path);
    }
    
    free(trajectory);
}

/* ============================================================================
 * MULTI-VALUE MAPPING
 * ============================================================================
 */

/* MultiValueMapping structure defined in compact_vector.h */

/**
 * @brief Create multi-value mapping
 */
MathError create_multi_value_mapping(
    const uint64_t* values,
    size_t num_values,
    MultiValueMapping** mapping
) {
    if (!values || !mapping || num_values == 0) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Allocate mapping
    MultiValueMapping* map = (MultiValueMapping*)malloc(sizeof(MultiValueMapping));
    if (!map) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Find maximum magnitude to select appropriate solid
    uint64_t max_magnitude = 0;
    for (size_t i = 0; i < num_values; i++) {
        uint64_t mag = values[i] / 12;
        if (mag > max_magnitude) {
            max_magnitude = mag;
        }
    }
    
    // Select solid with enough vertices
    map->solid = select_solid_for_magnitude(max_magnitude);
    if (!map->solid) {
        free(map);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Ensure solid has enough vertices
    if (map->solid->num_vertices < num_values) {
        // Need larger solid
        platonic_free(map->solid);
        
        // Use hypercube with appropriate dimension
        uint32_t dimension = 3;
        while ((1ULL << dimension) < num_values) {
            dimension++;
        }
        map->solid = platonic_generate_hypercube(dimension);
        
        if (!map->solid) {
            free(map);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // Allocate vertex IDs
    map->vertex_ids = (uint32_t*)malloc(num_values * sizeof(uint32_t));
    if (!map->vertex_ids) {
        platonic_free(map->solid);
        free(map);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    map->num_values = num_values;
    
    // Map each value to a vertex
    for (size_t i = 0; i < num_values; i++) {
        PreciseClockPosition pos;
        get_precise_clock_position(values[i], &pos);
        map->vertex_ids[i] = map_angle_to_vertex(pos.precise_angle, map->solid);
    }
    
    *mapping = map;
    return MATH_SUCCESS;
}

/**
 * @brief Free multi-value mapping
 */
void multi_value_mapping_free(MultiValueMapping* mapping) {
    if (!mapping) return;
    
    if (mapping->solid) {
        platonic_free(mapping->solid);
    }
    
    if (mapping->vertex_ids) {
        free(mapping->vertex_ids);
    }
    
    free(mapping);
}

/* ============================================================================
 * EXTENDED COMPACT VECTOR WITH SOLID MAPPING
 * ============================================================================
 */

/**
 * @brief Create extended compact vector with Platonic solid mapping
 */
MathError create_extended_vector(
    uint64_t number,
    ExtendedCompactVector* vector
) {
    if (!vector) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Get precise position
    PreciseClockPosition pos;
    MathError err = get_precise_clock_position(number, &pos);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Create base vector
    err = compact_vector_create(
        0,  // sphere_id
        (float)pos.precise_angle,
        (int32_t)pos.magnitude,
        &vector->base
    );
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Determine solid type based on magnitude
    if (pos.magnitude < 4) {
        vector->solid = SOLID_TETRAHEDRON;
    } else if (pos.magnitude < 8) {
        vector->solid = SOLID_CUBE;
    } else if (pos.magnitude < 12) {
        vector->solid = SOLID_OCTAHEDRON;
    } else if (pos.magnitude < 20) {
        vector->solid = SOLID_ICOSAHEDRON;
    } else if (pos.magnitude < 100) {
        vector->solid = SOLID_DODECAHEDRON;
    } else if (pos.magnitude < 1000) {
        vector->solid = SOLID_TESSERACT;
    } else {
        vector->solid = SOLID_HYPERCUBE_ND;
    }
    
    // Map to vertex
    PlatonicSolid* solid = select_solid_for_magnitude(pos.magnitude);
    if (solid) {
        vector->vertex_id = map_angle_to_vertex(pos.precise_angle, solid);
        platonic_free(solid);
    } else {
        vector->vertex_id = 0;
    }
    
    vector->ring = pos.ring;
    vector->reserved = 0;
    
    return MATH_SUCCESS;
}