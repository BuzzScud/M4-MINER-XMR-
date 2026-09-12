/**
 * @file abacus88d.c
 * @brief Implementation of 88-dimensional geometric abacus
 */

#include "math/abacus88d.h"
#include "math/abacus.h"
#include "math/clock.h"
#include "math/platonic_generator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * HELPER: Copy abacus value to existing abacus
 * ============================================================================
 */
static MathError abacus_copy_to(CrystallineAbacus* dest, const CrystallineAbacus* src) {
    if (!dest || !src) return MATH_ERROR_NULL_POINTER;
    
    // Strategy: Use abacus_add with zero to copy the value
    // First, initialize dest to zero
    MathError err = abacus_init_zero(dest);
    if (err != MATH_SUCCESS) return err;
    
    // Then add src to dest (0 + src = src)
    err = abacus_add(dest, dest, src);
    if (err != MATH_SUCCESS) return err;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Get Platonic solid generator function for layer
 */
static PlatonicSolid* (*get_platonic_generator(uint8_t layer))(void) {
    switch (layer % 5) {
        case 0: return platonic_generate_tetrahedron;
        case 1: return platonic_generate_cube;
        case 2: return platonic_generate_octahedron;
        case 3: return platonic_generate_dodecahedron;
        case 4: return platonic_generate_icosahedron;
        default: return platonic_generate_tetrahedron;
    }
}

/**
 * @brief Initialize a single layer
 */
static MathError init_layer(Layer88D* layer, uint8_t layer_index, uint32_t base) {
    if (!layer) return MATH_ERROR_NULL_POINTER;
    
    // Set layer properties
    layer->layer_index = layer_index;
    layer->type = (Layer88DType)layer_index;
    layer->magnitude_scale = LAYER_SCALES[layer_index];
    
    // Initialize all 11 dimensions with CrystallineAbacus
    for (int i = 0; i < ABACUS88D_DIMS_PER_LAYER; i++) {
        layer->dimensions[i] = abacus_new(base);
        if (!layer->dimensions[i]) {
            // Cleanup on error
            for (int j = 0; j < i; j++) {
                abacus_free(layer->dimensions[j]);
            }
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        // Initialize to zero
        MathError err = abacus_init_zero(layer->dimensions[i]);
        if (err != MATH_SUCCESS) {
            for (int j = 0; j <= i; j++) {
                abacus_free(layer->dimensions[j]);
            }
            return err;
        }
    }
    
    // Generate Platonic solid frame for this layer
    PlatonicSolid* (*generator)(void) = get_platonic_generator(layer_index);
    layer->frame = generator();
    
    if (!layer->frame) {
        for (int i = 0; i < ABACUS88D_DIMS_PER_LAYER; i++) {
            abacus_free(layer->dimensions[i]);
        }
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize shared vertices (will be populated later)
    layer->shared_vertices = NULL;
    layer->num_shared_vertices = 0;
    
    // Threading removed - pure mathematical structure
    // Mutex initialization should be in algorithms library wrapper
    
    return MATH_SUCCESS;
}

/**
 * @brief Free a single layer
 */
static void free_layer(Layer88D* layer) {
    if (!layer) return;
    
    // Free all dimension abacus
    for (int i = 0; i < ABACUS88D_DIMS_PER_LAYER; i++) {
        if (layer->dimensions[i]) {
            abacus_free(layer->dimensions[i]);
            layer->dimensions[i] = NULL;
        }
    }
    
    // Free Platonic solid frame
    if (layer->frame) {
        platonic_free(layer->frame);
        layer->frame = NULL;
    }
    
    // Free shared vertices (if allocated)
    if (layer->shared_vertices) {
        free(layer->shared_vertices);
        layer->shared_vertices = NULL;
    }
    
    // Threading removed - pure mathematical structure
}

/**
 * @brief Initialize tetration system
 */
static TetrationSystem88D* init_tetration_system(void) {
    TetrationSystem88D* system = calloc(1, sizeof(TetrationSystem88D));
    if (!system) return NULL;
    
    // Generate 186 tetration towers (6 bases × 31 depths)
    const uint64_t bases[] = {2, 3, 5, 7, 11, 13};
    const int num_bases = 6;
    const int max_depth = 31;
    
    int tower_idx = 0;
    for (int b = 0; b < num_bases; b++) {
        for (int d = 1; d <= max_depth; d++) {
            if (tower_idx >= 186) break;
            
            TetrationTower88D* tower = &system->towers[tower_idx];
            tower->base = bases[b];
            tower->depth = d;
            tower->value = abacus_new(60);
            tower->converged = false;
            
            // Compute tetration value (simplified for now)
            // TODO: Implement proper tetration computation
            uint64_t result = bases[b];
            for (int i = 1; i < d && i < 10; i++) {
                if (result > 1000000000000ULL) break;
                uint64_t temp = 1;
                for (uint64_t j = 0; j < result && j < 64; j++) {
                    temp *= bases[b];
                    if (temp > 1000000000000ULL) break;
                }
                result = temp;
            }
            
            // Set value in abacus
            CrystallineAbacus* temp = abacus_from_uint64(result, 60);
            if (temp) {
                abacus_copy_to(tower->value, temp);
                abacus_free(temp);
            }
            
            // Determine which layers this tower connects
            tower->source_layer = abacus88d_select_layer(bases[b]);
            tower->target_layer = abacus88d_select_layer(result);
            
            // Store in mapping
            system->tower_layer_map[tower_idx][0] = tower->source_layer;
            system->tower_layer_map[tower_idx][1] = tower->target_layer;
            
            tower_idx++;
        }
    }
    
    system->num_towers = tower_idx;
    
    return system;
}

/**
 * @brief Free tetration system
 */
static void free_tetration_system(TetrationSystem88D* system) {
    if (!system) return;
    
    for (size_t i = 0; i < system->num_towers; i++) {
        if (system->towers[i].value) {
            abacus_free(system->towers[i].value);
        }
    }
    
    free(system);
}

/* ============================================================================
 * INITIALIZATION AND CLEANUP
 * ============================================================================
 */

Abacus88D* abacus88d_create(uint32_t base) {
    Abacus88D* abacus88d = calloc(1, sizeof(Abacus88D));
    if (!abacus88d) return NULL;
    
    abacus88d->base = base;
    abacus88d->active_layer = 0;
    abacus88d->active_dimension = 0;
    
    // Initialize all 8 layers
    for (int i = 0; i < ABACUS88D_NUM_LAYERS; i++) {
        MathError err = init_layer(&abacus88d->layers[i], i, base);
        if (err != MATH_SUCCESS) {
            // Cleanup on error
            for (int j = 0; j < i; j++) {
                free_layer(&abacus88d->layers[j]);
            }
            free(abacus88d);
            return NULL;
        }
    }
    
    // Initialize tetration system
    abacus88d->tetration = init_tetration_system();
    if (!abacus88d->tetration) {
        for (int i = 0; i < ABACUS88D_NUM_LAYERS; i++) {
            free_layer(&abacus88d->layers[i]);
        }
        free(abacus88d);
        return NULL;
    }
    
    // Initialize clock lattice for prime generation
    abacus88d->clock = calloc(1, sizeof(ClockContext));
    if (!abacus88d->clock) {
        free_tetration_system(abacus88d->tetration);
        for (int i = 0; i < ABACUS88D_NUM_LAYERS; i++) {
            free_layer(&abacus88d->layers[i]);
        }
        free(abacus88d);
        return NULL;
    }
    
    MathError err = clock_init(abacus88d->clock);
    if (err != MATH_SUCCESS) {
        free(abacus88d->clock);
        free_tetration_system(abacus88d->tetration);
        for (int i = 0; i < ABACUS88D_NUM_LAYERS; i++) {
            free_layer(&abacus88d->layers[i]);
        }
        free(abacus88d);
        return NULL;
    }
    
    // Initialize geometric boundaries (will be computed on demand)
    abacus88d->boundaries = NULL;
    abacus88d->num_boundaries = 0;
    
    // Threading removed - pure mathematical structure
    
    return abacus88d;
}

void abacus88d_free(Abacus88D* abacus88d) {
    if (!abacus88d) return;
    
    // Free all layers
    for (int i = 0; i < ABACUS88D_NUM_LAYERS; i++) {
        free_layer(&abacus88d->layers[i]);
    }
    
    // Free tetration system
    if (abacus88d->tetration) {
        free_tetration_system(abacus88d->tetration);
    }
    
    // Free clock lattice
    if (abacus88d->clock) {
        clock_cleanup(abacus88d->clock);
        free(abacus88d->clock);
    }
    
    // Free boundaries
    if (abacus88d->boundaries) {
        for (size_t i = 0; i < abacus88d->num_boundaries; i++) {
            if (abacus88d->boundaries[i].shared_value) {
                abacus_free(abacus88d->boundaries[i].shared_value);
            }
            // Threading removed - pure mathematical structure
        }
        free(abacus88d->boundaries);
    }
    
    // Threading removed - pure mathematical structure
    
    free(abacus88d);
}

MathError abacus88d_init_zero(Abacus88D* abacus88d) {
    if (!abacus88d) return MATH_ERROR_NULL_POINTER;
    
    for (int layer = 0; layer < ABACUS88D_NUM_LAYERS; layer++) {
        for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
            MathError err = abacus_init_zero(abacus88d->layers[layer].dimensions[dim]);
            if (err != MATH_SUCCESS) return err;
        }
    }
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * LAYER OPERATIONS
 * ============================================================================
 */

uint8_t abacus88d_select_layer(uint64_t magnitude) {
    if (magnitude < 1000ULL) return 0;
    if (magnitude < 1000000ULL) return 1;
    if (magnitude < 1000000000ULL) return 2;
    if (magnitude < 1000000000000ULL) return 3;
    if (magnitude < 1000000000000000ULL) return 4;
    if (magnitude < 1000000000000000000ULL) return 5;
    return 6;
}

uint8_t abacus88d_select_layer_from_abacus(const CrystallineAbacus* value) {
    if (!value) return 0;
    
    // Convert to uint64 for comparison (if it fits)
    uint64_t magnitude;
    MathError err = abacus_to_uint64(value, &magnitude);
    if (err != MATH_SUCCESS) return 0;
    
    return abacus88d_select_layer(magnitude);
}

PlatonicSolid* abacus88d_get_layer_frame(Abacus88D* abacus88d, uint8_t layer) {
    if (!abacus88d || layer >= ABACUS88D_NUM_LAYERS) return NULL;
    return abacus88d->layers[layer].frame;
}

MathError abacus88d_scale_to_layer(Abacus88D* abacus88d,
                                    uint8_t source_layer,
                                    uint8_t target_layer,
                                    uint8_t dimension) {
    if (!abacus88d) return MATH_ERROR_NULL_POINTER;
    if (source_layer >= ABACUS88D_NUM_LAYERS) return MATH_ERROR_INVALID_ARG;
    if (target_layer >= ABACUS88D_NUM_LAYERS) return MATH_ERROR_INVALID_ARG;
    if (dimension >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    if (source_layer == target_layer) return MATH_SUCCESS;
    
    // Get source value
    CrystallineAbacus* source_value = abacus88d->layers[source_layer].dimensions[dimension];
    
    // Calculate scale factor
    uint64_t source_scale = LAYER_SCALES[source_layer];
    uint64_t target_scale = LAYER_SCALES[target_layer];
    
    // Create scaled value
    CrystallineAbacus* scale_factor = abacus_from_uint64(target_scale / source_scale, abacus88d->base);
    if (!scale_factor) return MATH_ERROR_OUT_OF_MEMORY;
    
    CrystallineAbacus* scaled_value = abacus_new(abacus88d->base);
    if (!scaled_value) {
        abacus_free(scale_factor);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    MathError err = abacus_mul(scaled_value, source_value, scale_factor);
    abacus_free(scale_factor);
    
    if (err != MATH_SUCCESS) {
        abacus_free(scaled_value);
        return err;
    }
    
    // Set target value
    err = abacus_copy_to(abacus88d->layers[target_layer].dimensions[dimension], scaled_value);
    abacus_free(scaled_value);
    
    return err;
}

/* ============================================================================
 * DIMENSION OPERATIONS
 * ============================================================================
 */

MathError abacus88d_set(Abacus88D* abacus88d,
                        uint8_t layer,
                        uint8_t dimension,
                        const CrystallineAbacus* value) {
    if (!abacus88d || !value) return MATH_ERROR_NULL_POINTER;
    if (layer >= ABACUS88D_NUM_LAYERS) return MATH_ERROR_INVALID_ARG;
    if (dimension >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    // Threading removed - pure mathematical operation
    // Thread safety should be handled by wrapper in algorithms library
    MathError err = abacus_copy_to(abacus88d->layers[layer].dimensions[dimension], value);
    
    return err;
}

MathError abacus88d_get(const Abacus88D* abacus88d,
                        uint8_t layer,
                        uint8_t dimension,
                        CrystallineAbacus* result) {
    if (!abacus88d || !result) return MATH_ERROR_NULL_POINTER;
    if (layer >= ABACUS88D_NUM_LAYERS) return MATH_ERROR_INVALID_ARG;
    if (dimension >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    return abacus_copy_to(result, abacus88d->layers[layer].dimensions[dimension]);
}

MathError abacus88d_set_uint64(Abacus88D* abacus88d,
                                uint8_t dimension,
                                uint64_t value) {
    if (!abacus88d) return MATH_ERROR_NULL_POINTER;
    if (dimension >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    // Select appropriate layer based on magnitude
    uint8_t layer = abacus88d_select_layer(value);
    
    // Create abacus from uint64
    CrystallineAbacus* abacus_value = abacus_from_uint64(value, abacus88d->base);
    if (!abacus_value) return MATH_ERROR_OUT_OF_MEMORY;
    
    MathError err = abacus88d_set(abacus88d, layer, dimension, abacus_value);
    abacus_free(abacus_value);
    
    return err;
}

uint64_t abacus88d_get_uint64(const Abacus88D* abacus88d,
                               uint8_t layer,
                               uint8_t dimension) {
    if (!abacus88d) return 0;
    if (layer >= ABACUS88D_NUM_LAYERS) return 0;
    if (dimension >= ABACUS88D_DIMS_PER_LAYER) return 0;
    
    uint64_t value;
    MathError err = abacus_to_uint64(abacus88d->layers[layer].dimensions[dimension], &value);
    if (err != MATH_SUCCESS) return 0;
    
    return value;
}

/* ============================================================================
 * CROSS-DIMENSIONAL OPERATIONS
 * ============================================================================
 */

MathError abacus88d_add_cross(Abacus88D* abacus88d,
                               uint8_t layer1, uint8_t dim1,
                               uint8_t layer2, uint8_t dim2,
                               uint8_t result_layer, uint8_t result_dim) {
    if (!abacus88d) return MATH_ERROR_NULL_POINTER;
    if (layer1 >= ABACUS88D_NUM_LAYERS || layer2 >= ABACUS88D_NUM_LAYERS) 
        return MATH_ERROR_INVALID_ARG;
    if (result_layer >= ABACUS88D_NUM_LAYERS) return MATH_ERROR_INVALID_ARG;
    if (dim1 >= ABACUS88D_DIMS_PER_LAYER || dim2 >= ABACUS88D_DIMS_PER_LAYER)
        return MATH_ERROR_INVALID_ARG;
    if (result_dim >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    // Get values
    CrystallineAbacus* val1 = abacus88d->layers[layer1].dimensions[dim1];
    CrystallineAbacus* val2 = abacus88d->layers[layer2].dimensions[dim2];
    
    // If layers are different, scale to common layer first
    CrystallineAbacus* scaled1 = val1;
    CrystallineAbacus* scaled2 = val2;
    bool need_free1 = false;
    bool need_free2 = false;
    
    if (layer1 != result_layer) {
        scaled1 = abacus_new(abacus88d->base);
        if (!scaled1) return MATH_ERROR_OUT_OF_MEMORY;
        need_free1 = true;
        
        // Scale val1 to result_layer
        uint64_t scale = LAYER_SCALES[result_layer] / LAYER_SCALES[layer1];
        CrystallineAbacus* scale_abacus = abacus_from_uint64(scale, abacus88d->base);
        if (!scale_abacus) {
            abacus_free(scaled1);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        MathError err = abacus_mul(scaled1, val1, scale_abacus);
        abacus_free(scale_abacus);
        if (err != MATH_SUCCESS) {
            abacus_free(scaled1);
            return err;
        }
    }
    
    if (layer2 != result_layer) {
        scaled2 = abacus_new(abacus88d->base);
        if (!scaled2) {
            if (need_free1) abacus_free(scaled1);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        need_free2 = true;
        
        // Scale val2 to result_layer
        uint64_t scale = LAYER_SCALES[result_layer] / LAYER_SCALES[layer2];
        CrystallineAbacus* scale_abacus = abacus_from_uint64(scale, abacus88d->base);
        if (!scale_abacus) {
            if (need_free1) abacus_free(scaled1);
            abacus_free(scaled2);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        MathError err = abacus_mul(scaled2, val2, scale_abacus);
        abacus_free(scale_abacus);
        if (err != MATH_SUCCESS) {
            if (need_free1) abacus_free(scaled1);
            abacus_free(scaled2);
            return err;
        }
    }
    
    // Perform addition
    MathError err = abacus_add(abacus88d->layers[result_layer].dimensions[result_dim],
                                scaled1, scaled2);
    
    // Cleanup
    if (need_free1) abacus_free(scaled1);
    if (need_free2) abacus_free(scaled2);
    
    return err;
}

MathError abacus88d_multiply_cross(Abacus88D* abacus88d,
                                    uint8_t layer1, uint8_t dim1,
                                    uint8_t layer2, uint8_t dim2,
                                    uint8_t result_layer, uint8_t result_dim) {
    if (!abacus88d) return MATH_ERROR_NULL_POINTER;
    if (layer1 >= ABACUS88D_NUM_LAYERS || layer2 >= ABACUS88D_NUM_LAYERS) 
        return MATH_ERROR_INVALID_ARG;
    if (result_layer >= ABACUS88D_NUM_LAYERS) return MATH_ERROR_INVALID_ARG;
    if (dim1 >= ABACUS88D_DIMS_PER_LAYER || dim2 >= ABACUS88D_DIMS_PER_LAYER)
        return MATH_ERROR_INVALID_ARG;
    if (result_dim >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    // Get values
    CrystallineAbacus* val1 = abacus88d->layers[layer1].dimensions[dim1];
    CrystallineAbacus* val2 = abacus88d->layers[layer2].dimensions[dim2];
    
    // Multiply
    return abacus_mul(abacus88d->layers[result_layer].dimensions[result_dim],
                      val1, val2);
}

/* ============================================================================
 * GEOMETRIC OPERATIONS
 * ============================================================================
 */

MathError abacus88d_find_boundaries(Abacus88D* abacus88d,
                                     uint8_t layer1,
                                     uint8_t layer2,
                                     GeometricBoundary88D** boundaries,
                                     size_t* num_boundaries) {
    if (!abacus88d || !boundaries || !num_boundaries) return MATH_ERROR_NULL_POINTER;
    if (layer1 >= ABACUS88D_NUM_LAYERS || layer2 >= ABACUS88D_NUM_LAYERS)
        return MATH_ERROR_INVALID_ARG;
    
    // TODO: Implement proper geometric boundary detection
    // For now, return empty set
    *boundaries = NULL;
    *num_boundaries = 0;
    
    return MATH_SUCCESS;
}

MathError abacus88d_transfer_boundary(Abacus88D* abacus88d,
                                       GeometricBoundary88D* boundary,
                                       uint8_t source_dim,
                                       uint8_t target_dim) {
    if (!abacus88d || !boundary) return MATH_ERROR_NULL_POINTER;
    if (source_dim >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    if (target_dim >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    // TODO: Implement boundary transfer
    return MATH_SUCCESS;
}

/* ============================================================================
 * TETRATION OPERATIONS
 * ============================================================================
 */

MathError abacus88d_apply_tetration(Abacus88D* abacus88d,
                                     TetrationTower88D* tower,
                                     uint8_t dimension) {
    if (!abacus88d || !tower) return MATH_ERROR_NULL_POINTER;
    if (dimension >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    // TODO: Implement tetration application
    return MATH_SUCCESS;
}

TetrationTower88D* abacus88d_find_tower(Abacus88D* abacus88d,
                                         uint8_t source_layer,
                                         uint8_t target_layer) {
    if (!abacus88d || !abacus88d->tetration) return NULL;
    if (source_layer >= ABACUS88D_NUM_LAYERS) return NULL;
    if (target_layer >= ABACUS88D_NUM_LAYERS) return NULL;
    
    // Search for tower connecting these layers
    for (size_t i = 0; i < abacus88d->tetration->num_towers; i++) {
        if (abacus88d->tetration->tower_layer_map[i][0] == source_layer &&
            abacus88d->tetration->tower_layer_map[i][1] == target_layer) {
            return &abacus88d->tetration->towers[i];
        }
    }
    
    return NULL;
}

/* ============================================================================
 * PRIME GENERATION
 * ============================================================================
 */

MathError abacus88d_generate_prime(Abacus88D* abacus88d,
                                    uint8_t layer,
                                    uint8_t dimension,
                                    uint64_t magnitude,
                                    CrystallineAbacus* prime) {
    if (!abacus88d || !prime) return MATH_ERROR_NULL_POINTER;
    if (layer >= ABACUS88D_NUM_LAYERS) return MATH_ERROR_INVALID_ARG;
    if (dimension >= ABACUS88D_DIMS_PER_LAYER) return MATH_ERROR_INVALID_ARG;
    
    // Map dimension to valid clock position
    // Only positions 0, 1, 2, 3, 6, 9 have primes in the clock lattice
    // Map 11 dimensions to these 6 positions cyclically
    static const uint32_t valid_positions[] = {0, 1, 2, 3, 6, 9};
    uint32_t position = valid_positions[dimension % 6];
    
    // Use clock lattice to generate prime
    uint64_t prime_value = clock_position_to_prime_exact(0, position, magnitude);
    
    if (prime_value == 0) return MATH_ERROR_INVALID_ARG;
    
    // Convert to abacus
    CrystallineAbacus* temp = abacus_from_uint64(prime_value, abacus88d->base);
    if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
    
    MathError err = abacus_copy_to(prime, temp);
    abacus_free(temp);
    
    return err;
}

MathError abacus88d_map_prime(Abacus88D* abacus88d,
                               const CrystallineAbacus* prime,
                               uint8_t* layer,
                               uint8_t* dimension,
                               uint64_t* magnitude) {
    if (!abacus88d || !prime || !layer || !dimension || !magnitude) 
        return MATH_ERROR_NULL_POINTER;
    
    // Convert prime to uint64
    uint64_t prime_value;
    MathError err_convert = abacus_to_uint64(prime, &prime_value);
    if (err_convert != MATH_SUCCESS) return err_convert;
    
    // Map to clock position
    ClockPosition pos;
    MathError err = clock_map_prime_to_position(prime_value, &pos);
    if (err != MATH_SUCCESS) return err;
    
    // Determine layer from magnitude
    *layer = abacus88d_select_layer(prime_value);
    
    // Map clock position to dimension (position 1-11 → dimension 0-10)
    if (pos.position == 0 || pos.position > 11) return MATH_ERROR_INVALID_ARG;
    *dimension = pos.position - 1;
    
    // Calculate magnitude (which "lap" around the clock)
    *magnitude = prime_value / 12;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

const char* abacus88d_layer_name(Layer88DType type) {
    switch (type) {
        case LAYER_TETRAHEDRON: return "Tetrahedron";
        case LAYER_CUBE: return "Cube";
        case LAYER_OCTAHEDRON: return "Octahedron";
        case LAYER_DODECAHEDRON: return "Dodecahedron";
        case LAYER_ICOSAHEDRON: return "Icosahedron";
        case LAYER_TETRAHEDRON_2: return "Tetrahedron";
        case LAYER_CUBE_2: return "Cube";
        case LAYER_OCTAHEDRON_2: return "Octahedron";
        default: return "Unknown";
    }
}

void abacus88d_print(const Abacus88D* abacus88d) {
    if (!abacus88d) return;
    
    printf("88D Abacus (base %u)\n", abacus88d->base);
    printf("Active: Layer %u, Dimension %u\n", 
           abacus88d->active_layer, abacus88d->active_dimension);
    printf("\n");
    
    for (int layer = 0; layer < ABACUS88D_NUM_LAYERS; layer++) {
        printf("Layer %d: %s (scale: %lu)\n", 
               layer, 
               abacus88d_layer_name(abacus88d->layers[layer].type),
               abacus88d->layers[layer].magnitude_scale);
        
        for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
            uint64_t value;
            if (abacus_to_uint64(abacus88d->layers[layer].dimensions[dim], &value) == MATH_SUCCESS) {
                if (value != 0) {
                    printf("  Dim %2d (pos %2d): %lu\n", dim, dim + 1, value);
                }
            }
        }
    }
}

void abacus88d_print_layer(const Abacus88D* abacus88d, uint8_t layer) {
    if (!abacus88d || layer >= ABACUS88D_NUM_LAYERS) return;
    
    printf("Layer %d: %s (scale: %lu)\n", 
           layer, 
           abacus88d_layer_name(abacus88d->layers[layer].type),
           abacus88d->layers[layer].magnitude_scale);
    
    for (int dim = 0; dim < ABACUS88D_DIMS_PER_LAYER; dim++) {
        uint64_t value;
        if (abacus_to_uint64(abacus88d->layers[layer].dimensions[dim], &value) == MATH_SUCCESS) {
            printf("  Dim %2d (pos %2d): %lu\n", dim, dim + 1, value);
        }
    }
}