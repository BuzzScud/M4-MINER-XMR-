/**
 * @file geometric_number.h
 * @brief DEPRECATED - Geometric Number System
 * 
 * ⚠️ THIS FILE IS DEPRECATED ⚠️
 * 
 * The geometric number implementations (GeometricNumber64, BabylonianNumber, 
 * GeometricNumberBig) have been superseded by the CrystallineAbacus system.
 * 
 * All functionality previously provided by this module is now available
 * through the CrystallineAbacus API in math/abacus.h
 * 
 * MIGRATION GUIDE:
 * ----------------
 * 
 * Old Code:
 *   GeometricNumber64 num = geo64_from_int(42, 12);
 *   geo64_add(&result, &a, &b);
 * 
 * New Code:
 *   CrystallineAbacus* num = abacus_from_uint64(42, 12);
 *   abacus_add(result, a, b);
 *   abacus_free(num);
 * 
 * BENEFITS OF CRYSTALLINE ABACUS:
 * --------------------------------
 * 
 * 1. TRUE O(1) operations for values < 2^64 (fast path)
 * 2. Arbitrary precision for larger values
 * 3. Automatic base conversion (any base 2-256)
 * 4. Integrated with entire math library
 * 5. Better memory management
 * 6. More comprehensive test coverage
 * 7. Full transcendental function support
 * 
 * REMOVED IMPLEMENTATIONS:
 * ------------------------
 * 
 * - geometric_number_64.c (removed 2024-12-15)
 * - geometric_number_babylon.c (removed 2024-12-15)
 * 
 * These files were never used in the codebase and have been removed
 * to reduce maintenance burden and code complexity.
 * 
 * For all geometric arithmetic needs, use:
 *   #include "math/abacus.h"
 * 
 * @see math/abacus.h for the current implementation
 * @see math/transcendental.h for transcendental functions
 * @see math/platonic_clock.h for geometric coordinate generation
 */

#ifndef MATH_GEOMETRIC_NUMBER_H
#define MATH_GEOMETRIC_NUMBER_H

#warning "geometric_number.h is deprecated. Use math/abacus.h instead."

/* This header is kept for backward compatibility only.
 * All functionality has been moved to CrystallineAbacus.
 * 
 * Include the new header instead:
 */
#include "math/abacus.h"

#endif /* MATH_GEOMETRIC_NUMBER_H */