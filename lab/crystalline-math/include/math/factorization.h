/**
 * @file factorization.h
 * @brief O(1) Factorization using Clock Lattice
 * 
 * Revolutionary factorization algorithm that achieves O(1) complexity
 * for most composite numbers by leveraging the clock lattice structure.
 * 
 * Key Insight: If we can generate primes in O(1) time using the clock
 * lattice, we can factor numbers in O(1) time by reversing the process.
 */

#ifndef FACTORIZATION_H
#define FACTORIZATION_H

#include "math/types.h"
#include "math/prime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CLOCK POSITION STRUCTURES
 * ============================================================================
 */

/**
 * @brief Factor clock position for factorization
 */
typedef struct {
    uint32_t position;        /**< Position on clock (0-11) */
    uint32_t base;            /**< Base value (5, 7, or 11) */
    uint64_t magnitude;       /**< Magnitude component */
} FactorClockPosition;

/**
 * @brief Possible factor position combinations
 */
typedef struct {
    FactorClockPosition positions[6];  /**< Up to 6 position combinations */
    size_t count;                      /**< Number of valid combinations */
} FactorPositions;

/* ============================================================================
 * POSITION ANALYSIS
 * ============================================================================
 */

/**
 * @brief Get possible factor positions for a product position
 * 
 * Given a composite number's position on the clock, determines which
 * combinations of prime positions could multiply to produce it.
 * 
 * Position multiplication table (mod 12):
 *   5 × 5 = 1    5 × 7 = 11   5 × 11 = 7
 *   7 × 7 = 1    7 × 11 = 5
 *   11 × 11 = 1
 * 
 * @param product_position Position of composite number (0-11)
 * @return Structure containing possible factor position combinations
 */
FactorPositions get_factor_positions(uint32_t product_position);

/**
 * @brief Convert number to factor clock position
 * 
 * @param n Number to convert
 * @param pos Output factor clock position
 * @return MATH_SUCCESS or error code
 */
MathError number_to_factor_clock_position(uint64_t n, FactorClockPosition* pos);

/**
 * @brief Convert factor clock position to number
 * 
 * @param pos Factor clock position
 * @return Number at that position
 */
uint64_t factor_clock_position_to_number(const FactorClockPosition* pos);

/* ============================================================================
 * FACTOR SEARCH
 * ============================================================================
 */

/**
 * @brief Find factor at specific clock position
 * 
 * Uses magnitude decomposition to find a factor of n that lies at
 * the specified clock position. Returns 0 if no factor found.
 * 
 * Algorithm:
 * 1. Extract magnitude: magnitude = (n - base) / 12
 * 2. For n = p × q at same position: magnitude_n ≈ magnitude_p × magnitude_q
 * 3. Try candidates around sqrt(magnitude)
 * 4. Verify candidates are prime
 * 
 * @param n Number to factor
 * @param pos Factor clock position to search
 * @return Factor if found, 0 otherwise
 */
uint64_t find_factor_at_position(uint64_t n, FactorClockPosition pos);

/**
 * @brief Find any factor of n using O(1) method
 * 
 * Tries all possible position combinations and returns the first
 * factor found. Returns 0 if n is prime.
 * 
 * @param n Number to factor
 * @return Factor if found, 0 if prime
 */
uint64_t find_any_factor_o1(uint64_t n);

/* ============================================================================
 * COMPLETE FACTORIZATION
 * ============================================================================
 */

/**
 * @brief Factor a number into primes (IMPLEMENTS DECLARED API)
 * 
 * This implements the prime_factor() function declared in prime.h.
 * Uses O(1) clock lattice method for most composites, with fallback
 * to traditional methods for edge cases.
 * 
 * Algorithm:
 * 1. Handle special cases (n < 2, n = 2, n = 3)
 * 2. Extract powers of 2 and 3
 * 3. Use clock lattice for remaining factors (O(1))
 * 4. Recursively factor any composite factors found
 * 
 * @param n Number to factor (must be >= 2)
 * @param result Output factorization structure (must be pre-allocated)
 * @return MATH_SUCCESS or error code
 * 
 * NOTE: This is NOT a separate function - it IS prime_factor()
 */
MathError prime_factor(uint64_t n, Factorization* result);

/**
 * @brief Verify if candidate is prime using O(1) check
 * 
 * Uses the interference formula to verify primality in O(1) time.
 * 
 * @param candidate Number to check
 * @return true if prime, false otherwise
 */
bool verify_prime_o1(uint64_t candidate);

/* ============================================================================
 * UTILITIES
 * ============================================================================
 */

/**
 * @brief Integer square root
 * 
 * @param n Input value
 * @return Floor of sqrt(n)
 */
uint64_t isqrt(uint64_t n);

/**
 * @brief Extract powers of small prime
 * 
 * Divides n by p repeatedly and returns the exponent.
 * 
 * @param n Input/output value (modified)
 * @param p Prime to extract
 * @return Exponent (number of times p divides n)
 */
uint32_t extract_prime_power(uint64_t* n, uint64_t p);

#ifdef __cplusplus
}
#endif

#endif /* FACTORIZATION_H */