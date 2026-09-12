/*
 * Prime Math Library - Core Type Definitions
 * 
 * This file contains ALL fundamental type definitions for the
 * Crystalline Lattice Arbitrary Precision Math Library.
 * 
 * CRITICAL: This library uses ARBITRARY PRECISION for all calculations.
 * Floating point is ONLY used for type conversion and user output.
 */

#ifndef PRIME_TYPES_H
#define PRIME_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "types.h"  // PHASE 2: For MATH_PI

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ============================================================================
 * BIGINT - Arbitrary Precision Integer
 * ============================================================================
 * 
 * The fundamental type for all integer calculations in the library.
 * Uses dynamic array of 32-bit digits for unlimited precision.
 */
typedef struct {
    uint32_t *d;              // Array of 32-bit digits (little-endian)
    size_t len;               // Current number of digits used
    size_t capacity;          // Allocated capacity
    size_t target_len;        // Target length for operations
    size_t extended_len;      // Extended length for special operations
    int negative;             // Sign: 0 = positive, 1 = negative
    int extended_mode;        // Extended mode flag
} BigInt;

/*
 * ============================================================================
 * BIGFIXED - Arbitrary Precision Fixed-Point
 * ============================================================================
 * 
 * Fixed-point representation for fractional calculations.
 * Maintains both integer and fractional parts with arbitrary precision.
 */
typedef struct {
    BigInt *integer_part;     // Integer portion (pointer)
    BigInt *fractional_part;  // Fractional portion (pointer)
    int negative;             // Sign: 0 = positive, 1 = negative
    size_t scale_bits;        // Number of fractional bits
} BigFixed;

/*
 * ============================================================================
 * CRYSTAL ABACUS - Prime Number Generator
 * ============================================================================
 * 
 * The Crystalline Lattice Abacus - fundamental calculator for the system.
 * Maintains state for efficient prime generation and can be retained in
 * memory for the entire program lifetime.
 * 
 * Thread-safe: Can be copied for simultaneous calculations in multi-threaded
 * environments.
 */

// Hash table node for seen numbers
typedef struct SeenNode {
    int value;
    struct SeenNode *next;
} SeenNode;

// Hash table for tracking seen numbers
typedef struct {
    SeenNode **buckets;
    int num_buckets;
} SeenTable;

// Crystal Abacus structure
typedef struct {
    int *primes;              // Array of generated primes
    size_t num_primes;        // Number of primes generated
    size_t capacity;          // Allocated capacity
    int candidate;            // Current candidate for prime testing
    SeenTable *seen;          // Hash table for seen numbers
} CrystalAbacus;

/*
 * ============================================================================
 * BIGINT CRYSTAL ABACUS - Arbitrary Precision Prime Generator
 * ============================================================================
 * 
 * Extended version of Crystal Abacus using BigInt for unlimited prime size.
 */
typedef struct {
    BigInt *primes;           // Array of BigInt primes
    size_t num_primes;        // Number of primes generated
    size_t capacity;          // Allocated capacity
    BigInt candidate;         // Current candidate (BigInt)
    uint64_t current_index;   // Current index for iteration
} CrystalAbacusBig;

/*
 * ============================================================================
 * MATRIX - Double Precision Matrix
 * ============================================================================
 * 
 * Matrix structure for double precision operations.
 * For arbitrary precision, use BigMatrix.
 */
typedef struct {
    int rows;
    int cols;
    double **data;
} Matrix;

/*
 * ============================================================================
 * BIGMATRIX - Arbitrary Precision Matrix
 * ============================================================================
 * 
 * Matrix structure using BigInt for arbitrary precision calculations.
 */
typedef struct {
    int rows;
    int cols;
    BigInt ***data;           // 2D array of BigInt pointers
} BigMatrix;

/*
 * ============================================================================
 * HYPERVECTOR - Hyperdimensional Vector (Double Precision)
 * ============================================================================
 * 
 * Vector in arbitrary dimensions using double precision.
 * For arbitrary precision vector operations, use BigHyperVector.
 */
typedef struct {
    double *components;       // Array of double components
    int dim;                  // Number of dimensions
} HyperVector;

/*
 * ============================================================================
 * BIGHYPERVECTOR - Arbitrary Precision Hyperdimensional Vector
 * ============================================================================
 * 
 * Vector in arbitrary dimensions using BigInt for arbitrary precision.
 */
typedef struct {
    BigInt **components;      // Array of BigInt pointers
    int dim;                  // Number of dimensions
} BigHyperVector;

/*
 * ============================================================================
 * LATTICE POINT - Point in Prime Lattice
 * ============================================================================
 * 
 * Represents a point in the crystalline prime lattice structure.
 */
typedef struct {
    BigInt coordinates[3];    // 3D coordinates in lattice
    BigInt prime_value;       // Associated prime number
    int symmetry_group;       // Symmetry group classification
    double resonance;         // Resonance value (for visualization only)
} LatticePoint;

/*
 * ============================================================================
 * LATTICE STRUCTURE - Complete Lattice
 * ============================================================================
 * 
 * Complete crystalline lattice structure containing all points.
 */
typedef struct {
    LatticePoint *points;     // Array of lattice points
    size_t num_points;        // Number of points
    size_t capacity;          // Allocated capacity
    BigInt origin[3];         // Origin coordinates
    int symmetry_order;       // Symmetry order (typically 12)
} Lattice;

/*
 * ============================================================================
 * GEOMETRIC TYPES - Clock and Lattice Geometry
 * ============================================================================
 */

// Legacy clock position on 12-hour face (DEPRECATED - use BabylonianClockPosition from clock_lattice.h)
typedef struct {
    int degree;               // Degree on 361-circle (0-360)
    double angle_radians;     // Angle in radians
    int position;             // Clock position (0-11)
    int quadrant;             // Quadrant (1-4)
    bool on_boundary;         // On 3 o'clock boundary
    double theta;             // Theta angle (for lattice_core compatibility)
    double r;                 // Radial distance (for lattice_core compatibility)
    uint8_t clock_pos;        // Clock position as uint8_t (for lattice_core compatibility)
} LegacyClockPosition;

// Quadrant folding information
typedef struct {
    int quadrant;             // Original quadrant (1-4)
    double folded_angle;      // Angle folded to Q1
    bool reflected_x;         // X-axis reflection flag
    bool reflected_y;         // Y-axis reflection flag
    bool flip_x;              // X-axis flip flag (alias)
    bool flip_y;              // Y-axis flip flag (alias)
    int polarity;             // Polarity indicator
} QuadrantFold;

// Lattice sphere for kissing sphere packing
typedef struct {
    double center_x;          // X coordinate of center
    double center_y;          // Y coordinate of center
    double center_z;          // Z coordinate of center
    double radius;            // Sphere radius
    uint64_t prime;           // Associated prime number
} LatticeSphere;

// 2D vector for geometric operations
typedef struct {
    double x;                 // X component
    double y;                 // Y component
} Vector2D;

// Pythagorean triple for lattice geometry
typedef struct {
    uint64_t a;               // p² - q²
    uint64_t b;               // 2pq
    uint64_t c;               // p² + q²
    int layer;                // n mod 3 (for 3-renewal)
    uint64_t p;               // First parameter
    uint64_t q;               // Second parameter
    double ratio;             // Ratio p/q
} PythagoreanTriple;

// Vedic triple (similar to Pythagorean)
typedef struct {
    uint64_t a;
    uint64_t b;
    uint64_t c;
} VedicTriple;

// 12-dimensional manifold point
typedef struct {
    double coordinates[12];
    uint64_t prime;
    uint64_t index;
} M12Point;

// 15-dimensional lattice embedding
typedef struct {
    uint64_t residues[15];
    uint64_t value;
} LatticeEmbed15D;

// Generic lattice embedding (alias for compatibility)
typedef LatticeEmbed15D LatticeEmbed;

// Complete clock mapping with all lattice properties
typedef struct {
    LegacyClockPosition clock;      // Basic clock position (legacy)
    double theta;             // θ(n,k,λ,ω,ψ)
    double r;                 // r(p)
    double O_exp;             // O(n,k,λ)
    double L_value;           // L(n,d,k,λ)
    M12Point m12;             // M₁₂ projection
    LatticeEmbed15D embed;    // 15D embedding
    PythagoreanTriple psi;    // ψ(p,q) triple
} CompleteClockMapping;

// Known lattice artifact
typedef struct {
    double L_value;           // L function value
    const char *artifact;     // Artifact name
    const char *description;  // Description
} KnownArtifact;

/*
 * ============================================================================
 * RAINBOW TABLE - Prime Rainbow Structure
 * ============================================================================
 */

// Rainbow table entry - OPTIMIZED VERSION
// Stores prime INDEX instead of BigInt value (10x memory reduction)
// Prime value generated on-demand using deterministic functions
typedef struct {
    uint32_t prime_index;     // Prime index (1-based): 1st prime, 2nd prime, etc.
    uint8_t symmetry_group;   // 0-11 (12-fold symmetry: prime % 12)
    uint8_t ring;             // Clock ring (0-7 for Babylonian structure)
    uint16_t position;        // Position in ring
} ClockLatticeEntry;  // Renamed from ClockLatticeEntry to avoid conflict with math/rainbow.h

// Legacy tree node structure - DEPRECATED, kept for compatibility
// TODO: Remove after migration to array-based structure
typedef struct PrimeRainbowNode {
    BigInt *prime;                         // Prime number (legacy)
    struct PrimeRainbowNode **children;    // Child nodes (legacy)
    int child_count;                       // Number of children (legacy)
    int capacity;                          // Allocated capacity (legacy)
    // Note: This structure is deprecated and will be removed in future versions
    // Use the optimized array-based ClockLatticeEntry structure instead
} PrimeRainbowNode;

// Rainbow table structure - OPTIMIZED VERSION
// Simple array instead of tree structure (2-5x speed improvement)
// O(1) access time vs O(log n) tree traversal
typedef struct {
    // NEW: Optimized array-based storage
    ClockLatticeEntry* entries;    // Simple array of entries (O(1) access)
    uint32_t count;           // Number of entries
    uint32_t capacity;        // Allocated capacity
    
    // LEGACY: Tree-based storage (deprecated, for backward compatibility)
    PrimeRainbowNode *root;   // Root node (legacy, will be removed)
    
    // Metadata (preserved for compatibility)
    bool is_stable;                       // Stability flag
    double fold_progression[12];          // Fold progression values
    double negative_space[12];            // Negative space values
} PrimeRainbowTable;

/*
 * ============================================================================
 * STATISTICS AND METADATA
 * ============================================================================
 */

// Prime math statistics
typedef struct {
    size_t lattice_depth;           // Current lattice depth
    size_t prime_cache_size;        // Size of prime cache
    size_t rainbow_table_count;     // Rainbow table entries
    int rainbow_stable;             // Rainbow stability flag
    double rainbow_self_similarity; // Self-similarity measure
    size_t abacus_primes_generated; // Primes generated by abacus
} PrimeMathStats;

/*
 * ============================================================================
 * CONSTANTS
 * ============================================================================
 */

// Maximum dimensions for various structures
#define MAX_BIGINT_DIGITS 1024        // Maximum digits for BigInt
#define MAX_MATRIX_SIZE 256           // Maximum matrix dimension
#define MAX_HYPERDIM 64               // Maximum hyperdimensions
#define DEFAULT_ABACUS_CAPACITY 1000  // Default prime cache size

// Symmetry and lattice constants
#define SYMMETRY_ORDER 12             // 12-fold symmetry (clock structure)
#define GOLDEN_RATIO 1.618033988749895
#define LATTICE_SQRT5 2.23606797749979     // Square root of 5

// Einstein's Λ (Lambda) correction - CRITICAL CONSTANT
#define EINSTEIN_LAMBDA_NUMERATOR 3
#define EINSTEIN_LAMBDA_DENOMINATOR 144000
#define EINSTEIN_LAMBDA (3.0 / 144000.0)  // ω = 3/144000

// Vector culmination and twin primes

// π dust (Zu Chongzhi approximation) - Use MATH_PI_DUST_NUM/DEN from math/constants.h

// Cymatic frequencies (Hz)
#define CYMATIC_BASE_432 432          // 432 Hz triad chant
#define CYMATIC_DNA_528 528           // 528 Hz DNA repair
#define CYMATIC_TRANSFORM_639 639     // 639 Hz transformation
#define CYMATIC_AWAKEN_741 741        // 741 Hz awakening
#define CYMATIC_INTUITION_852 852     // 852 Hz intuition
#define CYMATIC_SPIRIT_963 963        // 963 Hz spirit

// Earth and cosmic resonances
#define SCHUMANN_RESONANCE 7.83       // Earth resonance (Hz)
#define GAMMA_BURST_FREQUENCY 40      // 40 Hz consciousness binding

// Cycle constants
#define SAROS_CYCLE 223               // Eclipse cycle (months)
#define METONIC_CYCLE 235             // Lunar-solar sync (months)
#define METONIC_YEARS 19              // 19-year cycle
#define CROWN_DAYS 31                 // Leonardo crown
#define SOLAR_RAYS 7                  // 7 days, 7 planets
#define ZODIAC_DIVISIONS 12           // 12 hours, 12 months

// Geometric mapping constants
#define SQUARE_CIRCLE_MAP 361         // 19² = 361 (squares to circles)
#define CIRCLE_DEGREES 360            // Degrees in a circle
#define CLOCK_POSITIONS 12            // 12 positions on clock face
#define BOUNDARY_PRIME 143999         // Boundary prime (inside edge)

// Lattice core constants
#define NUM_SMALL_PRIMES 18           // Number of foundation primes
#define NUM_PHI_FREQS 12              // Number of dimensional frequencies
#define NUM_VIABLE_RESIDUES 8         // Number of viable residues (mod 30)
#define NUM_QR_ALLOW 2                // Number of allowed quadratic residues
#define NUM_CRNS_FREQ 6               // Number of CRNS frequencies
#define NUM_VEDIC_TRIPLES 6           // Number of Vedic triples
#define CLOCK_MOD 12                  // Clock modulus (12-hour face)
#define EXT_CLOCK_MOD 30              // Extended clock modulus
#define GROWTH_FACTOR 1.618033988749895  // Growth factor (golden ratio)
#define EMBED_DIM 15                  // Embedding dimension
#define PHI_NUM 89                    // Golden ratio numerator approximation
#define PHI_DEN 55                    // Golden ratio denominator approximation
#define RAINBOW_LAYERS 12             // Number of rainbow layers

// Precision constants
#define DEFAULT_PRECISION_BITS 256    // Default precision for calculations
#define MAX_PRECISION_BITS 4096       // Maximum precision
#define FIXED_POINT_BITS_DEFAULT 64   // Default fixed-point scale bits
#define FIXED_POINT_SCALE (1ULL << 32) // Fixed-point scale factor

// Dust guard bits (for precision control in transcendental functions)
#define DUST_GUARD_BITS_STD 64        // Standard guard bits
#define DUST_GUARD_BITS_MIN 32        // Minimum guard bits
#define DUST_GUARD_BITS_MAX 128       // Maximum guard bits

// Math constants (standard names)
// NOTE: Use MATH_PI directly from math/types.h - no alias needed
// Removed: #define M_PI MATH_PI (duplicate)
#define M_LN2 0.69314718055994530942
#define M_LN10 2.30258509299404568402

/*
 * ============================================================================
 * UTILITY MACROS
 * ============================================================================
 */

// Check if BigInt is zero
#define BIGINT_IS_ZERO(n) ((n)->len == 0 || ((n)->len == 1 && (n)->d[0] == 0))

// Check if BigInt is negative
#define BIGINT_IS_NEGATIVE(n) ((n)->negative != 0)

// Get BigInt sign
#define BIGINT_SIGN(n) ((n)->negative ? -1 : 1)

#ifdef __cplusplus
}
#endif

#endif /* PRIME_TYPES_H */