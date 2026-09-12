/**
 * @file sphere_hopping.c
 * @brief Sphere Hopping Implementation
 * 
 * Implements sphere hierarchy and hopping for memory optimization.
 * Enables navigation through recursive sphere structure without copying data.
 */

#include "math/compact_vector.h"
#include "math/transcendental.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * SPHERE HIERARCHY STRUCTURES
 * ============================================================================
 */

/* CompactSphere structure defined in compact_vector.h */

/* ============================================================================
 * SPHERE HIERARCHY OPERATIONS
 * ============================================================================
 */

/**
 * @brief Create a sphere at specific level
 */
static CompactSphere* create_sphere(
    uint32_t id,
    uint32_t parent_id,
    uint32_t level
) {
    CompactSphere* sphere = (CompactSphere*)calloc(1, sizeof(CompactSphere));
    if (!sphere) {
        return NULL;
    }
    
    sphere->id = id;
    sphere->parent_id = parent_id;
    sphere->level = level;
    sphere->data = NULL;
    
    // Scale factor: each level is 1/12 of parent
    sphere->scale_factor = 1.0;
    for (uint32_t i = 0; i < level; i++) {
        sphere->scale_factor /= 12.0;
    }
    
    return sphere;
}

/**
 * @brief Create recursive sphere hierarchy
 */
CompactSphere* create_sphere_hierarchy(uint32_t max_depth) {
    CompactSphere* root = create_sphere(0, 0, 0);
    if (!root) {
        return NULL;
    }
    
    // Recursively create children
    create_children_recursive(root, max_depth, 1);
    
    return root;
}

/**
 * @brief Recursively create child spheres
 */
void create_children_recursive(
    CompactSphere* parent,
    uint32_t max_depth,
    uint32_t current_depth
) {
    if (!parent || current_depth >= max_depth) {
        return;
    }
    
    // Create 12 children (kissing spheres - 12-fold symmetry)
    for (uint32_t i = 0; i < 12; i++) {
        double angle = i * 30.0;  // 30° spacing
        
        CompactSphere* child = create_sphere(
            parent->id * 12 + i + 1,
            parent->id,
            current_depth
        );
        
        if (!child) {
            continue;
        }
        
        // Position child relative to parent
        compact_vector_create(
            parent->id,
            (float)angle,
            (int32_t)current_depth,
            &child->position
        );
        
        parent->children[i] = child;
        
        // Recurse to next level
        create_children_recursive(child, max_depth, current_depth + 1);
    }
}

/**
 * @brief Free sphere hierarchy
 */
void free_sphere_hierarchy(CompactSphere* root) {
    if (!root) return;
    
    // Free children first
    for (uint32_t i = 0; i < 12; i++) {
        if (root->children[i]) {
            free_sphere_hierarchy(root->children[i]);
        }
    }
    
    // Free data
    if (root->data) {
        compact_number_free(root->data);
    }
    
    // Free self
    free(root);
}

/* ============================================================================
 * SPHERE HOPPING OPERATIONS
 * ============================================================================
 */

/**
 * @brief Calculate phase difference between two spheres
 */
double calculate_sphere_phase_difference(uint32_t sphere1, uint32_t sphere2) {
    // Phase difference based on sphere IDs
    // Each sphere has a unique phase in the hierarchy
    
    int32_t diff = (int32_t)sphere2 - (int32_t)sphere1;
    
    // Convert to angle (each sphere offset by 30°)
    double phase_diff = (diff % 12) * 30.0;
    
    return phase_diff;
}

/**
 * @brief Calculate magnitude scale between two spheres
 */
int32_t calculate_magnitude_scale(uint32_t sphere1, uint32_t sphere2) {
    // Each level scales by 12
    // If sphere2 is at deeper level, scale up
    // If sphere2 is at shallower level, scale down
    
    // For now, simple calculation based on ID difference
    int32_t diff = (int32_t)sphere2 - (int32_t)sphere1;
    
    if (diff > 0) {
        // Moving to deeper level - scale up
        return 12;
    } else if (diff < 0) {
        // Moving to shallower level - scale down
        return 1; // Will be divided by 12 in caller
    }
    
    return 1; // Same level
}

/**
 * @brief Hop from one sphere to another
 */
MathError sphere_hop(
    const CompactNumber* number,
    uint32_t from_sphere,
    uint32_t to_sphere,
    CompactVector* result
) {
    if (!number || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Find vector at from_sphere
    const CompactVector* v_from = NULL;
    for (size_t i = 0; i < number->num_vectors; i++) {
        if (number->vectors[i].sphere_id == from_sphere) {
            v_from = &number->vectors[i];
            break;
        }
    }
    
    if (!v_from) {
        return MATH_ERROR_NOT_FOUND;
    }
    
    // Calculate phase difference between spheres
    double phase_diff = calculate_sphere_phase_difference(from_sphere, to_sphere);
    
    // Adjust magnitude based on hierarchy level
    int32_t magnitude_scale = calculate_magnitude_scale(from_sphere, to_sphere);
    
    // Create result vector
    result->sphere_id = to_sphere;
    result->phase_angle = (float)math_fmod(v_from->phase_angle + phase_diff, 360.0);
    result->magnitude_offset = v_from->magnitude_offset * magnitude_scale;
    result->phase_offset = v_from->phase_offset;
    
    return MATH_SUCCESS;
}

/**
 * @brief Navigate sphere hierarchy to find target sphere
 */
MathError navigate_hierarchy(
    CompactSphere* root,
    uint64_t magnitude,
    CompactSphere** target
) {
    if (!root || !target) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Calculate target level based on magnitude
    uint32_t target_level = magnitude_to_level(magnitude);
    
    // Start at root
    CompactSphere* current = root;
    
    // Navigate down to target level
    for (uint32_t level = 0; level < target_level && current; level++) {
        // Select child based on magnitude
        uint32_t child_index = (magnitude >> (level * 4)) % 12;
        current = current->children[child_index];
    }
    
    if (!current) {
        return MATH_ERROR_NOT_FOUND;
    }
    
    *target = current;
    return MATH_SUCCESS;
}

/* ============================================================================
 * MAGNITUDE SCALING OPERATIONS
 * ============================================================================
 */

/**
 * @brief Calculate sphere radius based on magnitude
 */
double calculate_sphere_radius(uint64_t magnitude) {
    // Logarithmic scaling
    const double BASE_RADIUS = 1.0;
    
    if (magnitude == 0) {
        return BASE_RADIUS;
    }
    
    // Use crystalline log (base 12)
    double log_mag = math_log((double)magnitude) / math_log(12.0);
    
    return BASE_RADIUS * (1.0 + log_mag);
}

/**
 * @brief Map magnitude to hierarchy level
 */
uint32_t magnitude_to_level(uint64_t magnitude) {
    if (magnitude < 12) return 0;
    if (magnitude < 144) return 1;   // 12^2
    if (magnitude < 1728) return 2;  // 12^3
    if (magnitude < 20736) return 3; // 12^4
    
    // General case: log_12(magnitude)
    uint32_t level = 0;
    uint64_t threshold = 12;
    
    while (magnitude >= threshold && level < 32) {
        threshold *= 12;
        level++;
    }
    
    return level;
}

/**
 * @brief Get scale factor for level
 */
double get_scale_factor(uint32_t level) {
    double scale = 1.0;
    
    for (uint32_t i = 0; i < level; i++) {
        scale /= 12.0;
    }
    
    return scale;
}

/* ============================================================================
 * MEMORY OPTIMIZATION
 * ============================================================================
 */

/**
 * @brief Calculate memory usage of sphere hierarchy
 */
size_t calculate_hierarchy_memory(const CompactSphere* root) {
    if (!root) return 0;
    
    size_t total = sizeof(CompactSphere);
    
    // Add data memory
    if (root->data) {
        total += compact_number_memory_usage(root->data);
    }
    
    // Add children memory (recursive)
    for (uint32_t i = 0; i < 12; i++) {
        if (root->children[i]) {
            total += calculate_hierarchy_memory(root->children[i]);
        }
    }
    
    return total;
}

/**
 * @brief Count total spheres in hierarchy
 */
uint32_t count_spheres(const CompactSphere* root) {
    if (!root) return 0;
    
    uint32_t count = 1; // This sphere
    
    // Count children
    for (uint32_t i = 0; i < 12; i++) {
        if (root->children[i]) {
            count += count_spheres(root->children[i]);
        }
    }
    
    return count;
}