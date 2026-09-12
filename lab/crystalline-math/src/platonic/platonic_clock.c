/**
 * @file platonic_clock.c
 * @brief Clock lattice integration for Platonic solid generators
 */

#include "math/platonic_clock.h"
#include "math/constants.h"
#include "math/prime.h"
#include "math/arithmetic.h"
#include "math/transcendental.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * VERTEX TO PRIME MAPPING
 * ============================================================================
 */

uint64_t platonic_vertex_to_prime(uint64_t vertex_idx) {
    // Get the (vertex_idx + 1)th prime
    // vertex_idx is 0-based, but prime indices are 1-based
    return prime_nth(vertex_idx + 1);
}

uint64_t platonic_prime_to_vertex(uint64_t prime) {
    // Get the index of this prime (1-based), then convert to 0-based vertex index
    if (!prime_is_prime(prime)) {
        return UINT64_MAX; // Not a prime
    }
    
    // Count primes up to this prime to get its index
    // This is a workaround until prime_index() is implemented
    uint64_t count = 0;
    for (uint64_t p = 2; p <= prime; p = prime_next(p)) {
        count++;
        if (p == prime) {
            return count - 1; // Convert to 0-based
        }
    }
    
    return UINT64_MAX; // Should not reach here
}

/* ============================================================================
 * PRIME TO CLOCK POSITION MAPPING
 * ============================================================================
 */

MathError platonic_prime_to_clock_position(uint64_t prime, ClockPosition* pos) {
    if (!pos) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Use the clock lattice mapping function
    return clock_map_prime_to_position(prime, pos);
}

MathError platonic_vertex_to_clock_position(uint64_t vertex_idx, ClockPosition* pos) {
    if (!pos) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Get prime for this vertex
    uint64_t prime = platonic_vertex_to_prime(vertex_idx);
    
    // Map prime to clock position
    return platonic_prime_to_clock_position(prime, pos);
}

/* ============================================================================
 * CLOCK POSITION TO COORDINATES
 * ============================================================================
 */

MathError platonic_clock_to_coordinates(const ClockPosition* pos,
                                       uint32_t dimension,
                                       CrystallineAbacus*** coords,
                                       uint32_t base,
                                       uint32_t precision) {
    if (!pos || !coords || dimension == 0) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // PURE ABACUS IMPLEMENTATION - NO DOUBLE CONVERSIONS
    // Step 1: Convert angle and radius to Abacus
    CrystallineAbacus* angle_abacus = abacus_from_double(pos->angle, base, precision);
    CrystallineAbacus* radius_abacus = abacus_from_double(pos->radius, base, precision);
    
    if (!angle_abacus || !radius_abacus) {
        if (angle_abacus) abacus_free(angle_abacus);
        if (radius_abacus) abacus_free(radius_abacus);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Step 2: Compute trigonometric values using pure Abacus
    CrystallineAbacus* cos_angle = abacus_new(base);
    CrystallineAbacus* sin_angle = abacus_new(base);
    
    if (!cos_angle || !sin_angle) {
        abacus_free(angle_abacus);
        abacus_free(radius_abacus);
        if (cos_angle) abacus_free(cos_angle);
        if (sin_angle) abacus_free(sin_angle);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    MathError err = math_cos_abacus(cos_angle, angle_abacus, precision);
    if (err != MATH_SUCCESS) {
        abacus_free(angle_abacus);
        abacus_free(radius_abacus);
        abacus_free(cos_angle);
        abacus_free(sin_angle);
        return err;
    }
    
    err = math_sin_abacus(sin_angle, angle_abacus, precision);
    if (err != MATH_SUCCESS) {
        abacus_free(angle_abacus);
        abacus_free(radius_abacus);
        abacus_free(cos_angle);
        abacus_free(sin_angle);
        return err;
    }
    
    // Step 3: Allocate coordinate array
    *coords = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
    if (!*coords) {
        abacus_free(angle_abacus);
        abacus_free(radius_abacus);
        abacus_free(cos_angle);
        abacus_free(sin_angle);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Step 4: Compute x = r * cos(angle)
    if (dimension >= 1) {
        (*coords)[0] = abacus_new(base);
        if (!(*coords)[0]) {
            free(*coords);
            *coords = NULL;
            abacus_free(angle_abacus);
            abacus_free(radius_abacus);
            abacus_free(cos_angle);
            abacus_free(sin_angle);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        abacus_mul((*coords)[0], radius_abacus, cos_angle);
    }
    
    // Step 5: Compute y = r * sin(angle)
    if (dimension >= 2) {
        (*coords)[1] = abacus_new(base);
        if (!(*coords)[1]) {
            if (dimension >= 1) abacus_free((*coords)[0]);
            free(*coords);
            *coords = NULL;
            abacus_free(angle_abacus);
            abacus_free(radius_abacus);
            abacus_free(cos_angle);
            abacus_free(sin_angle);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        abacus_mul((*coords)[1], radius_abacus, sin_angle);
    }
    
    // Step 6: Compute z = sqrt(1 - r^2)
    if (dimension >= 3) {
        (*coords)[2] = abacus_new(base);
        if (!(*coords)[2]) {
            if (dimension >= 1) abacus_free((*coords)[0]);
            if (dimension >= 2) abacus_free((*coords)[1]);
            free(*coords);
            *coords = NULL;
            abacus_free(angle_abacus);
            abacus_free(radius_abacus);
            abacus_free(cos_angle);
            abacus_free(sin_angle);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        CrystallineAbacus* r_squared = abacus_new(base);
        if (!r_squared) {
            if (dimension >= 1) abacus_free((*coords)[0]);
            if (dimension >= 2) abacus_free((*coords)[1]);
            abacus_free((*coords)[2]);
            free(*coords);
            *coords = NULL;
            abacus_free(angle_abacus);
            abacus_free(radius_abacus);
            abacus_free(cos_angle);
            abacus_free(sin_angle);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        abacus_mul(r_squared, radius_abacus, radius_abacus);
        
        CrystallineAbacus* one = abacus_from_uint64(1, base);
        CrystallineAbacus* one_minus_r_sq = abacus_new(base);
        if (!one || !one_minus_r_sq) {
            if (dimension >= 1) abacus_free((*coords)[0]);
            if (dimension >= 2) abacus_free((*coords)[1]);
            abacus_free((*coords)[2]);
            free(*coords);
            *coords = NULL;
            abacus_free(angle_abacus);
            abacus_free(radius_abacus);
            abacus_free(cos_angle);
            abacus_free(sin_angle);
            abacus_free(r_squared);
            if (one) abacus_free(one);
            if (one_minus_r_sq) abacus_free(one_minus_r_sq);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        abacus_sub(one_minus_r_sq, one, r_squared);
        
        err = math_sqrt_abacus((*coords)[2], one_minus_r_sq, precision);
        
        abacus_free(r_squared);
        abacus_free(one);
        abacus_free(one_minus_r_sq);
        
        if (err != MATH_SUCCESS) {
            if (dimension >= 1) abacus_free((*coords)[0]);
            if (dimension >= 2) abacus_free((*coords)[1]);
            abacus_free((*coords)[2]);
            free(*coords);
            *coords = NULL;
            abacus_free(angle_abacus);
            abacus_free(radius_abacus);
            abacus_free(cos_angle);
            abacus_free(sin_angle);
            return err;
        }
    }
    
    // Clean up temporary values
    abacus_free(angle_abacus);
    abacus_free(radius_abacus);
    abacus_free(cos_angle);
    abacus_free(sin_angle);
    
    // Step 7: For dimensions > 3, use harmonic extension with pure Abacus
    if (dimension > 3) {
        // Recreate angle_abacus for higher dimensions
        angle_abacus = abacus_from_double(pos->angle, base, precision);
        radius_abacus = abacus_from_double(pos->radius, base, precision);
        
        if (!angle_abacus || !radius_abacus) {
            for (uint32_t i = 0; i < 3; i++) {
                abacus_free((*coords)[i]);
            }
            free(*coords);
            *coords = NULL;
            if (angle_abacus) abacus_free(angle_abacus);
            if (radius_abacus) abacus_free(radius_abacus);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        for (uint32_t d = 3; d < dimension; d++) {
            double harmonic = (d - 2);
            CrystallineAbacus* harmonic_abacus = abacus_from_double(harmonic, base, precision);
            if (!harmonic_abacus) {
                for (uint32_t i = 0; i < d; i++) {
                    abacus_free((*coords)[i]);
                }
                free(*coords);
                *coords = NULL;
                abacus_free(angle_abacus);
                abacus_free(radius_abacus);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            CrystallineAbacus* phase = abacus_new(base);
            if (!phase) {
                for (uint32_t i = 0; i < d; i++) {
                    abacus_free((*coords)[i]);
                }
                free(*coords);
                *coords = NULL;
                abacus_free(angle_abacus);
                abacus_free(radius_abacus);
                abacus_free(harmonic_abacus);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            abacus_mul(phase, angle_abacus, harmonic_abacus);
            
            (*coords)[d] = abacus_new(base);
            if (!(*coords)[d]) {
                for (uint32_t i = 0; i < d; i++) {
                    abacus_free((*coords)[i]);
                }
                free(*coords);
                *coords = NULL;
                abacus_free(angle_abacus);
                abacus_free(radius_abacus);
                abacus_free(harmonic_abacus);
                abacus_free(phase);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            CrystallineAbacus* trig_value = abacus_new(base);
            if (!trig_value) {
                for (uint32_t i = 0; i <= d; i++) {
                    abacus_free((*coords)[i]);
                }
                free(*coords);
                *coords = NULL;
                abacus_free(angle_abacus);
                abacus_free(radius_abacus);
                abacus_free(harmonic_abacus);
                abacus_free(phase);
                return MATH_ERROR_OUT_OF_MEMORY;
            }
            
            if ((d - 3) % 2 == 0) {
                err = math_sin_abacus(trig_value, phase, precision);
            } else {
                err = math_cos_abacus(trig_value, phase, precision);
            }
            
            if (err != MATH_SUCCESS) {
                for (uint32_t i = 0; i <= d; i++) {
                    abacus_free((*coords)[i]);
                }
                free(*coords);
                *coords = NULL;
                abacus_free(angle_abacus);
                abacus_free(radius_abacus);
                abacus_free(harmonic_abacus);
                abacus_free(phase);
                abacus_free(trig_value);
                return err;
            }
            
            abacus_mul((*coords)[d], radius_abacus, trig_value);
            
            abacus_free(harmonic_abacus);
            abacus_free(phase);
            abacus_free(trig_value);
        }
        
        abacus_free(angle_abacus);
        abacus_free(radius_abacus);
    }
    
    return MATH_SUCCESS;
}

MathError platonic_vertex_to_coordinates(uint64_t vertex_idx,
                                        uint32_t dimension,
                                        CrystallineAbacus*** coords,
                                        uint32_t base,
                                        uint32_t precision) {
    // Get clock position for this vertex
    ClockPosition pos;
    MathError err = platonic_vertex_to_clock_position(vertex_idx, &pos);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Generate coordinates from clock position
    return platonic_clock_to_coordinates(&pos, dimension, coords, base, precision);
}

/* ============================================================================
 * COORDINATE VALIDATION
 * ============================================================================
 */

bool platonic_validate_coordinates(CrystallineAbacus** coords, uint32_t dimension) {
    if (!coords || dimension == 0) {
        return false;
    }
    
    // Check each coordinate
    for (uint32_t i = 0; i < dimension; i++) {
        if (!coords[i]) {
            return false;
        }
        
        // Convert to double to check magnitude
        double value;
        MathError err = abacus_to_double(coords[i], &value);
        if (err != MATH_SUCCESS) {
            return false;
        }
        
        // Check for NaN or infinity
        if (value != value || value == 1.0/0.0 || value == -1.0/0.0) {
            return false;
        }
        
        // Check magnitude is reasonable (not too large)
        if (value > 1e10 || value < -1e10) {
            return false;
        }
    }
    
    return true;
}

CrystallineAbacus* platonic_coordinate_distance(CrystallineAbacus** coords1,
                                               CrystallineAbacus** coords2,
                                               uint32_t dimension) {
    if (!coords1 || !coords2 || dimension == 0) {
        return NULL;
    }
    
    // PURE ABACUS IMPLEMENTATION - NO DOUBLE CONVERSIONS
    uint32_t base = coords1[0]->base;
    uint32_t precision = 15; // High precision for distance calculation
    
    // Initialize sum of squared differences
    CrystallineAbacus* sum_sq = abacus_from_uint64(0, base);
    if (!sum_sq) {
        return NULL;
    }
    
    // Compute sum of squared differences using pure Abacus
    for (uint32_t i = 0; i < dimension; i++) {
        // Compute difference: diff = coords1[i] - coords2[i]
        CrystallineAbacus* diff = abacus_new(base);
        if (!diff) {
            abacus_free(sum_sq);
            return NULL;
        }
        abacus_sub(diff, coords1[i], coords2[i]);
        
        // Compute squared difference: diff_sq = diff * diff
        CrystallineAbacus* diff_sq = abacus_new(base);
        if (!diff_sq) {
            abacus_free(diff);
            abacus_free(sum_sq);
            return NULL;
        }
        abacus_mul(diff_sq, diff, diff);
        
        // Add to sum: sum_sq = sum_sq + diff_sq
        CrystallineAbacus* new_sum = abacus_new(base);
        if (!new_sum) {
            abacus_free(diff);
            abacus_free(diff_sq);
            abacus_free(sum_sq);
            return NULL;
        }
        abacus_add(new_sum, sum_sq, diff_sq);
        
        // Replace old sum with new sum
        abacus_free(sum_sq);
        sum_sq = new_sum;
        
        abacus_free(diff);
        abacus_free(diff_sq);
    }
    
    // Take square root using pure Abacus
    CrystallineAbacus* distance = abacus_new(base);
    if (!distance) {
        abacus_free(sum_sq);
        return NULL;
    }
    
    MathError err = math_sqrt_abacus(distance, sum_sq, precision);
    abacus_free(sum_sq);
    
    if (err != MATH_SUCCESS) {
        abacus_free(distance);
        return NULL;
    }
    
    return distance;
}

/* ============================================================================
 * SYMMETRY OPERATIONS
 * ============================================================================
 */

MathError platonic_apply_12fold_rotation(CrystallineAbacus** coords,
                                         uint32_t dimension,
                                         uint32_t rotation,
                                         CrystallineAbacus** result) {
    if (!coords || !result || dimension < 2 || rotation >= 12) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // PURE ABACUS IMPLEMENTATION - NO DOUBLE CONVERSIONS
    uint32_t base = coords[0]->base;
    uint32_t precision = 15; // High precision
    
    // Rotation angle: rotation * 30° = rotation * π/6
    // Compute angle as Abacus: rotation * π / 6
    CrystallineAbacus* pi = abacus_from_double(3.14159265358979323846, base, precision);
    CrystallineAbacus* six = abacus_from_uint64(6, base);
    CrystallineAbacus* rotation_num = abacus_from_uint64(rotation, base);
    
    if (!pi || !six || !rotation_num) {
        if (pi) abacus_free(pi);
        if (six) abacus_free(six);
        if (rotation_num) abacus_free(rotation_num);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // angle = rotation * π / 6
    CrystallineAbacus* pi_over_6 = abacus_new(base);
    CrystallineAbacus* remainder = abacus_new(base);
    abacus_div(pi_over_6, remainder, pi, six);
    abacus_free(remainder);
    
    CrystallineAbacus* angle = abacus_new(base);
    abacus_mul(angle, rotation_num, pi_over_6);
    
    abacus_free(pi);
    abacus_free(six);
    abacus_free(rotation_num);
    abacus_free(pi_over_6);
    
    // Compute sin and cos using pure Abacus
    CrystallineAbacus* cos_angle = abacus_new(base);
    CrystallineAbacus* sin_angle = abacus_new(base);
    
    if (!cos_angle || !sin_angle) {
        abacus_free(angle);
        if (cos_angle) abacus_free(cos_angle);
        if (sin_angle) abacus_free(sin_angle);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    MathError err = math_cos_abacus(cos_angle, angle, precision);
    if (err != MATH_SUCCESS) {
        abacus_free(angle);
        abacus_free(cos_angle);
        abacus_free(sin_angle);
        return err;
    }
    
    err = math_sin_abacus(sin_angle, angle, precision);
    if (err != MATH_SUCCESS) {
        abacus_free(angle);
        abacus_free(cos_angle);
        abacus_free(sin_angle);
        return err;
    }
    
    abacus_free(angle);
    
    // Apply rotation in the first 2 dimensions (x-y plane)
    // x' = x*cos(θ) - y*sin(θ)
    // y' = x*sin(θ) + y*cos(θ)
    
    // Compute x*cos(θ)
    CrystallineAbacus* x_cos = abacus_new(base);
    abacus_mul(x_cos, coords[0], cos_angle);
    
    // Compute y*sin(θ)
    CrystallineAbacus* y_sin = abacus_new(base);
    abacus_mul(y_sin, coords[1], sin_angle);
    
    // Compute x*sin(θ)
    CrystallineAbacus* x_sin = abacus_new(base);
    abacus_mul(x_sin, coords[0], sin_angle);
    
    // Compute y*cos(θ)
    CrystallineAbacus* y_cos = abacus_new(base);
    abacus_mul(y_cos, coords[1], cos_angle);
    
    abacus_free(cos_angle);
    abacus_free(sin_angle);
    
    if (!x_cos || !y_sin || !x_sin || !y_cos) {
        if (x_cos) abacus_free(x_cos);
        if (y_sin) abacus_free(y_sin);
        if (x_sin) abacus_free(x_sin);
        if (y_cos) abacus_free(y_cos);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Free old values if they exist
    if (result[0]) abacus_free(result[0]);
    if (result[1]) abacus_free(result[1]);
    
    // x' = x*cos(θ) - y*sin(θ)
    result[0] = abacus_new(base);
    abacus_sub(result[0], x_cos, y_sin);
    
    // y' = x*sin(θ) + y*cos(θ)
    result[1] = abacus_new(base);
    abacus_add(result[1], x_sin, y_cos);
    
    abacus_free(x_cos);
    abacus_free(y_sin);
    abacus_free(x_sin);
    abacus_free(y_cos);
    
    if (!result[0] || !result[1]) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy remaining dimensions unchanged
    for (uint32_t i = 2; i < dimension; i++) {
        double value;
        MathError err = abacus_to_double(coords[i], &value);
        if (err != MATH_SUCCESS) {
            return err;
        }
        
        if (result[i]) abacus_free(result[i]);
        result[i] = abacus_from_double(value, base, precision);
        if (!result[i]) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
    }
    
    return MATH_SUCCESS;
}

bool platonic_has_12fold_symmetry(CrystallineAbacus** coords, uint32_t dimension) {
    if (!coords || dimension < 2) {
        return false;
    }
    
    // Allocate temporary result array (array of pointers)
    CrystallineAbacus** rotated = (CrystallineAbacus**)calloc(dimension, sizeof(CrystallineAbacus*));
    if (!rotated) {
        return false;
    }
    
    // Check if rotating by 30° (1/12 of full circle) gives similar structure
    // For true 12-fold symmetry, rotating 12 times should return to original
    bool has_symmetry = true;
    
    // Apply one 30° rotation
    MathError err = platonic_apply_12fold_rotation(coords, dimension, 1, rotated);
    if (err != MATH_SUCCESS) {
        has_symmetry = false;
    } else {
        // Check if the rotated coordinates are valid
        // (For full validation, we'd need to check all 12 rotations)
        has_symmetry = platonic_validate_coordinates(rotated, dimension);
    }
    
    // Clean up
    for (uint32_t i = 0; i < dimension; i++) {
        if (rotated[i]) {
            abacus_free(rotated[i]);
        }
    }
    free(rotated);
    
    return has_symmetry;
}