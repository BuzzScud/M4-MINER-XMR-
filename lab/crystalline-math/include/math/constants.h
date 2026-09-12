/**
 * @file constants.h
 * @brief Unified mathematical constants for Crystalline Math Library
 * 
 * This is the SINGLE authoritative source for all mathematical constants.
 * All other files should include this header and use these definitions.
 * 
 * DO NOT define constants elsewhere - add them here!
 */

#ifndef MATH_CONSTANTS_H
#define MATH_CONSTANTS_H

/* ============================================================================
 * FUNDAMENTAL MATHEMATICAL CONSTANTS
 * ============================================================================ */

// π (Pi) - Circle constant
#define MATH_PI 3.14159265358979323846
#define MATH_TWO_PI (2.0 * MATH_PI)
#define MATH_PI_OVER_2 (MATH_PI / 2.0)
#define MATH_PI_OVER_4 (MATH_PI / 4.0)
#define MATH_PI_OVER_6 (MATH_PI / 6.0)

// φ (Phi) - Golden ratio
#define MATH_PHI 1.61803398874989484820
#define MATH_PHI_SQUARED (MATH_PHI * MATH_PHI)
#define MATH_PHI_INVERSE (1.0 / MATH_PHI)

// e (Euler's number)
#define MATH_E 2.71828182845904523536
#define MATH_LN2 0.69314718055994530942
#define MATH_LN10 2.30258509299404568402
#define MATH_LOG2E 1.44269504088896340736
#define MATH_LOG10E 0.43429448190325182765

// Square roots
#define MATH_SQRT2 1.41421356237309504880
#define MATH_SQRT3 1.73205080756887729353
#define MATH_SQRT5 2.23606797749978969641
#define MATH_SQRT_PI 1.77245385090551602730
#define MATH_SQRT_TWO_PI 2.50662827463100050242

// Infinity and NaN
#define MATH_INFINITY (__builtin_inf())
#define MATH_NEG_INFINITY (-__builtin_inf())
#define MATH_NAN (__builtin_nan(""))

/* ============================================================================
 * BABYLONIAN MATHEMATICS CONSTANTS
 * ============================================================================ */

// Babylonian π approximation (π ≈ 3)
#define MATH_PI_BABYLONIAN 3.0

// Zu Chongzhi π approximation (π dust)
#define MATH_PI_DUST_NUM 355
#define MATH_PI_DUST_DEN 113
#define MATH_PI_DUST (355.0 / 113.0)  // ≈ 3.14159292035398

// Fibonacci φ approximation
#define MATH_PHI_FIBONACCI_NUM 89
#define MATH_PHI_FIBONACCI_DEN 55
#define MATH_PHI_FIBONACCI (89.0 / 55.0)  // ≈ 1.618181818...

// π × φ (Critical relationship discovered by user)
#define MATH_PI_TIMES_PHI (MATH_PI * MATH_PHI)  // ≈ 5.08318...

/* ============================================================================
 * CLOCK LATTICE CONSTANTS
 * ============================================================================ */

// Clock ring structure (Babylonian time system)
#define CLOCK_POSITIONS 12
#define CLOCK_RING_1_POSITIONS 60  // Minutes
#define CLOCK_RING_2_POSITIONS 60  // Seconds  
#define CLOCK_RING_3_POSITIONS 100 // Milliseconds

// Complete clock cycle (12 × 60 × 60 × 100)
#define CLOCK_CYCLE_TOTAL 4320000

// Prime positions on clock (mod 12)
#define PRIME_POSITION_3 3   // mod 12 = 5, base = 5
#define PRIME_POSITION_6 6   // mod 12 = 7, base = 7
#define PRIME_POSITION_9 9   // mod 12 = 11, base = 11

// Prime bases for each position
#define PRIME_BASE_POSITION_3 5
#define PRIME_BASE_POSITION_6 7
#define PRIME_BASE_POSITION_9 11

/* ============================================================================
 * CYMATIC CONSTANTS (Vibrational Frequencies)
 * ============================================================================ */

#define CYMATIC_432_HZ 432.0   // 432 Hz base frequency (Verdi tuning)
#define CYMATIC_528_HZ 528.0   // 528 Hz healing frequency
#define CYMATIC_963_HZ 963.0   // 963 Hz spirit frequency
#define SCHUMANN_RESONANCE 7.83 // Earth's resonance frequency

/* ============================================================================
 * ASTRONOMICAL CONSTANTS
 * ============================================================================ */

#define SAROS_CYCLE 223        // Eclipse cycle (223 synodic months)
#define METONIC_CYCLE 235      // Lunar-solar sync (19 years = 235 months)
#define METONIC_YEARS 19       // 19-year Metonic cycle
#define LUNAR_MONTH 29.53059   // Synodic month in days

/* ============================================================================
 * SACRED GEOMETRY CONSTANTS
 * ============================================================================ */

#define VECTOR_CULMINATION 144000  // 3 × 12³ × (250/9)
#define EINSTEIN_LAMBDA (3.0 / 144000.0)  // Cosmological constant correction

// Platonic solid counts
#define PLATONIC_TETRAHEDRON_VERTICES 4
#define PLATONIC_TETRAHEDRON_EDGES 6
#define PLATONIC_TETRAHEDRON_FACES 4

#define PLATONIC_CUBE_VERTICES 8
#define PLATONIC_CUBE_EDGES 12
#define PLATONIC_CUBE_FACES 6

#define PLATONIC_OCTAHEDRON_VERTICES 6
#define PLATONIC_OCTAHEDRON_EDGES 12
#define PLATONIC_OCTAHEDRON_FACES 8

#define PLATONIC_DODECAHEDRON_VERTICES 20
#define PLATONIC_DODECAHEDRON_EDGES 30
#define PLATONIC_DODECAHEDRON_FACES 12

#define PLATONIC_ICOSAHEDRON_VERTICES 12
#define PLATONIC_ICOSAHEDRON_EDGES 30
#define PLATONIC_ICOSAHEDRON_FACES 20

/* ============================================================================
 * QUADRATURE CONSTANTS (90° Phase Relationships)
 * ============================================================================ */

#define QUADRATURE_ANGLE (MATH_PI / 2.0)  // 90° in radians
#define QUADRATURE_POSITIONS 3             // Positions 3, 6, 9 are in quadrature

/* ============================================================================
 * DIMENSIONAL FREQUENCIES (φᵢ from lattice formula)
 * ============================================================================ */

#define PHI_FREQUENCIES_COUNT 12
// Dimensional frequencies: [3, 7, 31, 12, 19, 5, 11, 13, 17, 23, 29, ...]
// These interact with angular position θ in the lattice formula

/* ============================================================================
 * TWIN PRIMES (Vector culmination already defined above)
 * ============================================================================ */

#define TWIN_PRIME_LOWER 143999ULL    // Lower twin prime
#define TWIN_PRIME_UPPER 144001ULL    // Upper twin prime

#endif // MATH_CONSTANTS_H