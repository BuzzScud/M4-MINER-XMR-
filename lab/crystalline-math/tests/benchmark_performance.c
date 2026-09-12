/**
 * Performance Benchmark Suite
 * 
 * Comprehensive benchmarking of:
 * - Memory usage (dense vs sparse)
 * - Operation speed (addition, multiplication, exponentiation)
 * - Precision analysis
 * - Base comparison
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "math/abacus.h"
#include "math/transcendental.h"

/* Ensure math.h functions are available */
#ifndef fabs
#define fabs(x) __builtin_fabs(x)
#endif

#define ITERATIONS 1000
#define LARGE_ITERATIONS 100

// Timing utilities
typedef struct {
    clock_t start;
    clock_t end;
    double elapsed_ms;
} Timer;

void timer_start(Timer* t) {
    t->start = clock();
}

void timer_stop(Timer* t) {
    t->end = clock();
    t->elapsed_ms = ((double)(t->end - t->start)) / CLOCKS_PER_SEC * 1000.0;
}

// ============================================================================
// BENCHMARK 1: MEMORY USAGE
// ============================================================================

void benchmark_memory_usage() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARK 1: MEMORY USAGE                                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int base = 10;
    
    // Test different number sizes
    uint64_t test_values[] = {
        10ULL,                    // 10
        1000ULL,                  // 10^3
        1000000ULL,               // 10^6
        1000000000ULL,            // 10^9
        1000000000000ULL          // 10^12
    };
    
    const char* labels[] = {
        "10 (10^1)",
        "1,000 (10^3)",
        "1,000,000 (10^6)",
        "1,000,000,000 (10^9)",
        "1,000,000,000,000 (10^12)"
    };
    
    int num_tests = 5;
    
    printf("%-30s %12s %12s %12s\n", "Number", "Dense (B)", "Sparse (B)", "Reduction");
    printf("%-30s %12s %12s %12s\n", "------", "--------", "---------", "---------");
    
    for (int i = 0; i < num_tests; i++) {
        CrystallineAbacus* num = abacus_from_uint64(test_values[i], base);
        
        // Measure dense memory
        size_t dense_mem = abacus_memory_usage(num);
        
        // Sparsify and measure
        abacus_sparsify(num);
        size_t sparse_mem = abacus_memory_usage(num);
        
        double reduction = 100.0 * (1.0 - (double)sparse_mem / (double)dense_mem);
        
        printf("%-30s %12zu %12zu %11.1f%%\n", 
               labels[i], dense_mem, sparse_mem, reduction);
        
        abacus_free(num);
    }
    
    printf("\n");
}

// ============================================================================
// BENCHMARK 2: OPERATION SPEED - ADDITION
// ============================================================================

void benchmark_addition_speed() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARK 2: ADDITION SPEED                                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int base = 10;
    Timer timer;
    
    // Test 1: Small numbers (dense)
    printf("Test 1: Small Numbers (100 + 50)\n");
    CrystallineAbacus* a1 = abacus_from_uint64(100ULL, base);
    CrystallineAbacus* b1 = abacus_from_uint64(50ULL, base);
    CrystallineAbacus* result1 = abacus_new(base);
    
    timer_start(&timer);
    for (int i = 0; i < ITERATIONS; i++) {
        abacus_add(result1, a1, b1);
    }
    timer_stop(&timer);
    
    printf("  Dense: %.3f ms (%d iterations)\n", timer.elapsed_ms, ITERATIONS);
    printf("  Per operation: %.6f ms\n", timer.elapsed_ms / ITERATIONS);
    
    abacus_free(a1);
    abacus_free(b1);
    abacus_free(result1);
    
    // Test 2: Large sparse numbers
    printf("\nTest 2: Large Sparse Numbers (10^9 + 10^6)\n");
    CrystallineAbacus* a2 = abacus_from_uint64(1000000000ULL, base);
    CrystallineAbacus* b2 = abacus_from_uint64(1000000ULL, base);
    CrystallineAbacus* result2 = abacus_new(base);
    
    // Dense timing
    timer_start(&timer);
    for (int i = 0; i < ITERATIONS; i++) {
        abacus_add(result2, a2, b2);
    }
    timer_stop(&timer);
    double dense_time = timer.elapsed_ms;
    
    printf("  Dense: %.3f ms (%d iterations)\n", dense_time, ITERATIONS);
    printf("  Per operation: %.6f ms\n", dense_time / ITERATIONS);
    
    // Sparse timing
    abacus_sparsify(a2);
    abacus_sparsify(b2);
    
    timer_start(&timer);
    for (int i = 0; i < ITERATIONS; i++) {
        abacus_add(result2, a2, b2);
    }
    timer_stop(&timer);
    double sparse_time = timer.elapsed_ms;
    
    printf("  Sparse: %.3f ms (%d iterations)\n", sparse_time, ITERATIONS);
    printf("  Per operation: %.6f ms\n", sparse_time / ITERATIONS);
    printf("  Speedup: %.2fx\n", dense_time / sparse_time);
    
    abacus_free(a2);
    abacus_free(b2);
    abacus_free(result2);
    
    printf("\n");
}

// ============================================================================
// BENCHMARK 3: OPERATION SPEED - MULTIPLICATION
// ============================================================================

void benchmark_multiplication_speed() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARK 3: MULTIPLICATION SPEED                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int base = 10;
    Timer timer;
    
    // Test 1: Small numbers
    printf("Test 1: Small Numbers (100 × 50)\n");
    CrystallineAbacus* a1 = abacus_from_uint64(100ULL, base);
    CrystallineAbacus* b1 = abacus_from_uint64(50ULL, base);
    CrystallineAbacus* result1 = abacus_new(base);
    
    timer_start(&timer);
    for (int i = 0; i < ITERATIONS; i++) {
        abacus_mul(result1, a1, b1);
    }
    timer_stop(&timer);
    
    printf("  Dense: %.3f ms (%d iterations)\n", timer.elapsed_ms, ITERATIONS);
    printf("  Per operation: %.6f ms\n", timer.elapsed_ms / ITERATIONS);
    
    abacus_free(a1);
    abacus_free(b1);
    abacus_free(result1);
    
    // Test 2: Large sparse numbers
    printf("\nTest 2: Large Sparse Numbers (10^6 × 10^3)\n");
    CrystallineAbacus* a2 = abacus_from_uint64(1000000ULL, base);
    CrystallineAbacus* b2 = abacus_from_uint64(1000ULL, base);
    CrystallineAbacus* result2 = abacus_new(base);
    
    // Dense timing
    timer_start(&timer);
    for (int i = 0; i < LARGE_ITERATIONS; i++) {
        abacus_mul(result2, a2, b2);
    }
    timer_stop(&timer);
    double dense_time = timer.elapsed_ms;
    
    printf("  Dense: %.3f ms (%d iterations)\n", dense_time, LARGE_ITERATIONS);
    printf("  Per operation: %.6f ms\n", dense_time / LARGE_ITERATIONS);
    
    // Sparse timing
    abacus_sparsify(a2);
    abacus_sparsify(b2);
    
    timer_start(&timer);
    for (int i = 0; i < LARGE_ITERATIONS; i++) {
        abacus_mul(result2, a2, b2);
    }
    timer_stop(&timer);
    double sparse_time = timer.elapsed_ms;
    
    printf("  Sparse: %.3f ms (%d iterations)\n", sparse_time, LARGE_ITERATIONS);
    printf("  Per operation: %.6f ms\n", sparse_time / LARGE_ITERATIONS);
    printf("  Speedup: %.2fx\n", dense_time / sparse_time);
    
    abacus_free(a2);
    abacus_free(b2);
    abacus_free(result2);
    
    printf("\n");
}

// ============================================================================
// BENCHMARK 4: EXPONENTIATION SPEED
// ============================================================================

void benchmark_exponentiation_speed() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARK 4: EXPONENTIATION SPEED                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int base = 10;
    Timer timer;
    
    // Test different exponents
    struct {
        uint64_t base_val;
        uint64_t exponent;
        const char* label;
    } tests[] = {
        {2, 10, "2^10"},
        {2, 20, "2^20"},
        {10, 6, "10^6"},
        {10, 9, "10^9"}
    };
    
    int num_tests = 4;
    
    printf("%-15s %15s %20s\n", "Operation", "Time (ms)", "Per Op (ms)");
    printf("%-15s %15s %20s\n", "---------", "---------", "-----------");
    
    for (int i = 0; i < num_tests; i++) {
        CrystallineAbacus* a = abacus_from_uint64(tests[i].base_val, base);
        CrystallineAbacus* result = abacus_new(base);
        
        timer_start(&timer);
        for (int j = 0; j < LARGE_ITERATIONS; j++) {
            abacus_pow_uint64(result, a, tests[i].exponent);
        }
        timer_stop(&timer);
        
        printf("%-15s %15.3f %20.6f\n", 
               tests[i].label, 
               timer.elapsed_ms, 
               timer.elapsed_ms / LARGE_ITERATIONS);
        
        abacus_free(a);
        abacus_free(result);
    }
    
    printf("\n");
}

// ============================================================================
// BENCHMARK 5: TRANSCENDENTAL FUNCTIONS
// ============================================================================

void benchmark_transcendental_speed() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARK 5: TRANSCENDENTAL FUNCTIONS                      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int base = 60;
    int precision = 20;
    Timer timer;
    
    CrystallineAbacus* input = abacus_from_double(1.5, base, precision);
    CrystallineAbacus* result = abacus_new(base);
    
    // Test sqrt
    printf("Test 1: Square Root\n");
    timer_start(&timer);
    for (int i = 0; i < ITERATIONS; i++) {
        math_sqrt_abacus(result, input, precision);
    }
    timer_stop(&timer);
    printf("  Time: %.3f ms (%d iterations)\n", timer.elapsed_ms, ITERATIONS);
    printf("  Per operation: %.6f ms\n", timer.elapsed_ms / ITERATIONS);
    
    // Test sin
    printf("\nTest 2: Sine\n");
    timer_start(&timer);
    for (int i = 0; i < ITERATIONS; i++) {
        math_sin_abacus(result, input, precision);
    }
    timer_stop(&timer);
    printf("  Time: %.3f ms (%d iterations)\n", timer.elapsed_ms, ITERATIONS);
    printf("  Per operation: %.6f ms\n", timer.elapsed_ms / ITERATIONS);
    
    // Test cos
    printf("\nTest 3: Cosine\n");
    timer_start(&timer);
    for (int i = 0; i < ITERATIONS; i++) {
        math_cos_abacus(result, input, precision);
    }
    timer_stop(&timer);
    printf("  Time: %.3f ms (%d iterations)\n", timer.elapsed_ms, ITERATIONS);
    printf("  Per operation: %.6f ms\n", timer.elapsed_ms / ITERATIONS);
    
    abacus_free(input);
    abacus_free(result);
    
    printf("\n");
}

// ============================================================================
// BENCHMARK 6: BASE COMPARISON
// ============================================================================

void benchmark_base_comparison() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARK 6: BASE COMPARISON                               ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int bases[] = {10, 12, 60, 100};
    int num_bases = 4;
    Timer timer;
    
    printf("%-10s %15s %20s\n", "Base", "Time (ms)", "Per Op (ms)");
    printf("%-10s %15s %20s\n", "----", "---------", "-----------");
    
    for (int i = 0; i < num_bases; i++) {
        int base = bases[i];
        
        CrystallineAbacus* a = abacus_from_uint64(1000ULL, base);
        CrystallineAbacus* b = abacus_from_uint64(500ULL, base);
        CrystallineAbacus* result = abacus_new(base);
        
        timer_start(&timer);
        for (int j = 0; j < ITERATIONS; j++) {
            abacus_add(result, a, b);
            abacus_mul(result, a, b);
        }
        timer_stop(&timer);
        
        printf("%-10d %15.3f %20.6f\n", 
               base, 
               timer.elapsed_ms, 
               timer.elapsed_ms / (ITERATIONS * 2));
        
        abacus_free(a);
        abacus_free(b);
        abacus_free(result);
    }
    
    printf("\n");
}

// ============================================================================
// BENCHMARK 7: PRECISION ANALYSIS
// ============================================================================

void benchmark_precision_analysis() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARK 7: PRECISION ANALYSIS                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int base = 60;
    int precision = 20;
    
    // Test sqrt precision
    printf("Test 1: Square Root Precision\n");
    double test_values[] = {4.0, 9.0, 16.0, 25.0, 100.0};
    double expected[] = {2.0, 3.0, 4.0, 5.0, 10.0};
    
    printf("%-15s %15s %15s %15s\n", "Input", "Result", "Expected", "Error");
    printf("%-15s %15s %15s %15s\n", "-----", "------", "--------", "-----");
    
    for (int i = 0; i < 5; i++) {
        CrystallineAbacus* input = abacus_from_double(test_values[i], base, precision);
        CrystallineAbacus* result = abacus_new(base);
        
        math_sqrt_abacus(result, input, precision);
        
        double result_val = 0;
        abacus_to_double(result, &result_val);
        double error = fabs(result_val - expected[i]);
        
        printf("%-15.1f %15.10f %15.10f %15.2e\n", 
               test_values[i], result_val, expected[i], error);
        
        abacus_free(input);
        abacus_free(result);
    }
    
    // Test transcendental precision
    printf("\nTest 2: Transcendental Function Precision\n");
    printf("%-15s %15s %15s %15s\n", "Function", "Result", "Expected", "Error");
    printf("%-15s %15s %15s %15s\n", "--------", "------", "--------", "-----");
    
    CrystallineAbacus* zero = abacus_from_double(0.0, base, precision);
    CrystallineAbacus* result = abacus_new(base);
    
    // sin(0)
    math_sin_abacus(result, zero, precision);
    double sin_val = 0;
    abacus_to_double(result, &sin_val);
    printf("%-15s %15.10f %15.10f %15.2e\n", "sin(0)", sin_val, 0.0, fabs(sin_val));
    
    // cos(0)
    math_cos_abacus(result, zero, precision);
    double cos_val = 0;
    abacus_to_double(result, &cos_val);
    printf("%-15s %15.10f %15.10f %15.2e\n", "cos(0)", cos_val, 1.0, fabs(cos_val - 1.0));
    
    abacus_free(zero);
    abacus_free(result);
    
    printf("\n");
}

// ============================================================================
// MAIN BENCHMARK RUNNER
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     CRYSTALLINE MATH LIBRARY - PERFORMANCE BENCHMARKS      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    benchmark_memory_usage();
    benchmark_addition_speed();
    benchmark_multiplication_speed();
    benchmark_exponentiation_speed();
    benchmark_transcendental_speed();
    benchmark_base_comparison();
    benchmark_precision_analysis();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ BENCHMARKS COMPLETE                                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}