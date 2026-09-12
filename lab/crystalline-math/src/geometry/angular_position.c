/**
 * @file angular_position.c
 * @brief Angular position calculations implementation
 * 
 * Implements the complete angular position formula using NEW math library.
 * All OLD library dependencies have been replaced with NEW library equivalents.
 */

#include "math/angular_position.h"
#include "math/transcendental.h"
#include "math/arithmetic.h"
#include "math/types.h"
#include "math/constants.h"
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Get Einstein's Lambda constant
 */
double cllm_get_einstein_lambda(void) {
    return 3.0 / 144000.0;
}

/**
 * @brief Calculate GCD using Euclidean algorithm
 */
static uint64_t gcd(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

/**
 * @brief Absolute value for doubles
 */
static double fabs_local(double x) {
    return (x < 0.0) ? -x : x;
}

/* ============================================================================
 * PLIMPTON 322 FUNCTIONS
 * ============================================================================
 */

bool is_coprime(uint64_t p, uint64_t q) {
    return gcd(p, q) == 1;
}

bool verify_plimpton_constraints(uint64_t p, uint64_t q) {
    // Constraint 1: p > q > 0
    if (p <= q || q == 0) {
        return false;
    }
    
    // Constraint 2: gcd(p,q) = 1 (coprime)
    if (!is_coprime(p, q)) {
        return false;
    }
    
    // Constraint 3: Not both odd
    if ((p % 2 == 1) && (q % 2 == 1)) {
        return false;
    }
    
    return true;
}

PlimptonRatios calculate_plimpton_ratios(uint64_t p, uint64_t q) {
    PlimptonRatios ratios = {0.0, 0.0, 0.0};
    
    // Verify constraints
    if (!verify_plimpton_constraints(p, q)) {
        // Return default ratios if constraints not satisfied
        ratios.ratio_b_d = 0.5;
        ratios.ratio_c_d = 0.5;
        ratios.product = 0.25;
        return ratios;
    }
    
    // Calculate p² and q²
    uint64_t p2 = p * p;
    uint64_t q2 = q * q;
    uint64_t denom = p2 + q2;
    
    if (denom == 0) {
        ratios.ratio_b_d = 0.5;
        ratios.ratio_c_d = 0.5;
        ratios.product = 0.25;
        return ratios;
    }
    
    // Calculate ratios
    ratios.ratio_b_d = (double)(p2 - q2) / (double)denom;
    ratios.ratio_c_d = (2.0 * p * q) / (double)denom;
    ratios.product = ratios.ratio_b_d * ratios.ratio_c_d;
    
    return ratios;
}

bool find_optimal_generators(uint64_t prime, uint64_t* p, uint64_t* q) {
    if (!p || !q) return false;
    
    // Try q from 1 to reasonable limit
    uint64_t max_q = 100;
    if (prime < 10000) {
        max_q = 50;
    }
    
    for (uint64_t q_try = 1; q_try <= max_q; q_try++) {
        // Calculate p such that p² - q² ≈ prime
        uint64_t q2 = q_try * q_try;
        uint64_t p2_target = prime + q2;
        
        // Try p values around sqrt(p2_target)
        uint64_t p_approx = 1;
        while (p_approx * p_approx < p2_target) {
            p_approx++;
        }
        
        // Try p_approx and nearby values
        for (int delta = -2; delta <= 2; delta++) {
            uint64_t p_try = p_approx + delta;
            if (p_try <= q_try) continue;
            
            // Verify constraints
            if (verify_plimpton_constraints(p_try, q_try)) {
                *p = p_try;
                *q = q_try;
                return true;
            }
        }
    }
    
    // Default fallback: p=3, q=2 (generates 5, 12, 13)
    *p = 3;
    *q = 2;
    return true;
}

int find_nearest_plimpton_triple(uint64_t prime, PythagoreanTriple* triple) {
    uint64_t p, q;
    
    if (!find_optimal_generators(prime, &p, &q)) {
        return -1;
    }
    
    if (triple) {
        uint64_t p2 = p * p;
        uint64_t q2 = q * q;
        
        triple->p = p;
        triple->q = q;
        triple->a = p2 - q2;
        triple->b = 2 * p * q;
        triple->c = p2 + q2;
        triple->layer = 0;
        triple->ratio = (double)p / (double)q;
    }
    
    return 0;
}

double calculate_plimpton_correction_factor(uint64_t prime, 
                                           const PythagoreanTriple* triple) {
    if (!triple) return 0.0;
    
    // Calculate distance from prime to triple's 'a' value
    double distance = fabs_local((double)prime - (double)triple->a);
    
    // Correction factor decreases with distance
    // Use exponential decay: e^(-distance/scale)
    double scale = 1000.0;  // Scale factor
    double correction = math_exp(-distance / scale);
    
    // Scale by ratio to incorporate Plimpton geometry
    correction *= triple->ratio;
    
    return correction * 0.1;  // Scale to reasonable magnitude
}

/* ============================================================================
 * FREQUENCY AND WAVELENGTH CONVERSIONS
 * ============================================================================
 */

double wavelength_to_frequency(double wavelength) {
    if (wavelength <= 0.0) {
        return 0.0;
    }
    return SPEED_OF_SOUND / wavelength;
}

double frequency_to_wavelength(double frequency) {
    if (frequency <= 0.0) {
        return 0.0;
    }
    return SPEED_OF_SOUND / frequency;
}

double get_phonetic_wavelength(char character) {
    // Convert to lowercase
    char c = tolower((unsigned char)character);
    
    // Map characters to approximate phonetic wavelengths
    // Based on formant frequencies of speech sounds
    switch (c) {
        // Vowels (lower frequencies, longer wavelengths)
        case 'a': return 1.0;   // ~343 Hz
        case 'e': return 0.8;   // ~429 Hz
        case 'i': return 0.6;   // ~572 Hz
        case 'o': return 0.9;   // ~381 Hz
        case 'u': return 0.7;   // ~490 Hz
        
        // Consonants (higher frequencies, shorter wavelengths)
        case 'b': case 'p': return 0.3;  // ~1143 Hz
        case 'c': case 'k': return 0.25; // ~1372 Hz
        case 'd': case 't': return 0.28; // ~1225 Hz
        case 'f': case 'v': return 0.2;  // ~1715 Hz
        case 'g': return 0.27;           // ~1270 Hz
        case 'h': return 0.15;           // ~2287 Hz
        case 'j': return 0.22;           // ~1559 Hz
        case 'l': return 0.4;            // ~858 Hz
        case 'm': case 'n': return 0.45; // ~762 Hz
        case 'r': return 0.35;           // ~980 Hz
        case 's': case 'z': return 0.12; // ~2858 Hz
        case 'w': return 0.5;            // ~686 Hz
        case 'y': return 0.55;           // ~624 Hz
        
        // Default for other characters
        default: return 0.5;
    }
}

double get_phonetic_frequency(char character) {
    double wavelength = get_phonetic_wavelength(character);
    return wavelength_to_frequency(wavelength);
}

/* ============================================================================
 * INDIVIDUAL TERM CALCULATIONS
 * ============================================================================
 */

double angular_position_spiral_term(uint64_t prime_index) {
    // k·π(1+√5)
    // Golden ratio: φ = (1+√5)/2, so (1+√5) = 2φ - 1 ≈ 2.236067977...
    double one_plus_sqrt5 = 1.0 + math_sqrt(5.0);
    return (double)prime_index * MATH_PI * one_plus_sqrt5;
}

double angular_position_index_term(int dimension) {
    // (n-1)·2π/(12·ln3)
    if (dimension <= 0) {
        return 0.0;
    }
    
    return (double)(dimension - 1) * (2.0 * MATH_PI) / (12.0 * LN_3);
}

double angular_position_phonetic_term(double phonetic_wavelength) {
    // log₃(ν(λ))
    // ν(λ) = c/λ (frequency from wavelength)
    
    if (phonetic_wavelength <= 0.0) {
        return 0.0;
    }
    
    double frequency = wavelength_to_frequency(phonetic_wavelength);
    
    if (frequency <= 0.0) {
        return 0.0;
    }
    
    // log₃(x) = ln(x) / ln(3)
    return math_log(frequency) / LN_3;
}

double angular_position_omega_correction(uint64_t prime) {
    // ω(p) = (3/144000)·f(p)
    // where f(p) is a function of distance to 144000
    
    double lambda = cllm_get_einstein_lambda();
    
    // Calculate distance to 144000
    double distance = fabs_local((double)prime - (double)VECTOR_CULMINATION);
    
    // f(p) = 1 / (1 + distance/144000)
    // This makes the correction stronger near 144000
    double f_p = 1.0 / (1.0 + distance / (double)VECTOR_CULMINATION);
    
    // Apply Einstein's Lambda
    double omega = lambda * f_p;
    
    // Special handling for twin primes
    if (prime == TWIN_PRIME_LOWER || prime == TWIN_PRIME_UPPER) {
        omega *= 2.0;  // Double correction for twin primes
    }
    
    return omega;
}

double angular_position_psi_correction(uint64_t prime) {
    // ψ(p) = Plimpton 322 correction
    
    PythagoreanTriple triple;
    int index = find_nearest_plimpton_triple(prime, &triple);
    
    if (index < 0) {
        return 0.0;
    }
    
    return calculate_plimpton_correction_factor(prime, &triple);
}

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

double angular_position_normalize(double theta) {
    // Normalize to [0, 2π)
    double two_pi = 2.0 * MATH_PI;
    
    // Reduce to range
    while (theta < 0.0) {
        theta += two_pi;
    }
    while (theta >= two_pi) {
        theta -= two_pi;
    }
    
    return theta;
}

void angular_position_to_clock(double theta, int* hour, double* minute) {
    if (!hour || !minute) return;
    
    // Normalize theta to [0, 2π)
    theta = angular_position_normalize(theta);
    
    // Convert to 12-hour clock
    // 0 radians = 3 o'clock, π/2 = 12 o'clock
    // Rotate by -π/2 to make 0 = 12 o'clock
    double adjusted = theta - MATH_PI / 2.0;
    if (adjusted < 0.0) {
        adjusted += 2.0 * MATH_PI;
    }
    
    // Convert to hours (0-12)
    double hours_float = (adjusted / (2.0 * MATH_PI)) * 12.0;
    
    // Extract hour and minute
    *hour = (int)hours_float % 12;
    *minute = (hours_float - (int)hours_float) * 60.0;
}

int angular_position_symmetry_group(uint64_t prime) {
    return (int)(prime % 12);
}

int angular_position_is_near_boundary(uint64_t prime, double* distance) {
    double dist = fabs_local((double)prime - (double)VECTOR_CULMINATION);
    
    if (distance) {
        *distance = dist;
    }
    
    // Consider "near" if within 1000 of 144000
    return (dist < 1000.0) ? 1 : 0;
}

int angular_position_is_twin_prime(uint64_t prime) {
    return (prime == TWIN_PRIME_LOWER || prime == TWIN_PRIME_UPPER) ? 1 : 0;
}

/* ============================================================================
 * ANGULAR POSITION CALCULATION
 * ============================================================================
 */

void angular_position_calculate(uint64_t prime,
                                uint64_t prime_index,
                                int dimension,
                                double phonetic_wavelength,
                                AngularPosition* result) {
    if (!result) return;
    
    // Zero out structure
    memset(result, 0, sizeof(AngularPosition));
    
    // Set input parameters
    result->prime = prime;
    result->prime_index = prime_index;
    result->dimension = dimension;
    result->phonetic_wavelength = phonetic_wavelength;
    
    // Calculate individual terms
    result->spiral_term = angular_position_spiral_term(prime_index);
    result->index_term = angular_position_index_term(dimension);
    result->phonetic_term = angular_position_phonetic_term(phonetic_wavelength);
    result->omega_correction = angular_position_omega_correction(prime);
    result->psi_correction = angular_position_psi_correction(prime);
    
    // Calculate complete theta
    result->theta = result->spiral_term + 
                   result->index_term + 
                   result->phonetic_term + 
                   result->omega_correction + 
                   result->psi_correction;
    
    // Normalize to [0, 2π)
    result->theta_normalized = angular_position_normalize(result->theta);
    
    // Calculate clock position
    angular_position_to_clock(result->theta_normalized, 
                             &result->clock_hour, 
                             &result->clock_minute);
    
    // Calculate symmetry group
    result->symmetry_group = angular_position_symmetry_group(prime);
    
    // Check boundary information
    result->is_near_144000 = angular_position_is_near_boundary(prime, 
                                                               &result->distance_to_144000);
    result->is_twin_prime = angular_position_is_twin_prime(prime);
}