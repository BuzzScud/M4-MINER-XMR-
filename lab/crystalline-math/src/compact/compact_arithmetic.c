/**
 * @file compact_arithmetic.c
 * @brief Compact Arithmetic Operations
 * 
 * Implements arithmetic operations using triangulation-based computation
 * on compact vector representations.
 * 
 * This provides memory-efficient arithmetic using the compact vector
 * representation (16 bytes per vector vs 40 bytes per bead).
 */

#include "math/compact_vector.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================
 */

static MathError ensure_capacity(CompactNumber* number, size_t min_capacity);
static MathError add_vector(CompactNumber* number, const CompactVector* vector);

// Forward declare arithmetic operations for mutual recursion
MathError compact_subtract(const CompactNumber* a, const CompactNumber* b, CompactNumber** result);

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Ensure compact number has enough capacity
 */
static MathError ensure_capacity(CompactNumber* number, size_t min_capacity) {
    if (!number) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    if (number->capacity >= min_capacity) {
        return MATH_SUCCESS;
    }
    
    size_t new_capacity = number->capacity * 2;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }
    
    CompactVector* new_vectors = (CompactVector*)realloc(
        number->vectors,
        new_capacity * sizeof(CompactVector)
    );
    
    if (!new_vectors) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    number->vectors = new_vectors;
    number->capacity = new_capacity;
    
    return MATH_SUCCESS;
}

/**
 * @brief Add a vector to compact number
 */
static MathError add_vector(CompactNumber* number, const CompactVector* vector) {
    if (!number || !vector) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Ensure capacity
    MathError err = ensure_capacity(number, number->num_vectors + 1);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Add vector
    number->vectors[number->num_vectors] = *vector;
    number->num_vectors++;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * ARITHMETIC OPERATIONS
 * ============================================================================
 */

/**
 * @brief Add two compact numbers using triangulation
 * 
 * Uses geometric vector addition on the compact representation.
 * Each pair of vectors from a and b is combined using triangulation.
 */
MathError compact_add(
    const CompactNumber* a,
    const CompactNumber* b,
    CompactNumber** result
) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Validate same base
    if (a->base != b->base) {
        return MATH_ERROR_INVALID_BASE;
    }
    
    // Create result number
    MathError err = compact_number_create(
        a->base,
        (a->precision > b->precision) ? a->precision : b->precision,
        a->clock_ctx,
        result
    );
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    CompactNumber* res = *result;
    
    // Handle signs
    if (a->negative == b->negative) {
        // Same sign - add magnitudes
        res->negative = a->negative;
        
        // Process each vector from both numbers
        for (size_t i = 0; i < a->num_vectors; i++) {
            for (size_t j = 0; j < b->num_vectors; j++) {
                // Create triangle for addition
                Triangle tri;
                tri.p0.sphere_id = 0;
                tri.p0.phase_angle = 0.0f;
                tri.p0.magnitude_offset = 0;
                tri.p0.phase_offset = 0.0f;
                
                tri.p1 = a->vectors[i];
                tri.p2 = b->vectors[j];
                
                // Triangulate
                CompactVector result_vec;
                err = triangulate_addition(&tri, &result_vec);
                if (err != MATH_SUCCESS) {
                    compact_number_free(res);
                    *result = NULL;
                    return err;
                }
                
                // Add to result
                err = add_vector(res, &result_vec);
                if (err != MATH_SUCCESS) {
                    compact_number_free(res);
                    *result = NULL;
                    return err;
                }
            }
        }
    } else {
        // Different signs - use subtraction
        CompactNumber* b_negated = NULL;
        err = compact_number_create(b->base, b->precision, b->clock_ctx, &b_negated);
        if (err != MATH_SUCCESS) {
            compact_number_free(res);
            *result = NULL;
            return err;
        }
        
        // Copy b and negate
        for (size_t i = 0; i < b->num_vectors; i++) {
            add_vector(b_negated, &b->vectors[i]);
        }
        b_negated->negative = !b->negative;
        
        // Now do a - (-b) = a + b
        err = compact_subtract(a, b_negated, result);
        compact_number_free(b_negated);
        compact_number_free(res);
        return err;
    }
    
    return MATH_SUCCESS;
}

/**
 * @brief Subtract two compact numbers using triangulation
 */
MathError compact_subtract(
    const CompactNumber* a,
    const CompactNumber* b,
    CompactNumber** result
) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Validate same base
    if (a->base != b->base) {
        return MATH_ERROR_INVALID_BASE;
    }
    
    // Create result number
    MathError err = compact_number_create(
        a->base,
        (a->precision > b->precision) ? a->precision : b->precision,
        a->clock_ctx,
        result
    );
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    CompactNumber* res = *result;
    
    // Process each vector from both numbers
    for (size_t i = 0; i < a->num_vectors; i++) {
        for (size_t j = 0; j < b->num_vectors; j++) {
            // Create triangle for subtraction
            Triangle tri;
            tri.p0.sphere_id = 0;
            tri.p0.phase_angle = 0.0f;
            tri.p0.magnitude_offset = 0;
            tri.p0.phase_offset = 0.0f;
            
            tri.p1 = a->vectors[i];
            tri.p2 = b->vectors[j];
            
            // Triangulate
            CompactVector result_vec;
            err = triangulate_subtraction(&tri, &result_vec);
            if (err != MATH_SUCCESS) {
                compact_number_free(res);
                *result = NULL;
                return err;
            }
            
            // Add to result
            err = add_vector(res, &result_vec);
            if (err != MATH_SUCCESS) {
                compact_number_free(res);
                *result = NULL;
                return err;
            }
        }
    }
    
    // Handle sign
    res->negative = a->negative;
    
    return MATH_SUCCESS;
}

/**
 * @brief Multiply two compact numbers using triangulation
 */
MathError compact_multiply(
    const CompactNumber* a,
    const CompactNumber* b,
    CompactNumber** result
) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Validate same base
    if (a->base != b->base) {
        return MATH_ERROR_INVALID_BASE;
    }
    
    // Create result number
    MathError err = compact_number_create(
        a->base,
        a->precision + b->precision, // Multiplication can double precision
        a->clock_ctx,
        result
    );
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    CompactNumber* res = *result;
    
    // Process each vector from both numbers
    for (size_t i = 0; i < a->num_vectors; i++) {
        for (size_t j = 0; j < b->num_vectors; j++) {
            // Create triangle for multiplication
            Triangle tri;
            tri.p0.sphere_id = 0;
            tri.p0.phase_angle = 0.0f;
            tri.p0.magnitude_offset = 0;
            tri.p0.phase_offset = 0.0f;
            
            tri.p1 = a->vectors[i];
            tri.p2 = b->vectors[j];
            
            // Triangulate
            CompactVector result_vec;
            err = triangulate_multiplication(&tri, &result_vec);
            if (err != MATH_SUCCESS) {
                compact_number_free(res);
                *result = NULL;
                return err;
            }
            
            // Add to result
            err = add_vector(res, &result_vec);
            if (err != MATH_SUCCESS) {
                compact_number_free(res);
                *result = NULL;
                return err;
            }
        }
    }
    
    // Handle sign (negative × negative = positive)
    res->negative = (a->negative != b->negative);
    
    return MATH_SUCCESS;
}

/**
 * @brief Divide two compact numbers
 * 
 * Division is implemented using inverse rotation and magnitude division.
 */
MathError compact_divide(
    const CompactNumber* a,
    const CompactNumber* b,
    CompactNumber** result
) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Check for division by zero
    if (b->num_vectors == 0) {
        return MATH_ERROR_DIVISION_BY_ZERO;
    }
    
    // Validate same base
    if (a->base != b->base) {
        return MATH_ERROR_INVALID_BASE;
    }
    
    // Create result number
    MathError err = compact_number_create(
        a->base,
        a->precision,
        a->clock_ctx,
        result
    );
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    CompactNumber* res = *result;
    
    // Division: for each vector in a, divide by each vector in b
    // This is a simplified implementation
    for (size_t i = 0; i < a->num_vectors; i++) {
        for (size_t j = 0; j < b->num_vectors; j++) {
            CompactVector result_vec;
            
            // Divide angles (subtract in rotation space)
            result_vec.phase_angle = a->vectors[i].phase_angle - b->vectors[j].phase_angle;
            if (result_vec.phase_angle < 0.0f) {
                result_vec.phase_angle += 360.0f;
            }
            
            // Divide magnitudes
            if (b->vectors[j].magnitude_offset != 0) {
                result_vec.magnitude_offset = a->vectors[i].magnitude_offset / b->vectors[j].magnitude_offset;
            } else {
                compact_number_free(res);
                *result = NULL;
                return MATH_ERROR_DIVISION_BY_ZERO;
            }
            
            // Sphere stays the same for now (simplified)
            result_vec.sphere_id = a->vectors[i].sphere_id;
            result_vec.phase_offset = a->vectors[i].phase_offset - b->vectors[j].phase_offset;
            
            // Add to result
            err = add_vector(res, &result_vec);
            if (err != MATH_SUCCESS) {
                compact_number_free(res);
                *result = NULL;
                return err;
            }
        }
    }
    
    // Handle sign
    res->negative = (a->negative != b->negative);
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * COMPARISON OPERATIONS
 * ============================================================================
 */

/**
 * @brief Compare two compact numbers
 * 
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int compact_compare(
    const CompactNumber* a,
    const CompactNumber* b
) {
    if (!a || !b) return 0;
    
    // Handle signs
    if (a->negative && !b->negative) return -1;
    if (!a->negative && b->negative) return 1;
    
    // Same sign - compare magnitudes
    // Compare number of vectors first (rough estimate)
    if (a->num_vectors < b->num_vectors) {
        return a->negative ? 1 : -1;
    }
    if (a->num_vectors > b->num_vectors) {
        return a->negative ? -1 : 1;
    }
    
    // Same number of vectors - compare individual vectors
    for (size_t i = 0; i < a->num_vectors; i++) {
        int cmp = compact_vector_compare(&a->vectors[i], &b->vectors[i]);
        if (cmp != 0) {
            return a->negative ? -cmp : cmp;
        }
    }
    
    return 0; // Equal
}

/**
 * @brief Check if compact number is zero
 */
bool compact_is_zero(const CompactNumber* number) {
    if (!number) return true;
    return number->num_vectors == 0;
}

/**
 * @brief Check if compact number is negative
 */
bool compact_is_negative(const CompactNumber* number) {
    if (!number) return false;
    return number->negative && number->num_vectors > 0;
}

/* ============================================================================
 * END OF COMPACT ARITHMETIC
 * ============================================================================
 */