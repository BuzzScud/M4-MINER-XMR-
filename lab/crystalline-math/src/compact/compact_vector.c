#include "math/types.h"
/**
 * @file compact_vector.c
 * @brief Compact Vector Implementation
 * 
 * Implements compact vector operations for memory hopping architecture.
 * PURE crystalline mathematics - NO math.h!
 */

#include "math/compact_vector.h"
#include "math/prime.h"
#include "math/transcendental.h"
#include "math/constants.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CONSTANTS
 * ============================================================================
 */


/* ============================================================================
 * COMPACT VECTOR OPERATIONS
 * ============================================================================
 */

MathError compact_vector_create(
    uint32_t sphere_id,
    float phase_angle,
    int32_t magnitude_offset,
    CompactVector* vector
) {
    if (!vector) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Normalize angle to 0-360°
    while (phase_angle < 0.0f) phase_angle += 360.0f;
    while (phase_angle >= 360.0f) phase_angle -= 360.0f;
    
    vector->sphere_id = sphere_id;
    vector->phase_angle = phase_angle;
    vector->magnitude_offset = magnitude_offset;
    vector->phase_offset = 0.0f;
    
    return MATH_SUCCESS;
}

MathError compact_vector_distance(
    const CompactVector* v1,
    const CompactVector* v2,
    double* distance
) {
    if (!v1 || !v2 || !distance) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert angles to radians
    double angle1 = (v1->phase_angle * MATH_PI / 180.0);
    double angle2 = (v2->phase_angle * MATH_PI / 180.0);
    
    // Magnitude difference
    double mag_diff = (double)(v2->magnitude_offset - v1->magnitude_offset);
    
    // Angular distance on unit sphere
    double angular_dist = (angle2 > angle1) ? (angle2 - angle1) : (angle1 - angle2);
    if (angular_dist > MATH_PI) {
        angular_dist = 2.0 * MATH_PI - angular_dist;
    }
    
    // Combined distance (Euclidean in cylindrical coordinates)
    *distance = math_sqrt(mag_diff * mag_diff + angular_dist * angular_dist);
    
    return MATH_SUCCESS;
}

MathError compact_vector_angle(
    const CompactVector* v1,
    const CompactVector* v2,
    double* angle
) {
    if (!v1 || !v2 || !angle) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Angular difference
    double diff = v2->phase_angle - v1->phase_angle;
    
    // Normalize to -180 to 180
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;
    
    *angle = diff;
    
    return MATH_SUCCESS;
}

int compact_vector_compare(
    const CompactVector* v1,
    const CompactVector* v2
) {
    if (!v1 || !v2) return 0;
    
    // Compare by magnitude first
    if (v1->magnitude_offset < v2->magnitude_offset) return -1;
    if (v1->magnitude_offset > v2->magnitude_offset) return 1;
    
    // Same magnitude, compare by angle
    if (v1->phase_angle < v2->phase_angle) return -1;
    if (v1->phase_angle > v2->phase_angle) return 1;
    
    // Same magnitude and angle, compare by sphere
    if (v1->sphere_id < v2->sphere_id) return -1;
    if (v1->sphere_id > v2->sphere_id) return 1;
    
    return 0; // Equal
}

/* ============================================================================
 * COMPACT NUMBER OPERATIONS
 * ============================================================================
 */

MathError compact_number_create(
    uint32_t base,
    uint32_t precision,
    ClockContext* clock_ctx,
    CompactNumber** number
) {
    if (!number || !clock_ctx) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Validate base
    if (base != 12 && base != 60 && base != 100) {
        return MATH_ERROR_INVALID_BASE;
    }
    
    // Allocate number
    CompactNumber* n = (CompactNumber*)malloc(sizeof(CompactNumber));
    if (!n) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Initial capacity (start small, grow as needed)
    size_t initial_capacity = 8;
    n->vectors = (CompactVector*)malloc(initial_capacity * sizeof(CompactVector));
    if (!n->vectors) {
        free(n);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    n->num_vectors = 0;
    n->capacity = initial_capacity;
    n->base = base;
    n->precision = precision;
    n->negative = false;
    n->clock_ctx = clock_ctx;
    
    *number = n;
    return MATH_SUCCESS;
}

void compact_number_free(CompactNumber* number) {
    if (!number) return;
    
    if (number->vectors) {
        free(number->vectors);
    }
    
    free(number);
}

MathError compact_number_from_uint64(
    uint64_t value,
    uint32_t base,
    uint32_t precision,
    ClockContext* clock_ctx,
    CompactNumber** number
) {
    if (!number || !clock_ctx) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Create number
    MathError err = compact_number_create(base, precision, clock_ctx, number);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    CompactNumber* n = *number;
    
    // Special case: zero
    if (value == 0) {
        return MATH_SUCCESS;
    }
    
    // Convert to compact representation
    // Strategy: Find significant positions only
    
    uint64_t remaining = value;
    int32_t exponent = 0;
    
    while (remaining > 0) {
        uint32_t digit = remaining % base;
        
        // Only store non-zero digits
        if (digit != 0) {
            // Ensure capacity
            if (n->num_vectors >= n->capacity) {
                size_t new_capacity = n->capacity * 2;
                CompactVector* new_vectors = (CompactVector*)realloc(
                    n->vectors,
                    new_capacity * sizeof(CompactVector)
                );
                if (!new_vectors) {
                    compact_number_free(n);
                    *number = NULL;
                    return MATH_ERROR_OUT_OF_MEMORY;
                }
                n->vectors = new_vectors;
                n->capacity = new_capacity;
            }
            
            // Calculate phase angle from digit
            float phase_angle = (digit % 12) * 30.0f;
            
            // Create vector
            CompactVector* v = &n->vectors[n->num_vectors];
            v->sphere_id = 0; // Root sphere
            v->phase_angle = phase_angle;
            v->magnitude_offset = exponent;
            v->phase_offset = 0.0f;
            
            n->num_vectors++;
        }
        
        remaining /= base;
        exponent++;
    }
    
    return MATH_SUCCESS;
}

MathError compact_number_to_uint64(
    const CompactNumber* number,
    uint64_t* value
) {
    if (!number || !value) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    *value = 0;
    
    // Reconstruct from vectors
    for (size_t i = 0; i < number->num_vectors; i++) {
        const CompactVector* v = &number->vectors[i];
        
        // Calculate digit from phase angle
        uint32_t digit = (uint32_t)(v->phase_angle / 30.0f + 0.5f) % 12;
        
        // Calculate weight
        uint64_t weight = 1;
        for (int32_t j = 0; j < v->magnitude_offset; j++) {
            weight *= number->base;
        }
        
        // Add contribution
        *value += digit * weight;
    }
    
    if (number->negative) {
        // Note: uint64_t can't represent negative, return error
        return MATH_ERROR_OVERFLOW;
    }
    
    return MATH_SUCCESS;
}

MathError compact_number_get_digit(
    const CompactNumber* number,
    int32_t exponent,
    uint32_t* digit
) {
    if (!number || !digit) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Find vector covering this exponent
    for (size_t i = 0; i < number->num_vectors; i++) {
        const CompactVector* v = &number->vectors[i];
        
        if (v->magnitude_offset == exponent) {
            // Found it - extract digit from phase angle
            *digit = (uint32_t)(v->phase_angle / 30.0f + 0.5f) % 12;
            return MATH_SUCCESS;
        }
    }
    
    // Not found - implicit zero
    *digit = 0;
    return MATH_SUCCESS;
}

MathError compact_number_to_string(
    const CompactNumber* number,
    char* buffer,
    size_t buffer_size
) {
    if (!number || !buffer || buffer_size == 0) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Find maximum exponent
    int32_t max_exp = 0;
    for (size_t i = 0; i < number->num_vectors; i++) {
        if (number->vectors[i].magnitude_offset > max_exp) {
            max_exp = number->vectors[i].magnitude_offset;
        }
    }
    
    size_t pos = 0;
    
    // Add sign
    if (number->negative && pos < buffer_size - 1) {
        buffer[pos++] = '-';
    }
    
    // Convert digits from most significant to least
    bool started = false;
    for (int32_t exp = max_exp; exp >= 0 && pos < buffer_size - 1; exp--) {
        uint32_t digit;
        MathError err = compact_number_get_digit(number, exp, &digit);
        if (err != MATH_SUCCESS) {
            return err;
        }
        
        // Skip leading zeros
        if (!started && digit == 0 && exp > 0) {
            continue;
        }
        started = true;
        
        // Convert digit to character
        if (digit < 10) {
            buffer[pos++] = '0' + digit;
        } else {
            buffer[pos++] = 'A' + (digit - 10);
        }
    }
    
    // Handle zero case
    if (!started && pos < buffer_size - 1) {
        buffer[pos++] = '0';
    }
    
    buffer[pos] = '\0';
    return MATH_SUCCESS;
}

/* ============================================================================
 * TRIANGULATION OPERATIONS
 * ============================================================================
 */

MathError triangulate_addition(
    const Triangle* tri,
    CompactVector* result
) {
    if (!tri || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert angles to radians
    double angle_a = (tri->p1.phase_angle * MATH_PI / 180.0);
    double angle_b = (tri->p2.phase_angle * MATH_PI / 180.0);
    
    // Spherical law of cosines for result angle
    // For addition on a sphere, we use vector addition
    double result_angle_rad = math_atan2(
        math_sin(angle_a) + math_sin(angle_b),
        math_cos(angle_a) + math_cos(angle_b)
    );
    
    // Convert back to degrees
    double result_angle = (result_angle_rad * 180.0 / MATH_PI);
    if (result_angle < 0.0) result_angle += 360.0;
    
    // Magnitude is simple addition
    int32_t result_magnitude = tri->p1.magnitude_offset + tri->p2.magnitude_offset;
    
    // Create result vector
    result->sphere_id = tri->p1.sphere_id; // Stay in same sphere
    result->phase_angle = (float)result_angle;
    result->magnitude_offset = result_magnitude;
    result->phase_offset = 0.0f;
    
    return MATH_SUCCESS;
}

MathError triangulate_subtraction(
    const Triangle* tri,
    CompactVector* result
) {
    if (!tri || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Subtraction: A - B
    // Geometrically: rotate B by 180° and adjust magnitude
    
    // Convert angles to radians
    double angle_a = (tri->p1.phase_angle * MATH_PI / 180.0);
    double angle_b = (tri->p2.phase_angle * MATH_PI / 180.0);
    
    // For subtraction, we negate B (180° rotation)
    angle_b += MATH_PI;
    
    // Spherical law of cosines for result angle
    double result_angle_rad = math_atan2(
        math_sin(angle_a) + math_sin(angle_b),
        math_cos(angle_a) + math_cos(angle_b)
    );
    
    // Convert back to degrees
    double result_angle = (result_angle_rad * 180.0 / MATH_PI);
    if (result_angle < 0.0) result_angle += 360.0;
    
    // Magnitude is subtraction (not addition!)
    int32_t result_magnitude = tri->p1.magnitude_offset - tri->p2.magnitude_offset;
    
    // Create result vector
    result->sphere_id = tri->p1.sphere_id;
    result->phase_angle = (float)result_angle;
    result->magnitude_offset = result_magnitude;
    result->phase_offset = 0.0f;
    
    return MATH_SUCCESS;
}

MathError triangulate_multiplication(
    const Triangle* tri,
    CompactVector* result
) {
    if (!tri || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Multiplication uses rotation composition
    // Result angle = sum of angles (modulo 360)
    float result_angle = (float)math_fmod(
        tri->p1.phase_angle + tri->p2.phase_angle,
        360.0f
    );
    
    // Result magnitude = product of magnitudes
    int32_t result_magnitude = tri->p1.magnitude_offset * tri->p2.magnitude_offset;
    
    // May need to move to higher sphere for larger magnitude
    uint32_t result_sphere = tri->p1.sphere_id;
    if (result_magnitude > 100) {
        result_sphere++;
    }
    
    // Create result vector
    result->sphere_id = result_sphere;
    result->phase_angle = result_angle;
    result->magnitude_offset = result_magnitude;
    result->phase_offset = 0.0f;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * PRECISE CLOCK OPERATIONS
 * ============================================================================
 */

MathError get_precise_clock_position(
    uint64_t number,
    PreciseClockPosition* position
) {
    if (!position) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Base position (mod 12)
    uint32_t base_pos = number % 12;
    double base_angle = base_pos * 30.0;
    
    // Calculate magnitude
    uint64_t magnitude = number / 12;
    
    // Determine ring based on magnitude
    uint32_t ring = 0;
    if (magnitude >= 100) ring = 3;
    else if (magnitude >= 60) ring = 2;
    else if (magnitude >= 12) ring = 1;
    
    // Apply factor-based refinement for enhanced precision
    // Simple factorization for small numbers
    uint64_t factors[16];
    size_t num_factors = 0;
    uint64_t n = number;
    
    // Find factors (simple trial division for demonstration)
    for (uint64_t f = 2; f <= n && num_factors < 16; f++) {
        while (n % f == 0 && num_factors < 16) {
            factors[num_factors++] = f;
            n /= f;
        }
    }
    
    // Calculate precise angle with factor refinement
    double precise_angle;
    if (num_factors > 0) {
        MathError err = calculate_precise_angle_with_factors(
            number, factors, num_factors, &precise_angle
        );
        if (err != MATH_SUCCESS) {
            precise_angle = base_angle;
        }
    } else {
        precise_angle = base_angle;
    }
    
    position->ring = ring;
    position->precise_angle = precise_angle;
    position->magnitude = magnitude;
    position->phase_offset = 0.0;
    
    return MATH_SUCCESS;
}

MathError calculate_precise_angle_with_factors(
    uint64_t number,
    const uint64_t* factors,
    size_t num_factors,
    double* angle
) {
    if (!angle) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Base angle (30° per position)
    uint32_t base_pos = number % 12;
    double base_angle = base_pos * 30.0;
    
    // Factor-based refinement
    double refinement = 0.0;
    
    if (factors && num_factors > 0) {
        for (size_t i = 0; i < num_factors; i++) {
            // Each factor contributes angular precision
            double factor_angle = (double)(factors[i] % 360);
            
            // Weight decreases with index
            double weight = 1.0 / (double)(i + 2);
            
            // Accumulate
            refinement += factor_angle * weight;
        }
        
        // Normalize refinement to within one position (30°)
        refinement = math_fmod(refinement, 30.0);
    }
    
    // Final angle with full 360° precision
    *angle = math_fmod(base_angle + refinement, 360.0);
    
    return MATH_SUCCESS;
}

MathError find_prime_phase_offset(
    uint32_t position,
    uint64_t magnitude,
    double* phase_offset
) {
    if (!phase_offset) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Get base for position
    uint64_t base;
    switch (position % 12) {
        case 1: base = 13; break;
        case 5: base = 5; break;
        case 7: base = 7; break;
        case 11: base = 11; break;
        default:
            *phase_offset = 0.0;
            return MATH_SUCCESS; // Not a prime position
    }
    
    // Calculate candidate
    uint64_t candidate = base + magnitude * 12;
    
    // Check if already prime
    bool is_prime_val = prime_is_prime(candidate);
    
    if (is_prime_val) {
        *phase_offset = 0.0;
        return MATH_SUCCESS;
    }
    
    // Search for prime within ±15° (half a clock position)
    for (double theta = 0.1; theta < 15.0; theta += 0.1) {
        // Positive offset
        uint64_t adjusted = candidate + (uint64_t)(theta * magnitude / 15.0);
        is_prime_val = prime_is_prime(adjusted);
        if (is_prime_val) {
            *phase_offset = theta;
            return MATH_SUCCESS;
        }
        
        // Negative offset
        if (candidate > (uint64_t)(theta * magnitude / 15.0)) {
            adjusted = candidate - (uint64_t)(theta * magnitude / 15.0);
            is_prime_val = prime_is_prime(adjusted);
            if (is_prime_val) {
                *phase_offset = -theta;
                return MATH_SUCCESS;
            }
        }
    }
    
    // No prime found in range
    *phase_offset = 0.0;
    return MATH_SUCCESS;
}

/* ============================================================================
 * MEMORY STATISTICS
 * ============================================================================
 */

size_t compact_number_memory_usage(const CompactNumber* number) {
    if (!number) return 0;
    
    size_t usage = sizeof(CompactNumber);
    usage += number->capacity * sizeof(CompactVector);
    
    return usage;
}

double compact_number_reduction_ratio(
    const CompactNumber* number,
    size_t traditional_size
) {
    if (!number || traditional_size == 0) return 0.0;
    
    size_t compact_size = compact_number_memory_usage(number);
    
    return (double)traditional_size / (double)compact_size;
}