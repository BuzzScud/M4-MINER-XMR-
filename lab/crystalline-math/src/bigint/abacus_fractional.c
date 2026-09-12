/**
 * @file abacus_fractional.c
 * @brief Pure Abacus fractional operations (no double intermediate)
 */

#include "math/abacus.h"
#include "math/abacus_fractional.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * FRACTIONAL DIVISION - Simple Long Division Algorithm
 * ============================================================================
 */

MathError abacus_div_fractional(CrystallineAbacus* result,
                                const CrystallineAbacus* a,
                                const CrystallineAbacus* b,
                                uint32_t precision) {
    if (!result || !a || !b) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (a->base != b->base || result->base != a->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (abacus_is_zero(b)) {
        return MATH_ERROR_DIVISION_BY_ZERO;
    }
    
    if (abacus_is_zero(a)) {
        abacus_init_zero(result);
        return MATH_SUCCESS;
    }
    
    uint32_t base = a->base;
    
    /* Step 1: Scale both operands to eliminate fractional parts if present
     * We need to scale by the maximum of the two min_exponents to ensure
     * both numbers become integers
     */
    int32_t scale_factor = 0;
    if (a->min_exponent < 0 || b->min_exponent < 0) {
        int32_t min_exp_a = (a->min_exponent < 0) ? a->min_exponent : 0;
        int32_t min_exp_b = (b->min_exponent < 0) ? b->min_exponent : 0;
        scale_factor = (min_exp_a < min_exp_b) ? -min_exp_a : -min_exp_b;
    }
    
    CrystallineAbacus* a_scaled = abacus_new(base);
    CrystallineAbacus* b_scaled = abacus_new(base);
    
    if (!a_scaled || !b_scaled) {
        abacus_free(a_scaled);
        abacus_free(b_scaled);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Copy a and shift exponents up by scale_factor */
    if (a->capacity > a_scaled->capacity) {
        free(a_scaled->beads);
        a_scaled->beads = (AbacusBead*)calloc(a->capacity, sizeof(AbacusBead));
        if (!a_scaled->beads) {
            abacus_free(a_scaled);
            abacus_free(b_scaled);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        a_scaled->capacity = a->capacity;
    }
    
    for (size_t i = 0; i < a->num_beads; i++) {
        a_scaled->beads[i].value = a->beads[i].value;
        a_scaled->beads[i].weight_exponent = a->beads[i].weight_exponent + scale_factor;
    }
    a_scaled->num_beads = a->num_beads;
    a_scaled->negative = a->negative;
    a_scaled->min_exponent = a->min_exponent + scale_factor;
    
    /* Copy b and scale it by the same factor */
    if (b->capacity > b_scaled->capacity) {
        free(b_scaled->beads);
        b_scaled->beads = (AbacusBead*)calloc(b->capacity, sizeof(AbacusBead));
        if (!b_scaled->beads) {
            abacus_free(a_scaled);
            abacus_free(b_scaled);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        b_scaled->capacity = b->capacity;
    }
    
    /* Scale b by the same factor to ensure it's also an integer */
    for (size_t i = 0; i < b->num_beads; i++) {
        b_scaled->beads[i].value = b->beads[i].value;
        b_scaled->beads[i].weight_exponent = b->beads[i].weight_exponent + scale_factor;
    }
    b_scaled->num_beads = b->num_beads;
    b_scaled->negative = b->negative;
    b_scaled->min_exponent = b->min_exponent + scale_factor;
    
    /* Step 2: Integer division on scaled values */
    CrystallineAbacus* q_int = abacus_new(base);
    CrystallineAbacus* remainder = abacus_new(base);
    
    if (!q_int || !remainder) {
        abacus_free(a_scaled);
        abacus_free(b_scaled);
        abacus_free(q_int);
        abacus_free(remainder);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    MathError err = abacus_div(q_int, remainder, a_scaled, b_scaled);
    if (err != MATH_SUCCESS) {
        abacus_free(a_scaled);
        abacus_free(b_scaled);
        abacus_free(q_int);
        abacus_free(remainder);
        return err;
    }
    
    /* Step 3: No adjustment needed!
     * When we scale both a and b by the same factor:
     *   (a × 10^k) / (b × 10^k) = a / b
     * The quotient is already in the correct scale.
     * 
     * Example: 6.25 / 2
     *   - Scale both by 100: 625 / 200 = 3 remainder 25
     *   - Quotient = 3 (correct integer part)
     *   - Fractional part computed in long division loop
     */
    /* No adjustment to q_int */
    
    /* Step 4: Allocate result with space for integer + fractional parts */
    size_t max_beads = q_int->num_beads + precision + 10;
    free(result->beads);
    result->beads = (AbacusBead*)calloc(max_beads, sizeof(AbacusBead));
    if (!result->beads) {
        abacus_free(a_scaled);
        abacus_free(b_scaled);
        abacus_free(q_int);
        abacus_free(remainder);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    result->capacity = max_beads;
    result->num_beads = 0;
    result->negative = (a->negative != b->negative);
    result->min_exponent = 0;
    
    /* Step 5: Copy integer and fractional parts from quotient */
    for (size_t i = 0; i < q_int->num_beads; i++) {
        result->beads[result->num_beads++] = q_int->beads[i];
        if (q_int->beads[i].weight_exponent < result->min_exponent) {
            result->min_exponent = q_int->beads[i].weight_exponent;
        }
    }
    
    /* Step 6: Compute additional fractional digits using long division */
    if (!abacus_is_zero(remainder) && precision > 0) {
        CrystallineAbacus* base_abacus = abacus_from_uint64(base, base);
        CrystallineAbacus* digit = abacus_new(base);
        CrystallineAbacus* product = abacus_new(base);
        CrystallineAbacus* scaled = abacus_new(base);
        CrystallineAbacus* new_remainder = abacus_new(base);
        
        if (!base_abacus || !digit || !product || !scaled || !new_remainder) {
            abacus_free(a_scaled);
            abacus_free(b_scaled);
            abacus_free(q_int);
            abacus_free(remainder);
            abacus_free(base_abacus);
            abacus_free(digit);
            abacus_free(product);
            abacus_free(scaled);
            abacus_free(new_remainder);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        /* Start from the position after existing fractional digits */
        int32_t start_exp = result->min_exponent - 1;
        
        for (uint32_t i = 0; i < precision; i++) {
            /* Scale remainder by base */
            err = abacus_mul(scaled, remainder, base_abacus);
            if (err != MATH_SUCCESS) break;
            
            /* Divide to get next digit
             * IMPORTANT: Use b_scaled, not b, because the remainder is from
             * dividing a_scaled by b_scaled
             */
            err = abacus_div(digit, new_remainder, scaled, b_scaled);
            if (err != MATH_SUCCESS) break;
            
            /* Store digit at negative exponent */
            if (!abacus_is_zero(digit) && digit->num_beads > 0) {
                result->beads[result->num_beads].value = digit->beads[0].value;
                result->beads[result->num_beads].weight_exponent = start_exp - (int32_t)i;
                result->num_beads++;
                
                if (start_exp - (int32_t)i < result->min_exponent) {
                    result->min_exponent = start_exp - (int32_t)i;
                }
            }
            
            /* Update remainder for next iteration */
            if (remainder->capacity < new_remainder->num_beads) {
                free(remainder->beads);
                remainder->beads = (AbacusBead*)calloc(new_remainder->num_beads, sizeof(AbacusBead));
                if (!remainder->beads) {
                    abacus_free(a_scaled);
                    abacus_free(b_scaled);
                    abacus_free(q_int);
                    abacus_free(remainder);
                    abacus_free(base_abacus);
                    abacus_free(digit);
                    abacus_free(product);
                    abacus_free(scaled);
                    abacus_free(new_remainder);
                    return MATH_ERROR_OUT_OF_MEMORY;
                }
                remainder->capacity = new_remainder->num_beads;
            }
            
            memcpy(remainder->beads, new_remainder->beads, new_remainder->num_beads * sizeof(AbacusBead));
            remainder->num_beads = new_remainder->num_beads;
            remainder->min_exponent = new_remainder->min_exponent;
            
            /* If remainder is zero, we're done */
            if (abacus_is_zero(remainder)) {
                break;
            }
        }
        
        abacus_free(base_abacus);
        abacus_free(digit);
        abacus_free(product);
        abacus_free(scaled);
        abacus_free(new_remainder);
    }
    
    abacus_free(a_scaled);
    abacus_free(b_scaled);
    abacus_free(q_int);
    abacus_free(remainder);
    
    abacus_normalize(result);
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * PURE ABACUS SQUARE ROOT (Newton-Raphson)
 * ============================================================================
 */

MathError abacus_sqrt_pure(CrystallineAbacus* result,
                           const CrystallineAbacus* a,
                           uint32_t precision) {
    if (!result || !a) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (result->base != a->base) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (a->negative) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (abacus_is_zero(a)) {
        abacus_init_zero(result);
        return MATH_SUCCESS;
    }
    
    uint32_t base = a->base;
    
    /* Initial guess */
    uint64_t a_uint;
    MathError err = abacus_to_uint64(a, &a_uint);
    
    uint64_t guess_uint = 1;
    if (err == MATH_SUCCESS && a_uint > 0) {
        guess_uint = a_uint / 2;  /* Better initial guess */
        if (guess_uint == 0) guess_uint = 1;
    }
    
    CrystallineAbacus* x = abacus_from_uint64(guess_uint, base);
    CrystallineAbacus* x_prev = abacus_new(base);
    CrystallineAbacus* two = abacus_from_uint64(2, base);
    CrystallineAbacus* quotient = abacus_new(base);
    CrystallineAbacus* sum = abacus_new(base);
    CrystallineAbacus* x_next = abacus_new(base);
    CrystallineAbacus* diff = abacus_new(base);
    
    if (!x || !x_prev || !two || !quotient || !sum || !x_next || !diff) {
        abacus_free(x);
        abacus_free(x_prev);
        abacus_free(two);
        abacus_free(quotient);
        abacus_free(sum);
        abacus_free(x_next);
        abacus_free(diff);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Newton-Raphson iterations with convergence check
     * Increase max iterations from 10 to 30 for better precision
     * Add early stopping when change is negligible
     */
    uint32_t max_iterations = (precision > 30) ? precision : 30;
    
    for (uint32_t i = 0; i < max_iterations; i++) {
        /* Safety check: x should never be zero */
        if (abacus_is_zero(x)) {
            abacus_free(x);
            abacus_free(x_prev);
            abacus_free(two);
            abacus_free(quotient);
            abacus_free(sum);
            abacus_free(x_next);
            abacus_free(diff);
            return MATH_ERROR_DIVISION_BY_ZERO;
        }
        
        /* quotient = a / x 
         * Use precision of 10 to avoid memory issues with many fractional digits
         */
        err = abacus_div_fractional(quotient, a, x, 10);
        if (err != MATH_SUCCESS) {
            abacus_free(x);
            abacus_free(x_prev);
            abacus_free(two);
            abacus_free(quotient);
            abacus_free(sum);
            abacus_free(x_next);
            abacus_free(diff);
            return err;
        }
        
        /* sum = x + quotient */
        err = abacus_add(sum, x, quotient);
        if (err != MATH_SUCCESS) {
            abacus_free(x);
            abacus_free(x_prev);
            abacus_free(two);
            abacus_free(quotient);
            abacus_free(sum);
            abacus_free(x_next);
            abacus_free(diff);
            return err;
        }
        
        /* Safety check: sum should never be zero */
        if (abacus_is_zero(sum)) {
            abacus_free(x);
            abacus_free(x_prev);
            abacus_free(two);
            abacus_free(quotient);
            abacus_free(sum);
            abacus_free(x_next);
            abacus_free(diff);
            return MATH_ERROR_DIVISION_BY_ZERO;
        }
        
        /* x_next = sum / 2
         * Use precision of 10 to avoid memory issues with many fractional digits
         */
        err = abacus_div_fractional(x_next, sum, two, 10);
        if (err != MATH_SUCCESS) {
            abacus_free(x);
            abacus_free(x_prev);
            abacus_free(two);
            abacus_free(quotient);
            abacus_free(sum);
            abacus_free(x_next);
            abacus_free(diff);
            return err;
        }
        
        /* TODO: Convergence check disabled due to issues with fractional division
         * For now, we just run the full number of iterations
         * This is slower but more reliable
         */
        
        /* Save current x as x_prev for next iteration */
        if (x_prev->capacity < x->num_beads) {
            free(x_prev->beads);
            x_prev->beads = (AbacusBead*)calloc(x->num_beads, sizeof(AbacusBead));
            if (!x_prev->beads) {
                abacus_free(x);
                abacus_free(x_prev);
                abacus_free(two);
                abacus_free(quotient);
                abacus_free(sum);
                abacus_free(x_next);
                abacus_free(diff);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            x_prev->capacity = x->num_beads;
        }
        memcpy(x_prev->beads, x->beads, x->num_beads * sizeof(AbacusBead));
        x_prev->num_beads = x->num_beads;
        x_prev->min_exponent = x->min_exponent;
        x_prev->negative = x->negative;
        
        /* Update x with x_next */
        if (x->capacity < x_next->num_beads) {
            free(x->beads);
            x->beads = (AbacusBead*)calloc(x_next->num_beads, sizeof(AbacusBead));
            if (!x->beads) {
                abacus_free(x);
                abacus_free(x_prev);
                abacus_free(two);
                abacus_free(quotient);
                abacus_free(sum);
                abacus_free(x_next);
                abacus_free(diff);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            x->capacity = x_next->num_beads;
        }
        
        memcpy(x->beads, x_next->beads, x_next->num_beads * sizeof(AbacusBead));
        x->num_beads = x_next->num_beads;
        x->min_exponent = x_next->min_exponent;
        x->negative = x_next->negative;
    }
    
    /* Copy final result */
    if (result->capacity < x->num_beads) {
        free(result->beads);
        result->beads = (AbacusBead*)calloc(x->num_beads, sizeof(AbacusBead));
        if (!result->beads) {
            abacus_free(x);
            abacus_free(x_prev);
            abacus_free(two);
            abacus_free(quotient);
            abacus_free(sum);
            abacus_free(x_next);
            abacus_free(diff);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        result->capacity = x->num_beads;
    }
    
    memcpy(result->beads, x->beads, x->num_beads * sizeof(AbacusBead));
    result->num_beads = x->num_beads;
    result->negative = false;
    result->min_exponent = x->min_exponent;
    
    abacus_free(x);
    abacus_free(x_prev);
    abacus_free(two);
    abacus_free(quotient);
    abacus_free(sum);
    abacus_free(x_next);
    abacus_free(diff);
    
    abacus_normalize(result);
    
    return MATH_SUCCESS;
}