/**
 * @file abacus_geometric_fractional.c
 * @brief Implementation of Fractional Geometric Operations
 * 
 * Week 2 - Phase 1: Fractional Division & Floating-Point Support
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "math/abacus_geometric_fractional.h"
#include "math/abacus_geometric.h"
#include "math/constants.h"
#include "math/transcendental.h"
#include "math/arithmetic.h"

// Use math functions from the library
#define floor(x) math_floor(x)
#define fabs(x) math_abs(x)
#define pow(x, y) math_pow(x, y)
#define round(x) math_round(x)

/* ============================================================================
 * FRACTIONAL DIVISION OPERATIONS
 * ============================================================================
 */

MathError abacus_div_geometric_fractional(const CrystallineAbacus* a,
                                         const CrystallineAbacus* b,
                                         CrystallineAbacus* result,
                                         int32_t precision) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    if (precision < 0) {
        precision = 0;
    }
    
    MathError err;
    
    // Initialize clock triangle (for O(1) geometric framework)
    ClockTriangle3D triangle;
    err = clock_triangle_init(&triangle, 1.0, 1.0);
    if (err != MATH_SUCCESS) return err;
    
    // Step 1: MAP both operands to clock positions
    ClockPosition pos_a, pos_b;
    err = abacus_to_clock_position(a, &pos_a);
    if (err != MATH_SUCCESS) {
        clock_triangle_free(&triangle);
        return err;
    }
    
    err = abacus_to_clock_position(b, &pos_b);
    if (err != MATH_SUCCESS) {
        clock_triangle_free(&triangle);
        return err;
    }
    
    // Check for division by zero
    if (pos_b.position == 0) {
        clock_triangle_free(&triangle);
        return MATH_ERROR_DIVISION_BY_ZERO;
    }
    
    // Step 2: FOLD to Q1 (normalize signs)
    // (polarity is handled in the double conversion)
    
    // Step 3: OPERATE - Perform fractional division
    // Convert to double for fractional calculation
    double val_a_d, val_b_d;
    err = abacus_to_double(a, &val_a_d);
    if (err != MATH_SUCCESS) {
        clock_triangle_free(&triangle);
        return err;
    }
    
    err = abacus_to_double(b, &val_b_d);
    if (err != MATH_SUCCESS) {
        clock_triangle_free(&triangle);
        return err;
    }
    
    // Perform fractional division
    double result_val = val_a_d / val_b_d;
    
    // Step 4: TRACK polarity
    int8_t result_polarity = (result_val >= 0.0) ? 1 : -1;
    double abs_result = fabs(result_val);
    
    // Step 5: Convert result to abacus with precision
    CrystallineAbacus* temp = abacus_from_double(abs_result, result->base, precision);
    if (!temp) {
        clock_triangle_free(&triangle);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Apply sign
    temp->negative = (result_polarity < 0);
    
    // Step 6: MAP BACK - Copy to result
    if (result->beads) {
        free(result->beads);
        result->beads = NULL;
    }
    if (result->sparse_beads) {
        free(result->sparse_beads);
        result->sparse_beads = NULL;
    }
    
    result->base = temp->base;
    result->negative = temp->negative;
    result->min_exponent = temp->min_exponent;
    result->max_exponent = temp->max_exponent;
    result->is_sparse = temp->is_sparse;
    
    if (temp->is_sparse) {
        result->num_nonzero = temp->num_nonzero;
        result->sparse_capacity = temp->sparse_capacity;
        result->sparse_beads = (SparseBead*)malloc(temp->sparse_capacity * sizeof(SparseBead));
        if (!result->sparse_beads) {
            abacus_free(temp);
            clock_triangle_free(&triangle);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        memcpy(result->sparse_beads, temp->sparse_beads,
               temp->num_nonzero * sizeof(SparseBead));
    } else {
        result->num_beads = temp->num_beads;
        result->capacity = temp->capacity;
        result->beads = (AbacusBead*)malloc(temp->capacity * sizeof(AbacusBead));
        if (!result->beads) {
            abacus_free(temp);
            clock_triangle_free(&triangle);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        memcpy(result->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
    }
    
    abacus_free(temp);
    clock_triangle_free(&triangle);
    return MATH_SUCCESS;
}

MathError abacus_divmod_geometric(const CrystallineAbacus* a,
                                  const CrystallineAbacus* b,
                                  CrystallineAbacus* quotient,
                                  CrystallineAbacus* remainder) {
    if (!a || !b || !quotient || !remainder) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    MathError err;
    
    // Get quotient using integer division
    err = abacus_div_geometric(a, b, quotient);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Calculate remainder: remainder = a - (quotient * b)
    CrystallineAbacus* temp = abacus_new(a->base);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    err = abacus_mul_geometric(quotient, b, temp);
    if (err != MATH_SUCCESS) {
        abacus_free(temp);
        return err;
    }
    
    err = abacus_sub_geometric(a, temp, remainder);
    abacus_free(temp);
    
    return err;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

MathError abacus_round_geometric(const CrystallineAbacus* value,
                                CrystallineAbacus* result,
                                int32_t precision) {
    if (!value || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert to double
    double val;
    MathError err = abacus_to_double(value, &val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Round to precision
    double multiplier = pow(10.0, precision);
    double rounded = round(val * multiplier) / multiplier;
    
    // Convert back to abacus
    CrystallineAbacus* temp = abacus_from_double(rounded, result->base, precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy to result
    if (result->beads) free(result->beads);
    if (result->sparse_beads) free(result->sparse_beads);
    
    *result = *temp;
    free(temp);
    
    return MATH_SUCCESS;
}

MathError abacus_get_fractional_part(const CrystallineAbacus* value,
                                    CrystallineAbacus* fractional) {
    if (!value || !fractional) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert to double
    double val;
    MathError err = abacus_to_double(value, &val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Get fractional part
    double frac = val - floor(fabs(val));
    if (val < 0.0) {
        frac = -frac;
    }
    
    // Convert back to abacus
    CrystallineAbacus* temp = abacus_from_double(frac, fractional->base, 10);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy to result
    if (fractional->beads) free(fractional->beads);
    if (fractional->sparse_beads) free(fractional->sparse_beads);
    
    *fractional = *temp;
    free(temp);
    
    return MATH_SUCCESS;
}

MathError abacus_get_integer_part(const CrystallineAbacus* value,
                                 CrystallineAbacus* integer) {
    if (!value || !integer) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert to double
    double val;
    MathError err = abacus_to_double(value, &val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Get integer part
    double int_part = floor(fabs(val));
    if (val < 0.0) {
        int_part = -int_part;
    }
    
    // Convert back to abacus
    CrystallineAbacus* temp = abacus_from_double(int_part, integer->base, 0);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy to result
    if (integer->beads) free(integer->beads);
    if (integer->sparse_beads) free(integer->sparse_beads);
    
    *integer = *temp;
    free(temp);
    
    return MATH_SUCCESS;
}

MathError abacus_to_double_precise(const CrystallineAbacus* abacus,
                                  double* value,
                                  int32_t precision) {
    if (!abacus || !value) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert to double
    MathError err = abacus_to_double(abacus, value);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Round to precision
    if (precision >= 0) {
        double multiplier = pow(10.0, precision);
        *value = round(*value * multiplier) / multiplier;
    }
    
    return MATH_SUCCESS;
}