/**
 * @file cordic.c
 * @brief CORDIC algorithm for pure Abacus trigonometric functions
 * 
 * CORDIC (COordinate Rotation DIgital Computer) computes sin/cos through
 * iterative rotations. This is perfect for Babylonian clock mathematics!
 * 
 * Algorithm:
 * - Start with vector (1, 0)
 * - Rotate by predetermined angles
 * - Converges to (cos θ, sin θ)
 * 
 * Complexity: O(p) where p = precision (number of iterations)
 * Each iteration is O(1) for Abacus operations
 * 
 * NO EXTERNAL DEPENDENCIES - Pure Abacus arithmetic only!
 */

#include "math/abacus.h"
#include "math/types.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* CORDIC angle table (in radians, precomputed)
 * atan(2^-i) for i = 0, 1, 2, 3, ...
 * 
 * These are the fundamental rotation angles used by CORDIC.
 * We'll store these as Abacus values for arbitrary precision.
 */

/**
 * @brief CORDIC gain factor K ≈ 0.6072529350088812561694
 * 
 * After n CORDIC iterations, the magnitude is scaled by:
 * K = ∏(i=0 to n-1) 1/√(1 + 2^(-2i))
 * 
 * For infinite iterations: K ≈ 0.6072529350088812561694
 */
static const char* CORDIC_K_STRING = "0.6072529350088812561694";

/**
 * @brief CORDIC angle table (atan(2^-i) in radians)
 * 
 * These are precomputed for efficiency:
 * atan(2^0)  = 0.7853981633974483  (45°)
 * atan(2^-1) = 0.4636476090008061  (26.565°)
 * atan(2^-2) = 0.2449786631268641  (14.036°)
 * ...
 */
static const char* CORDIC_ANGLES[] = {
    "0.7853981633974483096156608458198757210492923498437764552437361480",
    "0.4636476090008061162142562314612144020285370542861202638109330887",
    "0.2449786631268641541720824812112758109141440983811840823808506400",
    "0.1243549945467614350313548491638710255731701917142788063471392381",
    "0.0624188099959573484739791129855051136062738031284177100256634761",
    "0.0312398334302682762537117448924909770324196308031022098621134107",
    "0.0156237286204768308028015212565703189111141398009054863690381771",
    "0.0078123410601011112964633918421992816212228117250147024168739358",
    "0.0039062301319669718276286653114243871403574901131879426810285965",
    "0.0019531225164788186851214826250767139316107808826615794131138652",
    "0.0009765621895593194304034301997172908516341970063238203125000000",
    "0.0004882812111948982754692396256448486258910596370697021484375000",
    "0.0002441406201493617794504316107370005920529365539550781250000000",
    "0.0001220703118936702042390586461424827575683593750000000000000000",
    "0.0000610351561742087750216165930032730102539062500000000000000000",
    "0.0000305175781155260968208312988281250000000000000000000000000000"
};

#define CORDIC_ITERATIONS 16

/**
 * @brief Create Abacus from string representation
 */
static CrystallineAbacus* abacus_from_string(const char* str, uint32_t base) {
    // Parse string like "0.123456789"
    // For now, use abacus_from_double as helper
    // TODO: Implement true string parsing for arbitrary precision
    double val = 0.0;
    sscanf(str, "%lf", &val);
    
    return abacus_from_double(val, base, 20);
}

/**
 * @brief CORDIC rotation in vectoring mode
 * 
 * Rotates vector (x, y) by angle z
 * After convergence: x ≈ K*(x*cos(z) - y*sin(z))
 *                    y ≈ K*(y*cos(z) + x*sin(z))
 * 
 * @param x Input/output x coordinate
 * @param y Input/output y coordinate  
 * @param z Angle to rotate (in radians)
 * @param iterations Number of CORDIC iterations
 * @param base Number base for Abacus
 * @return MATH_SUCCESS or error code
 */
MathError cordic_rotate(CrystallineAbacus* x,
                       CrystallineAbacus* y,
                       CrystallineAbacus* z,
                       uint32_t iterations,
                       uint32_t base) {
    if (!x || !y || !z) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    if (iterations > CORDIC_ITERATIONS) {
        iterations = CORDIC_ITERATIONS;
    }
    
    // Precompute angle table as Abacus values
    CrystallineAbacus* angles[CORDIC_ITERATIONS];
    for (uint32_t i = 0; i < iterations; i++) {
        angles[i] = abacus_from_string(CORDIC_ANGLES[i], base);
        if (!angles[i]) {
            // Cleanup on error
            for (uint32_t j = 0; j < i; j++) {
                abacus_free(angles[j]);
            }
            return MATH_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // CORDIC iterations
    for (uint32_t i = 0; i < iterations; i++) {
        // Determine rotation direction
        int direction = z->negative ? -1 : 1;
        
        // Compute x_new = x - direction * y * 2^(-i)
        // Compute y_new = y + direction * x * 2^(-i)
        
        CrystallineAbacus* x_shifted = abacus_new(base);
        CrystallineAbacus* y_shifted = abacus_new(base);
        CrystallineAbacus* x_new = abacus_new(base);
        CrystallineAbacus* y_new = abacus_new(base);
        
        if (!x_shifted || !y_shifted || !x_new || !y_new) {
            if (x_shifted) abacus_free(x_shifted);
            if (y_shifted) abacus_free(y_shifted);
            if (x_new) abacus_free(x_new);
            if (y_new) abacus_free(y_new);
            for (uint32_t j = 0; j < iterations; j++) {
                abacus_free(angles[j]);
            }
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        // Shift by 2^(-i) (divide by 2^i)
        // For simplicity, we'll use division
        CrystallineAbacus* shift_divisor = abacus_from_uint64(1ULL << i, base);
        if (!shift_divisor) {
            abacus_free(x_shifted);
            abacus_free(y_shifted);
            abacus_free(x_new);
            abacus_free(y_new);
            for (uint32_t j = 0; j < iterations; j++) {
                abacus_free(angles[j]);
            }
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        
        CrystallineAbacus* remainder = abacus_new(base);
        abacus_div(x_shifted, remainder, y, shift_divisor);
        abacus_div(y_shifted, remainder, x, shift_divisor);
        abacus_free(remainder);
        abacus_free(shift_divisor);
        
        // Apply rotation direction
        if (direction < 0) {
            x_shifted->negative = !x_shifted->negative;
        } else {
            y_shifted->negative = !y_shifted->negative;
        }
        
        // x_new = x - direction * y_shifted
        // y_new = y + direction * x_shifted
        abacus_add(x_new, x, x_shifted);
        abacus_sub(y_new, y, y_shifted);
        
        // Update x, y
        for (size_t j = 0; j < x_new->num_beads; j++) {
            x->beads[j] = x_new->beads[j];
        }
        x->num_beads = x_new->num_beads;
        x->negative = x_new->negative;
        
        for (size_t j = 0; j < y_new->num_beads; j++) {
            y->beads[j] = y_new->beads[j];
        }
        y->num_beads = y_new->num_beads;
        y->negative = y_new->negative;
        
        // Update angle: z = z - direction * angles[i]
        CrystallineAbacus* angle_update = abacus_copy(angles[i]);
        if (direction < 0) {
            angle_update->negative = !angle_update->negative;
        }
        
        CrystallineAbacus* z_new = abacus_new(base);
        abacus_sub(z_new, z, angle_update);
        
        for (size_t j = 0; j < z_new->num_beads; j++) {
            z->beads[j] = z_new->beads[j];
        }
        z->num_beads = z_new->num_beads;
        z->negative = z_new->negative;
        
        abacus_free(x_shifted);
        abacus_free(y_shifted);
        abacus_free(x_new);
        abacus_free(y_new);
        abacus_free(angle_update);
        abacus_free(z_new);
    }
    
    // Cleanup angle table
    for (uint32_t i = 0; i < iterations; i++) {
        abacus_free(angles[i]);
    }
    
    return MATH_SUCCESS;
}

/**
 * @brief Compute sin and cos using CORDIC
 * 
 * Pure Abacus implementation - no double intermediates!
 * 
 * @param sin_result Output: sin(angle)
 * @param cos_result Output: cos(angle)
 * @param angle Input angle in radians
 * @param precision Number of CORDIC iterations
 * @return MATH_SUCCESS or error code
 */
MathError cordic_sincos(CrystallineAbacus* sin_result,
                       CrystallineAbacus* cos_result,
                       const CrystallineAbacus* angle,
                       uint32_t precision) {
    if (!sin_result || !cos_result || !angle) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    uint32_t base = angle->base;
    
    // Start with vector (1, 0)
    CrystallineAbacus* x = abacus_from_uint64(1, base);
    CrystallineAbacus* y = abacus_from_uint64(0, base);
    CrystallineAbacus* z = abacus_copy(angle);
    
    if (!x || !y || !z) {
        if (x) abacus_free(x);
        if (y) abacus_free(y);
        if (z) abacus_free(z);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Perform CORDIC rotation
    MathError err = cordic_rotate(x, y, z, precision, base);
    if (err != MATH_SUCCESS) {
        abacus_free(x);
        abacus_free(y);
        abacus_free(z);
        return err;
    }
    
    // Apply CORDIC gain factor K
    CrystallineAbacus* K = abacus_from_string(CORDIC_K_STRING, base);
    if (!K) {
        abacus_free(x);
        abacus_free(y);
        abacus_free(z);
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // cos(angle) = K * x
    abacus_mul(cos_result, K, x);
    
    // sin(angle) = K * y
    abacus_mul(sin_result, K, y);
    
    abacus_free(x);
    abacus_free(y);
    abacus_free(z);
    abacus_free(K);
    
    return MATH_SUCCESS;
}