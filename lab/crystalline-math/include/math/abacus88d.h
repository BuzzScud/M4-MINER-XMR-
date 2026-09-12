/**
 * @file abacus88d.h
 * @brief 88-Dimensional Geometric Abacus System
 * 
 * This module implements the 88D geometric computational space where:
 * - 88 dimensions = 8 layers × 11 dimensions per layer
 * - Each layer uses a different Platonic solid as coordinate frame
 * - Layers represent magnitude scales (10^0, 10^3, 10^6, ..., 10^21)
 * - Dimensions correspond to clock positions 1-11 (excluding control position 0)
 * - Platonic solids fold into each other through shared vertices
 * - Tetration towers create non-overlapping prime distributions
 * 
 * Key Principles:
 * - NOT counting units - counting GEOMETRIC POINTS
 * - Prime generation through geometric point counting (no lookup tables)
 * - 12-fold symmetry maintained across all layers
 * - Shared memory at geometric boundaries (platonic solid vertices)
 * - Exact arithmetic via CrystallineAbacus at each dimension
 * 
 * Revolutionary Approach:
 * - Each dimension has its own CrystallineAbacus for exact computation
 * - Layers are connected through platonic solid geometry
 * - Operations work across dimensions using geometric relationships
 * - Tetration attractors guide cross-layer operations
 * - Prime generation is deterministic through clock lattice
 */

#ifndef MATH_ABACUS88D_H
#define MATH_ABACUS88D_H

#include "abacus.h"
#include "clock.h"
#include "platonic_generator.h"
#include <stdint.h>
#include <stdbool.h>
// Threading removed - math library is pure mathematics
// Threading support should be in algorithms library

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================
 */

#define ABACUS88D_NUM_LAYERS 8      // 8 magnitude layers
#define ABACUS88D_DIMS_PER_LAYER 11 // 11 dimensions per layer (positions 1-11)
#define ABACUS88D_TOTAL_DIMS 88     // 8 × 11 = 88 total dimensions

/* ============================================================================
 * LAYER STRUCTURE
 * ============================================================================
 */

/**
 * @brief Platonic solid type for each layer
 * 
 * Each layer uses a different Platonic solid as its coordinate frame.
 * The solids provide geometric structure for operations within that layer.
 */
typedef enum {
    LAYER_TETRAHEDRON = 0,   // Layer 0: 10^0  to 10^3   (4 vertices)
    LAYER_CUBE = 1,          // Layer 1: 10^3  to 10^6   (8 vertices)
    LAYER_OCTAHEDRON = 2,    // Layer 2: 10^6  to 10^9   (6 vertices)
    LAYER_DODECAHEDRON = 3,  // Layer 3: 10^9  to 10^12  (20 vertices)
    LAYER_ICOSAHEDRON = 4,   // Layer 4: 10^12 to 10^15  (12 vertices)
    LAYER_TETRAHEDRON_2 = 5, // Layer 5: 10^15 to 10^18  (repeat pattern)
    LAYER_CUBE_2 = 6,        // Layer 6: 10^18 to 10^21
    LAYER_OCTAHEDRON_2 = 7   // Layer 7: 10^21+
} Layer88DType;

/**
 * @brief Magnitude scale for each layer
 * 
 * Each layer handles a different magnitude range.
 * These are the base scales - actual values use CrystallineAbacus for precision.
 */
static const uint64_t LAYER_SCALES[ABACUS88D_NUM_LAYERS] = {
    1ULL,                    // Layer 0: 10^0
    1000ULL,                 // Layer 1: 10^3
    1000000ULL,              // Layer 2: 10^6
    1000000000ULL,           // Layer 3: 10^9
    1000000000000ULL,        // Layer 4: 10^12
    1000000000000000ULL,     // Layer 5: 10^15
    1000000000000000000ULL,  // Layer 6: 10^18
    1000000000000000000ULL   // Layer 7: 10^21 (use abacus for exact)
};

/**
 * @brief A single layer in the 88D space
 * 
 * Each layer contains 11 dimensions (clock positions 1-11).
 * Each dimension has its own CrystallineAbacus for exact computation.
 * The layer uses a Platonic solid as its geometric coordinate frame.
 */
typedef struct {
    // Dimensional values (11 abacus, one per dimension)
    CrystallineAbacus* dimensions[ABACUS88D_DIMS_PER_LAYER];
    
    // Geometric structure
    Layer88DType type;              // Which Platonic solid
    PlatonicSolid* frame;           // Platonic solid coordinate frame
    uint8_t layer_index;            // 0-7
    uint64_t magnitude_scale;       // Base scale for this layer
    
    // Shared vertices (geometric boundaries with other layers)
    void* shared_vertices;          // SharedVertex* from geometric_anchors.h
    size_t num_shared_vertices;
    
    // Thread safety
    // Threading removed - pure mathematical structure
    // Thread safety should be handled by wrapper in algorithms library
    
} Layer88D;

/* ============================================================================
 * TETRATION TOWER STRUCTURE
 * ============================================================================
 */

/**
 * @brief Tetration tower for cross-layer relationships
 * 
 * Tetration towers create non-overlapping prime distributions.
 * Each tower connects layers through exponential relationships.
 * 
 * Example: base=2, depth=4 → 2^2^2^2 = 65536
 */
typedef struct {
    uint64_t base;              // Base of tetration (2, 3, 5, 7, 11, 13)
    uint32_t depth;             // Depth of tower (1-31)
    CrystallineAbacus* value;   // Computed value (exact)
    bool converged;             // Has tower converged?
    
    // Layer connections
    uint8_t source_layer;       // Which layer this tower starts from
    uint8_t target_layer;       // Which layer this tower reaches
    
} TetrationTower88D;

/**
 * @brief Tetration attractor system for 88D space
 * 
 * 186 tetration towers (6 bases × 31 depths) guide operations
 * across layers and dimensions.
 */
typedef struct {
    TetrationTower88D towers[186];  // 6 bases × 31 depths
    size_t num_towers;
    
    // Mapping: which towers affect which layers
    uint8_t tower_layer_map[186][2]; // [tower_id][source_layer, target_layer]
    
} TetrationSystem88D;

/* ============================================================================
 * GEOMETRIC BOUNDARY STRUCTURE
 * ============================================================================
 */

/**
 * @brief Shared geometric boundary between layers
 * 
 * Platonic solids share vertices where they "fold" into each other.
 * These shared vertices are geometric boundaries where values can
 * transfer between layers.
 */
typedef struct {
    // Which layers share this boundary
    uint8_t layer1;
    uint8_t layer2;
    
    // Geometric position (13D coordinates from geometric_anchors.h)
    double position[13];
    
    // Which dimensions are connected at this boundary
    uint8_t connected_dims[ABACUS88D_DIMS_PER_LAYER];
    size_t num_connected;
    
    // Shared memory for cross-layer operations
    CrystallineAbacus* shared_value;
    // Threading removed - pure mathematical structure
    
} GeometricBoundary88D;

/* ============================================================================
 * MAIN 88D ABACUS STRUCTURE
 * ============================================================================
 */

/**
 * @brief The complete 88-dimensional geometric abacus
 * 
 * This is the main structure that represents the entire 88D computational space.
 * It contains:
 * - 8 layers, each with 11 dimensions
 * - Geometric structure (Platonic solids)
 * - Tetration towers for cross-layer operations
 * - Shared boundaries for geometric folding
 * - Clock lattice for prime generation
 */
typedef struct {
    // The 8 layers (each with 11 dimensions)
    Layer88D layers[ABACUS88D_NUM_LAYERS];
    
    // Tetration tower system
    TetrationSystem88D* tetration;
    
    // Geometric boundaries (where layers fold into each other)
    GeometricBoundary88D* boundaries;
    size_t num_boundaries;
    
    // Clock lattice for prime generation
    ClockContext* clock;
    
    // Current active layer and dimension
    uint8_t active_layer;
    uint8_t active_dimension;
    
    // Base for all abacus (typically 60 for Babylonian)
    uint32_t base;
    
    // Thread safety for global operations
    // Threading removed - pure mathematical structure
    
} Abacus88D;

/* ============================================================================
 * INITIALIZATION AND CLEANUP
 * ============================================================================
 */

/**
 * @brief Create a new 88D abacus
 * 
 * Initializes all 88 dimensions (8 layers × 11 dimensions) with
 * CrystallineAbacus instances, sets up Platonic solid frames,
 * creates tetration towers, and establishes geometric boundaries.
 * 
 * @param base Number base for all abacus (typically 60)
 * @return Pointer to new Abacus88D, or NULL on error
 */
Abacus88D* abacus88d_create(uint32_t base);

/**
 * @brief Free an 88D abacus
 * 
 * Frees all resources including all 88 CrystallineAbacus instances,
 * Platonic solid frames, tetration towers, and geometric boundaries.
 * 
 * @param abacus88d Abacus to free
 */
void abacus88d_free(Abacus88D* abacus88d);

/**
 * @brief Initialize all dimensions to zero
 * 
 * @param abacus88d Abacus to initialize
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_init_zero(Abacus88D* abacus88d);

/* ============================================================================
 * LAYER OPERATIONS
 * ============================================================================
 */

/**
 * @brief Get the appropriate layer for a given magnitude
 * 
 * Determines which layer (0-7) should handle a value based on its magnitude.
 * Uses logarithmic scaling to select layer.
 * 
 * @param magnitude Value magnitude
 * @return Layer index (0-7)
 */
uint8_t abacus88d_select_layer(uint64_t magnitude);

/**
 * @brief Get the appropriate layer for an abacus value
 * 
 * @param value CrystallineAbacus value
 * @return Layer index (0-7)
 */
uint8_t abacus88d_select_layer_from_abacus(const CrystallineAbacus* value);

/**
 * @brief Get the Platonic solid frame for a layer
 * 
 * @param abacus88d Abacus
 * @param layer Layer index (0-7)
 * @return Pointer to Platonic solid frame, or NULL on error
 */
PlatonicSolid* abacus88d_get_layer_frame(Abacus88D* abacus88d, uint8_t layer);

/**
 * @brief Scale value to different layer
 * 
 * Moves a value from one layer to another, applying appropriate
 * magnitude scaling based on the layer scales.
 * 
 * @param abacus88d Abacus
 * @param source_layer Source layer (0-7)
 * @param target_layer Target layer (0-7)
 * @param dimension Dimension within layer (0-10)
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_scale_to_layer(Abacus88D* abacus88d,
                                    uint8_t source_layer,
                                    uint8_t target_layer,
                                    uint8_t dimension);

/* ============================================================================
 * DIMENSION OPERATIONS
 * ============================================================================
 */

/**
 * @brief Set value at specific layer and dimension
 * 
 * @param abacus88d Abacus
 * @param layer Layer index (0-7)
 * @param dimension Dimension index (0-10, corresponding to positions 1-11)
 * @param value Value to set
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_set(Abacus88D* abacus88d,
                        uint8_t layer,
                        uint8_t dimension,
                        const CrystallineAbacus* value);

/**
 * @brief Get value at specific layer and dimension
 * 
 * @param abacus88d Abacus
 * @param layer Layer index (0-7)
 * @param dimension Dimension index (0-10)
 * @param result Output value (must be pre-allocated)
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_get(const Abacus88D* abacus88d,
                        uint8_t layer,
                        uint8_t dimension,
                        CrystallineAbacus* result);

/**
 * @brief Set value from uint64
 * 
 * Convenience function that automatically selects appropriate layer
 * based on magnitude.
 * 
 * @param abacus88d Abacus
 * @param dimension Dimension index (0-10)
 * @param value Value to set
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_set_uint64(Abacus88D* abacus88d,
                                uint8_t dimension,
                                uint64_t value);

/**
 * @brief Get value as uint64
 * 
 * @param abacus88d Abacus
 * @param layer Layer index (0-7)
 * @param dimension Dimension index (0-10)
 * @return Value as uint64, or 0 on error
 */
uint64_t abacus88d_get_uint64(const Abacus88D* abacus88d,
                               uint8_t layer,
                               uint8_t dimension);

/* ============================================================================
 * CROSS-DIMENSIONAL OPERATIONS
 * ============================================================================
 */

/**
 * @brief Add values across dimensions
 * 
 * Adds values from two dimensions, handling cross-layer operations
 * if needed. Uses geometric boundaries for cross-layer addition.
 * 
 * @param abacus88d Abacus
 * @param layer1 First layer
 * @param dim1 First dimension
 * @param layer2 Second layer
 * @param dim2 Second dimension
 * @param result_layer Output layer
 * @param result_dim Output dimension
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_add_cross(Abacus88D* abacus88d,
                               uint8_t layer1, uint8_t dim1,
                               uint8_t layer2, uint8_t dim2,
                               uint8_t result_layer, uint8_t result_dim);

/**
 * @brief Multiply values across dimensions
 * 
 * @param abacus88d Abacus
 * @param layer1 First layer
 * @param dim1 First dimension
 * @param layer2 Second layer
 * @param dim2 Second dimension
 * @param result_layer Output layer
 * @param result_dim Output dimension
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_multiply_cross(Abacus88D* abacus88d,
                                    uint8_t layer1, uint8_t dim1,
                                    uint8_t layer2, uint8_t dim2,
                                    uint8_t result_layer, uint8_t result_dim);

/* ============================================================================
 * GEOMETRIC OPERATIONS
 * ============================================================================
 */

/**
 * @brief Find geometric boundaries between two layers
 * 
 * Identifies shared vertices where two Platonic solids fold into each other.
 * These boundaries enable cross-layer operations.
 * 
 * @param abacus88d Abacus
 * @param layer1 First layer
 * @param layer2 Second layer
 * @param boundaries Output array of boundaries
 * @param num_boundaries Output number of boundaries found
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_find_boundaries(Abacus88D* abacus88d,
                                     uint8_t layer1,
                                     uint8_t layer2,
                                     GeometricBoundary88D** boundaries,
                                     size_t* num_boundaries);

/**
 * @brief Transfer value across geometric boundary
 * 
 * Moves a value from one layer to another through a shared geometric boundary.
 * This is how values "fold" between layers.
 * 
 * @param abacus88d Abacus
 * @param boundary Geometric boundary to use
 * @param source_dim Source dimension
 * @param target_dim Target dimension
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_transfer_boundary(Abacus88D* abacus88d,
                                       GeometricBoundary88D* boundary,
                                       uint8_t source_dim,
                                       uint8_t target_dim);

/* ============================================================================
 * TETRATION OPERATIONS
 * ============================================================================
 */

/**
 * @brief Apply tetration tower to move between layers
 * 
 * Uses a tetration tower to scale a value from one layer to another.
 * Tetration towers create the exponential relationships between layers.
 * 
 * @param abacus88d Abacus
 * @param tower Tetration tower to use
 * @param dimension Dimension to operate on
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_apply_tetration(Abacus88D* abacus88d,
                                     TetrationTower88D* tower,
                                     uint8_t dimension);

/**
 * @brief Find tetration tower connecting two layers
 * 
 * @param abacus88d Abacus
 * @param source_layer Source layer
 * @param target_layer Target layer
 * @return Pointer to tower, or NULL if none found
 */
TetrationTower88D* abacus88d_find_tower(Abacus88D* abacus88d,
                                         uint8_t source_layer,
                                         uint8_t target_layer);

/* ============================================================================
 * PRIME GENERATION (Geometric Point Counting)
 * ============================================================================
 */

/**
 * @brief Generate prime at specific layer and dimension
 * 
 * Uses clock lattice to generate prime deterministically.
 * This is NOT trial division - it's geometric point counting.
 * 
 * @param abacus88d Abacus
 * @param layer Layer index
 * @param dimension Dimension index (maps to clock position 1-11)
 * @param magnitude Which "lap" around the clock
 * @param prime Output prime number
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_generate_prime(Abacus88D* abacus88d,
                                    uint8_t layer,
                                    uint8_t dimension,
                                    uint64_t magnitude,
                                    CrystallineAbacus* prime);

/**
 * @brief Map prime to 88D coordinates
 * 
 * Given a prime, determines which layer and dimension it belongs to.
 * 
 * @param abacus88d Abacus
 * @param prime Prime number
 * @param layer Output layer
 * @param dimension Output dimension
 * @param magnitude Output magnitude
 * @return MATH_SUCCESS or error code
 */
MathError abacus88d_map_prime(Abacus88D* abacus88d,
                               const CrystallineAbacus* prime,
                               uint8_t* layer,
                               uint8_t* dimension,
                               uint64_t* magnitude);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Get name of layer type
 * 
 * @param type Layer type
 * @return String name of Platonic solid
 */
const char* abacus88d_layer_name(Layer88DType type);

/**
 * @brief Print 88D abacus state
 * 
 * @param abacus88d Abacus to print
 */
void abacus88d_print(const Abacus88D* abacus88d);

/**
 * @brief Print specific layer
 * 
 * @param abacus88d Abacus
 * @param layer Layer index
 */
void abacus88d_print_layer(const Abacus88D* abacus88d, uint8_t layer);

#ifdef __cplusplus
}
#endif

#endif // MATH_ABACUS88D_H