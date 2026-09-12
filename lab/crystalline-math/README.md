# Crystalline Math Library

⚠️ **CRITICAL: ALL WORK ON THIS LIBRARY MUST USE THE 'audit' FEATURE BRANCH** ⚠️

## Overview

The Crystalline Math Library is a **self-contained, production-quality mathematical library** designed to completely replace `math.h` and provide arbitrary precision arithmetic. It is the **absolute core** of the entire Crystalline system and must be treated with tremendous care and patience.

## Design Principles

1. **Self-Contained**: No dependencies on `math.h` or external math libraries (except OpenSSL for cryptographic operations)
2. **Arbitrary Precision**: Support for both double precision and arbitrary precision (bigfixed, bigint)
3. **Deterministic**: Prime generation based on clock lattice geometry, not probabilistic methods
4. **Production Quality**: Clean, well-organized code with standardized naming conventions
5. **Performance**: Optimized implementations, SIMD where appropriate
6. **Thread-Safe**: All operations designed for concurrent use

## Directory Structure

```
math/
├── include/math/          # Public API headers
│   ├── arithmetic.h       # Basic arithmetic operations
│   ├── transcendental.h   # Transcendental functions (sqrt, sin, cos, etc.)
│   ├── rounding.h         # Rounding and utility functions
│   ├── bigfixed.h         # Arbitrary precision fixed-point
│   ├── bigint.h           # Arbitrary precision integer
│   ├── clock_lattice.h    # Clock lattice geometry
│   ├── prime.h            # Prime number operations
│   ├── lattice.h          # Lattice geometry
│   └── crypto.h           # Cryptographic wrappers (OpenSSL)
├── src/
│   ├── core/              # Core arithmetic and transcendental functions
│   ├── bigfixed/          # Arbitrary precision fixed-point implementation
│   ├── bigint/            # Arbitrary precision integer implementation
│   ├── geometry/          # Geometric operations (lattice, clock)
│   ├── prime/             # Prime number generation and validation
│   └── crypto/            # Cryptographic operations
├── tests/                 # Unit and integration tests
├── docs/                  # Documentation
├── Makefile              # Build system
└── README.md             # This file
```

## Naming Conventions

### Module Prefixes

- `math_*` - Basic double-precision arithmetic and transcendental functions
- `bigfixed_*` - Arbitrary precision fixed-point operations
- `bigint_*` - Arbitrary precision integer operations
- `lattice_*` - Geometric lattice operations
- `clock_*` - Clock lattice operations (deterministic prime generation)
- `prime_*` - Prime number specific operations
- `crypto_*` - Cryptographic operations (OpenSSL wrappers)

### Function Naming Pattern

```
<module>_<operation>[_<variant>]
```

Examples:
- `math_sqrt(double x)` - Double precision square root
- `bigfixed_sqrt(BigFixed* result, const BigFixed* x)` - Arbitrary precision square root
- `clock_position_to_prime(const ClockPosition* pos)` - Convert clock position to prime
- `prime_nth(uint64_t n)` - Get the nth prime number

### NO Ridiculous Naming

❌ **WRONG**: `prime_sqrt()` (confusing - is it sqrt of a prime? sqrt using primes?)
✅ **CORRECT**: `math_sqrt()` for double, `bigfixed_sqrt()` for arbitrary precision

## Core Modules

### 1. Core Arithmetic (`math/src/core/arithmetic.c`)

Basic double-precision arithmetic operations:
- Addition, subtraction, multiplication, division
- Absolute value, min, max
- Modulo operations

### 2. Transcendental Functions (`math/src/core/transcendental.c`)

Double-precision transcendental functions:
- Square root, cube root, power
- Exponential and logarithms (natural, base-10, base-2)
- Trigonometric functions (sin, cos, tan, asin, acos, atan, atan2)
- Hyperbolic functions (sinh, cosh, tanh)

### 3. Rounding Functions (`math/src/core/rounding.c`)

Rounding and utility functions:
- floor, ceil, round, trunc
- fmod, remainder

### 4. BigFixed Arbitrary Precision (`math/src/bigfixed/`)

Fixed-point arbitrary precision arithmetic:
- Configurable precision
- All arithmetic operations
- All transcendental functions
- Conversion to/from double

### 5. BigInt Arbitrary Precision (`math/src/bigint/`)

Integer arbitrary precision arithmetic:
- Unlimited precision integers
- All arithmetic operations
- Modular arithmetic
- GCD, modular exponentiation
- Conversion to/from int64

### 6. Clock Lattice (`math/src/geometry/clock_lattice.c`)

Deterministic prime generation based on Babylonian clock structure:
- Ring structure: 12, 60, 60, 100
- Map primes to clock positions
- Map clock positions to primes
- Validate positions
- Navigate to next/previous prime

### 7. Prime Numbers (`math/src/prime/prime_generation.c`)

Prime number operations using clock lattice:
- Get nth prime deterministically
- Next/previous prime
- Prime validation (using clock position)
- Count primes below n

**NO LEGACY CODE**: No trial division, no sieving, no probabilistic tests

### 8. Lattice Geometry (`math/src/geometry/lattice.c`)

Geometric operations on lattice structures:
- Distance calculations
- Sphere mapping
- Stereographic projection

### 9. Cryptographic Wrappers (`math/src/crypto/`)

Clean wrappers around OpenSSL for cryptographic operations:
- BigNum operations
- Elliptic curve operations
- ECDSA operations
- Hash functions

## Key Features

### Deterministic Prime Generation

The library uses a revolutionary **clock lattice** approach for prime generation:

1. **Clock Structure**: Babylonian rings (12, 60, 60, 100)
2. **Position Mapping**: Each prime maps to a unique position on the clock
3. **Deterministic**: Given position, can compute prime; given prime, can compute position
4. **No Testing**: Structure IS validation - no trial division needed

This replaces ALL legacy prime generation methods (sieving, trial division, probabilistic tests).

### Arbitrary Precision

Two types of arbitrary precision:

1. **BigFixed**: Fixed-point with configurable precision
   - Best for: Transcendental functions, scientific computing
   - Precision: User-defined decimal places
   - Performance: Faster than BigInt for fixed precision

2. **BigInt**: Unlimited precision integers
   - Best for: Cryptography, number theory
   - Precision: Limited only by memory
   - Performance: Optimized for integer operations

### Precision Hierarchy

```
float (DEPRECATED) → double → bigfixed → bigint
```

- **Never use float**: Use double for standard precision
- **Use bigfixed**: When you need more precision than double
- **Use bigint**: For integer-only operations requiring unlimited precision

## Usage Examples

### Basic Arithmetic

```c
#include "math/arithmetic.h"
#include "math/transcendental.h"

double x = 2.0;
double y = math_sqrt(x);           // Square root
double z = math_pow(x, 3.0);       // Power
double w = math_sin(x);            // Sine
```

### Arbitrary Precision

```c
#include "math/bigfixed.h"

// Create bigfixed with 50 decimal places precision
BigFixed* x = bigfixed_from_double(2.0, 50);
BigFixed* result = bigfixed_new(50);

// Compute square root with 50 decimal places
bigfixed_sqrt(result, x);

// Convert back to double
double approx = bigfixed_to_double(result);

bigfixed_free(x);
bigfixed_free(result);
```

### Prime Generation

```c
#include "math/prime.h"

// Get the 1000th prime number
uint64_t p = prime_nth(1000);

// Get next prime after 1000
uint64_t next = prime_next(1000);

// Check if number is prime
if (prime_is_prime(1009)) {
    // It's prime
}
```

### Clock Lattice

```c
#include "math/clock_lattice.h"

ClockContext ctx;
clock_init(&ctx);

// Map prime to clock position
ClockPosition pos;
clock_map_prime_to_position(17, &pos);

// Map position back to prime
uint64_t prime = clock_position_to_prime(&pos);
```

## Building

```bash
cd math
make clean
make -j$(nproc)
make test
```

This produces:
- `lib/libmath.a` - Static library
- `lib/libmath.so` - Shared library

## Testing

```bash
make test
```

Runs comprehensive test suite covering:
- All arithmetic operations
- All transcendental functions
- Arbitrary precision operations
- Prime generation
- Clock lattice operations
- Edge cases and error handling

## Performance

Target performance:
- **Double precision**: Within 10% of `math.h`
- **Arbitrary precision**: Competitive with GMP
- **Prime generation**: O(1) deterministic vs O(n log log n) sieve

Optimizations:
- SIMD instructions where applicable
- Lookup tables for common values
- Inline functions for hot paths
- Cache-friendly data structures

## Dependencies

**Internal**: None (self-contained)

**External**:
- OpenSSL (only for cryptographic operations in `crypto` module)
- Standard C library (stdlib, string, etc.)

**Explicitly NOT used**:
- `math.h` - We replace this
- GMP - We provide our own arbitrary precision

## Migration from Old Code

### Replacing math.h

```c
// OLD
#include <math.h>
double y = sqrt(x);

// NEW
#include "math/transcendental.h"
double y = math_sqrt(x);
```

### Replacing "prime_" prefix

```c
// OLD (confusing naming)
#include "prime_math.h"
double y = prime_sqrt(x);

// NEW (clear naming)
#include "math/transcendental.h"
double y = math_sqrt(x);
```

### Replacing float with double

```c
// OLD
float x = 1.0f;
float y = sqrtf(x);

// NEW
double x = 1.0;
double y = math_sqrt(x);
```

## Design Philosophy

### 1. Clarity Over Cleverness

Code should be:
- Easy to read and understand
- Well-commented
- Self-documenting through clear naming

### 2. Correctness Over Speed

First make it correct, then make it fast:
- Comprehensive tests
- Edge case handling
- Proper error reporting

### 3. Consistency Over Convenience

Maintain consistent patterns:
- Naming conventions
- Error handling
- API design

### 4. Self-Contained Over Dependencies

Minimize external dependencies:
- Implement our own algorithms
- Don't rely on external math libraries
- Only use OpenSSL for crypto (where necessary)

## Contributing

When adding new functions:

1. **Choose the right module**: arithmetic, transcendental, bigfixed, etc.
2. **Follow naming conventions**: `<module>_<operation>`
3. **Add tests**: Unit tests for all new functions
4. **Document**: Add to header with clear description
5. **Verify**: No math.h dependencies, no float usage

## Status

**Current Phase**: Initial development
**Branch**: audit (REQUIRED)
**Completion**: 0%

---

**Created**: December 10, 2024  
**Last Updated**: December 10, 2024  
**Maintainer**: Crystalline Team