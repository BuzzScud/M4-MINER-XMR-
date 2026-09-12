/**
 * @file angular_position.h
 * @brief Angular position calculations for prime lattice
 * 
 * Complete implementation of the angular position formula with all corrections:
 * 
 * θ(p, k, n, λ) = k·π(1+√5) + (n-1)·2π/(12·ln3) + log₃(ν(λ)) + ω(p) + ψ(p)
 * 
 * Where:
 * - k = prime index in sequence
 * - p = prime value
 * - n = dimension index
 * - λ = phonetic wavelength
 * - ω(p) = Einstein's Λ correction = (3/144000)·f(p)
 * - ψ(p) = Plimpton 322 correction based on Pythagorean triples
 * 
 * This formula determines the angular position of a prime in the lattice,
 * incorporating spiral geometry, dimensional indexing, phonetic resonance,
 * and mathematical corrections.
 */

#ifndef MATH_ANGULAR_POSITION_H
#define MATH_ANGULAR_POSITION_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================
 */

/** Speed of sound in air (m/s) for wavelength/frequency conversions */
#define SPEED_OF_SOUND 343.0

/** Vector culmination point (144000) */

/** Twin prime lower bound */

/** Twin prime upper bound */

/** Natural logarithm of 3 */
#define LN_3 1.0986122886681096

/* ============================================================================
 * TYPES
 * ============================================================================
 */

/* Forward declaration - full definition in prime_types.h if available */
#ifndef PRIME_TYPES_H
/**
 * @brief Pythagorean triple structure
 */
typedef struct {
    uint64_t a;      /**< p² - q² */
    uint64_t b;      /**< 2pq */
    uint64_t c;      /**< p² + q² */
    uint64_t p;      /**< First generator */
    uint64_t q;      /**< Second generator */
    int layer;       /**< Layer index */
    double ratio;    /**< Ratio p/q */
} PythagoreanTriple;
#endif

/**
 * @brief Plimpton 322 ratios
 */
typedef struct {
    double ratio_b_d;  /**< (p²-q²)/(p²+q²) */
    double ratio_c_d;  /**< 2pq/(p²+q²) */
    double product;    /**< ratio_b_d × ratio_c_d */
} PlimptonRatios;

/**
 * @brief Angular position structure
 * 
 * Complete angular position with all terms and corrections.
 */
typedef struct {
    /* Input parameters */
    uint64_t prime;             /**< Prime value (p) */
    uint64_t prime_index;       /**< Prime index in sequence (k) */
    int dimension;              /**< Dimension index (n) */
    double phonetic_wavelength; /**< Phonetic wavelength (λ) */
    
    /* Individual terms */
    double spiral_term;         /**< k·π(1+√5) */
    double index_term;          /**< (n-1)·2π/(12·ln3) */
    double phonetic_term;       /**< log₃(ν(λ)) */
    double omega_correction;    /**< ω(p) = Einstein's Λ correction */
    double psi_correction;      /**< ψ(p) = Plimpton 322 correction */
    
    /* Final result */
    double theta;               /**< Complete angular position */
    double theta_normalized;    /**< Normalized to [0, 2π) */
    
    /* Clock position (12-hour face) */
    int clock_hour;             /**< Hour position (0-11) */
    double clock_minute;        /**< Minute position (0.0-60.0) */
    
    /* Symmetry group */
    int symmetry_group;         /**< p mod 12 */
    
    /* Boundary information */
    int is_near_144000;         /**< 1 if near 144000 boundary */
    int is_twin_prime;          /**< 1 if twin prime (143999 or 144001) */
    double distance_to_144000;  /**< Distance to 144000 */
} AngularPosition;

/* ============================================================================
 * ANGULAR POSITION CALCULATION
 * ============================================================================
 */

/**
 * @brief Calculate complete angular position
 * 
 * Computes all terms and corrections for the angular position formula.
 * 
 * @param prime Prime value (p)
 * @param prime_index Prime index in sequence (k)
 * @param dimension Dimension index (n)
 * @param phonetic_wavelength Phonetic wavelength (λ)
 * @param result Pointer to AngularPosition structure to fill
 */
void angular_position_calculate(uint64_t prime,
                                uint64_t prime_index,
                                int dimension,
                                double phonetic_wavelength,
                                AngularPosition* result);

/* ============================================================================
 * INDIVIDUAL TERM CALCULATIONS
 * ============================================================================
 */

/**
 * @brief Calculate spiral term: k·π(1+√5)
 * 
 * This term represents the golden spiral component.
 * 
 * @param prime_index Prime index (k)
 * @return Spiral term value
 */
double angular_position_spiral_term(uint64_t prime_index);

/**
 * @brief Calculate index term: (n-1)·2π/(12·ln3)
 * 
 * This term represents the dimensional indexing.
 * 
 * @param dimension Dimension index (n)
 * @return Index term value
 */
double angular_position_index_term(int dimension);

/**
 * @brief Calculate phonetic term: log₃(ν(λ))
 * 
 * This term represents the phonetic resonance.
 * ν(λ) is the frequency corresponding to wavelength λ.
 * 
 * @param phonetic_wavelength Wavelength (λ)
 * @return Phonetic term value
 */
double angular_position_phonetic_term(double phonetic_wavelength);

/**
 * @brief Calculate omega correction: ω(p) = (3/144000)·f(p)
 * 
 * Einstein's Λ correction based on distance to 144000.
 * 
 * @param prime Prime value (p)
 * @return Omega correction value
 */
double angular_position_omega_correction(uint64_t prime);

/**
 * @brief Calculate psi correction: ψ(p)
 * 
 * Plimpton 322 correction based on Pythagorean triples.
 * 
 * @param prime Prime value (p)
 * @return Psi correction value
 */
double angular_position_psi_correction(uint64_t prime);

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Normalize angle to [0, 2π)
 * 
 * @param theta Angle in radians
 * @return Normalized angle
 */
double angular_position_normalize(double theta);

/**
 * @brief Convert angle to clock position
 * 
 * Maps angle to 12-hour clock face.
 * 
 * @param theta Angle in radians
 * @param hour Pointer to store hour (0-11)
 * @param minute Pointer to store minute (0.0-60.0)
 */
void angular_position_to_clock(double theta, int* hour, double* minute);

/**
 * @brief Get symmetry group from prime
 * 
 * @param prime Prime value
 * @return Symmetry group (p mod 12)
 */
int angular_position_symmetry_group(uint64_t prime);

/**
 * @brief Check if prime is near 144000 boundary
 * 
 * @param prime Prime value
 * @param distance Pointer to store distance (optional)
 * @return 1 if near boundary, 0 otherwise
 */
int angular_position_is_near_boundary(uint64_t prime, double* distance);

/**
 * @brief Check if prime is a twin prime (143999 or 144001)
 * 
 * @param prime Prime value
 * @return 1 if twin prime, 0 otherwise
 */
int angular_position_is_twin_prime(uint64_t prime);

/* ============================================================================
 * FREQUENCY AND WAVELENGTH CONVERSIONS
 * ============================================================================
 */

/**
 * @brief Convert wavelength to frequency
 * 
 * ν = c/λ where c is speed of sound in air (343 m/s)
 * 
 * @param wavelength Wavelength in meters
 * @return Frequency in Hz
 */
double wavelength_to_frequency(double wavelength);

/**
 * @brief Convert frequency to wavelength
 * 
 * λ = c/ν where c is speed of sound in air (343 m/s)
 * 
 * @param frequency Frequency in Hz
 * @return Wavelength in meters
 */
double frequency_to_wavelength(double frequency);

/**
 * @brief Get phonetic wavelength for character
 * 
 * Maps characters to phonetic wavelengths based on
 * acoustic properties of speech sounds.
 * 
 * @param character Character to map
 * @return Phonetic wavelength in meters
 */
double get_phonetic_wavelength(char character);

/**
 * @brief Get phonetic frequency for character
 * 
 * @param character Character to map
 * @return Phonetic frequency in Hz
 */
double get_phonetic_frequency(char character);

/* ============================================================================
 * PLIMPTON 322 FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Verify if two numbers are coprime (gcd = 1)
 * 
 * @param p First number
 * @param q Second number
 * @return true if gcd(p,q) = 1, false otherwise
 */
bool is_coprime(uint64_t p, uint64_t q);

/**
 * @brief Verify Plimpton 322 constraints
 * 
 * All three constraints must be satisfied:
 * 1. gcd(p,q) = 1 (coprime)
 * 2. Not both odd
 * 3. p > q > 0
 * 
 * @param p First generator
 * @param q Second generator
 * @return true if all constraints satisfied, false otherwise
 */
bool verify_plimpton_constraints(uint64_t p, uint64_t q);

/**
 * @brief Calculate Plimpton 322 ratios
 * 
 * Computes the two fundamental ratios:
 * - ratio_b_d = (p²-q²)/(p²+q²)
 * - ratio_c_d = 2pq/(p²+q²)
 * 
 * @param p First generator (must satisfy constraints)
 * @param q Second generator (must satisfy constraints)
 * @return PlimptonRatios structure with both ratios and their product
 */
PlimptonRatios calculate_plimpton_ratios(uint64_t p, uint64_t q);

/**
 * @brief Find nearest Plimpton 322 triple for prime
 * 
 * Finds the Pythagorean triple that is closest to the given prime value.
 * 
 * @param prime Prime value
 * @param triple Pointer to store triple (optional)
 * @return Index of nearest triple, or -1 if none found
 */
int find_nearest_plimpton_triple(uint64_t prime, PythagoreanTriple* triple);

/**
 * @brief Calculate Plimpton 322 correction factor
 * 
 * Computes the correction factor based on the relationship
 * between the prime and the nearest Pythagorean triple.
 * 
 * @param prime Prime value
 * @param triple Plimpton 322 triple
 * @return Correction factor
 */
double calculate_plimpton_correction_factor(uint64_t prime, 
                                           const PythagoreanTriple* triple);

/**
 * @brief Find optimal p,q generators for a given prime
 * 
 * Finds the p,q pair that best represents the prime in the
 * Plimpton 322 system.
 * 
 * @param prime Prime number
 * @param p Output: optimal p value
 * @param q Output: optimal q value
 * @return true if found, false otherwise
 */
bool find_optimal_generators(uint64_t prime, uint64_t* p, uint64_t* q);

/**
 * @brief Get Einstein's Lambda constant
 * 
 * @return Lambda value (3.0/144000.0)
 */
double cllm_get_einstein_lambda(void);

#ifdef __cplusplus
}
#endif

#endif /* MATH_ANGULAR_POSITION_H */