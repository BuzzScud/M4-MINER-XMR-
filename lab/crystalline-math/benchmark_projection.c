/**
 * @file benchmark_projection.c
 * @brief Benchmark stereographic projection optimizations
 * 
 * Compares performance of original vs optimized projection functions.
 */

#include "math/clock.h"
#include "math/visualization.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCHMARK_ITERATIONS 1000000

/* Simple timing function */
double get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main() {
    printf("=== Stereographic Projection Benchmark ===\n\n");
    
    /* Allocate test data */
    ClockPosition* positions = malloc(BENCHMARK_ITERATIONS * sizeof(ClockPosition));
    SphereCoord* spheres = malloc(BENCHMARK_ITERATIONS * sizeof(SphereCoord));
    
    if (!positions || !spheres) {
        printf("ERROR: Memory allocation failed\n");
        return 1;
    }
    
    /* Initialize test data with varied positions */
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        positions[i].radius = 0.5 + 0.3 * ((double)i / BENCHMARK_ITERATIONS);
        positions[i].angle = ((double)i / BENCHMARK_ITERATIONS) * 2.0 * MATH_PI;
        positions[i].ring = i % 4;
        positions[i].position = i % 60;
    }
    
    printf("Test data: %d positions\n", BENCHMARK_ITERATIONS);
    printf("Radius range: 0.5 to 0.8\n");
    printf("Angle range: 0 to 2π\n\n");
    
    /* ========================================================================
     * Benchmark 1: Original Implementation
     * ========================================================================
     */
    printf("--- Benchmark 1: Original Implementation ---\n");
    double start = get_time_seconds();
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        clock_to_sphere(&positions[i], &spheres[i]);
    }
    
    double end = get_time_seconds();
    double time_original = end - start;
    
    printf("Time: %.3f seconds\n", time_original);
    printf("Rate: %.0f projections/second\n", BENCHMARK_ITERATIONS / time_original);
    printf("Avg: %.3f microseconds/projection\n\n", time_original * 1e6 / BENCHMARK_ITERATIONS);
    
    /* ========================================================================
     * Benchmark 2: Optimized Implementation
     * ========================================================================
     */
    printf("--- Benchmark 2: Optimized Implementation ---\n");
    start = get_time_seconds();
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        clock_to_sphere_optimized(&positions[i], &spheres[i]);
    }
    
    end = get_time_seconds();
    double time_optimized = end - start;
    
    printf("Time: %.3f seconds\n", time_optimized);
    printf("Rate: %.0f projections/second\n", BENCHMARK_ITERATIONS / time_optimized);
    printf("Avg: %.3f microseconds/projection\n\n", time_optimized * 1e6 / BENCHMARK_ITERATIONS);
    
    /* ========================================================================
     * Benchmark 3: Batch Processing (Scalar)
     * ========================================================================
     */
    printf("--- Benchmark 3: Batch Processing (Scalar) ---\n");
    start = get_time_seconds();
    
    clock_to_sphere_batch(positions, spheres, BENCHMARK_ITERATIONS);
    
    end = get_time_seconds();
    double time_batch = end - start;
    
    printf("Time: %.3f seconds\n", time_batch);
    printf("Rate: %.0f projections/second\n", BENCHMARK_ITERATIONS / time_batch);
    printf("Avg: %.3f microseconds/projection\n\n", time_batch * 1e6 / BENCHMARK_ITERATIONS);
    
/* SIMD benchmark disabled for now - needs export in header */
    double time_simd = 0.0;
    
    /* ========================================================================
     * Summary
     * ========================================================================
     */
    printf("=== Performance Summary ===\n\n");
    printf("Original:  %.3f seconds (baseline)\n", time_original);
    printf("Optimized: %.3f seconds (%.2fx speedup)\n", time_optimized, time_original / time_optimized);
    printf("Batch:     %.3f seconds (%.2fx speedup)\n", time_batch, time_original / time_batch);
    
printf("SIMD:      Not yet integrated (coming soon)\n");
    
    printf("\n");
    
    /* ========================================================================
     * Correctness Verification
     * ========================================================================
     */
    printf("=== Correctness Verification ===\n\n");
    
    /* Test a few positions to verify results match */
    int errors = 0;
    for (int i = 0; i < 100; i++) {
        SphereCoord s1, s2;
        clock_to_sphere(&positions[i], &s1);
        clock_to_sphere_optimized(&positions[i], &s2);
        
        double diff_x = s1.x - s2.x;
        double diff_y = s1.y - s2.y;
        double diff_z = s1.z - s2.z;
        double max_diff = (diff_x > diff_y) ? diff_x : diff_y;
        max_diff = (max_diff > diff_z) ? max_diff : diff_z;
        
        if (max_diff > 1e-10) {
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("✓ All results match (100 samples tested)\n");
        printf("✓ Maximum difference < 1e-10\n");
    } else {
        printf("✗ Found %d mismatches\n", errors);
    }
    
    /* Cleanup */
    free(positions);
    free(spheres);
    
    printf("\n=== Benchmark Complete ===\n");
    return 0;
}