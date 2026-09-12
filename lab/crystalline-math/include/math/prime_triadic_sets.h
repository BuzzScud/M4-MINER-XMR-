#ifndef MATH_PRIME_TRIADIC_SETS_H
#define MATH_PRIME_TRIADIC_SETS_H

/**
 * @file prime_triadic_sets.h
 * @brief Prime Triadic Set Generation for Polytope Discovery
 * 
 * Generates valid prime triadic sets following the non-overlapping constraint:
 * - Cannot use 3 (triadic structure itself)
 * - Cannot use D (dimension, if prime)
 * - Cannot use N (number of sets, if prime)
 * 
 * Used for discovering new regular polytopes via tetration attractors.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief A single triadic set (3 primes)
 */
typedef struct {
    uint32_t primes[3];         /**< The 3 primes in the set */
    bool is_valid;              /**< Passes non-overlapping constraint */
} TriadicSet;

/**
 * @brief Collection of triadic sets
 */
typedef struct {
    TriadicSet* sets;           /**< Array of triadic sets */
    uint32_t num_sets;          /**< Number of sets */
    
    uint32_t dimension;         /**< Target dimension */
    uint32_t* excluded_primes;  /**< Primes excluded by constraint */
    uint32_t num_excluded;      /**< Number of excluded primes */
    
    uint32_t* all_bases;        /**< All bases (flattened) */
    uint32_t num_bases;         /**< Total number of bases */
} PrimeTriadicSets;

/**
 * @brief Configuration for generating triadic sets
 */
typedef struct {
    uint32_t dimension;         /**< Target dimension (5D, 6D, 7D, ...) */
    uint32_t num_sets;          /**< Number of triadic sets (usually 2) */
    uint32_t max_prime;         /**< Maximum prime to consider */
} TriadicSetConfig;

// ============================================================================
// GENERATION
// ============================================================================

/**
 * @brief Generate prime triadic sets for a given dimension
 * 
 * Follows non-overlapping constraint:
 * - Excludes 3 (triadic structure)
 * - Excludes dimension (if prime)
 * - Excludes num_sets (if prime)
 * 
 * @param config Configuration
 * @return Prime triadic sets, or NULL on error
 * 
 * Example for 5D with 2 sets:
 *   Excluded: 2, 3, 5
 *   Set 1: {7, 11, 13}
 *   Set 2: {17, 19, 23}
 */
PrimeTriadicSets* prime_triadic_generate(const TriadicSetConfig* config);

/**
 * @brief Generate triadic sets with custom excluded primes
 * 
 * @param num_sets Number of triadic sets
 * @param excluded_primes Array of primes to exclude
 * @param num_excluded Number of excluded primes
 * @param max_prime Maximum prime to consider
 * @return Prime triadic sets, or NULL on error
 */
PrimeTriadicSets* prime_triadic_generate_custom(
    uint32_t num_sets,
    const uint32_t* excluded_primes,
    uint32_t num_excluded,
    uint32_t max_prime
);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * @brief Check if a prime is excluded by the constraint
 * 
 * @param prime Prime to check
 * @param dimension Target dimension
 * @param num_sets Number of sets
 * @return true if excluded, false otherwise
 */
bool prime_triadic_is_excluded(
    uint32_t prime,
    uint32_t dimension,
    uint32_t num_sets
);

/**
 * @brief Validate a triadic set
 * 
 * Checks:
 * - All 3 members are prime
 * - No duplicates
 * - None are excluded
 * - All are distinct
 * 
 * @param set Triadic set to validate
 * @param excluded_primes Array of excluded primes
 * @param num_excluded Number of excluded primes
 * @return true if valid, false otherwise
 */
bool prime_triadic_validate_set(
    const TriadicSet* set,
    const uint32_t* excluded_primes,
    uint32_t num_excluded
);

/**
 * @brief Validate entire collection of triadic sets
 * 
 * Checks:
 * - Each set is valid
 * - No overlaps between sets
 * - Correct number of sets
 * 
 * @param sets Prime triadic sets
 * @return true if valid, false otherwise
 */
bool prime_triadic_validate_all(const PrimeTriadicSets* sets);

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * @brief Get all bases from triadic sets (flattened array)
 * 
 * @param sets Prime triadic sets
 * @param out_num_bases Output: number of bases
 * @return Array of all bases, or NULL on error
 */
uint32_t* prime_triadic_get_all_bases(
    const PrimeTriadicSets* sets,
    uint32_t* out_num_bases
);

/**
 * @brief Print triadic sets
 * 
 * @param sets Prime triadic sets
 */
void prime_triadic_print(const PrimeTriadicSets* sets);

/**
 * @brief Print excluded primes and reasoning
 * 
 * @param dimension Target dimension
 * @param num_sets Number of sets
 */
void prime_triadic_print_exclusions(uint32_t dimension, uint32_t num_sets);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Free prime triadic sets
 * 
 * @param sets Prime triadic sets to free
 */
void prime_triadic_free(PrimeTriadicSets* sets);

// ============================================================================
// PRESETS
// ============================================================================

/**
 * @brief Generate triadic sets for 5D polytope discovery
 * 
 * Excluded: 2, 3, 5
 * Set 1: {7, 11, 13}
 * Set 2: {17, 19, 23}
 * 
 * @return Prime triadic sets for 5D
 */
PrimeTriadicSets* prime_triadic_preset_5d(void);

/**
 * @brief Generate triadic sets for 6D polytope discovery
 * 
 * Excluded: 2, 3
 * Set 1: {5, 7, 11}
 * Set 2: {13, 17, 19}
 * 
 * @return Prime triadic sets for 6D
 */
PrimeTriadicSets* prime_triadic_preset_6d(void);

/**
 * @brief Generate triadic sets for 7D polytope discovery
 * 
 * Excluded: 2, 3, 7
 * Set 1: {5, 11, 13}
 * Set 2: {17, 19, 23}
 * 
 * @return Prime triadic sets for 7D
 */
PrimeTriadicSets* prime_triadic_preset_7d(void);

/**
 * @brief Generate classical triadic set (3D)
 * 
 * Set 1: {2, 3, 5}
 * 
 * @return Prime triadic set for classical 3D solids
 */
PrimeTriadicSets* prime_triadic_preset_classical_3d(void);

/**
 * @brief Generate extended set (4D)
 * 
 * Set 1: {2, 3, 5}
 * Set 2: {7, 11, 13}
 * 
 * @return Prime triadic sets for 4D polychora
 */
PrimeTriadicSets* prime_triadic_preset_4d(void);

#ifdef __cplusplus
}
#endif

#endif /* MATH_PRIME_TRIADIC_SETS_H */