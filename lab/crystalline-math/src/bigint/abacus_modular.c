/**
 * @file abacus_modular.c
 * @brief Modular arithmetic operations for Crystalline Abacus
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * Implements modular arithmetic on the clock lattice using geometric operations.
 * This is the pure crystalline approach - no BigInt arrays, just geometric transformations.
 */

#include "math/abacus.h"
#include "math/arithmetic.h"
#include <stdlib.h>

/* ============================================================================
 * MODULAR ARITHMETIC OPERATIONS
 * ============================================================================ */

MathError abacus_mod(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* modulus) {
    if (!result || !a || !modulus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Use division to compute a mod m = a - floor(a/m) * m */
    CrystallineAbacus* quotient = abacus_new(a->base);
    CrystallineAbacus* remainder = abacus_new(a->base);
    
    if (!quotient || !remainder) {
        if (quotient) abacus_free(quotient);
        if (remainder) abacus_free(remainder);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    MathError err = abacus_div(quotient, remainder, a, modulus);
    if (err != MATH_SUCCESS) {
        abacus_free(quotient);
        abacus_free(remainder);
        return err;
    }
    
    /* Copy remainder to result */
    CrystallineAbacus* temp = abacus_copy(remainder);
    if (!temp) {
        abacus_free(quotient);
        abacus_free(remainder);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* Clear result and copy temp into it */
    result->num_beads = temp->num_beads;
    result->negative = temp->negative;
    for (size_t i = 0; i < temp->num_beads && i < result->capacity; i++) {
        result->beads[i] = temp->beads[i];
    }
    
    abacus_free(quotient);
    abacus_free(remainder);
    abacus_free(temp);
    
    return MATH_SUCCESS;
}

MathError abacus_mod_add(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b, const CrystallineAbacus* modulus) {
    if (!result || !a || !b || !modulus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Compute (a + b) mod m */
    CrystallineAbacus* temp = abacus_new(a->base);
    if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
    
    MathError err = abacus_add(temp, a, b);
    if (err != MATH_SUCCESS) {
        abacus_free(temp);
        return err;
    }
    
    err = abacus_mod(result, temp, modulus);
    abacus_free(temp);
    
    return err;
}

MathError abacus_mod_sub(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b, const CrystallineAbacus* modulus) {
    if (!result || !a || !b || !modulus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Compute (a - b) mod m */
    CrystallineAbacus* temp = abacus_new(a->base);
    if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
    
    MathError err = abacus_sub(temp, a, b);
    if (err != MATH_SUCCESS) {
        abacus_free(temp);
        return err;
    }
    
    /* Handle negative results: if temp < 0, add modulus */
    if (temp->negative) {
        CrystallineAbacus* adjusted = abacus_new(a->base);
        if (!adjusted) {
            abacus_free(temp);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_add(adjusted, temp, modulus);
        if (err != MATH_SUCCESS) {
            abacus_free(temp);
            abacus_free(adjusted);
            return err;
        }
        
        abacus_free(temp);
        temp = adjusted;
    }
    
    err = abacus_mod(result, temp, modulus);
    abacus_free(temp);
    
    return err;
}

MathError abacus_mod_mul(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* b, const CrystallineAbacus* modulus) {
    if (!result || !a || !b || !modulus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Compute (a * b) mod m */
    CrystallineAbacus* temp = abacus_new(a->base);
    if (!temp) return MATH_ERROR_OUT_OF_MEMORY;
    
    MathError err = abacus_mul(temp, a, b);
    if (err != MATH_SUCCESS) {
        abacus_free(temp);
        return err;
    }
    
    err = abacus_mod(result, temp, modulus);
    abacus_free(temp);
    
    return err;
}

MathError abacus_mod_exp(CrystallineAbacus* result, const CrystallineAbacus* base, const CrystallineAbacus* exponent, const CrystallineAbacus* modulus) {
    if (!result || !base || !exponent || !modulus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Binary exponentiation: compute base^exponent mod modulus */
    CrystallineAbacus* res = abacus_from_uint64(1, base->base);
    CrystallineAbacus* b = abacus_new(base->base);
    CrystallineAbacus* exp = abacus_copy(exponent);
    CrystallineAbacus* two = abacus_from_uint64(2, base->base);
    
    if (!res || !b || !exp || !two) {
        if (res) abacus_free(res);
        if (b) abacus_free(b);
        if (exp) abacus_free(exp);
        if (two) abacus_free(two);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    /* b = base mod modulus */
    MathError err = abacus_mod(b, base, modulus);
    if (err != MATH_SUCCESS) {
        abacus_free(res);
        abacus_free(b);
        abacus_free(exp);
        abacus_free(two);
        return err;
    }
    
    /* Binary exponentiation loop */
    while (!abacus_is_zero(exp)) {
        /* If exp is odd, multiply result by b */
        CrystallineAbacus* exp_mod_2 = abacus_new(base->base);
        if (!exp_mod_2) {
            abacus_free(res);
            abacus_free(b);
            abacus_free(exp);
            abacus_free(two);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_mod(exp_mod_2, exp, two);
        if (err != MATH_SUCCESS) {
            abacus_free(res);
            abacus_free(b);
            abacus_free(exp);
            abacus_free(two);
            abacus_free(exp_mod_2);
            return err;
        }
        
        if (!abacus_is_zero(exp_mod_2)) {
            CrystallineAbacus* temp = abacus_new(base->base);
            if (!temp) {
                abacus_free(res);
                abacus_free(b);
                abacus_free(exp);
                abacus_free(two);
                abacus_free(exp_mod_2);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            err = abacus_mod_mul(temp, res, b, modulus);
            if (err != MATH_SUCCESS) {
                abacus_free(res);
                abacus_free(b);
                abacus_free(exp);
                abacus_free(two);
                abacus_free(exp_mod_2);
                abacus_free(temp);
                return err;
            }
            
            abacus_free(res);
            res = temp;
        }
        abacus_free(exp_mod_2);
        
        /* Square b and halve exp */
        CrystallineAbacus* b_squared = abacus_new(base->base);
        if (!b_squared) {
            abacus_free(res);
            abacus_free(b);
            abacus_free(exp);
            abacus_free(two);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_mod_mul(b_squared, b, b, modulus);
        if (err != MATH_SUCCESS) {
            abacus_free(res);
            abacus_free(b);
            abacus_free(exp);
            abacus_free(two);
            abacus_free(b_squared);
            return err;
        }
        
        abacus_free(b);
        b = b_squared;
        
        /* exp = exp / 2 */
        CrystallineAbacus* exp_halved = abacus_new(base->base);
        CrystallineAbacus* remainder = abacus_new(base->base);
        if (!exp_halved || !remainder) {
            abacus_free(res);
            abacus_free(b);
            abacus_free(exp);
            abacus_free(two);
            if (exp_halved) abacus_free(exp_halved);
            if (remainder) abacus_free(remainder);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_div(exp_halved, remainder, exp, two);
        if (err != MATH_SUCCESS) {
            abacus_free(res);
            abacus_free(b);
            abacus_free(exp);
            abacus_free(two);
            abacus_free(exp_halved);
            abacus_free(remainder);
            return err;
        }
        
        abacus_free(exp);
        abacus_free(remainder);
        exp = exp_halved;
    }
    
    /* Copy result */
    result->num_beads = res->num_beads;
    result->negative = res->negative;
    for (size_t i = 0; i < res->num_beads && i < result->capacity; i++) {
        result->beads[i] = res->beads[i];
    }
    
    abacus_free(res);
    abacus_free(b);
    abacus_free(exp);
    abacus_free(two);
    
    return MATH_SUCCESS;
}

MathError abacus_mod_inverse(CrystallineAbacus* result, const CrystallineAbacus* a, const CrystallineAbacus* modulus) {
    if (!result || !a || !modulus) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Extended Euclidean algorithm to find a^-1 mod m */
    CrystallineAbacus* old_r = abacus_copy(modulus);
    CrystallineAbacus* r = abacus_copy(a);
    CrystallineAbacus* old_s = abacus_from_uint64(0, a->base);
    CrystallineAbacus* s = abacus_from_uint64(1, a->base);
    
    if (!old_r || !r || !old_s || !s) {
        if (old_r) abacus_free(old_r);
        if (r) abacus_free(r);
        if (old_s) abacus_free(old_s);
        if (s) abacus_free(s);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    while (!abacus_is_zero(r)) {
        CrystallineAbacus* quotient = abacus_new(a->base);
        CrystallineAbacus* remainder = abacus_new(a->base);
        
        if (!quotient || !remainder) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            if (quotient) abacus_free(quotient);
            if (remainder) abacus_free(remainder);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        MathError err = abacus_div(quotient, remainder, old_r, r);
        if (err != MATH_SUCCESS) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            abacus_free(quotient);
            abacus_free(remainder);
            return err;
        }
        
        /* Update old_r, r */
        abacus_free(old_r);
        old_r = r;
        r = remainder;
        
        /* Update old_s, s: new_s = old_s - quotient * s */
        CrystallineAbacus* temp = abacus_new(a->base);
        if (!temp) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            abacus_free(quotient);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_mul(temp, quotient, s);
        if (err != MATH_SUCCESS) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            abacus_free(quotient);
            abacus_free(temp);
            return err;
        }
        
        CrystallineAbacus* new_s = abacus_new(a->base);
        if (!new_s) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            abacus_free(quotient);
            abacus_free(temp);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        err = abacus_sub(new_s, old_s, temp);
        if (err != MATH_SUCCESS) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            abacus_free(quotient);
            abacus_free(temp);
            abacus_free(new_s);
            return err;
        }
        
        abacus_free(old_s);
        old_s = s;
        s = new_s;
        
        abacus_free(quotient);
        abacus_free(temp);
    }
    
    /* If old_s is negative, add modulus */
    if (old_s->negative) {
        CrystallineAbacus* adjusted = abacus_new(a->base);
        if (!adjusted) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        MathError err = abacus_add(adjusted, old_s, modulus);
        if (err != MATH_SUCCESS) {
            abacus_free(old_r);
            abacus_free(r);
            abacus_free(old_s);
            abacus_free(s);
            abacus_free(adjusted);
            return err;
        }
        
        result->num_beads = adjusted->num_beads;
        result->negative = adjusted->negative;
        for (size_t i = 0; i < adjusted->num_beads && i < result->capacity; i++) {
            result->beads[i] = adjusted->beads[i];
        }
        
        abacus_free(adjusted);
    } else {
        result->num_beads = old_s->num_beads;
        result->negative = old_s->negative;
        for (size_t i = 0; i < old_s->num_beads && i < result->capacity; i++) {
            result->beads[i] = old_s->beads[i];
        }
    }
    
    abacus_free(old_r);
    abacus_free(r);
    abacus_free(old_s);
    abacus_free(s);
    
    return MATH_SUCCESS;
}