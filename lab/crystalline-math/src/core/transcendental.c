/**
 * @file transcendental.c
 * @brief Transcendental functions implementation (double precision)
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * Self-contained implementations of transcendental functions.
 * No dependencies on math.h or external libraries.
 * 
 * Implementations use:
 * - Taylor series expansions
 * - Newton-Raphson iteration
 * - CORDIC algorithms
 * - Range reduction techniques
 * - Lookup tables with interpolation
 */

#include "math/transcendental.h"
#include "math/arithmetic.h"
#include "math/cordic.h"
#include "math/constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================
 */

/* Cache for powers of 3 (common in lattice formulas) */
static double pow3_cache[100] = {0};
static bool pow3_cache_initialized = false;

static void init_pow3_cache(void) {
    if (pow3_cache_initialized) return;
    
    /* Pre-compute 3^x for x in [0.0, 10.0] with 0.1 step */
    for (int i = 0; i < 100; i++) {
        double exp = (double)i / 10.0;
        /* Bootstrap: compute 3^exp using series */
        double ln3 = 1.0986122886681098;  /* ln(3) */
        double power = exp * ln3;
        
        /* exp(power) using Taylor series */
        double result = 1.0;
        double term = 1.0;
        for (int n = 1; n < 20; n++) {
            term *= power / (double)n;
            result += term;
            if (math_abs(term) < 1e-15) break;
        }
        
        pow3_cache[i] = result;
    }
    
    pow3_cache_initialized = true;
}

/* ============================================================================
 * POWER AND ROOT FUNCTIONS
 * ============================================================================
 */

double math_sqrt(double x) {
    if (x < 0.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    if (x == 0.0 || x == 1.0) {
        return x;
    }
    
    /* Newton-Raphson iteration: x_{n+1} = (x_n + a/x_n) / 2 */
    double guess = x / 2.0;
    
    for (int i = 0; i < 20; i++) {
        double new_guess = (guess + x / guess) * 0.5;
        if (math_abs(new_guess - guess) < 1e-15) {
            break;
        }
        guess = new_guess;
    }
    
    return guess;
}

double math_cbrt(double x) {
    if (x == 0.0) return 0.0;
    
    /* Handle negative values */
    bool negative = (x < 0.0);
    if (negative) x = -x;
    
    /* Newton-Raphson for cube root: x_{n+1} = (2*x_n + a/x_n²) / 3 */
    double guess = x / 3.0;
    
    for (int i = 0; i < 20; i++) {
        double guess_sq = guess * guess;
        double new_guess = (2.0 * guess + x / guess_sq) / 3.0;
        if (math_abs(new_guess - guess) < 1e-15) {
            break;
        }
        guess = new_guess;
    }
    
    return negative ? -guess : guess;
}

double math_pow(double x, double y) {
    /* Special cases */
    if (y == 0.0) return 1.0;
    if (y == 1.0) return x;
    if (x == 0.0) return (y > 0.0) ? 0.0 : (0.0 / 0.0);  /* NaN for 0^negative */
    if (x == 1.0) return 1.0;
    
    /* Handle negative base with integer exponent */
    if (x < 0.0) {
        int int_y = (int)y;
        if ((double)int_y == y) {
            /* Integer exponent - use binary exponentiation */
            return math_powi(x, int_y);
        }
        return 0.0 / 0.0;  /* NaN for negative base with non-integer exponent */
    }
    
    /* Fast path: Powers of 3 (very common in lattice formulas) */
    if (x == 3.0 && y >= 0.0 && y <= 10.0) {
        if (!pow3_cache_initialized) {
            init_pow3_cache();
        }
        
        int cache_idx = (int)(y * 10.0);
        if (cache_idx >= 0 && cache_idx < 100) {
            double frac = (y * 10.0) - cache_idx;
            if (frac < 0.01 || cache_idx == 99) {
                return pow3_cache[cache_idx];
            } else {
                /* Linear interpolation */
                double v1 = pow3_cache[cache_idx];
                double v2 = pow3_cache[cache_idx + 1];
                return v1 + frac * (v2 - v1);
            }
        }
    }
    
    /* Fast path: Powers of 2 with integer exponent */
    if (x == 2.0 && y == (int)y && y >= 0.0 && y < 64.0) {
        return (double)(1ULL << (int)y);
    }
    
    /* General case: x^y = exp(y * log(x)) */
    return math_exp(y * math_log(x));
}

double math_powi(double x, int n) {
    if (n == 0) return 1.0;
    if (n == 1) return x;
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;
    
    /* Handle negative exponent */
    bool negative_exp = (n < 0);
    if (negative_exp) {
        n = -n;
        x = 1.0 / x;
    }
    
    /* Binary exponentiation */
    double result = 1.0;
    double base = x;
    
    while (n > 0) {
        if (n & 1) {
            result *= base;
        }
        base *= base;
        n >>= 1;
    }
    
    return result;
}

/* ============================================================================
 * EXPONENTIAL AND LOGARITHM FUNCTIONS
 * ============================================================================
 */

double math_exp(double x) {
    /* Handle extreme values */
    if (x > 709.0) return 1.7976931348623157e+308;  /* Max double */
    if (x < -709.0) return 0.0;
    if (x == 0.0) return 1.0;
    
    /* For large |x|, use exp(x) = exp(x/2)² */
    if (x > 10.0 || x < -10.0) {
        double half = math_exp(x * 0.5);
        return half * half;
    }
    
    /* Taylor series: e^x = 1 + x + x²/2! + x³/3! + ... */
    double result = 1.0;
    double term = 1.0;
    
    for (int n = 1; n < 30; n++) {
        term *= x / (double)n;
        result += term;
        if (math_abs(term) < 1e-15) break;
    }
    
    return result;
}

double math_expm1(double x) {
    /* For small x, use Taylor series directly to avoid cancellation */
    if (math_abs(x) < 1e-5) {
        double result = x;
        double term = x;
        
        for (int n = 2; n < 20; n++) {
            term *= x / (double)n;
            result += term;
            if (math_abs(term) < 1e-15) break;
        }
        
        return result;
    }
    
    /* For larger x, use exp(x) - 1 */
    return math_exp(x) - 1.0;
}

double math_log(double x) {
    if (x < 0.0) {
        return 0.0 / 0.0;  /* NaN for negative numbers */
    }
    if (x == 0.0) {
        return -1.0 / 0.0;  /* -Inf for log(0) */
    }
    if (x == 1.0) return 0.0;
    
    /* Reduce to range [0.5, 1.5] using ln(a*b) = ln(a) + ln(b) */
    int exp_adjust = 0;
    while (x > 1.5) {
        x *= 0.5;
        exp_adjust++;
    }
    while (x < 0.5) {
        x *= 2.0;
        exp_adjust--;
    }
    
    /* Compute ln(x) using ln(1+y) where y = x-1 */
    /* Series: ln(1+y) = y - y²/2 + y³/3 - y⁴/4 + ... */
    double y = x - 1.0;
    double result = 0.0;
    double term = y;
    
    for (int n = 1; n < 30; n++) {
        result += term / (double)n;
        term *= -y;
        if (math_abs(term) < 1e-15) break;
    }
    
    /* Add back adjustment: ln(x * 2^n) = ln(x) + n*ln(2) */
    const double LN2 = 0.69314718055994530942;
    result += (double)exp_adjust * LN2;
    
    return result;
}

double math_log1p(double x) {
    /* For small x, use series directly */
    if (math_abs(x) < 0.5) {
        double result = 0.0;
        double term = x;
        
        for (int n = 1; n < 30; n++) {
            result += term / (double)n;
            term *= -x;
            if (math_abs(term) < 1e-15) break;
        }
        
        return result;
    }
    
    /* For larger x, use log(1+x) */
    return math_log(1.0 + x);
}

double math_log10(double x) {
    if (x <= 0.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    
    /* log₁₀(x) = ln(x) / ln(10) */
    const double LN10 = 2.30258509299404568402;
    return math_log(x) / LN10;
}

double math_log2(double x) {
    if (x <= 0.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    
    /* log₂(x) = ln(x) / ln(2) */
    const double LN2 = 0.69314718055994530942;
    return math_log(x) / LN2;
}

/* ============================================================================
 * TRIGONOMETRIC FUNCTIONS
 * ============================================================================
 */

double math_sin(double x) {
    /* Reduce to range [-π, π] */
    const double two_pi = 2.0 * MATH_PI;
    while (x > MATH_PI) x -= two_pi;
    while (x < -MATH_PI) x += two_pi;
    
    /* Taylor series: sin(x) = x - x³/3! + x⁵/5! - x⁷/7! + ... */
    double result = x;
    double term = x;
    double x_squared = x * x;
    
    for (int n = 1; n < 15; n++) {
        term *= -x_squared / ((double)(2*n) * (double)(2*n + 1));
        result += term;
        if (math_abs(term) < 1e-15) break;
    }
    
    return result;
}

double math_cos(double x) {
    /* Reduce to range [-π, π] */
    const double two_pi = 2.0 * MATH_PI;
    while (x > MATH_PI) x -= two_pi;
    while (x < -MATH_PI) x += two_pi;
    
    /* Taylor series: cos(x) = 1 - x²/2! + x⁴/4! - x⁶/6! + ... */
    double result = 1.0;
    double term = 1.0;
    double x_squared = x * x;
    
    for (int n = 1; n < 15; n++) {
        term *= -x_squared / ((double)(2*n - 1) * (double)(2*n));
        result += term;
        if (math_abs(term) < 1e-15) break;
    }
    
    return result;
}

double math_tan(double x) {
    double cos_x = math_cos(x);
    if (math_abs(cos_x) < 1e-15) {
        return (x > 0.0) ? 1e308 : -1e308;  /* Large value for undefined */
    }
    return math_sin(x) / cos_x;
}

void math_sincos(double x, double *sin_x, double *cos_x) {
    /* More efficient to compute both simultaneously */
    /* Reduce to range [-π, π] */
    const double two_pi = 2.0 * MATH_PI;
    while (x > MATH_PI) x -= two_pi;
    while (x < -MATH_PI) x += two_pi;
    
    double x_squared = x * x;
    
    /* Compute sin */
    double sin_result = x;
    double sin_term = x;
    for (int n = 1; n < 15; n++) {
        sin_term *= -x_squared / ((double)(2*n) * (double)(2*n + 1));
        sin_result += sin_term;
        if (math_abs(sin_term) < 1e-15) break;
    }
    
    /* Compute cos */
    double cos_result = 1.0;
    double cos_term = 1.0;
    for (int n = 1; n < 15; n++) {
        cos_term *= -x_squared / ((double)(2*n - 1) * (double)(2*n));
        cos_result += cos_term;
        if (math_abs(cos_term) < 1e-15) break;
    }
    
    *sin_x = sin_result;
    *cos_x = cos_result;
}

/* ============================================================================
 * INVERSE TRIGONOMETRIC FUNCTIONS
 * ============================================================================
 */

double math_asin(double x) {
    if (x < -1.0 || x > 1.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    if (x == 0.0) return 0.0;
    if (x == 1.0) return MATH_PI / 2.0;
    if (x == -1.0) return -MATH_PI / 2.0;
    
    /* For |x| close to 1, use asin(x) = π/2 - acos(x) */
    if (math_abs(x) > 0.7) {
        double result = MATH_PI / 2.0 - math_acos(x);
        return (x < 0.0) ? -result : result;
    }
    
    /* Taylor series: asin(x) = x + x³/6 + 3x⁵/40 + ... */
    double result = x;
    double term = x;
    double x_squared = x * x;
    
    for (int n = 1; n < 20; n++) {
        term *= x_squared * (2.0*n - 1.0) / (2.0*n);
        result += term / (2.0*n + 1.0);
        if (math_abs(term) < 1e-15) break;
    }
    
    return result;
}

double math_acos(double x) {
    if (x < -1.0 || x > 1.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    if (x == 1.0) return 0.0;
    if (x == -1.0) return MATH_PI;
    if (x == 0.0) return MATH_PI / 2.0;
    
    /* Use identity: acos(x) = π/2 - asin(x) */
    return MATH_PI / 2.0 - math_asin(x);
}

double math_atan(double x) {
    if (x == 0.0) return 0.0;
    
    /* Handle special cases for better accuracy */
    if (x == 1.0) return MATH_PI / 4.0;
    if (x == -1.0) return -MATH_PI / 4.0;
    
    /* For large |x|, use atan(x) = π/2 - atan(1/x) */
    if (x > 1.0) {
        return MATH_PI / 2.0 - math_atan(1.0 / x);
    }
    if (x < -1.0) {
        return -MATH_PI / 2.0 - math_atan(1.0 / x);
    }
    
    /* For |x| > 0.5, use argument reduction for better convergence
     * atan(x) = 2*atan(x/(1+sqrt(1+x²))) */
    if (math_abs(x) > 0.5) {
        double sqrt_term = math_sqrt(1.0 + x * x);
        double reduced = x / (1.0 + sqrt_term);
        return 2.0 * math_atan(reduced);
    }
    
    /* Taylor series for |x| <= 0.5: atan(x) = x - x³/3 + x⁵/5 - x⁷/7 + ... */
    double result = x;
    double term = x;
    double x_squared = x * x;
    
    for (int n = 1; n < 50; n++) {
        term *= -x_squared;
        double next_term = term / (double)(2*n + 1);
        result += next_term;
        if (math_abs(next_term) < 1e-16) break;
    }
    
    return result;
}

double math_atan2(double y, double x) {
    /* Handle special cases */
    if (x == 0.0 && y == 0.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    
    if (x > 0.0) {
        return math_atan(y / x);
    } else if (x < 0.0) {
        if (y >= 0.0) {
            return math_atan(y / x) + MATH_PI;
        } else {
            return math_atan(y / x) - MATH_PI;
        }
    } else {  /* x == 0 */
        return (y > 0.0) ? (MATH_PI / 2.0) : (-MATH_PI / 2.0);
    }
}

/* ============================================================================
 * HYPERBOLIC FUNCTIONS
 * ============================================================================
 */

double math_sinh(double x) {
    if (x == 0.0) return 0.0;
    
    /* sinh(x) = (e^x - e^(-x)) / 2 */
    if (math_abs(x) > 20.0) {
        /* For large |x|, e^(-x) is negligible */
        return (x > 0.0) ? (math_exp(x) / 2.0) : (-math_exp(-x) / 2.0);
    }
    
    double exp_x = math_exp(x);
    double exp_neg_x = math_exp(-x);
    return (exp_x - exp_neg_x) / 2.0;
}

double math_cosh(double x) {
    /* cosh(x) = (e^x + e^(-x)) / 2 */
    if (math_abs(x) > 20.0) {
        /* For large |x|, e^(-|x|) is negligible */
        return math_exp(math_abs(x)) / 2.0;
    }
    
    double exp_x = math_exp(x);
    double exp_neg_x = math_exp(-x);
    return (exp_x + exp_neg_x) / 2.0;
}

double math_tanh(double x) {
    if (x > 20.0) return 1.0;
    if (x < -20.0) return -1.0;
    if (x == 0.0) return 0.0;
    
    /* tanh(x) = (e^x - e^(-x)) / (e^x + e^(-x)) */
    double exp_x = math_exp(x);
    double exp_neg_x = math_exp(-x);
    return (exp_x - exp_neg_x) / (exp_x + exp_neg_x);
}

double math_asinh(double x) {
    /* asinh(x) = ln(x + √(x² + 1)) */
    return math_log(x + math_sqrt(x * x + 1.0));
}

double math_acosh(double x) {
    if (x < 1.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    
    /* acosh(x) = ln(x + √(x² - 1)) */
    return math_log(x + math_sqrt(x * x - 1.0));
}

double math_atanh(double x) {
    if (x <= -1.0 || x >= 1.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    
    /* atanh(x) = 0.5 * ln((1+x)/(1-x)) */
    return 0.5 * math_log((1.0 + x) / (1.0 - x));
}

/* ============================================================================
 * MODULO FUNCTIONS
 * ============================================================================
 */

/* Note: math_floor, math_ceil, math_round, math_trunc are defined in arithmetic.c
 * and included via arithmetic.h. We use them here but don't redefine them. */

double math_fmod(double x, double y) {
    if (y == 0.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    
    /* fmod(x, y) = x - n*y where n = trunc(x/y) */
    double n = math_trunc(x / y);
    return x - n * y;
}

double math_remainder(double x, double y) {
    if (y == 0.0) {
        return 0.0 / 0.0;  /* NaN */
    }
    
    /* IEEE remainder: x - n*y where n = round(x/y) */
    double n = math_round(x / y);
    return x - n * y;
}

/* ============================================================================
 * ABACUS TRANSCENDENTAL FUNCTIONS (ARBITRARY PRECISION)
 * ============================================================================
 */

/**
 * @brief Helper: Properly copy abacus including min_exponent
 */
static MathError copy_abacus_complete(CrystallineAbacus* dest, const CrystallineAbacus* src) {
    if (!dest || !src) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Ensure capacity - use realloc if needed
    if (dest->capacity < src->num_beads) {
        AbacusBead* new_beads = (AbacusBead*)realloc(
            dest->beads,
            src->num_beads * sizeof(AbacusBead)
        );
        if (!new_beads) {
            return MATH_ERROR_OUT_OF_MEMORY;
        }
        dest->beads = new_beads;
        dest->capacity = src->num_beads;
    }
    
    // Copy all fields
    memcpy(dest->beads, src->beads, src->num_beads * sizeof(AbacusBead));
    dest->num_beads = src->num_beads;
    dest->negative = src->negative;
    dest->min_exponent = src->min_exponent;
    dest->base = src->base;
    
    return MATH_SUCCESS;
}

/**
 * @brief Square root with Abacus input/output
 * 
 * Wrapper around existing abacus_sqrt with precision control.
 */
MathError math_sqrt_abacus(CrystallineAbacus* result,
                           const CrystallineAbacus* x,
                           uint32_t precision) {
    if (!result || !x) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Always use double as intermediate for now to get fractional results
    // TODO: Implement pure Abacus Newton-Raphson for arbitrary precision
    double x_double;
    MathError err = abacus_to_double(x, &x_double);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    if (x_double < 0.0) {
        return MATH_ERROR_DOMAIN;
    }
    
    double sqrt_val = math_sqrt(x_double);
    
    // Convert back to Abacus
    CrystallineAbacus* temp = abacus_from_double(sqrt_val, result->base, precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy to result
    err = copy_abacus_complete(result, temp);
    abacus_free(temp);
    
    return err;
}

/**
 * @brief Sine function with Abacus input/output
 * 
 * Currently uses double precision as intermediate step.
 * TODO: Implement pure Abacus Taylor series for true arbitrary precision.
 */
MathError math_sin_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision) {
    if (!result || !x) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Convert to double
    double x_val;
    MathError err = abacus_to_double(x, &x_val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Compute sin
    double sin_val = math_sin(x_val);
    
    // Convert back to Abacus
    CrystallineAbacus* temp = abacus_from_double(sin_val, result->base, (int32_t)precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy completely
    err = copy_abacus_complete(result, temp);
    
    abacus_free(temp);
    return err;
}

/**
 * @brief Cosine function with Abacus input/output
 * 
 * Currently uses double precision as intermediate step.
 * TODO: Implement pure Abacus Taylor series for true arbitrary precision.
 */
MathError math_cos_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision) {
    if (!result || !x) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Convert to double
    double x_val;
    MathError err = abacus_to_double(x, &x_val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Compute cos
    double cos_val = math_cos(x_val);
    
    // Convert back to Abacus
    CrystallineAbacus* temp = abacus_from_double(cos_val, result->base, (int32_t)precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy completely
    err = copy_abacus_complete(result, temp);
    
    abacus_free(temp);
    return err;
}

/**
 * @brief Compute sine and cosine simultaneously with Abacus
 */
MathError math_sincos_abacus(CrystallineAbacus* sin_result,
                             CrystallineAbacus* cos_result,
                             const CrystallineAbacus* x,
                             uint32_t precision) {
    if (!sin_result || !cos_result || !x) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    MathError err = math_sin_abacus(sin_result, x, precision);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    return math_cos_abacus(cos_result, x, precision);
}

/**
 * @brief Two-argument arctangent with Abacus input/output
 * 
 * Currently uses double precision as intermediate step.
 * TODO: Implement pure Abacus CORDIC algorithm for true arbitrary precision.
 */
MathError math_atan2_abacus(CrystallineAbacus* result,
                            const CrystallineAbacus* y,
                            const CrystallineAbacus* x,
                            uint32_t precision) {
    if (!result || !y || !x) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Convert to double
    double x_val, y_val;
    MathError err = abacus_to_double(x, &x_val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    err = abacus_to_double(y, &y_val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Compute atan2
    double atan2_val = math_atan2(y_val, x_val);
    
    // Convert back to Abacus
    CrystallineAbacus* temp = abacus_from_double(atan2_val, result->base, (int32_t)precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy completely
    err = copy_abacus_complete(result, temp);
    
    abacus_free(temp);
    return err;
}

/**
 * @brief Natural logarithm with Abacus input/output
 * 
 * Currently uses double precision as intermediate step.
 * TODO: Implement pure Abacus Taylor series for true arbitrary precision.
 */
MathError math_log_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision) {
    if (!result || !x) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Convert to double
    double x_val;
    MathError err = abacus_to_double(x, &x_val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    if (x_val <= 0.0) {
        return MATH_ERROR_DOMAIN;
    }
    
    // Compute log
    double log_val = math_log(x_val);
    
    // Convert back to Abacus
    CrystallineAbacus* temp = abacus_from_double(log_val, result->base, (int32_t)precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy completely
    err = copy_abacus_complete(result, temp);
    
    abacus_free(temp);
    return err;
}

/**
 * @brief Exponential function with Abacus input/output
 * 
 * Currently uses double precision as intermediate step.
 * TODO: Implement pure Abacus Taylor series for true arbitrary precision.
 */
MathError math_exp_abacus(CrystallineAbacus* result,
                          const CrystallineAbacus* x,
                          uint32_t precision) {
    if (!result || !x) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    // Convert to double
    double x_val;
    MathError err = abacus_to_double(x, &x_val);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    // Compute exp
    double exp_val = math_exp(x_val);
    
    // Convert back to Abacus
    CrystallineAbacus* temp = abacus_from_double(exp_val, result->base, (int32_t)precision);
    if (!temp) {
        return MATH_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy completely
    err = copy_abacus_complete(result, temp);
    
    abacus_free(temp);
    return err;
}
