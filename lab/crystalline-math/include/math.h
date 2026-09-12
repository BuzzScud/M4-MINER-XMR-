/**
 * @file math.h
 * @brief Crystalline Math Library - Main Header
 * 
 * ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️
 * 
 * This is the main header for the Crystalline Math Library.
 * Include this file to access all mathematical functions.
 * 
 * The Crystalline Math Library is a self-contained, production-quality
 * mathematical library designed to completely replace <math.h> and provide
 * arbitrary precision arithmetic.
 * 
 * Design Principles:
 * - Self-contained (no math.h dependencies)
 * - Pure crystalline clock lattice geometry
 * - Deterministic prime generation (clock lattice)
 * - Clean, standardized API
 * - Production quality code
 * - Thread-safe operations
 * 
 * Module Organization:
 * - arithmetic.h: Basic double-precision arithmetic
 * - transcendental.h: Transcendental functions (sqrt, sin, cos, log, exp)
 * - abacus.h: Arbitrary precision using crystalline clock lattice
 * - ntt.h: Number Theoretic Transform for fast multiplication
 * - clock.h: Clock lattice for deterministic operations
 * - prime.h: Prime number operations
 
 * 
 * Usage:
 * 
 * Basic math:
 *   #include <math.h>  // OLD
 *   #include "math.h"  // NEW
 *   
 *   double y = sqrt(x);      // OLD
 *   double y = math_sqrt(x); // NEW
 * 
 * Arbitrary precision (Crystalline Abacus):
 *   CrystallineAbacus* x = abacus_new();
 *   abacus_from_uint64(x, 12345);
 *   CrystallineAbacus* result = abacus_new();
 *   abacus_mul(result, x, x);
 * 
 * Prime numbers:
 *   uint64_t p = prime_nth(1000);  // Get 1000th prime
 *   bool is_prime = prime_is_prime(1009);
 */

#ifndef CRYSTALLINE_MATH_H
#define CRYSTALLINE_MATH_H

/* Core types */
#include "math/types.h"

/* Basic arithmetic */
#include "math/arithmetic.h"

/* Transcendental functions */
#include "math/transcendental.h"

/* Arbitrary precision - Use Crystalline Abacus instead */

/* Clock lattice and prime numbers */
#include "math/clock.h"
#include "math/prime.h"
#include "math/rainbow.h"

/* Crystalline Abacus */
#include "math/abacus.h"

/* Number Theoretic Transform */
#include "math/ntt.h"

#endif /* CRYSTALLINE_MATH_H */