/**
 * @file abacus_gcd.c
 * @brief GCD, LCM, and coprimality operations for CrystallineAbacus
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * Implements arbitrary precision GCD, LCM, and coprimality testing
 * using pure CrystallineAbacus operations (no uint64 shortcuts).
 */

#include "math/abacus.h"
#include "math/types.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================================
 * GCD, LCM, AND COPRIMALITY OPERATIONS
 * ============================================================================
 */

/**
 * @brief Compute GCD of two arbitrary precision numbers using Euclidean algorithm
 * 
 * Uses the classical Euclidean algorithm:
 *   gcd(a, b) = gcd(b, a mod b)
 *   gcd(a, 0) = a
 * 
 * This is a pure geometric operation on the CrystallineAbacus.
 * 
 * @param result Output: GCD of a and b
 * @param a First number
 * @param b Second number
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_gcd(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!result || !a || !b) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    if (a->base != b->base) {
        return MATH_ERROR_INVALID_BASE;
    }
    
    /* Create working copies */
    CrystallineAbacus *temp_a = abacus_copy(a);
    if (!temp_a) return MATH_ERROR_OUT_OF_MEMORY;
    
    CrystallineAbacus *temp_b = abacus_copy(b);
    if (!temp_b) {
        abacus_free(temp_a);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* GCD is always positive */
    temp_a->negative = false;
    temp_b->negative = false;
    
    /* Euclidean algorithm: gcd(a, b) = gcd(b, a mod b) */
    while (!abacus_is_zero(temp_b)) {
        /* remainder = temp_a mod temp_b */
        CrystallineAbacus *quotient = abacus_new(a->base);
        CrystallineAbacus *remainder = abacus_new(a->base);
        
        if (!quotient || !remainder) {
            abacus_free(quotient);
            abacus_free(remainder);
            abacus_free(temp_a);
            abacus_free(temp_b);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        MathError err = abacus_div(quotient, remainder, temp_a, temp_b);
        abacus_free(quotient);
        
        if (err != MATH_SUCCESS) {
            abacus_free(remainder);
            abacus_free(temp_a);
            abacus_free(temp_b);
            return err;
        }
        
        /* temp_a = temp_b */
        abacus_free(temp_a);
        temp_a = temp_b;
        
        /* temp_b = remainder */
        temp_b = remainder;
    }
    
    /* Copy result from temp_a */
    CrystallineAbacus *result_copy = abacus_copy(temp_a);
    if (!result_copy) {
        abacus_free(temp_a);
        abacus_free(temp_b);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Free old result beads and copy new ones */
    if (result->beads) {
        free(result->beads);
    }
    result->beads = result_copy->beads;
    result->num_beads = result_copy->num_beads;
    result->capacity = result_copy->capacity;
    result->base = result_copy->base;
    result->negative = false;
    result->min_exponent = result_copy->min_exponent;
    
    /* Free the wrapper but not the beads (we transferred ownership) */
    free(result_copy);
    
    abacus_free(temp_a);
    abacus_free(temp_b);
    
    return MATH_SUCCESS;
}

/**
 * @brief Compute LCM of two arbitrary precision numbers
 * 
 * Uses the formula: lcm(a, b) = (a * b) / gcd(a, b)
 * 
 * @param result Output: LCM of a and b
 * @param a First number
 * @param b Second number
 * @return MATH_SUCCESS on success, error code on failure
 */
MathError abacus_lcm(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!result || !a || !b) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    if (a->base != b->base) {
        return MATH_ERROR_INVALID_BASE;
    }
    
    /* Handle zero cases: lcm(0, x) = 0 */
    if (abacus_is_zero(a) || abacus_is_zero(b)) {
        result->num_beads = 0;
        result->negative = false;
        return MATH_SUCCESS;
    }
    
    CrystallineAbacus *gcd_val = abacus_new(a->base);
    CrystallineAbacus *product = abacus_new(a->base);
    CrystallineAbacus *remainder = abacus_new(a->base);
    
    if (!gcd_val || !product || !remainder) {
        abacus_free(gcd_val);
        abacus_free(product);
        abacus_free(remainder);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Compute gcd(a, b) */
    MathError err = abacus_gcd(gcd_val, a, b);
    if (err != MATH_SUCCESS) {
        abacus_free(gcd_val);
        abacus_free(product);
        abacus_free(remainder);
        return err;
    }
    
    /* Compute a * b */
    err = abacus_mul(product, a, b);
    if (err != MATH_SUCCESS) {
        abacus_free(gcd_val);
        abacus_free(product);
        abacus_free(remainder);
        return err;
    }
    
    /* Compute (a * b) / gcd(a, b) */
    err = abacus_div(result, remainder, product, gcd_val);
    if (err != MATH_SUCCESS) {
        abacus_free(gcd_val);
        abacus_free(product);
        abacus_free(remainder);
        return err;
    }
    
    /* LCM is always positive */
    result->negative = false;
    
    abacus_free(gcd_val);
    abacus_free(product);
    abacus_free(remainder);
    
    return MATH_SUCCESS;
}

/**
 * @brief Test if two arbitrary precision numbers are coprime
 * 
 * Two numbers are coprime if gcd(a, b) = 1
 * 
 * @param a First number
 * @param b Second number
 * @return true if coprime, false otherwise
 */
bool abacus_coprime(const CrystallineAbacus* a, const CrystallineAbacus* b) {
    if (!a || !b) {
        return false;
    }
    
    if (a->base != b->base) {
        return false;
    }
    
    CrystallineAbacus *gcd_val = abacus_new(a->base);
    CrystallineAbacus *one = abacus_from_uint64(1, a->base);
    
    if (!gcd_val || !one) {
        abacus_free(gcd_val);
        abacus_free(one);
        return false;
    }
    
    /* Compute gcd(a, b) */
    MathError err = abacus_gcd(gcd_val, a, b);
    if (err != MATH_SUCCESS) {
        abacus_free(gcd_val);
        abacus_free(one);
        return false;
    }
    
    /* Check if gcd == 1 */
    int cmp = abacus_compare(gcd_val, one);
    
    abacus_free(gcd_val);
    abacus_free(one);
    
    return (cmp == 0);
}
/**
 * @brief Integer square root using Newton-Raphson method
 * 
 * Finds the largest integer x such that x² ≤ n
 * 
 * Mathematical Foundation:
 * ========================
 * Newton-Raphson iteration for finding √n:
 *   x_{k+1} = (x_k + n/x_k) / 2
 * 
 * This formula comes from Newton's method applied to f(x) = x² - n:
 *   x_{k+1} = x_k - f(x_k)/f'(x_k)
 *           = x_k - (x_k² - n)/(2x_k)
 *           = (2x_k² - x_k² + n)/(2x_k)
 *           = (x_k² + n)/(2x_k)
 *           = (x_k + n/x_k) / 2
 * 
 * Convergence:
 * - Quadratic convergence: doubles the number of correct digits each iteration
 * - Typically converges in O(log log n) iterations
 * - Each iteration is O(1) with geometric arithmetic
 * 
 * Geometric Interpretation:
 * ========================
 * Finding the side length of a square with area n
 * 
 * In Babylonian mathematics:
 * - The number n is a point on the clock
 * - √n is the number that, when multiplied by itself, gives n
 * - Geometrically: finding the edge of a square with area n
 * 
 * Initial Guess:
 * ==============
 * We use bit_length to get a good initial guess:
 *   x_0 = 2^(⌈log₂(n)/2⌉)
 * 
 * This ensures we start close to the actual root, minimizing iterations.
 * 
 * @param result Output: floor(√n)
 * @param n Input number (must be non-negative)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_sqrt(CrystallineAbacus* result, const CrystallineAbacus* n) {
    if (!result || !n) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (result->base != n->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Check for negative input */
    if (n->negative) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Handle special cases */
    if (abacus_is_zero(n)) {
        abacus_init_zero(result);
        return MATH_SUCCESS;
    }
    
    /* Check if n == 1 */
    CrystallineAbacus* one = abacus_from_uint64(1, n->base);
    if (!one) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    if (abacus_compare(n, one) == 0) {
        CrystallineAbacus* temp = abacus_copy(one);
        if (!temp) {
            abacus_free(one);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        /* Copy to result using abacus_copy internals */
        result->num_beads = temp->num_beads;
        result->negative = false;
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(temp);
        abacus_free(one);
        return MATH_SUCCESS;
    }
    
    /* For small numbers, use direct calculation */
    uint64_t n_val;
    if (abacus_to_uint64(n, &n_val) == MATH_SUCCESS) {
        /* Handle zero case explicitly */
        if (n_val == 0) {
            abacus_init_zero(result);
            abacus_free(one);
            return MATH_SUCCESS;
        }
        
        /* Fast path: compute sqrt directly */
        uint64_t x = 1ULL << ((64 - __builtin_clzll(n_val) + 1) / 2);
        
        /* Safety check: ensure x is never zero */
        if (x == 0) x = 1;
        
        /* Newton-Raphson iteration */
        while (true) {
            uint64_t x_new = (x + n_val / x) / 2;
            if (x_new >= x) {
                break;
            }
            x = x_new;
        }
        
        /* Convert result */
        CrystallineAbacus* temp = abacus_from_uint64(x, n->base);
        if (!temp) {
            abacus_free(one);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        /* Copy to result */
        result->num_beads = temp->num_beads;
        result->negative = false;
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(one);
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
    abacus_free(one);
    return MATH_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief Integer nth root using Newton-Raphson method
 * 
 * Finds the largest integer x such that x^root ≤ n
 * 
 * Mathematical Foundation:
 * ========================
 * Newton-Raphson iteration for finding root√n:
 *   x_{k+1} = ((root-1)*x_k + n/x_k^(root-1)) / root
 * 
 * This formula comes from Newton's method applied to f(x) = x^root - n:
 *   x_{k+1} = x_k - f(x_k)/f'(x_k)
 *           = x_k - (x_k^root - n)/(root * x_k^(root-1))
 *           = (root*x_k^root - x_k^root + n)/(root * x_k^(root-1))
 *           = ((root-1)*x_k^root + n)/(root * x_k^(root-1))
 *           = ((root-1)*x_k + n/x_k^(root-1)) / root
 * 
 * Convergence:
 * - Quadratic convergence near the root
 * - Typically converges in O(log log n) iterations
 * - Each iteration requires one exponentiation and one division
 * 
 * Geometric Interpretation:
 * ========================
 * Finding the edge length of an n-dimensional hypercube with volume n
 * 
 * In Babylonian mathematics:
 * - The number n is a point on the clock
 * - root√n is the number that, when raised to power root, gives n
 * - Geometrically: finding the edge of a hypercube with volume n
 * 
 * Examples:
 * - root=2: Square root (edge of square with area n)
 * - root=3: Cube root (edge of cube with volume n)
 * - root=4: 4th root (edge of tesseract with hypervolume n)
 * 
 * Initial Guess:
 * ==============
 * We use bit_length to get a good initial guess:
 *   x_0 = 2^(⌈log₂(n)/root⌉)
 * 
 * This ensures we start close to the actual root, minimizing iterations.
 * 
 * @param result Output: floor(root√n)
 * @param n Input number
 * @param root The root to extract (2 for square root, 3 for cube root, etc.)
 * @return MATH_SUCCESS or error code
 */
MathError abacus_root(CrystallineAbacus* result, const CrystallineAbacus* n, uint32_t root) {
    if (!result || !n) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (result->base != n->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Check for invalid root */
    if (root == 0) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Special case: root = 1 */
    if (root == 1) {
        /* 1st root is just the number itself */
        CrystallineAbacus* temp = abacus_copy(n);
        if (!temp) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        result->num_beads = temp->num_beads;
        result->negative = temp->negative;
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
    /* Special case: root = 2 (square root) */
    if (root == 2) {
        return abacus_sqrt(result, n);
    }
    
    /* Check for negative input with even root */
    if (n->negative && (root % 2 == 0)) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Handle special cases */
    if (abacus_is_zero(n)) {
        abacus_init_zero(result);
        return MATH_SUCCESS;
    }
    
    /* Check if n == 1 */
    CrystallineAbacus* one = abacus_from_uint64(1, n->base);
    if (!one) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    if (abacus_compare(n, one) == 0) {
        result->num_beads = one->num_beads;
        result->negative = false;
        for (size_t i = 0; i < one->num_beads; i++) {
            result->beads[i] = one->beads[i];
        }
        abacus_free(one);
        return MATH_SUCCESS;
    }
    
    /* For small numbers, use direct calculation */
    uint64_t n_val;
    if (abacus_to_uint64(n, &n_val) == MATH_SUCCESS) {
        /*
         * Fast path: compute nth root directly using Newton-Raphson
         * 
         * Algorithm:
         * 1. Initial guess: x = 2^(⌈log₂(n)/root⌉)
         * 2. Iterate: x_new = ((root-1)*x + n/x^(root-1)) / root
         * 3. Stop when x_new >= x (converged)
         */
        
        /* Initial guess using bit length */
        int bit_length = 64 - __builtin_clzll(n_val);
        uint64_t x = 1ULL << ((bit_length + root - 1) / root);
        
        /* Newton-Raphson iteration */
        while (true) {
            /* Compute x^(root-1) */
            uint64_t x_power = 1;
            for (uint32_t i = 0; i < root - 1; i++) {
                x_power *= x;
            }
            
            /* Compute x_new = ((root-1)*x + n/x^(root-1)) / root */
            uint64_t x_new = ((root - 1) * x + n_val / x_power) / root;
            
            /* Check convergence */
            if (x_new >= x) {
                break;
            }
            
            x = x_new;
        }
        
        /* Convert result */
        CrystallineAbacus* temp = abacus_from_uint64(x, n->base);
        if (!temp) {
            abacus_free(one);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        /* Copy to result */
        result->num_beads = temp->num_beads;
        result->negative = n->negative;  /* Preserve sign for odd roots */
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(one);
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
    /*
     * Slow path: For arbitrary precision numbers that don't fit in uint64_t
     * 
     * This would require implementing arbitrary precision exponentiation
     * and division, which is complex. For now, return not implemented.
     * 
     * Future work: Implement using CrystallineAbacus operations directly
     */
    
    abacus_free(one);
    return MATH_ERROR_NOT_IMPLEMENTED;
}

/* ============================================================================
 * EXPONENTIATION
 * ============================================================================ */

/**
 * @brief Compute base^exponent using binary exponentiation
 */
MathError abacus_pow(CrystallineAbacus* result,
                     const CrystallineAbacus* base,
                     const CrystallineAbacus* exponent) {
    if (!result || !base || !exponent) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (result->base != base->base || result->base != exponent->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Check for negative exponent */
    if (exponent->negative) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Try to convert exponent to uint64_t for fast path */
    uint64_t exp_val;
    if (abacus_to_uint64(exponent, &exp_val) == MATH_SUCCESS) {
        return abacus_pow_uint64(result, base, exp_val);
    }
    
    return MATH_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief Fast path for exponentiation with uint64_t exponent
 */
MathError abacus_pow_uint64(CrystallineAbacus* result,
                            const CrystallineAbacus* base,
                            uint64_t exponent) {
    if (!result || !base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (result->base != base->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Special case: exponent = 0, result = 1 */
    if (exponent == 0) {
        CrystallineAbacus* one = abacus_from_uint64(1, base->base);
        if (!one) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        result->num_beads = one->num_beads;
        result->negative = false;
        result->min_exponent = one->min_exponent;
        for (size_t i = 0; i < one->num_beads; i++) {
            result->beads[i] = one->beads[i];
        }
        
        abacus_free(one);
        return MATH_SUCCESS;
    }
    
    /* Special case: exponent = 1, result = base */
    if (exponent == 1) {
        result->num_beads = base->num_beads;
        result->negative = base->negative;
        result->min_exponent = base->min_exponent;
        for (size_t i = 0; i < base->num_beads; i++) {
            result->beads[i] = base->beads[i];
        }
        return MATH_SUCCESS;
    }
    
    /* Binary exponentiation */
    CrystallineAbacus* power = abacus_copy(base);
    CrystallineAbacus* temp_result = abacus_from_uint64(1, base->base);
    CrystallineAbacus* temp_mul = abacus_new(base->base);
    
    if (!power || !temp_result || !temp_mul) {
        if (power) abacus_free(power);
        if (temp_result) abacus_free(temp_result);
        if (temp_mul) abacus_free(temp_mul);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    uint64_t exp = exponent;
    
    while (exp > 0) {
        if (exp & 1) {
            MathError err = abacus_mul(temp_mul, temp_result, power);
            if (err != MATH_SUCCESS) {
                abacus_free(power);
                abacus_free(temp_result);
                abacus_free(temp_mul);
                return err;
            }
            
            temp_result->num_beads = temp_mul->num_beads;
            temp_result->negative = temp_mul->negative;
            temp_result->min_exponent = temp_mul->min_exponent;
            for (size_t i = 0; i < temp_mul->num_beads; i++) {
                temp_result->beads[i] = temp_mul->beads[i];
            }
        }
        
        exp >>= 1;
        if (exp > 0) {
            MathError err = abacus_mul(temp_mul, power, power);
            if (err != MATH_SUCCESS) {
                abacus_free(power);
                abacus_free(temp_result);
                abacus_free(temp_mul);
                return err;
            }
            
            power->num_beads = temp_mul->num_beads;
            power->negative = temp_mul->negative;
            power->min_exponent = temp_mul->min_exponent;
            for (size_t i = 0; i < temp_mul->num_beads; i++) {
                power->beads[i] = temp_mul->beads[i];
            }
        }
    }
    
    result->num_beads = temp_result->num_beads;
    result->negative = temp_result->negative;
    result->min_exponent = temp_result->min_exponent;
    for (size_t i = 0; i < temp_result->num_beads; i++) {
        result->beads[i] = temp_result->beads[i];
    }
    
    abacus_free(power);
    abacus_free(temp_result);
    abacus_free(temp_mul);
    
    return MATH_SUCCESS;
}

/**
 * @brief Compute (base^exponent) mod modulus
 */
MathError abacus_powmod(CrystallineAbacus* result,
                       const CrystallineAbacus* base,
                       const CrystallineAbacus* exponent,
                       const CrystallineAbacus* modulus) {
    if (!result || !base || !exponent || !modulus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (result->base != base->base || result->base != exponent->base || 
        result->base != modulus->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (exponent->negative) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (abacus_is_zero(modulus) || modulus->negative) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Fast path: all values fit in uint64_t */
    uint64_t base_val, exp_val, mod_val;
    MathError err_base = abacus_to_uint64(base, &base_val);
    MathError err_exp = abacus_to_uint64(exponent, &exp_val);
    MathError err_mod = abacus_to_uint64(modulus, &mod_val);
    
    if (err_base == MATH_SUCCESS && err_exp == MATH_SUCCESS && 
        err_mod == MATH_SUCCESS) {
        /* Reduce base modulo modulus first */
        base_val %= mod_val;
        
        uint64_t result_val = 1;
        uint64_t power = base_val;
        uint64_t exp = exp_val;
        
        /* Binary exponentiation with modulo at each step */
        while (exp > 0) {
            if (exp & 1) {
                result_val = (result_val * power) % mod_val;
            }
            power = (power * power) % mod_val;
            exp >>= 1;
        }
        
        CrystallineAbacus* temp = abacus_from_uint64(result_val, base->base);
        if (!temp) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        result->num_beads = temp->num_beads;
        result->negative = false;
        result->min_exponent = temp->min_exponent;
        for (size_t i = 0; i < temp->num_beads; i++) {
            result->beads[i] = temp->beads[i];
        }
        
        abacus_free(temp);
        return MATH_SUCCESS;
    }
    
    /* Slow path: arbitrary precision not yet implemented */
    return MATH_ERROR_NOT_IMPLEMENTED;
}
