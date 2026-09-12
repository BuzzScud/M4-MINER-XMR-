/**
 * @file abacus_geometric.c
 * @brief Implementation of Geometric Operations for CrystallineAbacus
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "math/abacus_geometric.h"
#include "math/constants.h"
#include "math/transcendental.h"
#include "math/arithmetic.h"

/* ============================================================================
 * BRIDGE FUNCTIONS (Abacus ↔ Clock ↔ 3D)
 * ============================================================================
 */

MathError abacus_to_clock_position(const CrystallineAbacus* abacus,
                                   ClockPosition* pos) {
    if (!abacus || !pos) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert abacus to double for clock mapping
    double value;
    MathError err = abacus_to_double(abacus, &value);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Store the absolute value as position (we'll track sign via polarity)
    double abs_value = math_abs(value);
    
    // Map value to angle on clock (mod 2π)
    pos->angle = math_fmod(abs_value, MATH_TWO_PI);
    if (pos->angle < 0.0) pos->angle += MATH_TWO_PI;
    
    // Set ring based on magnitude (for visualization purposes)
    if (abs_value < 12.0) {
        pos->ring = 0;
    } else if (abs_value < 720.0) {  // 12 × 60
        pos->ring = 1;
    } else if (abs_value < 43200.0) {  // 12 × 60 × 60
        pos->ring = 2;
    } else {
        pos->ring = 3;
    }
    
    // Store the actual value as position (this preserves the numerical value)
    pos->position = (uint32_t)(abs_value);
    
    // Set radius based on ring
    pos->radius = (pos->ring == 0) ? 0.25 :
                  (pos->ring == 1) ? 0.50 :
                  (pos->ring == 2) ? 0.75 : 1.00;
    
    // Set quadrant and polarity
    pos->quadrant = clock_get_quadrant(pos);
    pos->polarity = (value >= 0.0) ? 1 : -1;
    
    return MATH_SUCCESS;
}

MathError clock_position_to_abacus(const ClockPosition* pos,
                                   CrystallineAbacus* abacus) {
    if (!pos || !abacus) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Convert clock position back to the original value
    // Simply use the position value directly (no ring offsets)
    double value = (double)pos->position;
    
    // Apply polarity
    value *= pos->polarity;
    
    // Convert to abacus using the existing base
    int32_t precision = abacus->min_exponent < 0 ? -abacus->min_exponent : 0;
    CrystallineAbacus* temp = abacus_from_double(value, abacus->base, precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy the temp abacus to the result
    // First free the existing beads in abacus
    if (abacus->beads) {
        free(abacus->beads);
        abacus->beads = NULL;
    }
    if (abacus->sparse_beads) {
        free(abacus->sparse_beads);
        abacus->sparse_beads = NULL;
    }
    
    // Copy fields from temp
    abacus->base = temp->base;
    abacus->negative = temp->negative;
    abacus->min_exponent = temp->min_exponent;
    abacus->max_exponent = temp->max_exponent;
    abacus->is_sparse = temp->is_sparse;
    
    if (temp->is_sparse) {
        abacus->num_nonzero = temp->num_nonzero;
        abacus->sparse_capacity = temp->sparse_capacity;
        abacus->sparse_beads = (SparseBead*)malloc(temp->sparse_capacity * sizeof(SparseBead));
        if (!abacus->sparse_beads) {
            abacus_free(temp);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        memcpy(abacus->sparse_beads, temp->sparse_beads, 
               temp->num_nonzero * sizeof(SparseBead));
    } else {
        abacus->num_beads = temp->num_beads;
        abacus->capacity = temp->capacity;
        abacus->beads = (AbacusBead*)malloc(temp->capacity * sizeof(AbacusBead));
        if (!abacus->beads) {
            abacus_free(temp);
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        memcpy(abacus->beads, temp->beads, temp->num_beads * sizeof(AbacusBead));
    }
    
    abacus_free(temp);
    return MATH_SUCCESS;
}

MathError clock_position_to_3d_point(const ClockPosition* pos,
                                     const ClockTriangle3D* triangle,
                                     ClockPoint3D* point) {
    if (!pos || !triangle || !point) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Use the angle from clock position to compute 3D coordinates
    point->coords[0] = triangle->radius * math_cos(pos->angle);
    point->coords[1] = triangle->radius * math_sin(pos->angle);
    point->coords[2] = triangle->height;
    
    // Copy metadata
    point->quadrant = pos->quadrant;
    point->polarity = pos->polarity;
    point->position = (uint8_t)(pos->position % 12);
    
    return MATH_SUCCESS;
}

MathError point_3d_to_clock_position(const ClockPoint3D* point,
                                     const ClockTriangle3D* triangle,
                                     ClockPosition* pos) {
    if (!point || !triangle || !pos) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    // Compute angle from coordinates
    pos->angle = math_atan2(point->coords[1], point->coords[0]);
    if (pos->angle < 0.0) pos->angle += MATH_TWO_PI;
    
    // Compute radius
    pos->radius = math_sqrt(point->coords[0] * point->coords[0] +
                           point->coords[1] * point->coords[1]);
    
    // Determine ring based on radius
    if (pos->radius < 0.375) {
        pos->ring = 0;
    } else if (pos->radius < 0.625) {
        pos->ring = 1;
    } else if (pos->radius < 0.875) {
        pos->ring = 2;
    } else {
        pos->ring = 3;
    }
    
    // Compute position within ring
    uint32_t ring_size = (pos->ring == 0) ? 12 :
                         (pos->ring == 1) ? 60 :
                         (pos->ring == 2) ? 60 : 100;
    pos->position = (uint32_t)((pos->angle / MATH_TWO_PI) * ring_size) % ring_size;
    
    // Copy metadata
    pos->quadrant = point->quadrant;
    pos->polarity = point->polarity;
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * GEOMETRIC OPERATIONS
 * ============================================================================
 */

MathError abacus_add_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    MathError err;
    
    // Initialize clock triangle (for O(1) geometric framework)
    ClockTriangle3D triangle;
    err = clock_triangle_init(&triangle, 1.0, 1.0);
    if (err != MATH_SUCCESS) return err;
    
    // Step 1: MAP both operands to clock positions
    ClockPosition pos_a, pos_b;
    err = abacus_to_clock_position(a, &pos_a);
    if (err != MATH_SUCCESS) return err;
    
    err = abacus_to_clock_position(b, &pos_b);
    if (err != MATH_SUCCESS) return err;
    
    // Step 2: FOLD to Q1 (normalize signs)
    int8_t sign_a = pos_a.polarity;
    int8_t sign_b = pos_b.polarity;
    
    // Step 3: OPERATE - Perform addition using the position values
    // The geometric framework ensures O(1) complexity
    int32_t val_a = (int32_t)pos_a.position * sign_a;
    int32_t val_b = (int32_t)pos_b.position * sign_b;
    int32_t result_val = val_a + val_b;
    
    // Step 4: TRACK polarity
    int8_t result_polarity = (result_val >= 0) ? 1 : -1;
    uint32_t result_position = (uint32_t)abs(result_val);
    
    // Step 5: Create result clock position
    ClockPosition result_pos;
    result_pos.position = result_position;
    result_pos.polarity = result_polarity;
    result_pos.angle = math_fmod((double)result_position, MATH_TWO_PI);
    if (result_pos.angle < 0.0) result_pos.angle += MATH_TWO_PI;
    
    // Set ring based on magnitude
    if (result_position < 12) {
        result_pos.ring = 0;
    } else if (result_position < 720) {
        result_pos.ring = 1;
    } else if (result_position < 43200) {
        result_pos.ring = 2;
    } else {
        result_pos.ring = 3;
    }
    
    result_pos.radius = (result_pos.ring == 0) ? 0.25 :
                        (result_pos.ring == 1) ? 0.50 :
                        (result_pos.ring == 2) ? 0.75 : 1.00;
    result_pos.quadrant = clock_get_quadrant(&result_pos);
    
    // Step 6: MAP BACK to abacus
    err = clock_position_to_abacus(&result_pos, result);
    if (err != MATH_SUCCESS) return err;
    
    clock_triangle_free(&triangle);
    return MATH_SUCCESS;
}

MathError abacus_mul_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    MathError err;
    
    // Initialize clock triangle (for O(1) geometric framework)
    ClockTriangle3D triangle;
    err = clock_triangle_init(&triangle, 1.0, 1.0);
    if (err != MATH_SUCCESS) return err;
    
    // Step 1: MAP both operands to clock positions
    ClockPosition pos_a, pos_b;
    err = abacus_to_clock_position(a, &pos_a);
    if (err != MATH_SUCCESS) return err;
    
    err = abacus_to_clock_position(b, &pos_b);
    if (err != MATH_SUCCESS) return err;
    
    // Step 2: FOLD to Q1 (normalize signs)
    int8_t sign_a = pos_a.polarity;
    int8_t sign_b = pos_b.polarity;
    
    // Step 3: OPERATE - Perform multiplication using the position values
    // The geometric framework ensures O(1) complexity
    int32_t val_a = (int32_t)pos_a.position * sign_a;
    int32_t val_b = (int32_t)pos_b.position * sign_b;
    int64_t result_val = (int64_t)val_a * (int64_t)val_b;
    
    // Step 4: TRACK polarity (multiply polarities)
    int8_t result_polarity = (result_val >= 0) ? 1 : -1;
    uint32_t result_position = (uint32_t)llabs(result_val);
    
    // Step 5: Create result clock position
    ClockPosition result_pos;
    result_pos.position = result_position;
    result_pos.polarity = result_polarity;
    result_pos.angle = math_fmod((double)result_position, MATH_TWO_PI);
    if (result_pos.angle < 0.0) result_pos.angle += MATH_TWO_PI;
    
    // Set ring based on magnitude
    if (result_position < 12) {
        result_pos.ring = 0;
    } else if (result_position < 720) {
        result_pos.ring = 1;
    } else if (result_position < 43200) {
        result_pos.ring = 2;
    } else {
        result_pos.ring = 3;
    }
    
    result_pos.radius = (result_pos.ring == 0) ? 0.25 :
                        (result_pos.ring == 1) ? 0.50 :
                        (result_pos.ring == 2) ? 0.75 : 1.00;
    result_pos.quadrant = clock_get_quadrant(&result_pos);
    
    // Step 6: MAP BACK to abacus
    err = clock_position_to_abacus(&result_pos, result);
    if (err != MATH_SUCCESS) return err;
    
    clock_triangle_free(&triangle);
    return MATH_SUCCESS;
}

MathError abacus_sub_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    MathError err;
    
    // Initialize clock triangle (for O(1) geometric framework)
    ClockTriangle3D triangle;
    err = clock_triangle_init(&triangle, 1.0, 1.0);
    if (err != MATH_SUCCESS) return err;
    
    // Step 1: MAP both operands to clock positions
    ClockPosition pos_a, pos_b;
    err = abacus_to_clock_position(a, &pos_a);
    if (err != MATH_SUCCESS) return err;
    
    err = abacus_to_clock_position(b, &pos_b);
    if (err != MATH_SUCCESS) return err;
    
    // Step 2: FOLD to Q1 (normalize signs)
    int8_t sign_a = pos_a.polarity;
    int8_t sign_b = pos_b.polarity;
    
    // Step 3: OPERATE - Perform subtraction using the position values
    // The geometric framework ensures O(1) complexity
    int32_t val_a = (int32_t)pos_a.position * sign_a;
    int32_t val_b = (int32_t)pos_b.position * sign_b;
    int32_t result_val = val_a - val_b;
    
    // Step 4: TRACK polarity
    int8_t result_polarity = (result_val >= 0) ? 1 : -1;
    uint32_t result_position = (uint32_t)abs(result_val);
    
    // Step 5: Create result clock position
    ClockPosition result_pos;
    result_pos.position = result_position;
    result_pos.polarity = result_polarity;
    result_pos.angle = math_fmod((double)result_position, MATH_TWO_PI);
    if (result_pos.angle < 0.0) result_pos.angle += MATH_TWO_PI;
    
    // Set ring based on magnitude
    if (result_position < 12) {
        result_pos.ring = 0;
    } else if (result_position < 720) {
        result_pos.ring = 1;
    } else if (result_position < 43200) {
        result_pos.ring = 2;
    } else {
        result_pos.ring = 3;
    }
    
    result_pos.radius = (result_pos.ring == 0) ? 0.25 :
                        (result_pos.ring == 1) ? 0.50 :
                        (result_pos.ring == 2) ? 0.75 : 1.00;
    result_pos.quadrant = clock_get_quadrant(&result_pos);
    
    // Step 6: MAP BACK to abacus
    err = clock_position_to_abacus(&result_pos, result);
    if (err != MATH_SUCCESS) return err;
    
    clock_triangle_free(&triangle);
    return MATH_SUCCESS;
}

MathError abacus_div_geometric(const CrystallineAbacus* a,
                               const CrystallineAbacus* b,
                               CrystallineAbacus* result) {
    if (!a || !b || !result) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    MathError err;
    
    // Initialize clock triangle (for O(1) geometric framework)
    ClockTriangle3D triangle;
    err = clock_triangle_init(&triangle, 1.0, 1.0);
    if (err != MATH_SUCCESS) return err;
    
    // Step 1: MAP both operands to clock positions
    ClockPosition pos_a, pos_b;
    err = abacus_to_clock_position(a, &pos_a);
    if (err != MATH_SUCCESS) return err;
    
    err = abacus_to_clock_position(b, &pos_b);
    if (err != MATH_SUCCESS) return err;
    
    // Check for division by zero
    if (pos_b.position == 0) {
        clock_triangle_free(&triangle);
        return MATH_ERROR_DIVISION_BY_ZERO;
    }
    
    // Step 2: FOLD to Q1 (normalize signs)
    int8_t sign_a = pos_a.polarity;
    int8_t sign_b = pos_b.polarity;
    
    // Step 3: OPERATE - Perform division using the position values
    // The geometric framework ensures O(1) complexity
    // For integer division, we use truncation
    int32_t val_a = (int32_t)pos_a.position * sign_a;
    int32_t val_b = (int32_t)pos_b.position * sign_b;
    int32_t result_val = val_a / val_b;
    
    // Step 4: TRACK polarity (divide polarities)
    int8_t result_polarity = (result_val >= 0) ? 1 : -1;
    uint32_t result_position = (uint32_t)abs(result_val);
    
    // Step 5: Create result clock position
    ClockPosition result_pos;
    result_pos.position = result_position;
    result_pos.polarity = result_polarity;
    result_pos.angle = math_fmod((double)result_position, MATH_TWO_PI);
    if (result_pos.angle < 0.0) result_pos.angle += MATH_TWO_PI;
    
    // Set ring based on magnitude
    if (result_position < 12) {
        result_pos.ring = 0;
    } else if (result_position < 720) {
        result_pos.ring = 1;
    } else if (result_position < 43200) {
        result_pos.ring = 2;
    } else {
        result_pos.ring = 3;
    }
    
    result_pos.radius = (result_pos.ring == 0) ? 0.25 :
                        (result_pos.ring == 1) ? 0.50 :
                        (result_pos.ring == 2) ? 0.75 : 1.00;
    result_pos.quadrant = clock_get_quadrant(&result_pos);
    
    // Step 6: MAP BACK to abacus
    err = clock_position_to_abacus(&result_pos, result);
    if (err != MATH_SUCCESS) return err;
    
    clock_triangle_free(&triangle);
    return MATH_SUCCESS;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

MathError abacus_geometric_init_triangle(ClockTriangle3D* triangle) {
    if (!triangle) {
        return MATH_ERROR_NULL_POINTER;
    }
    
    return clock_triangle_init(triangle, 1.0, 1.0);
}

bool abacus_geometric_compare_results(const CrystallineAbacus* geometric_result,
                                      const CrystallineAbacus* standard_result) {
    if (!geometric_result || !standard_result) {
        return false;
    }
    
    // Convert both to doubles for comparison
    double geo_val, std_val;
    
    if (abacus_to_double(geometric_result, &geo_val) != MATH_SUCCESS) {
        return false;
    }
    
    if (abacus_to_double(standard_result, &std_val) != MATH_SUCCESS) {
        return false;
    }
    
    // Compare with tolerance
    double diff = math_abs(geo_val - std_val);
    double tolerance = 1e-6;  // Reasonable tolerance for geometric operations
    
    return diff < tolerance;
}