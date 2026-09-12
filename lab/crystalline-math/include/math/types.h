/**
 * @file types.h
 * @brief Core type definitions for Crystalline Math Library
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * This file contains all fundamental type definitions for the
 * Crystalline Math Library - a self-contained, production-quality
 * mathematical library designed to replace math.h and provide
 * arbitrary precision arithmetic.
 * 
 * Design Principles:
 * - Self-contained (no math.h dependencies)
 * - Arbitrary precision support
 * - Clean, standardized naming
 * - Production quality code
 */

#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BigInt removed - use CrystallineAbacus for arbitrary precision */

/* ============================================================================
 * BIGFIXED - Arbitrary Precision Fixed-Point
 * ============================================================================
 * 
 * Fixed-point representation for fractional calculations.
 * Maintains both integer and fractional parts with arbitrary precision.
 * 
 * Representation: value = (integer_part + fractional_part / 2^scale_bits) * sign
 */
/* BigFixed removed - use CrystallineAbacus for arbitrary precision */

/* ============================================================================
 * CLOCK LATTICE - Deterministic Prime Generation
 * ============================================================================
 * 
 * Babylonian clock structure for deterministic prime generation.
 * Ring structure: 12, 60, 60, 100
 * 
 * This replaces ALL legacy prime generation methods (sieving, trial division).
 */

/**
 * @brief Clock position on Babylonian lattice
 * 
 * Represents a position on the multi-ring clock structure:
 * - Ring 0: 12 positions (hours) - outer ring
 * - Ring 1: 60 positions (minutes)
 * - Ring 2: 60 positions (seconds)
 * - Ring 3: 100 positions (milliseconds) - inner ring
 */
typedef struct {
    uint32_t ring;            /**< Ring number (0-3 for base, 4+ for extended) */
    uint32_t position;        /**< Position within ring */
    double angle;             /**< Angle in radians (0 = 3 o'clock) */
    double radius;            /**< Radius from center (0.0 to 1.0+) */
    uint8_t quadrant;         /**< Quadrant (1-4) for geometric operations */
    int8_t polarity;          /**< Polarity (+1 or -1) for sign tracking */
} ClockPosition;

/**
 * @brief Modular arithmetic relationships for a prime
 */
typedef struct {
    uint64_t prime;           /**< The prime number */
    uint32_t mod_12;          /**< prime % 12 */
    uint32_t mod_60;          /**< prime % 60 */
    uint32_t mod_100;         /**< prime % 100 */
} PrimeModular;

/**
 * @brief 3D sphere coordinates (for stereographic projection)
 */
typedef struct {
    double x;                 /**< X coordinate */
    double y;                 /**< Y coordinate */
    double z;                 /**< Z coordinate */
} SphereCoord;

/**
 * @brief Clock lattice context
 */
typedef struct {
    uint64_t *prime_cache;    /**< Cached primes for performance */
    uint32_t cache_size;      /**< Number of cached primes */
    uint32_t cache_capacity;  /**< Allocated cache capacity */
} ClockContext;

/* ============================================================================
 * LATTICE GEOMETRY
 * ============================================================================
 */

/**
 * @brief 2D point in lattice space
 */
typedef struct {
    double x;
    double y;
} Point2D;

/**
 * @brief 3D point in lattice space
 */
typedef struct {
    double x;
    double y;
    double z;
} Point3D;

/**
 * @brief Lattice context for geometric operations
 */
typedef struct {
    uint32_t dimensions;      /**< Number of dimensions */
    double scale;             /**< Lattice scale factor */
} LatticeContext;

/* ============================================================================
 * CONSTANTS
 * ============================================================================
 */

/** Mathematical constant π (pi) */

/** Derived π constants */

/** Mathematical constant e (Euler's number) */
#define MATH_E 2.71828182845904523536

/** Mathematical constant for infinity */
#ifndef INFINITY
#define INFINITY (__builtin_inf())
#endif

/** Infinity constants with MATH_ prefix */
#define MATH_INFINITY (__builtin_inf())
#define MATH_NEG_INFINITY (-__builtin_inf())

/** Mathematical constant for NaN */
#ifndef NAN
#define NAN (__builtin_nan(""))
#endif

/** Mathematical constant φ (golden ratio) */

/** Square roots - MOVED TO constants.h to avoid duplicates */

/** Natural logarithm of 2 */
#define MATH_LN2 0.69314718055994530942

/** Natural logarithm of 10 */
#define MATH_LN10 2.30258509299404568402

/* BigFixed removed - use CrystallineAbacus for arbitrary precision */

/** Maximum safe integer in double precision */
#define MATH_MAX_SAFE_INTEGER 9007199254740991LL

/* ============================================================================
 * ERROR CODES
 * ============================================================================
 */

typedef enum {
    MATH_SUCCESS = 0,         /**< Operation successful */
    MATH_ERROR_INVALID_ARG,   /**< Invalid argument */
    MATH_ERROR_OVERFLOW,      /**< Arithmetic overflow */
    MATH_ERROR_UNDERFLOW,     /**< Arithmetic underflow */
    MATH_ERROR_DIVISION_BY_ZERO, /**< Division by zero */
    MATH_ERROR_OUT_OF_MEMORY, /**< Memory allocation failed */
    MATH_ERROR_DOMAIN,        /**< Domain error (e.g., sqrt of negative) */
    MATH_ERROR_RANGE,         /**< Range error (result too large/small) */
    MATH_ERROR_CONVERGENCE,   /**< Iterative algorithm failed to converge */
    MATH_ERROR_NOT_FOUND,     /**< Item not found in lookup */
    MATH_ERROR_OUT_OF_RANGE,  /**< Index or value out of valid range */
    MATH_ERROR_NULL_POINTER,  /**< Null pointer passed */
    MATH_ERROR_INVALID_BASE,  /**< Invalid number base */
    MATH_ERROR_NOT_IMPLEMENTED /**< Feature not yet implemented */
} MathError;

#ifdef __cplusplus
}
#endif

#endif /* MATH_TYPES_H */
/* Numerical tolerances */
#define MATH_EPSILON 1e-10
#define MATH_SMALL_VALUE 1e-15


/* Euler's number */
#define MATH_E 2.71828182845904523536

/* NOTE: MATH_TWO_PI already defined above at line 136 - duplicate removed */


/* Vector culmination point and twin primes - MOVED TO constants.h to avoid duplicates */


/* ============================================================================
 * PRIME TYPES - Include all prime-related type definitions
 * ============================================================================
 */
#include "prime_types.h"

