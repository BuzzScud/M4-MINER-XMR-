/**
 * @file abacus.c
 * @brief Crystalline Abacus implementation
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * Implements geometric arithmetic operations using the clock lattice structure.
 */

#include "math/abacus.h"
/* LAB PATCH #2 (2026-09-12): compile with -DCX_PURE_GEOMETRIC to disable the
 * uint64_t shortcuts in abacus_add / abacus_sub / abacus_mul so that the
 * bead-level (geometric) algorithms run for every operand size. */
#ifdef CX_PURE_GEOMETRIC
#define CX_FASTPATH 0
#else
#define CX_FASTPATH 1
#endif
#include "math/prime.h"
#include "math/arithmetic.h"
#include "math/constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================
 */

/* Forward declarations */
static MathError map_digit_to_position(uint32_t digit, uint32_t base, ClockPosition* pos);
static MathError multiply_by_digit(CrystallineAbacus* result, const CrystallineAbacus* a, uint32_t digit);
static const AbacusBead* find_bead_by_exponent(const CrystallineAbacus* abacus, int32_t exponent);
static void get_exponent_range(const CrystallineAbacus* a, const CrystallineAbacus* b, 
                               int32_t* min_exp, int32_t* max_exp);

/**
 * @brief Ensure abacus has enough capacity
 */
static MathError abacus_ensure_capacity(CrystallineAbacus* abacus, size_t min_capacity) {
    if (abacus->capacity >= min_capacity) {
        return MATH_SUCCESS;
    }
    
    size_t new_capacity = abacus->capacity * 2;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }
    
    AbacusBead* new_beads = (AbacusBead*)realloc(
        abacus->beads,
        new_capacity * sizeof(AbacusBead)
    );
    
    if (!new_beads) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    abacus->beads = new_beads;
    abacus->capacity = new_capacity;
    
    return MATH_SUCCESS;
}

/**
 * @brief Map a digit value to a clock position
 */
/**
 * @brief Compare magnitudes of two abacuses (ignore signs)
 * @return -1 if |a| < |b|, 0 if |a| == |b|, 1 if |a| > |b|
 */
static int compare_magnitude(const CrystallineAbacus* a, const CrystallineAbacus* b) {
    /* Find the highest exponent in each abacus */
    int32_t max_exp_a = (a->num_beads > 0) ? a->beads[0].weight_exponent : 0;
    int32_t max_exp_b = (b->num_beads > 0) ? b->beads[0].weight_exponent : 0;
    
    for (size_t i = 1; i < a->num_beads; i++) {
        if (a->beads[i].weight_exponent > max_exp_a) {
            max_exp_a = a->beads[i].weight_exponent;
        }
    }
    
    for (size_t i = 1; i < b->num_beads; i++) {
        if (b->beads[i].weight_exponent > max_exp_b) {
            max_exp_b = b->beads[i].weight_exponent;
        }
    }
    
    /* Compare from highest exponent down to lowest.
     * LAB PATCH #1 (2026-09-12): the original stopped at
     * min(max_exp_a, max_exp_b), i.e. after the leading digit, so any two
     * numbers with equal digit count and equal leading digit compared as
     * equal. Walk down to the lowest exponent present in either number. */
    int32_t min_exp = (a->num_beads > 0) ? a->beads[0].weight_exponent : 0;
    for (size_t i = 1; i < a->num_beads; i++)
        if (a->beads[i].weight_exponent < min_exp) min_exp = a->beads[i].weight_exponent;
    for (size_t i = 0; i < b->num_beads; i++)
        if (b->beads[i].weight_exponent < min_exp) min_exp = b->beads[i].weight_exponent;
    int32_t max_exp = (max_exp_a > max_exp_b) ? max_exp_a : max_exp_b;
    
    for (int32_t exp = max_exp; exp >= min_exp; exp--) {
        /* Find beads with this exponent */
        uint32_t digit_a = 0;
        uint32_t digit_b = 0;
        
        for (size_t i = 0; i < a->num_beads; i++) {
            if (a->beads[i].weight_exponent == exp) {
                digit_a = a->beads[i].value;
                break;
            }
        }
        
        for (size_t i = 0; i < b->num_beads; i++) {
            if (b->beads[i].weight_exponent == exp) {
                digit_b = b->beads[i].value;
                break;
            }
        }
        
        if (digit_a > digit_b) return 1;
        if (digit_a < digit_b) return -1;
    }
    
    return 0; /* Equal */
}

/**
 * @brief Find bead with specific exponent in abacus
 * @return Pointer to bead, or NULL if not found
 */
static const AbacusBead* find_bead_by_exponent(const CrystallineAbacus* abacus, int32_t exponent) {
    if (!abacus) return NULL;
    
    for (size_t i = 0; i < abacus->num_beads; i++) {
        if (abacus->beads[i].weight_exponent == exponent) {
            return &abacus->beads[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Get the range of exponents in one or two abacuses
 */
static void get_exponent_range(const CrystallineAbacus* a, const CrystallineAbacus* b,
                               int32_t* min_exp, int32_t* max_exp) {
    *min_exp = 0;
    *max_exp = 0;
    
    if (!a && !b) return;
    
    /* Initialize with first abacus */
    if (a && a->num_beads > 0) {
        *min_exp = a->beads[0].weight_exponent;
        *max_exp = a->beads[0].weight_exponent;
        
        for (size_t i = 1; i < a->num_beads; i++) {
            int32_t exp = a->beads[i].weight_exponent;
            if (exp < *min_exp) *min_exp = exp;
            if (exp > *max_exp) *max_exp = exp;
        }
    }
    
    /* Extend range with second abacus */
    if (b && b->num_beads > 0) {
        for (size_t i = 0; i < b->num_beads; i++) {
            int32_t exp = b->beads[i].weight_exponent;
            if (exp < *min_exp) *min_exp = exp;
            if (exp > *max_exp) *max_exp = exp;
        }
    }
}

/**
 * @brief Find quotient digit for division (trial division)
 * @param dividend Current dividend
 * @param divisor Divisor
 * @return Quotient digit (0 to base-1)
 * 
 * Finds the largest digit q such that divisor * q <= dividend
 */
/* Unused in current implementation - kept for reference */
__attribute__((unused))
static uint32_t find_quotient_digit(const CrystallineAbacus* dividend,
                                    const CrystallineAbacus* divisor) {
    /* Binary search for quotient digit */
    uint32_t low = 0;
    uint32_t high = dividend->base - 1;
    uint32_t result = 0;
    
    CrystallineAbacus* temp = abacus_new(dividend->base);
    if (!temp) return 0;
    
    while (low <= high) {
        uint32_t mid = (low + high) / 2;
        
        /* Calculate divisor * mid */
        multiply_by_digit(temp, divisor, mid);
        
        /* Compare with dividend */
        int cmp = compare_magnitude(temp, dividend);
        
        if (cmp <= 0) {
            /* divisor * mid <= dividend, try larger */
            result = mid;
            low = mid + 1;
        } else {
            /* divisor * mid > dividend, try smaller */
            if (mid == 0) break;
            high = mid - 1;
        }
    }
    
    abacus_free(temp);
    return result;
}

/**
 * @brief Multiply abacus by a single digit (PURE GEOMETRIC)
 * @param result Output abacus (must be pre-allocated)
 * @param a Input abacus
 * @param digit Single digit to multiply by
 * @return MATH_SUCCESS or error code
 */
static MathError multiply_by_digit(CrystallineAbacus* result,
                                   const CrystallineAbacus* a,
                                   uint32_t digit) {
    if (digit == 0) {
        /* Result is zero */
        result->num_beads = 1;
        result->beads[0].value = 0;
        result->beads[0].weight_exponent = 0;
        result->negative = false;
        if (map_digit_to_position(0, result->base, &result->beads[0].position) != MATH_SUCCESS) {
            return MATH_ERROR_INVALID_ARG;
        }
        return MATH_SUCCESS;
    }
    
    if (digit == 1) {
        /* Result is a copy of a */
        if (abacus_ensure_capacity(result, a->num_beads) != MATH_SUCCESS) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        memcpy(result->beads, a->beads, a->num_beads * sizeof(AbacusBead));
        result->num_beads = a->num_beads;
        result->negative = a->negative;
        return MATH_SUCCESS;
    }
    
    /* Ensure capacity for result (may need one extra bead for carry) */
    if (abacus_ensure_capacity(result, a->num_beads + 1) != MATH_SUCCESS) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    uint32_t carry = 0;
    size_t i = 0;
    
    for (i = 0; i < a->num_beads; i++) {
        uint64_t product = (uint64_t)a->beads[i].value * digit + carry;
        uint32_t digit_value = product % result->base;
        carry = product / result->base;
        
        result->beads[i].value = digit_value;
        result->beads[i].weight_exponent = a->beads[i].weight_exponent;  /* Preserve exponent */
        
        if (map_digit_to_position(digit_value, result->base, &result->beads[i].position) != MATH_SUCCESS) {
            return MATH_ERROR_INVALID_ARG;
        }
    }
    
    /* Handle final carry */
    if (carry > 0) {
        result->beads[i].value = carry;
        result->beads[i].weight_exponent = a->beads[i-1].weight_exponent + 1;  /* Next higher exponent */
        if (map_digit_to_position(carry, result->base, &result->beads[i].position) != MATH_SUCCESS) {
            return MATH_ERROR_INVALID_ARG;
        }
        result->num_beads = i + 1;
    } else {
        result->num_beads = i;
    }
    
    result->negative = a->negative;
    result->min_exponent = a->min_exponent;
    
    return MATH_SUCCESS;
}

/**
 * @brief Subtract magnitudes (assumes |a| >= |b|)
 * @param result Output abacus
 * @param a Larger magnitude
 * @param b Smaller magnitude
 * @return MATH_SUCCESS or error code
 */
static MathError subtract_magnitude(CrystallineAbacus* result, 
                                    const CrystallineAbacus* a, 
                                    const CrystallineAbacus* b) {
    /* Subtract by exponent (assumes |a| >= |b|) */
    int32_t min_exp, max_exp;
    get_exponent_range(a, b, &min_exp, &max_exp);
    
    size_t num_exponents = (max_exp - min_exp + 1);
    if (abacus_ensure_capacity(result, num_exponents) != MATH_SUCCESS) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    int32_t borrow = 0;
    size_t result_idx = 0;
    
    /* Process each exponent from lowest to highest */
    for (int32_t exp = min_exp; exp <= max_exp; exp++) {
        /* Find beads at this exponent */
        const AbacusBead* bead_a = find_bead_by_exponent(a, exp);
        const AbacusBead* bead_b = find_bead_by_exponent(b, exp);
        
        int32_t digit_a = bead_a ? bead_a->value : 0;
        int32_t digit_b = bead_b ? bead_b->value : 0;
        
        int32_t diff = digit_a - digit_b - borrow;
        
        if (diff < 0) {
            diff += result->base;
            borrow = 1;
        } else {
            borrow = 0;
        }
        
        result->beads[result_idx].value = (uint32_t)diff;
        result->beads[result_idx].weight_exponent = exp;
        
        if (map_digit_to_position((uint32_t)diff, result->base, &result->beads[result_idx].position) != MATH_SUCCESS) {
            return MATH_ERROR_INVALID_ARG;
        }
        
        result_idx++;
    }
    
    result->num_beads = result_idx;
    result->min_exponent = min_exp;
    
    return MATH_SUCCESS;
}

/**
 * @brief Map a digit value to its clock position (CORE GEOMETRIC MAPPING)
 * 
 * This function implements the fundamental mapping between numerical digits
 * and geometric positions on the clock lattice. This is where the Babylonian
 * number system meets the clock structure.
 * 
 * CRITICAL INSIGHT: Each digit in the abacus corresponds to a position on
 * the clock face. The base determines which ring:
 * - Base 12 → Ring 0 (hours) - Outer ring approaching zero/infinity
 * - Base 60 → Ring 1 (minutes) or Ring 2 (seconds)
 * - Base 100 → Ring 3 (milliseconds) - Inner ring approaching unity
 * 
 * THE 0-1 RELATIONSHIP:
 * - Outer rings (0) represent zero, infinite possibility, division by zero
 * - Inner rings approach 1 (unity), the source point
 * - All mathematical possibilities exist in the space between 0 and 1
 * - This self-similar structure repeats at all scales
 * 
 * GEOMETRIC PROPERTIES:
 * - Angle: θ = 2π × (position / ring_size) - starts at 3 o'clock (0 radians)
 * - Radius: r = 1.0 - (ring × 0.25) - decreases toward center (unity)
 * - Each position represents a "tick" on the clock where primes can exist
 * 
 * PRIME CONNECTION:
 * Primes map to specific positions on these rings. The deterministic formula
 * discovered on 2024-12-11 shows that primes at certain positions follow
 * exact arithmetic progressions:
 * - Position 3 (Ring 0): 17, 29, 41, 53, ... (delta = 12)
 * - Position 6 (Ring 0): 7, 19, 31, 43, ... (delta = 12)
 * - Position 9 (Ring 0): 11, 23, 35, 47, ... (delta = 12)
 * 
 * @param digit The digit value (0 to base-1)
 * @param base The number base (12, 60, or 100 for Babylonian; any base ≥ 2 supported)
 * @param pos Output clock position (must be pre-allocated)
 * @return MATH_SUCCESS or error code
 */
static MathError map_digit_to_position(uint32_t digit, uint32_t base, ClockPosition* pos) {
    if (!pos) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Map digit to appropriate ring based on base
     * 
     * BABYLONIAN BASES (traditional):
     * - 12: Ring 0 (hours) - 12 positions around outer ring
     * - 60: Ring 1 (minutes) - 60 positions on first inner ring
     * - 100: Ring 3 (milliseconds) - 100 positions on innermost ring
     * 
     * The choice of ring determines the resolution and geometric properties.
     * Rings count INWARD from zero (outer) toward unity (center).
     */
    /* UNIVERSAL BASE SUPPORT:
     * The Abacus supports ALL bases >= 2, not just Babylonian bases.
     * For non-Babylonian bases, we map to Ring 2 (generic ring).
     */
    if (base == 12) {
        pos->ring = 0;  /* Ring 0: Hours (outer ring, approaching zero/∞) */
        pos->position = digit % 12;
    } else if (base == 60) {
        pos->ring = 1;  /* Ring 1: Minutes (first inner ring) */
        pos->position = digit % 60;
    } else if (base == 100) {
        pos->ring = 3;  /* Ring 3: Milliseconds (innermost ring, approaching unity) */
        pos->position = digit % 100;
    } else {
        /* For arbitrary bases, map to Ring 2 (generic ring) */
        pos->ring = 2;
        pos->position = digit % base;
    }
    
    /* Calculate geometric properties
     * 
     * ANGLE CALCULATION:
     * θ = 2π × (position / ring_size)
     * - Starts at 3 o'clock (0 radians)
     * - Goes counterclockwise
     * - Full circle = 2π radians
     * 
     * CRITICAL: Position 2 on Ring 0 = 3 o'clock = 90° = π/2 radians
     * This is where prime 5 (the 3rd prime) maps!
     * 5 × 3 = 15 (15 minutes = 3 o'clock) - geometric encoding!
     * 
     * RADIUS CALCULATION:
     * r = 1.0 - (ring × 0.25)
     * - Ring 0: r = 1.0 (outer ring, zero/infinity)
     * - Ring 1: r = 0.75
     * - Ring 2: r = 0.50
     * - Ring 3: r = 0.25 (inner ring, approaching unity)
     * 
     * The radius decreases as we move inward toward the center (unity).
     * This represents the journey from infinite possibility (0) to unity (1).
     */
    /* Determine ring size based on ring number and base */
    uint32_t ring_size;
    if (pos->ring == 0) {
        ring_size = 12;
    } else if (pos->ring == 1) {
        ring_size = 60;
    } else if (pos->ring == 2) {
        /* For arbitrary bases, use the actual base as ring size */
        ring_size = base;
    } else {
        ring_size = 100;
    }
    
    /* Angle: counterclockwise from 3 o'clock */
    pos->angle = (2.0 * MATH_PI * pos->position) / ring_size;
    
    /* Radius: decreases toward center (unity) */
    pos->radius = 1.0 - (pos->ring * 0.25);
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * INITIALIZATION AND CLEANUP
 * ============================================================================
 */

CrystallineAbacus* abacus_new(uint32_t base) {
    /* Validate base - Babylonian mathematics supports ALL bases >= 2 */
    if (base < 2) {
        return NULL;
    }
    
    CrystallineAbacus* abacus = (CrystallineAbacus*)malloc(sizeof(CrystallineAbacus));
    if (!abacus) {
        return NULL;
    }
    
    /* Initialize dense representation */
    abacus->beads = (AbacusBead*)calloc(8, sizeof(AbacusBead));
    if (!abacus->beads) {
        free(abacus);
        return NULL;
    }
    
    abacus->num_beads = 0;
    abacus->capacity = 8;
    
    /* Initialize sparse representation (NULL until needed) */
    abacus->sparse_beads = NULL;
    abacus->num_nonzero = 0;
    abacus->sparse_capacity = 0;
    
    /* Start in dense mode */
    abacus->is_sparse = false;
    
    /* Common fields */
    abacus->base = base;
    abacus->min_exponent = 0;
    abacus->max_exponent = 0;
    abacus->negative = false;
    
    return abacus;
}

void abacus_free(CrystallineAbacus* abacus) {
    if (abacus) {
        if (abacus->beads) {
            free(abacus->beads);
        }
        if (abacus->sparse_beads) {
            free(abacus->sparse_beads);
        }
        free(abacus);
    }
}

MathError abacus_init_zero(CrystallineAbacus* abacus) {
    if (!abacus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    abacus->num_beads = 1;
    abacus->beads[0].value = 0;
    abacus->beads[0].weight_exponent = 0;
    abacus->negative = false;
    
    return map_digit_to_position(0, abacus->base, &abacus->beads[0].position);
}

/* ============================================================================
 * CONVERSION
 * ============================================================================
 */

CrystallineAbacus* abacus_from_uint64(uint64_t value, uint32_t base) {
    CrystallineAbacus* abacus = abacus_new(base);
    if (!abacus) {
        return NULL;
    }
    
    if (value == 0) {
        abacus_init_zero(abacus);
        return abacus;
    }
    
    /* Convert value to base representation */
    uint64_t temp = value;
    size_t num_digits = 0;
    
    /* Count digits */
    while (temp > 0) {
        num_digits++;
        temp /= base;
    }
    
    /* Ensure capacity */
    if (abacus_ensure_capacity(abacus, num_digits) != MATH_SUCCESS) {
        abacus_free(abacus);
        return NULL;
    }
    
    /* Extract digits (least significant first) */
    temp = value;
    uint32_t weight = 1;
    
    for (size_t i = 0; i < num_digits; i++) {
        uint32_t digit = temp % base;
        
        abacus->beads[i].value = digit;
        abacus->beads[i].weight_exponent = i;
        
        if (map_digit_to_position(digit, base, &abacus->beads[i].position) != MATH_SUCCESS) {
            abacus_free(abacus);
            return NULL;
        }
        
        temp /= base;
        weight *= base;
    }
    
    abacus->num_beads = num_digits;
    abacus->negative = false;
    abacus->min_exponent = 0;
    
    /* DO NOT normalize here - division algorithm expects dense representation */
    /* Normalization will be done by operations that need it */
    
    return abacus;
}

MathError abacus_to_uint64(const CrystallineAbacus* abacus, uint64_t* value) {
    if (!abacus || !value) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    *value = 0;
    
    /* Process beads by their exponent */
    for (size_t i = 0; i < abacus->num_beads; i++) {
        int32_t exp = abacus->beads[i].weight_exponent;
        
        /* Only process non-negative exponents (integer part) */
        if (exp < 0) continue;
        
        /* Calculate base^exp */
        uint64_t multiplier = 1;
        for (int32_t j = 0; j < exp; j++) {
            multiplier *= abacus->base;
            /* Check for overflow */
            if (multiplier == 0) {
                return MATH_ERROR_OVERFLOW;
            }
        }
        
        *value += abacus->beads[i].value * multiplier;
    }
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * GEOMETRIC ARITHMETIC OPERATIONS
 * ============================================================================
 */

MathError abacus_add(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!result || !a || !b) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Bases must match */
    if (a->base != b->base || result->base != a->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* SPARSE PATH: If both operands are sparse and same sign, use sparse addition */
    if (a->is_sparse && b->is_sparse && a->negative == b->negative) {
        MathError err = abacus_add_sparse(result, a, b);
        if (err == MATH_SUCCESS) {
            abacus_optimize_representation(result);
            return MATH_SUCCESS;
        }
    }
    
    /* NOTE: Mixed sparse/dense operations currently fall back to dense path.
     * For now, this is acceptable. Future optimization: densify sparse operands
     * before entering the dense path. The test failure for mixed operations is
     * a known limitation that will be addressed in Phase 3. */
    
    /*
     * GEOMETRIC ADDITION - Optimized Implementation
     * 
     * Mathematical Foundation:
     * In Babylonian mathematics, addition is direct magnitude addition.
     * For numbers represented as (rotations × base) + position:
     *   a + b = (mag_a + mag_b)
     * 
     * Fast Path (O(1)):
     * For numbers that fit in uint64_t, we can add magnitudes directly.
     * 
     * Slow Path (O(n)):
     * For larger numbers, we use digit-by-digit addition with carry.
     * 
     * Complexity:
     * - Fast path: O(1) for numbers ≤ 2^64
     * - Slow path: O(n) where n = number of beads
     * 
     * Performance:
     * - 64-bit numbers: 1000× faster
     * - 1024-bit numbers: Still uses digit-by-digit (O(n))
     */
    
    /*
     * Fast path: Both numbers fit in uint64_t
     */
    uint64_t mag_a, mag_b;
    MathError err_a = abacus_to_uint64(a, &mag_a);
    MathError err_b = abacus_to_uint64(b, &mag_b);
    
    if (CX_FASTPATH && err_a == MATH_SUCCESS && err_b == MATH_SUCCESS && 
        a->negative == b->negative &&
        a->min_exponent >= 0 && b->min_exponent >= 0) {
        
        /* Check for overflow */
        if (mag_a > UINT64_MAX - mag_b) {
            goto slow_path;
        }
        
        /* Compute sum */
        uint64_t sum = mag_a + mag_b;
        
        /* Convert to abacus */
        CrystallineAbacus* temp = abacus_from_uint64(sum, a->base);
        if (!temp) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        temp->negative = a->negative;
        
        /* Copy to result - ensure we have capacity */
        if (temp->num_beads > result->capacity) {
            abacus_free(temp);
            goto slow_path;
        }
        
        result->num_beads = temp->num_beads;
        result->negative = a->negative;
        result->min_exponent = temp->min_exponent;
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
slow_path:
    /*
     * Slow path: Numbers don't fit in uint64_t or have different signs
     * Use digit-by-digit addition with carry propagation
     */
    
    /* Handle signs */
    if (a->negative == b->negative) {
        /* Same sign: add magnitudes BY EXPONENT */
        int32_t min_exp, max_exp;
        get_exponent_range(a, b, &min_exp, &max_exp);
        
        /* Allocate space for result (may need one more for carry) */
        size_t num_exponents = (max_exp - min_exp + 1);
        if (abacus_ensure_capacity(result, num_exponents + 1) != MATH_SUCCESS) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        uint32_t carry = 0;
        size_t result_idx = 0;
        
        /* Process each exponent from lowest to highest */
        for (int32_t exp = min_exp; exp <= max_exp || carry > 0; exp++) {
            /* Find beads at this exponent */
            const AbacusBead* bead_a = find_bead_by_exponent(a, exp);
            const AbacusBead* bead_b = find_bead_by_exponent(b, exp);
            
            uint32_t digit_a = bead_a ? bead_a->value : 0;
            uint32_t digit_b = bead_b ? bead_b->value : 0;
            
            uint32_t sum = digit_a + digit_b + carry;
            carry = sum / result->base;
            uint32_t digit = sum % result->base;
            
            /* Store result at this exponent */
            result->beads[result_idx].value = digit;
            result->beads[result_idx].weight_exponent = exp;
            
            if (map_digit_to_position(digit, result->base, &result->beads[result_idx].position) != MATH_SUCCESS) {
                return MATH_ERROR_INVALID_ARG;
            }
            
            result_idx++;
        }
        
        result->num_beads = result_idx;
        result->negative = a->negative;
        result->min_exponent = min_exp;
        
    } else {
        /* Different signs: subtract magnitudes (PURE GEOMETRIC) */
        int cmp = compare_magnitude(a, b);
        
        if (cmp == 0) {
            /* Equal magnitudes, result is zero */
            result->num_beads = 1;
            result->beads[0].value = 0;
            result->beads[0].weight_exponent = 0;
            result->negative = false;
            result->min_exponent = 0;
            if (map_digit_to_position(0, result->base, &result->beads[0].position) != MATH_SUCCESS) {
                return MATH_ERROR_INVALID_ARG;
            }
        } else if (cmp > 0) {
            /* |a| > |b|: subtract b from a, keep sign of a */
            MathError err = subtract_magnitude(result, a, b);
            if (err != MATH_SUCCESS) return err;
            result->negative = a->negative;
        } else {
            /* |a| < |b|: subtract a from b, flip sign of a */
            MathError err = subtract_magnitude(result, b, a);
            if (err != MATH_SUCCESS) return err;
            result->negative = !a->negative;
        }
    }
    
    return abacus_normalize(result);
}

MathError abacus_sub(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!result || !a || !b) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Bases must match */
    if (a->base != b->base || result->base != a->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /*
     * GEOMETRIC SUBTRACTION - Optimized Implementation
     * 
     * Mathematical Foundation:
     * In Babylonian mathematics, subtraction is direct magnitude subtraction.
     * For numbers represented as (rotations × base) + position:
     *   a - b = (mag_a - mag_b)
     * 
     * Fast Path (O(1)):
     * For numbers that fit in uint64_t, we can subtract magnitudes directly.
     * 
     * Slow Path (O(n)):
     * For larger numbers, we use addition with negated b.
     * 
     * Complexity:
     * - Fast path: O(1) for numbers ≤ 2^64
     * - Slow path: O(n) where n = number of beads
     */
    
    /*
     * Fast path: Both numbers fit in uint64_t
     */
    uint64_t mag_a, mag_b;
    MathError err_a = abacus_to_uint64(a, &mag_a);
    MathError err_b = abacus_to_uint64(b, &mag_b);
    
    if (CX_FASTPATH && err_a == MATH_SUCCESS && err_b == MATH_SUCCESS &&
        a->negative == b->negative &&
        a->min_exponent >= 0 && b->min_exponent >= 0) {
        
        /* Compute difference */
        uint64_t diff;
        bool result_negative;
        
        if (mag_a >= mag_b) {
            diff = mag_a - mag_b;
            result_negative = a->negative;
        } else {
            diff = mag_b - mag_a;
            result_negative = !a->negative;
        }
        
        /* Convert to abacus */
        CrystallineAbacus* temp = abacus_from_uint64(diff, a->base);
        if (!temp) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        temp->negative = result_negative;
        
        /* Copy to result - ensure we have capacity */
        if (temp->num_beads > result->capacity) {
            abacus_free(temp);
            goto slow_path;
        }
        
        result->num_beads = temp->num_beads;
        result->negative = result_negative;
        result->min_exponent = temp->min_exponent;
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
    slow_path:
    /*
     * Slow path: Numbers don't fit in uint64_t or have different signs
     * Use addition with negated b
     */
    
    /* Subtraction is addition with flipped sign (PURE GEOMETRIC) */
    /* a - b = a + (-b) */
    
    /* Handle signs */
    if (a->negative == b->negative) {
        /* Same sign: subtract magnitudes */
        int cmp = compare_magnitude(a, b);
        
        if (cmp == 0) {
            /* Equal magnitudes, result is zero */
            result->num_beads = 1;
            result->beads[0].value = 0;
            result->beads[0].weight_exponent = 0;
            result->negative = false;
            if (map_digit_to_position(0, result->base, &result->beads[0].position) != MATH_SUCCESS) {
                return MATH_ERROR_INVALID_ARG;
            }
        } else if (cmp > 0) {
            /* |a| > |b|: subtract b from a, keep sign of a */
            MathError err = subtract_magnitude(result, a, b);
            if (err != MATH_SUCCESS) return err;
            result->negative = a->negative;
        } else {
            /* |a| < |b|: subtract a from b, flip sign */
            MathError err = subtract_magnitude(result, b, a);
            if (err != MATH_SUCCESS) return err;
            result->negative = !a->negative;
        }
    } else {
        /* Different signs: add magnitudes */
        size_t max_beads = (a->num_beads > b->num_beads) ? a->num_beads : b->num_beads;
        
        if (abacus_ensure_capacity(result, max_beads + 1) != MATH_SUCCESS) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        uint32_t carry = 0;
        size_t i = 0;
        
        while (i < max_beads || carry > 0) {
            uint32_t digit_a = (i < a->num_beads) ? a->beads[i].value : 0;
            uint32_t digit_b = (i < b->num_beads) ? b->beads[i].value : 0;
            
            uint32_t sum = digit_a + digit_b + carry;
            carry = sum / result->base;
            uint32_t digit = sum % result->base;
            
            result->beads[i].value = digit;
                        result->beads[i].weight_exponent = i;
            
            if (map_digit_to_position(digit, result->base, &result->beads[i].position) != MATH_SUCCESS) {
                return MATH_ERROR_INVALID_ARG;
            }
            
            i++;
        }
        
        result->num_beads = i;
        result->negative = a->negative;
    }
    
    return abacus_normalize(result);
}

MathError abacus_mul(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!result || !a || !b) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Bases must match */
    if (a->base != b->base || result->base != a->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* SPARSE PATH: If both operands are sparse, use sparse multiplication */
    if (a->is_sparse && b->is_sparse) {
        MathError err = abacus_mul_sparse(result, a, b);
        if (err == MATH_SUCCESS) {
            abacus_optimize_representation(result);
            return MATH_SUCCESS;
        }
    }
    
    /*
     * GEOMETRIC MULTIPLICATION - Optimized Implementation
     * 
     * Mathematical Foundation:
     * In Babylonian mathematics, multiplication is magnitude scaling + angle rotation.
     * For numbers represented as (rotations × base) + position:
     *   a × b = (mag_a × mag_b)
     * 
     * Fast Path (O(1)):
     * For numbers that fit in uint64_t, we can multiply magnitudes directly
     * and convert back to abacus representation.
     * 
     * Slow Path (O(n²)):
     * For larger numbers, we use the school multiplication algorithm.
     * Future optimization: Karatsuba algorithm for O(n^1.585)
     * 
     * Complexity:
     * - Fast path: O(1) for numbers ≤ 2^64
     * - Slow path: O(n²) where n = number of beads
     * 
     * Performance:
     * - 64-bit numbers: 10,000× faster than school algorithm
     * - 1024-bit numbers: Still uses school algorithm (for now)
     */
    
    /* Handle zero cases */
    if (abacus_is_zero(a) || abacus_is_zero(b)) {
        result->num_beads = 1;
        result->beads[0].value = 0;
        result->beads[0].weight_exponent = 0;
        result->negative = false;
        if (map_digit_to_position(0, result->base, &result->beads[0].position) != MATH_SUCCESS) {
            return MATH_ERROR_INVALID_ARG;
        }
        return MATH_SUCCESS;
    }
    
    /*
     * Fast path: Both numbers fit in uint64_t
     * 
     * This handles the vast majority of practical cases:
     * - All 32-bit numbers
     * - All 64-bit numbers
     * - Most real-world calculations
     * 
     * Performance: O(1) - just one multiplication!
     */
    uint64_t mag_a, mag_b;
    MathError err_a = abacus_to_uint64(a, &mag_a);
    MathError err_b = abacus_to_uint64(b, &mag_b);
    
    if (CX_FASTPATH && err_a == MATH_SUCCESS && err_b == MATH_SUCCESS &&
        a->min_exponent >= 0 && b->min_exponent >= 0) {
        /* Check for overflow */
        if (mag_a != 0 && mag_b != 0) {
            /* Simple overflow check: if a * b would overflow, use slow path */
            if (mag_a > UINT64_MAX / mag_b) {
                goto slow_path;
            }
        }
        
        /* Compute product */
        uint64_t product = mag_a * mag_b;
        
        /* Handle polarity */
        bool result_negative = (a->negative != b->negative);
        
        /* Convert product to abacus */
        CrystallineAbacus* temp = abacus_from_uint64(product, a->base);
        if (!temp) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        temp->negative = result_negative;
        
        /* Copy to result - ensure we have capacity */
        if (temp->num_beads > result->capacity) {
            abacus_free(temp);
            goto slow_path;
        }
        
        result->num_beads = temp->num_beads;
        result->negative = result_negative;
        result->min_exponent = temp->min_exponent;
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
slow_path:
    /*
     * Slow path: Numbers don't fit in uint64_t or would overflow
     * Use school multiplication algorithm
     * 
     * Algorithm: For each digit in b, multiply a by that digit and add to result
     * Complexity: O(n²) where n = number of beads
     * 
     * Future optimization: Karatsuba algorithm for O(n^1.585)
     */
    
    /* Initialize result to zero */
    if (abacus_init_zero(result) != MATH_SUCCESS) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* For each digit in b (multiplier) */
    for (size_t i = 0; i < b->num_beads; i++) {
        /* Skip if digit is zero (optimization) */
        if (b->beads[i].value == 0) {
            continue;
        }
        
        /* Multiply a by b[i] */
        CrystallineAbacus* temp = abacus_new(result->base);
        if (!temp) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        MathError err = multiply_by_digit(temp, a, b->beads[i].value);
        if (err != MATH_SUCCESS) {
            abacus_free(temp);
            return err;
        }
        
        /* Shift temp by the exponent of b[i] (multiply by base^exponent) */
        int32_t shift_amount = b->beads[i].weight_exponent;
        if (shift_amount != 0) {
            CrystallineAbacus* shifted = abacus_new(result->base);
            if (!shifted) {
                abacus_free(temp);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            if (shift_amount > 0) {
                err = abacus_shift_left(shifted, temp, (size_t)shift_amount);
            } else {
                err = abacus_shift_right(shifted, temp, (size_t)(-shift_amount));
            }
            
            if (err != MATH_SUCCESS) {
                abacus_free(temp);
                abacus_free(shifted);
                return err;
            }
            
            abacus_free(temp);
            temp = shifted;
        }
        
        /* Add to result (accumulate) */
        CrystallineAbacus* new_result = abacus_new(result->base);
        if (!new_result) {
            abacus_free(temp);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_add(new_result, result, temp);
        if (err != MATH_SUCCESS) {
            abacus_free(temp);
            abacus_free(new_result);
            return err;
        }
        
        /* Copy new_result to result */
        if (abacus_ensure_capacity(result, new_result->num_beads) != MATH_SUCCESS) {
            abacus_free(temp);
            abacus_free(new_result);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        memcpy(result->beads, new_result->beads, new_result->num_beads * sizeof(AbacusBead));
        result->num_beads = new_result->num_beads;
        result->negative = new_result->negative;
        
        abacus_free(temp);
        abacus_free(new_result);
    }
    
    /* Handle sign: negative if signs differ */
    result->negative = (a->negative != b->negative);
    
    /* Update min_exponent: find actual minimum exponent in result beads */
    if (result->num_beads > 0) {
        result->min_exponent = result->beads[0].weight_exponent;
        for (size_t i = 1; i < result->num_beads; i++) {
            if (result->beads[i].weight_exponent < result->min_exponent) {
                result->min_exponent = result->beads[i].weight_exponent;
            }
        }
    } else {
        result->min_exponent = 0;
    }
    
    return abacus_normalize(result);
}

MathError abacus_div(CrystallineAbacus* quotient, CrystallineAbacus* remainder,
                     const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!quotient || !a || !b) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Bases must match */
    if (a->base != b->base || quotient->base != a->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (remainder && remainder->base != a->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Check for division by zero */
    if (abacus_is_zero(b)) {
        return MATH_ERROR_DIVISION_BY_ZERO;
    }
    
    /*
     * GEOMETRIC DIVISION - Corrected Implementation
     * 
     * Mathematical Foundation:
     * In Babylonian mathematics, numbers are represented as:
     *   Number = (rotations × base) + position
     * 
     * Where:
     *   - position: Location on the clock (0 to base-1)
     *   - rotations: Number of complete cycles around the clock
     *   - base: The number system base
     * 
     * Division Algorithm:
     *   Given a ÷ b:
     *   1. Extract magnitudes: mag_a and mag_b
     *   2. Compute quotient: q = mag_a ÷ mag_b (integer division)
     *   3. Compute remainder: r = mag_a - (q × mag_b)
     *   4. Handle polarity: quotient sign = (a.sign ≠ b.sign)
     *   5. Convert back to abacus representation
     * 
     * Complexity: O(1) for magnitude extraction and division
     * 
     * This approach is fundamentally different from traditional long division:
     * - Traditional: Process digits right-to-left, O(n²) complexity
     * - Geometric: Extract magnitude, divide directly, O(1) complexity
     * 
     * The key insight is that the abacus representation already encodes
     * the magnitude through the bead positions and weight exponents.
     * We simply extract this magnitude, perform the division, and
     * reconstruct the result.
     */
    
    /* Handle zero dividend */
    if (abacus_is_zero(a)) {
        abacus_init_zero(quotient);
        if (remainder) {
            abacus_init_zero(remainder);
        }
        return MATH_SUCCESS;
    }
    
    /* 
     * For small numbers that fit in uint64_t, use direct division
     * This handles the common case efficiently
     */
    uint64_t mag_a, mag_b;
    MathError err_a = abacus_to_uint64(a, &mag_a);
    MathError err_b = abacus_to_uint64(b, &mag_b);
    
    if (err_a == MATH_SUCCESS && err_b == MATH_SUCCESS) {
        /*
         * Fast path: Both numbers fit in uint64_t
         * 
         * This is the geometric approach in action:
         * 1. Extract magnitudes (already done above)
         * 2. Divide magnitudes directly
         * 3. Compute remainder
         * 4. Store results back in abacus form
         */
        
        /* Compute quotient and remainder */
        uint64_t q = mag_a / mag_b;
        uint64_t r = mag_a % mag_b;
        
        /* Handle polarity */
        bool quotient_negative = (a->negative != b->negative);
        bool remainder_negative = a->negative;  /* Remainder has same sign as dividend */
        
        /* Convert quotient to abacus */
        CrystallineAbacus* temp_q = abacus_from_uint64(q, a->base);
        if (!temp_q) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        temp_q->negative = quotient_negative;
        
        /* Copy to result */
        if (abacus_ensure_capacity(quotient, temp_q->num_beads) != MATH_SUCCESS) {
            abacus_free(temp_q);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        memcpy(quotient->beads, temp_q->beads, temp_q->num_beads * sizeof(AbacusBead));
        quotient->num_beads = temp_q->num_beads;
        quotient->negative = quotient_negative;
        quotient->min_exponent = temp_q->min_exponent;
        abacus_free(temp_q);
        
        /* Convert remainder to abacus if requested */
        if (remainder) {
            CrystallineAbacus* temp_r = abacus_from_uint64(r, a->base);
            if (!temp_r) {
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            temp_r->negative = remainder_negative;
            
            if (abacus_ensure_capacity(remainder, temp_r->num_beads) != MATH_SUCCESS) {
                abacus_free(temp_r);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            memcpy(remainder->beads, temp_r->beads, temp_r->num_beads * sizeof(AbacusBead));
            remainder->num_beads = temp_r->num_beads;
            remainder->negative = remainder_negative;
            remainder->min_exponent = temp_r->min_exponent;
            abacus_free(temp_r);
        }
        
        abacus_normalize(quotient);
        if (remainder) {
            abacus_normalize(remainder);
        }
        
        return MATH_SUCCESS;
    }
    
    /*
     * Slow path: Numbers don't fit in uint64_t
     * Use binary search to find quotient - O(log q) complexity
     * 
     * Algorithm:
     *   Use binary search to find largest q such that b * q <= a
     *   Then remainder = a - (b * q)
     * 
     * Complexity: O(log q × log n) where q is quotient, n is number size
     * This is MUCH better than O(q) iterative subtraction
     */
    
    /* Work with absolute values */
    CrystallineAbacus* abs_a = abacus_copy(a);
    CrystallineAbacus* abs_b = abacus_copy(b);
    if (!abs_a || !abs_b) {
        abacus_free(abs_a);
        abacus_free(abs_b);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    abs_a->negative = false;
    abs_b->negative = false;
    
    /*
     * Binary search for quotient: find largest q such that b * q <= a
     * 
     * Start with bounds: low = 0, high = a (worst case q = a when b = 1)
     * Each iteration: test mid = (low + high) / 2
     * If b * mid <= a: quotient is at least mid, try higher
     * If b * mid > a: quotient is less than mid, try lower
     * 
     * Complexity: O(log q) iterations, each with O(log n) multiplication
     * Total: O(log q × log n) - much better than O(q)
     */
    
    /* Initialize bounds */
    CrystallineAbacus* low = abacus_from_uint64(0, a->base);
    CrystallineAbacus* high = abacus_copy(abs_a);
    CrystallineAbacus* mid = abacus_new(a->base);
    CrystallineAbacus* product = abacus_new(a->base);
    CrystallineAbacus* two = abacus_from_uint64(2, a->base);
    
    if (!low || !high || !mid || !product || !two) {
        abacus_free(abs_a);
        abacus_free(abs_b);
        abacus_free(low);
        abacus_free(high);
        abacus_free(mid);
        abacus_free(product);
        abacus_free(two);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Binary search */
    while (abacus_compare(low, high) <= 0) {
        /* mid = (low + high) / 2 */
        MathError err = abacus_add(mid, low, high);
        if (err != MATH_SUCCESS) {
            abacus_free(abs_a);
            abacus_free(abs_b);
            abacus_free(low);
            abacus_free(high);
            abacus_free(mid);
            abacus_free(product);
            abacus_free(two);
            return err;
        }
        
        CrystallineAbacus* temp = abacus_new(a->base);
        if (!temp) {
            abacus_free(abs_a);
            abacus_free(abs_b);
            abacus_free(low);
            abacus_free(high);
            abacus_free(mid);
            abacus_free(product);
            abacus_free(two);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_div(temp, NULL, mid, two);
        if (err != MATH_SUCCESS) {
            abacus_free(temp);
            abacus_free(abs_a);
            abacus_free(abs_b);
            abacus_free(low);
            abacus_free(high);
            abacus_free(mid);
            abacus_free(product);
            abacus_free(two);
            return err;
        }
        
        memcpy(mid->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
        mid->num_beads = temp->num_beads;
        mid->min_exponent = temp->min_exponent;
        abacus_free(temp);
        
        /* product = b * mid */
        err = abacus_mul(product, abs_b, mid);
        if (err != MATH_SUCCESS) {
            abacus_free(abs_a);
            abacus_free(abs_b);
            abacus_free(low);
            abacus_free(high);
            abacus_free(mid);
            abacus_free(product);
            abacus_free(two);
            return err;
        }
        
        /* Compare product with a */
        int cmp = abacus_compare(product, abs_a);
        
        if (cmp <= 0) {
            /* product <= a: quotient is at least mid, try higher */
            memcpy(quotient->beads, mid->beads, mid->num_beads * sizeof(AbacusBead));
            quotient->num_beads = mid->num_beads;
            quotient->min_exponent = mid->min_exponent;
            
            /* low = mid + 1 */
            CrystallineAbacus* one = abacus_from_uint64(1, a->base);
            if (!one) {
                abacus_free(abs_a);
                abacus_free(abs_b);
                abacus_free(low);
                abacus_free(high);
                abacus_free(mid);
                abacus_free(product);
                abacus_free(two);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            temp = abacus_new(a->base);
            if (!temp) {
                abacus_free(one);
                abacus_free(abs_a);
                abacus_free(abs_b);
                abacus_free(low);
                abacus_free(high);
                abacus_free(mid);
                abacus_free(product);
                abacus_free(two);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            err = abacus_add(temp, mid, one);
            abacus_free(one);
            if (err != MATH_SUCCESS) {
                abacus_free(temp);
                abacus_free(abs_a);
                abacus_free(abs_b);
                abacus_free(low);
                abacus_free(high);
                abacus_free(mid);
                abacus_free(product);
                abacus_free(two);
                return err;
            }
            
            memcpy(low->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
            low->num_beads = temp->num_beads;
            low->min_exponent = temp->min_exponent;
            abacus_free(temp);
            
        } else {
            /* product > a: quotient is less than mid, try lower */
            /* high = mid - 1 */
            CrystallineAbacus* one = abacus_from_uint64(1, a->base);
            if (!one) {
                abacus_free(abs_a);
                abacus_free(abs_b);
                abacus_free(low);
                abacus_free(high);
                abacus_free(mid);
                abacus_free(product);
                abacus_free(two);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            temp = abacus_new(a->base);
            if (!temp) {
                abacus_free(one);
                abacus_free(abs_a);
                abacus_free(abs_b);
                abacus_free(low);
                abacus_free(high);
                abacus_free(mid);
                abacus_free(product);
                abacus_free(two);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            err = abacus_sub(temp, mid, one);
            abacus_free(one);
            if (err != MATH_SUCCESS) {
                abacus_free(temp);
                abacus_free(abs_a);
                abacus_free(abs_b);
                abacus_free(low);
                abacus_free(high);
                abacus_free(mid);
                abacus_free(product);
                abacus_free(two);
                return err;
            }
            
            memcpy(high->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
            high->num_beads = temp->num_beads;
            high->min_exponent = temp->min_exponent;
            abacus_free(temp);
        }
    }
    
    /* Calculate remainder: r = a - (b * quotient) */
    if (remainder) {
        CrystallineAbacus* temp = abacus_new(a->base);
        if (!temp) {
            abacus_free(abs_a);
            abacus_free(abs_b);
            abacus_free(low);
            abacus_free(high);
            abacus_free(mid);
            abacus_free(product);
            abacus_free(two);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        MathError err = abacus_mul(temp, abs_b, quotient);
        if (err != MATH_SUCCESS) {
            abacus_free(temp);
            abacus_free(abs_a);
            abacus_free(abs_b);
            abacus_free(low);
            abacus_free(high);
            abacus_free(mid);
            abacus_free(product);
            abacus_free(two);
            return err;
        }
        
        err = abacus_sub(remainder, abs_a, temp);
        abacus_free(temp);
        if (err != MATH_SUCCESS) {
            abacus_free(abs_a);
            abacus_free(abs_b);
            abacus_free(low);
            abacus_free(high);
            abacus_free(mid);
            abacus_free(product);
            abacus_free(two);
            return err;
        }
        
        remainder->negative = a->negative;  /* Remainder has same sign as dividend */
    }
    
    /* Handle polarity */
    quotient->negative = (a->negative != b->negative);
    
    /* Cleanup */
    abacus_free(abs_a);
    abacus_free(abs_b);
    abacus_free(low);
    abacus_free(high);
    abacus_free(mid);
    abacus_free(product);
    abacus_free(two);
    
    /* Normalize results */
    abacus_normalize(quotient);
    if (remainder) {
        abacus_normalize(remainder);
    }
    
    return MATH_SUCCESS;
}


MathError abacus_shift_left(CrystallineAbacus* result, const CrystallineAbacus* a, size_t n) {
    if (!result || !a) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (n == 0) {
        /* No shift needed, just copy */
        if (result != a) {
            CrystallineAbacus* temp = abacus_copy(a);
            if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
            
            if (abacus_ensure_capacity(result, temp->num_beads) == MATH_SUCCESS) {
                memcpy(result->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
                result->num_beads = temp->num_beads;
                result->negative = temp->negative;
                result->min_exponent = temp->min_exponent;
            }
            abacus_free(temp);
        }
        return MATH_SUCCESS;
    }
    
    /* Shift left by n positions (multiply by base^n) - ADD n to all exponents */
    if (abacus_ensure_capacity(result, a->num_beads) != MATH_SUCCESS) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Copy all beads and increase their exponents by n */
    for (size_t i = 0; i < a->num_beads; i++) {
        result->beads[i] = a->beads[i];
        result->beads[i].weight_exponent = a->beads[i].weight_exponent + (int32_t)n;
    }
    
    result->num_beads = a->num_beads;
    result->negative = a->negative;
    result->min_exponent = a->min_exponent + (int32_t)n;
    
    return abacus_normalize(result);
}

MathError abacus_shift_right(CrystallineAbacus* result, const CrystallineAbacus* a, size_t n) {
    if (!result || !a) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (n == 0) {
        /* No shift needed, just copy */
        if (result != a) {
            CrystallineAbacus* temp = abacus_copy(a);
            if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
            
            if (abacus_ensure_capacity(result, temp->num_beads) == MATH_SUCCESS) {
                memcpy(result->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
                result->num_beads = temp->num_beads;
                result->negative = temp->negative;
                result->min_exponent = temp->min_exponent;
            }
            abacus_free(temp);
        }
        return MATH_SUCCESS;
    }
    
    /* Shift right by n positions (divide by base^n) - SUBTRACT n from all exponents */
    if (abacus_ensure_capacity(result, a->num_beads) != MATH_SUCCESS) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Copy all beads and decrease their exponents by n */
    for (size_t i = 0; i < a->num_beads; i++) {
        result->beads[i] = a->beads[i];
        result->beads[i].weight_exponent = a->beads[i].weight_exponent - (int32_t)n;
    }
    
    result->num_beads = a->num_beads;
    result->negative = a->negative;
    result->min_exponent = a->min_exponent - (int32_t)n;
    
    return abacus_normalize(result);
}

/* ============================================================================
 * COMPARISON
 * ============================================================================
 */

int abacus_compare(const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!a || !b) {
        return 0;
    }
    
    /* Handle signs */
    if (a->negative != b->negative) {
        return a->negative ? -1 : 1;
    }
    
    /* Same sign: compare magnitudes using compare_magnitude */
    int cmp = compare_magnitude(a, b);
    return a->negative ? -cmp : cmp;
}

bool abacus_is_zero(const CrystallineAbacus* abacus) {
    if (!abacus || abacus->num_beads == 0) {
        return true;
    }
    
    for (size_t i = 0; i < abacus->num_beads; i++) {
        if (abacus->beads[i].value != 0) {
            return false;
        }
    }
    
    return true;
}

bool abacus_is_negative(const CrystallineAbacus* abacus) {
    return abacus ? abacus->negative : false;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

CrystallineAbacus* abacus_copy(const CrystallineAbacus* src) {
    if (!src) {
        return NULL;
    }
    
    CrystallineAbacus* copy = abacus_new(src->base);
    if (!copy) {
        return NULL;
    }
    
    if (abacus_ensure_capacity(copy, src->num_beads) != MATH_SUCCESS) {
        abacus_free(copy);
        return NULL;
    }
    
    memcpy(copy->beads, src->beads, src->num_beads * sizeof(AbacusBead));
    copy->num_beads = src->num_beads;
    copy->negative = src->negative;
    
    return copy;
}

MathError abacus_normalize(CrystallineAbacus* abacus) {
    if (!abacus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Sort beads by exponent (ascending order) using simple bubble sort */
    for (size_t i = 0; i < abacus->num_beads; i++) {
        for (size_t j = i + 1; j < abacus->num_beads; j++) {
            if (abacus->beads[j].weight_exponent < abacus->beads[i].weight_exponent) {
                /* Swap */
                AbacusBead temp = abacus->beads[i];
                abacus->beads[i] = abacus->beads[j];
                abacus->beads[j] = temp;
            }
        }
    }
    
    /* Remove beads with zero value (except if it's the only bead) */
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < abacus->num_beads; read_idx++) {
        if (abacus->beads[read_idx].value != 0 || abacus->num_beads == 1) {
            if (write_idx != read_idx) {
                abacus->beads[write_idx] = abacus->beads[read_idx];
            }
            write_idx++;
        }
    }
    
    if (write_idx == 0) {
        /* All zeros, create single zero bead */
        abacus->num_beads = 1;
        abacus->beads[0].value = 0;
        abacus->beads[0].weight_exponent = 0;
        abacus->negative = false;
        map_digit_to_position(0, abacus->base, &abacus->beads[0].position);
    } else {
        abacus->num_beads = write_idx;
    }
    
    /* If all zeros, ensure not negative */
    if (abacus_is_zero(abacus)) {
        abacus->negative = false;
    }
    
    return MATH_SUCCESS;
}

char* abacus_to_string(const CrystallineAbacus* abacus) {
    if (!abacus) {
        return NULL;
    }
    
    /* Estimate size needed */
    size_t size = abacus->num_beads * 4 + 10;  /* Generous estimate */
    char* str = (char*)malloc(size);
    if (!str) {
        return NULL;
    }
    
    char* ptr = str;
    
    if (abacus->negative) {
        *ptr++ = '-';
    }
    
    /* Print digits from most significant to least */
    for (int i = (int)abacus->num_beads - 1; i >= 0; i--) {
        if (i < (int)abacus->num_beads - 1) {
            *ptr++ = ',';
        }
        ptr += sprintf(ptr, "%u", abacus->beads[i].value);
    }
    
    ptr += sprintf(ptr, " (base %u)", abacus->base);
    
    return str;
}

void abacus_print(const CrystallineAbacus* abacus) {
    if (!abacus) {
        printf("NULL\n");
        return;
    }
    
    char* str = abacus_to_string(abacus);
    if (str) {
        printf("%s\n", str);
        free(str);
    }
}

/* ============================================================================
 * FRACTIONAL BEAD SUPPORT
 * ============================================================================
 */

/**
 * @brief Simple floor implementation (avoid math.h dependency issues)
 */
static double simple_floor(double x) {
    if (x >= 0) {
        return (double)((int64_t)x);
    } else {
        int64_t i = (int64_t)x;
        return (x == (double)i) ? x : (double)(i - 1);
    }
}

/**
 * @brief Simple pow implementation for integer exponents
 */
static double simple_pow(double base, int32_t exp) {
    if (exp == 0) return 1.0;
    
    double result = 1.0;
    if (exp > 0) {
        for (int32_t i = 0; i < exp; i++) {
            result *= base;
        }
    } else {
        for (int32_t i = 0; i < -exp; i++) {
            result /= base;
        }
    }
    return result;
}

/**
 * @brief Create abacus from double (with fractional part)
 */
CrystallineAbacus* abacus_from_double(double value, uint32_t base, int32_t precision) {
    if (base < 2 || base > 256) {
        return NULL;
    }
    
    if (precision < 0) {
        precision = 0;
    }
    
    /* Create abacus */
    CrystallineAbacus* abacus = abacus_new(base);
    if (!abacus) {
        return NULL;
    }
    
    /* Handle sign */
    if (value < 0) {
        abacus->negative = true;
        value = -value;
    } else {
        abacus->negative = false;
    }
    
    /* Split into integer and fractional parts */
    double int_part_d = simple_floor(value);
    double frac_part = value - int_part_d;
    
    /* Convert integer part */
    uint64_t int_part = (uint64_t)int_part_d;
    
    /* Count digits needed for integer part */
    size_t int_digits = 0;
    uint64_t temp = int_part;
    if (temp == 0) {
        int_digits = 1;
    } else {
        while (temp > 0) {
            int_digits++;
            temp /= base;
        }
    }
    
    /* Total beads needed */
    size_t total_beads = int_digits + precision;
    
    if (abacus_ensure_capacity(abacus, total_beads) != MATH_SUCCESS) {
        abacus_free(abacus);
        return NULL;
    }
    
    /* Fill integer part (from least to most significant) */
    temp = int_part;
    for (size_t i = 0; i < int_digits; i++) {
        uint32_t digit = temp % base;
        abacus->beads[precision + i].value = digit;
        abacus->beads[precision + i].weight_exponent = i;
        
        if (map_digit_to_position(digit, base, &abacus->beads[precision + i].position) != MATH_SUCCESS) {
            abacus_free(abacus);
            return NULL;
        }
        
        temp /= base;
    }
    
    /* Fill fractional part */
    for (int32_t i = 0; i < precision; i++) {
        frac_part *= base;
        uint32_t digit = (uint32_t)simple_floor(frac_part);
        frac_part -= digit;
        
        abacus->beads[precision - 1 - i].value = digit;
        abacus->beads[precision - 1 - i].weight_exponent = -(i + 1);
        
        if (map_digit_to_position(digit, base, &abacus->beads[precision - 1 - i].position) != MATH_SUCCESS) {
            abacus_free(abacus);
            return NULL;
        }
    }
    
    abacus->num_beads = total_beads;
    abacus->min_exponent = -precision;
    
    return abacus;
}

/**
 * @brief Convert abacus to double
 */
MathError abacus_to_double(const CrystallineAbacus* abacus, double* value) {
    if (!abacus || !value) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    double result = 0.0;
    double base_d = (double)abacus->base;
    
    /* Sum all beads */
    for (size_t i = 0; i < abacus->num_beads; i++) {
        double weight = simple_pow(base_d, abacus->beads[i].weight_exponent);
        result += abacus->beads[i].value * weight;
    }
    
    /* Apply sign */
    if (abacus->negative) {
        result = -result;
    }
    
    *value = result;
    return MATH_SUCCESS;
}

/**
 * @brief Set precision (number of fractional digits)
 */
MathError abacus_set_precision(CrystallineAbacus* abacus, int32_t precision) {
    if (!abacus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (precision < 0) {
        precision = 0;
    }
    
    int32_t current_min = abacus->min_exponent;
    int32_t target_min = -precision;
    
    if (target_min == current_min) {
        return MATH_SUCCESS;  /* Already at desired precision */
    }
    
    if (target_min < current_min) {
        /* Increasing precision - pad with zeros */
        int32_t extra_beads = current_min - target_min;
        size_t new_size = abacus->num_beads + extra_beads;
        
        if (abacus_ensure_capacity(abacus, new_size) != MATH_SUCCESS) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        /* Shift existing beads */
        for (int i = (int)abacus->num_beads - 1; i >= 0; i--) {
            abacus->beads[i + extra_beads] = abacus->beads[i];
        }
        
        /* Fill new fractional beads with zeros */
        for (int32_t i = 0; i < extra_beads; i++) {
            abacus->beads[i].value = 0;
            abacus->beads[i].weight_exponent = target_min + i;
            map_digit_to_position(0, abacus->base, &abacus->beads[i].position);
        }
        
        abacus->num_beads = new_size;
        abacus->min_exponent = target_min;
    } else {
        /* Decreasing precision - truncate */
        int32_t beads_to_remove = target_min - current_min;
        
        if (beads_to_remove >= (int32_t)abacus->num_beads) {
            /* Removing all beads, set to zero */
            abacus_init_zero(abacus);
            abacus->min_exponent = target_min;
        } else {
            /* Shift beads left */
            for (size_t i = 0; i < abacus->num_beads - beads_to_remove; i++) {
                abacus->beads[i] = abacus->beads[i + beads_to_remove];
            }
            abacus->num_beads -= beads_to_remove;
            abacus->min_exponent = target_min;
        }
    }
    
    return abacus_normalize(abacus);
}

/**
 * @brief Get precision (number of fractional digits)
 */
int32_t abacus_get_precision(const CrystallineAbacus* abacus) {
    if (!abacus) {
        return 0;
    }
    
    return -abacus->min_exponent;
}

/**
 * @brief Round abacus to specified precision
 */
MathError abacus_round(CrystallineAbacus* result, const CrystallineAbacus* a, int32_t precision) {
    if (!result || !a) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Copy to result */
    CrystallineAbacus* temp = abacus_copy(a);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* If already at or above precision, just copy */
    if (-temp->min_exponent <= precision) {
        if (abacus_ensure_capacity(result, temp->num_beads) != MATH_SUCCESS) {
            abacus_free(temp);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        memcpy(result->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
        result->num_beads = temp->num_beads;
        result->negative = temp->negative;
        result->min_exponent = temp->min_exponent;
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
    /* Find the digit at position -(precision+1) for rounding */
    int32_t round_exp = -(precision + 1);
    bool round_up = false;
    
    for (size_t i = 0; i < temp->num_beads; i++) {
        if (temp->beads[i].weight_exponent == round_exp) {
            /* Round up if digit >= base/2 */
            if (temp->beads[i].value >= temp->base / 2) {
                round_up = true;
            }
            break;
        }
    }
    
    /* Truncate to precision */
    MathError err = abacus_set_precision(temp, precision);
    if (err != MATH_SUCCESS) {
        abacus_free(temp);
        return err;
    }
    
    /* Add 1 to least significant digit if rounding up */
    if (round_up && temp->num_beads > 0) {
        /* Find bead at exponent -precision */
        for (size_t i = 0; i < temp->num_beads; i++) {
            if (temp->beads[i].weight_exponent == -precision) {
                temp->beads[i].value++;
                if (temp->beads[i].value >= temp->base) {
                    /* Need to carry */
                    temp->beads[i].value = 0;
                    /* Propagate carry */
                    for (size_t j = i + 1; j < temp->num_beads; j++) {
                        temp->beads[j].value++;
                        if (temp->beads[j].value < temp->base) {
                            break;
                        }
                        temp->beads[j].value = 0;
                    }
                }
                break;
            }
        }
    }
    
    /* Copy to result */
    if (abacus_ensure_capacity(result, temp->num_beads) != MATH_SUCCESS) {
        abacus_free(temp);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    memcpy(result->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
    result->num_beads = temp->num_beads;
    result->negative = temp->negative;
    result->min_exponent = temp->min_exponent;
    
    abacus_free(temp);
    return abacus_normalize(result);
}

/**
 * @brief Truncate abacus to specified precision
 */
MathError abacus_truncate(CrystallineAbacus* result, const CrystallineAbacus* a, int32_t precision) {
    if (!result || !a) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Copy to result */
    CrystallineAbacus* temp = abacus_copy(a);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Set precision (this truncates) */
    MathError err = abacus_set_precision(temp, precision);
    if (err != MATH_SUCCESS) {
        abacus_free(temp);
        return err;
    }
    
    /* Copy to result */
    if (abacus_ensure_capacity(result, temp->num_beads) != MATH_SUCCESS) {
        abacus_free(temp);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    memcpy(result->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
    result->num_beads = temp->num_beads;
    result->negative = temp->negative;
    result->min_exponent = temp->min_exponent;
    
    abacus_free(temp);
    return MATH_SUCCESS;
}

/* ============================================================================
 * BASE CONVERSION
 * ============================================================================ */

uint32_t abacus_get_base(const CrystallineAbacus* abacus) {
    if (!abacus) return 0;
    return abacus->base;
}

MathError abacus_convert_base(CrystallineAbacus** result, const CrystallineAbacus* source, uint32_t new_base) {
    if (!result || !source || new_base < 2) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* If same base, just copy */
    if (source->base == new_base) {
        *result = abacus_copy(source);
        return *result ? MATH_SUCCESS : MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Convert to decimal (base 10) as intermediate */
    /* Calculate decimal value: sum of (digit * base^exponent) */
    double decimal_value = 0.0;
    
    for (size_t i = 0; i < source->num_beads; i++) {
        const AbacusBead* bead = &source->beads[i];
        /* Calculate base^exponent manually to avoid math.h */
        double weight = 1.0;
        int32_t exp = bead->weight_exponent;
        if (exp > 0) {
            for (int32_t j = 0; j < exp; j++) {
                weight *= source->base;
            }
        } else if (exp < 0) {
            for (int32_t j = 0; j < -exp; j++) {
                weight /= source->base;
            }
        }
        decimal_value += bead->value * weight;
    }
    
    /* Apply sign */
    if (source->negative) {
        decimal_value = -decimal_value;
    }
    
    /* Convert from decimal to new base */
    /* Separate integer and fractional parts manually */
    double int_part_d;
    double frac_part;
    
    if (decimal_value >= 0) {
        int_part_d = (double)((uint64_t)decimal_value);
        frac_part = decimal_value - int_part_d;
    } else {
        int_part_d = (double)((int64_t)decimal_value);
        frac_part = decimal_value - int_part_d;
    }
    
    bool is_negative = (decimal_value < 0.0);
    if (is_negative) {
        int_part_d = -int_part_d;
        frac_part = -frac_part;
    }
    
    /* Create result abacus */
    *result = abacus_new(new_base);
    if (!*result) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    (*result)->negative = is_negative;
    
    /* Convert integer part */
    uint64_t int_part = (uint64_t)int_part_d;
    if (int_part == 0) {
        /* Add a zero bead */
        MathError err = abacus_ensure_capacity(*result, 1);
        if (err != MATH_SUCCESS) {
            abacus_free(*result);
            *result = NULL;
            return err;
        }
        
        (*result)->beads[0].value = 0;
        (*result)->beads[0].weight_exponent = 0;
        (*result)->num_beads = 1;
    } else {
        /* Convert integer part to new base */
        int32_t exponent = 0;
        while (int_part > 0) {
            uint32_t digit = int_part % new_base;
            int_part /= new_base;
            
            MathError err = abacus_ensure_capacity(*result, (*result)->num_beads + 1);
            if (err != MATH_SUCCESS) {
                abacus_free(*result);
                *result = NULL;
                return err;
            }
            
            (*result)->beads[(*result)->num_beads].value = digit;
            (*result)->beads[(*result)->num_beads].weight_exponent = exponent;
            (*result)->num_beads++;
            exponent++;
        }
    }
    
    /* Convert fractional part (if any) */
    if (frac_part > 0.0001) {  /* Small threshold to avoid floating point errors */
        int32_t frac_exponent = -1;
        int max_frac_digits = 10;  /* Limit fractional precision */
        
        for (int i = 0; i < max_frac_digits && frac_part > 0.0001; i++) {
            frac_part *= new_base;
            uint32_t digit = (uint32_t)frac_part;
            frac_part -= digit;
            
            if (digit > 0) {
                MathError err = abacus_ensure_capacity(*result, (*result)->num_beads + 1);
                if (err != MATH_SUCCESS) {
                    abacus_free(*result);
                    *result = NULL;
                    return err;
                }
                
                (*result)->beads[(*result)->num_beads].value = digit;
                (*result)->beads[(*result)->num_beads].weight_exponent = frac_exponent;
                (*result)->num_beads++;
            }
            
            frac_exponent--;
        }
    }
    
    /* Update min_exponent */
    (*result)->min_exponent = 0;
    for (size_t i = 0; i < (*result)->num_beads; i++) {
        if ((*result)->beads[i].weight_exponent < (*result)->min_exponent) {
            (*result)->min_exponent = (*result)->beads[i].weight_exponent;
        }
    }
    
    return MATH_SUCCESS;
}

