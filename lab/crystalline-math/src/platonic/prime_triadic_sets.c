/**
 * @file prime_triadic_sets.c
 * @brief Prime Triadic Set Generation for Polytope Discovery
 */

#include "math/prime_triadic_sets.h"
#include "math/prime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Check if a number is prime (simple check)
 */
static bool is_prime_simple(uint32_t n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    
    for (uint32_t i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

/**
 * @brief Check if prime is in excluded list
 */
static bool is_excluded(uint32_t prime, const uint32_t* excluded, uint32_t num_excluded) {
    for (uint32_t i = 0; i < num_excluded; i++) {
        if (prime == excluded[i]) return true;
    }
    return false;
}

/**
 * @brief Get list of available primes (not excluded)
 */
static uint32_t* get_available_primes(
    const uint32_t* excluded,
    uint32_t num_excluded,
    uint32_t max_prime,
    uint32_t* out_count
) {
    // Count available primes
    uint32_t count = 0;
    for (uint32_t p = 2; p <= max_prime; p++) {
        if (is_prime_simple(p) && !is_excluded(p, excluded, num_excluded)) {
            count++;
        }
    }
    
    if (count == 0) {
        *out_count = 0;
        return NULL;
    }
    
    // Allocate and fill array
    uint32_t* primes = (uint32_t*)malloc(count * sizeof(uint32_t));
    if (!primes) {
        *out_count = 0;
        return NULL;
    }
    
    uint32_t idx = 0;
    for (uint32_t p = 2; p <= max_prime; p++) {
        if (is_prime_simple(p) && !is_excluded(p, excluded, num_excluded)) {
            primes[idx++] = p;
        }
    }
    
    *out_count = count;
    return primes;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool prime_triadic_is_excluded(
    uint32_t prime,
    uint32_t dimension,
    uint32_t num_sets
) {
    // Always exclude 3 (triadic structure)
    if (prime == 3) return true;
    
    // Exclude dimension if prime
    if (prime == dimension && is_prime_simple(dimension)) return true;
    
    // Exclude num_sets if prime
    if (prime == num_sets && is_prime_simple(num_sets)) return true;
    
    return false;
}

bool prime_triadic_validate_set(
    const TriadicSet* set,
    const uint32_t* excluded_primes,
    uint32_t num_excluded
) {
    if (!set) return false;
    
    // Check all 3 members are prime
    for (int i = 0; i < 3; i++) {
        if (!is_prime_simple(set->primes[i])) {
            return false;
        }
    }
    
    // Check no duplicates
    if (set->primes[0] == set->primes[1] ||
        set->primes[0] == set->primes[2] ||
        set->primes[1] == set->primes[2]) {
        return false;
    }
    
    // Check none are excluded
    for (int i = 0; i < 3; i++) {
        if (is_excluded(set->primes[i], excluded_primes, num_excluded)) {
            return false;
        }
    }
    
    return true;
}

bool prime_triadic_validate_all(const PrimeTriadicSets* sets) {
    if (!sets || !sets->sets) return false;
    
    // Validate each set
    for (uint32_t i = 0; i < sets->num_sets; i++) {
        if (!prime_triadic_validate_set(&sets->sets[i], 
                                        sets->excluded_primes, 
                                        sets->num_excluded)) {
            return false;
        }
    }
    
    // Check no overlaps between sets
    for (uint32_t i = 0; i < sets->num_sets; i++) {
        for (uint32_t j = i + 1; j < sets->num_sets; j++) {
            for (int a = 0; a < 3; a++) {
                for (int b = 0; b < 3; b++) {
                    if (sets->sets[i].primes[a] == sets->sets[j].primes[b]) {
                        return false; // Overlap found
                    }
                }
            }
        }
    }
    
    return true;
}

// ============================================================================
// GENERATION
// ============================================================================

PrimeTriadicSets* prime_triadic_generate(const TriadicSetConfig* config) {
    if (!config || config->num_sets == 0) return NULL;
    
    // Build excluded primes list
    uint32_t excluded[10];
    uint32_t num_excluded = 0;
    
    // Always exclude 3
    excluded[num_excluded++] = 3;
    
    // Exclude dimension if prime
    if (is_prime_simple(config->dimension)) {
        excluded[num_excluded++] = config->dimension;
    }
    
    // Exclude num_sets if prime
    if (is_prime_simple(config->num_sets)) {
        excluded[num_excluded++] = config->num_sets;
    }
    
    return prime_triadic_generate_custom(
        config->num_sets,
        excluded,
        num_excluded,
        config->max_prime
    );
}

PrimeTriadicSets* prime_triadic_generate_custom(
    uint32_t num_sets,
    const uint32_t* excluded_primes,
    uint32_t num_excluded,
    uint32_t max_prime
) {
    if (num_sets == 0) return NULL;
    
    // Get available primes
    uint32_t num_available;
    uint32_t* available = get_available_primes(
        excluded_primes,
        num_excluded,
        max_prime,
        &num_available
    );
    
    if (!available || num_available < num_sets * 3) {
        free(available);
        return NULL; // Not enough primes
    }
    
    // Allocate structure
    PrimeTriadicSets* sets = (PrimeTriadicSets*)calloc(1, sizeof(PrimeTriadicSets));
    if (!sets) {
        free(available);
        return NULL;
    }
    
    // Allocate sets array
    sets->sets = (TriadicSet*)calloc(num_sets, sizeof(TriadicSet));
    if (!sets->sets) {
        free(available);
        free(sets);
        return NULL;
    }
    
    sets->num_sets = num_sets;
    
    // Copy excluded primes
    sets->excluded_primes = (uint32_t*)malloc(num_excluded * sizeof(uint32_t));
    if (sets->excluded_primes) {
        memcpy(sets->excluded_primes, excluded_primes, num_excluded * sizeof(uint32_t));
        sets->num_excluded = num_excluded;
    }
    
    // Fill sets with consecutive primes
    uint32_t prime_idx = 0;
    for (uint32_t s = 0; s < num_sets; s++) {
        for (int i = 0; i < 3; i++) {
            if (prime_idx < num_available) {
                sets->sets[s].primes[i] = available[prime_idx++];
            }
        }
        sets->sets[s].is_valid = true;
    }
    
    // Create flattened bases array
    sets->num_bases = num_sets * 3;
    sets->all_bases = (uint32_t*)malloc(sets->num_bases * sizeof(uint32_t));
    if (sets->all_bases) {
        uint32_t idx = 0;
        for (uint32_t s = 0; s < num_sets; s++) {
            for (int i = 0; i < 3; i++) {
                sets->all_bases[idx++] = sets->sets[s].primes[i];
            }
        }
    }
    
    free(available);
    return sets;
}

// ============================================================================
// UTILITIES
// ============================================================================

uint32_t* prime_triadic_get_all_bases(
    const PrimeTriadicSets* sets,
    uint32_t* out_num_bases
) {
    if (!sets || !out_num_bases) return NULL;
    
    *out_num_bases = sets->num_bases;
    
    if (!sets->all_bases) return NULL;
    
    // Return copy
    uint32_t* bases = (uint32_t*)malloc(sets->num_bases * sizeof(uint32_t));
    if (bases) {
        memcpy(bases, sets->all_bases, sets->num_bases * sizeof(uint32_t));
    }
    
    return bases;
}

void prime_triadic_print(const PrimeTriadicSets* sets) {
    if (!sets) {
        printf("(null)\n");
        return;
    }
    
    printf("Prime Triadic Sets:\n");
    printf("  Dimension: %u\n", sets->dimension);
    printf("  Number of sets: %u\n", sets->num_sets);
    printf("  Total bases: %u\n", sets->num_bases);
    
    printf("  Excluded primes: {");
    for (uint32_t i = 0; i < sets->num_excluded; i++) {
        printf("%u", sets->excluded_primes[i]);
        if (i < sets->num_excluded - 1) printf(", ");
    }
    printf("}\n");
    
    printf("  Sets:\n");
    for (uint32_t s = 0; s < sets->num_sets; s++) {
        printf("    Set %u: {%u, %u, %u} - %s\n",
               s + 1,
               sets->sets[s].primes[0],
               sets->sets[s].primes[1],
               sets->sets[s].primes[2],
               sets->sets[s].is_valid ? "Valid" : "Invalid");
    }
    
    printf("  All bases: {");
    for (uint32_t i = 0; i < sets->num_bases; i++) {
        printf("%u", sets->all_bases[i]);
        if (i < sets->num_bases - 1) printf(", ");
    }
    printf("}\n");
}

void prime_triadic_print_exclusions(uint32_t dimension, uint32_t num_sets) {
    printf("Exclusion Analysis for %uD with %u sets:\n", dimension, num_sets);
    
    printf("  Excluded primes:\n");
    printf("    - 3 (triadic structure)\n");
    
    if (is_prime_simple(dimension)) {
        printf("    - %u (dimension is prime)\n", dimension);
    } else {
        printf("    - %u (dimension, but not prime - OK to use)\n", dimension);
    }
    
    if (is_prime_simple(num_sets)) {
        printf("    - %u (number of sets is prime)\n", num_sets);
    } else {
        printf("    - %u (number of sets, but not prime - OK to use)\n", num_sets);
    }
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void prime_triadic_free(PrimeTriadicSets* sets) {
    if (!sets) return;
    
    free(sets->sets);
    free(sets->excluded_primes);
    free(sets->all_bases);
    free(sets);
}

// ============================================================================
// PRESETS
// ============================================================================

PrimeTriadicSets* prime_triadic_preset_5d(void) {
    TriadicSetConfig config = {
        .dimension = 5,
        .num_sets = 2,
        .max_prime = 100
    };
    
    PrimeTriadicSets* sets = prime_triadic_generate(&config);
    if (sets) {
        sets->dimension = 5;
    }
    return sets;
}

PrimeTriadicSets* prime_triadic_preset_6d(void) {
    TriadicSetConfig config = {
        .dimension = 6,
        .num_sets = 2,
        .max_prime = 100
    };
    
    PrimeTriadicSets* sets = prime_triadic_generate(&config);
    if (sets) {
        sets->dimension = 6;
    }
    return sets;
}

PrimeTriadicSets* prime_triadic_preset_7d(void) {
    TriadicSetConfig config = {
        .dimension = 7,
        .num_sets = 2,
        .max_prime = 100
    };
    
    PrimeTriadicSets* sets = prime_triadic_generate(&config);
    if (sets) {
        sets->dimension = 7;
    }
    return sets;
}

PrimeTriadicSets* prime_triadic_preset_classical_3d(void) {
    // Classical: {2, 3, 5} - but this violates our constraint!
    // For 3D, we don't exclude anything except the triadic structure
    // But 3 IS the triadic structure, so we can't use it
    // This is a special case - classical solids predate the constraint
    
    PrimeTriadicSets* sets = (PrimeTriadicSets*)calloc(1, sizeof(PrimeTriadicSets));
    if (!sets) return NULL;
    
    sets->num_sets = 1;
    sets->dimension = 3;
    sets->num_bases = 3;
    
    sets->sets = (TriadicSet*)calloc(1, sizeof(TriadicSet));
    if (!sets->sets) {
        free(sets);
        return NULL;
    }
    
    // Classical set (violates constraint, but historical)
    sets->sets[0].primes[0] = 2;
    sets->sets[0].primes[1] = 3;
    sets->sets[0].primes[2] = 5;
    sets->sets[0].is_valid = true;
    
    sets->all_bases = (uint32_t*)malloc(3 * sizeof(uint32_t));
    if (sets->all_bases) {
        sets->all_bases[0] = 2;
        sets->all_bases[1] = 3;
        sets->all_bases[2] = 5;
    }
    
    sets->excluded_primes = (uint32_t*)malloc(sizeof(uint32_t));
    if (sets->excluded_primes) {
        sets->excluded_primes[0] = 3;
        sets->num_excluded = 1;
    }
    
    return sets;
}

PrimeTriadicSets* prime_triadic_preset_4d(void) {
    // 4D: Use {2, 3, 5, 7, 11, 13}
    // This is 2 sets: {2, 3, 5} and {7, 11, 13}
    // But violates constraint (uses 2 and 3)
    // Historical/transitional case
    
    PrimeTriadicSets* sets = (PrimeTriadicSets*)calloc(1, sizeof(PrimeTriadicSets));
    if (!sets) return NULL;
    
    sets->num_sets = 2;
    sets->dimension = 4;
    sets->num_bases = 6;
    
    sets->sets = (TriadicSet*)calloc(2, sizeof(TriadicSet));
    if (!sets->sets) {
        free(sets);
        return NULL;
    }
    
    // Set 1: {2, 3, 5}
    sets->sets[0].primes[0] = 2;
    sets->sets[0].primes[1] = 3;
    sets->sets[0].primes[2] = 5;
    sets->sets[0].is_valid = true;
    
    // Set 2: {7, 11, 13}
    sets->sets[1].primes[0] = 7;
    sets->sets[1].primes[1] = 11;
    sets->sets[1].primes[2] = 13;
    sets->sets[1].is_valid = true;
    
    sets->all_bases = (uint32_t*)malloc(6 * sizeof(uint32_t));
    if (sets->all_bases) {
        sets->all_bases[0] = 2;
        sets->all_bases[1] = 3;
        sets->all_bases[2] = 5;
        sets->all_bases[3] = 7;
        sets->all_bases[4] = 11;
        sets->all_bases[5] = 13;
    }
    
    sets->excluded_primes = (uint32_t*)malloc(2 * sizeof(uint32_t));
    if (sets->excluded_primes) {
        sets->excluded_primes[0] = 2;
        sets->excluded_primes[1] = 3;
        sets->num_excluded = 2;
    }
    
    return sets;
}