/**
 * @file test_tetration_discovery.c
 * @brief Comprehensive test suite for tetration discovery system
 */

#include "math/tetration_discovery.h"
#include "math/prime_triadic_sets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Color codes for output
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_RESET "\033[0m"

/**
 * @brief Print test result
 */
static void print_test_result(const char* test_name, int passed) {
    tests_run++;
    if (passed) {
        tests_passed++;
        printf(COLOR_GREEN "✓ PASS" COLOR_RESET " %s\n", test_name);
    } else {
        tests_failed++;
        printf(COLOR_RED "✗ FAIL" COLOR_RESET " %s\n", test_name);
    }
}

/**
 * @brief Test discovery system creation
 */
static void test_discovery_system_creation(void) {
    printf("\n" COLOR_BLUE "=== Test: Discovery System Creation ===" COLOR_RESET "\n");
    
    // Create triadic sets for 5D
    TriadicSetConfig config = {
        .dimension = 5,
        .num_sets = 2,
        .max_prime = 50
    };
    
    PrimeTriadicSets* triadic_sets = prime_triadic_generate(&config);
    print_test_result("Generate triadic sets", triadic_sets != NULL);
    
    if (!triadic_sets) return;
    
    // Create discovery system
    TetrationDiscoverySystem* system = tetration_discovery_create(
        triadic_sets,
        29,  // min_depth
        35,  // max_depth (smaller range for testing)
        5    // dimension
    );
    
    print_test_result("Create discovery system", system != NULL);
    
    if (system) {
        print_test_result("System has towers", system->num_towers > 0);
        print_test_result("System dimension correct", system->dimension == 5);
        print_test_result("System depth range correct", 
                         system->min_depth == 29 && system->max_depth == 35);
        
        // Count attractors
        uint32_t num_attractors = 0;
        for (uint32_t i = 0; i < system->num_towers; i++) {
            if (system->towers[i].is_attractor) num_attractors++;
        }
        
        printf("  Found %u towers, %u attractors\n", 
               system->num_towers, num_attractors);
        
        tetration_discovery_free(system);
    }
    
    prime_triadic_free(triadic_sets);
}

/**
 * @brief Test attractor clustering
 */
static void test_attractor_clustering(void) {
    printf("\n" COLOR_BLUE "=== Test: Attractor Clustering ===" COLOR_RESET "\n");
    
    // Create triadic sets
    TriadicSetConfig config = {
        .dimension = 5,
        .num_sets = 2,
        .max_prime = 50
    };
    
    PrimeTriadicSets* triadic_sets = prime_triadic_generate(&config);
    if (!triadic_sets) {
        print_test_result("Generate triadic sets", 0);
        return;
    }
    
    // Create discovery system
    TetrationDiscoverySystem* system = tetration_discovery_create(
        triadic_sets, 29, 35, 5
    );
    
    if (!system) {
        print_test_result("Create discovery system", 0);
        prime_triadic_free(triadic_sets);
        return;
    }
    
    // Find attractor clusters
    AttractorClusters* clusters = tetration_discovery_find_clusters(
        system, 0.1
    );
    
    print_test_result("Find attractor clusters", clusters != NULL);
    
    if (clusters) {
        print_test_result("Clusters found", clusters->num_clusters > 0);
        printf("  Found %u clusters\n", clusters->num_clusters);
        
        tetration_discovery_free_clusters(clusters);
    }
    
    tetration_discovery_free(system);
    prime_triadic_free(triadic_sets);
}

/**
 * @brief Test triadic pattern detection
 */
static void test_triadic_pattern_detection(void) {
    printf("\n" COLOR_BLUE "=== Test: Triadic Pattern Detection ===" COLOR_RESET "\n");
    
    // Create triadic sets
    TriadicSetConfig config = {
        .dimension = 5,
        .num_sets = 3,  // More sets for more patterns
        .max_prime = 50
    };
    
    PrimeTriadicSets* triadic_sets = prime_triadic_generate(&config);
    if (!triadic_sets) {
        print_test_result("Generate triadic sets", 0);
        return;
    }
    
    // Create discovery system
    TetrationDiscoverySystem* system = tetration_discovery_create(
        triadic_sets, 29, 35, 5
    );
    
    if (!system) {
        print_test_result("Create discovery system", 0);
        prime_triadic_free(triadic_sets);
        return;
    }
    
    // Find attractor clusters
    AttractorClusters* clusters = tetration_discovery_find_clusters(
        system, 0.1
    );
    
    if (!clusters) {
        print_test_result("Find attractor clusters", 0);
        tetration_discovery_free(system);
        prime_triadic_free(triadic_sets);
        return;
    }
    
    // Find triadic patterns
    TriadicPatterns* patterns = tetration_discovery_detect_patterns(clusters);
    
    print_test_result("Find triadic patterns", patterns != NULL);
    
    if (patterns) {
        printf("  Found %u triadic patterns\n", patterns->num_patterns);
        
        if (patterns->num_patterns > 0) {
            print_test_result("Patterns have regularity scores", 
                             patterns->patterns[0].regularity_score >= 0.0);
        }
        
        tetration_discovery_free_patterns(patterns);
    }
    
    tetration_discovery_free_clusters(clusters);
    tetration_discovery_free(system);
    prime_triadic_free(triadic_sets);
}

/**
 * @brief Test polytope verification
 */
static void test_polytope_verification(void) {
    printf("\n" COLOR_BLUE "=== Test: Polytope Verification ===" COLOR_RESET "\n");
    
    // Create triadic sets
    TriadicSetConfig config = {
        .dimension = 5,
        .num_sets = 3,
        .max_prime = 50
    };
    
    PrimeTriadicSets* triadic_sets = prime_triadic_generate(&config);
    if (!triadic_sets) {
        print_test_result("Generate triadic sets", 0);
        return;
    }
    
    // Create discovery system
    TetrationDiscoverySystem* system = tetration_discovery_create(
        triadic_sets, 29, 35, 5
    );
    
    if (!system) {
        print_test_result("Create discovery system", 0);
        prime_triadic_free(triadic_sets);
        return;
    }
    
    // Find attractor clusters
    AttractorClusters* clusters = tetration_discovery_find_clusters(
        system, 0.1
    );
    
    if (!clusters) {
        print_test_result("Find attractor clusters", 0);
        tetration_discovery_free(system);
        prime_triadic_free(triadic_sets);
        return;
    }
    
    // Find triadic patterns
    TriadicPatterns* patterns = tetration_discovery_detect_patterns(clusters);
    
    if (!patterns || patterns->num_patterns == 0) {
        print_test_result("Find triadic patterns", 0);
        tetration_discovery_free_clusters(clusters);
        tetration_discovery_free(system);
        prime_triadic_free(triadic_sets);
        return;
    }
    
    // Verify polytope
    PolytopeCandidate* candidate = tetration_discovery_verify_polytope(
        patterns, 5
    );
    
    print_test_result("Verify polytope candidate", candidate != NULL);
    
    if (candidate) {
        print_test_result("Candidate has dimension", candidate->dimension == 5);
        print_test_result("Candidate has regularity score", 
                         candidate->regularity_score >= 0.0);
        
        printf("  Regularity score: %.3f\n", candidate->regularity_score);
        printf("  Regular: %s\n", candidate->is_regular ? "YES" : "NO");
        
        tetration_discovery_free_candidate(candidate);
    }
    
    tetration_discovery_free_patterns(patterns);
    tetration_discovery_free_clusters(clusters);
    tetration_discovery_free(system);
    prime_triadic_free(triadic_sets);
}

/**
 * @brief Test 5D discovery preset
 */
static void test_5d_discovery_preset(void) {
    printf("\n" COLOR_BLUE "=== Test: 5D Discovery Preset ===" COLOR_RESET "\n");
    
    DiscoveryConfig config = tetration_discovery_config_5d();
    
    print_test_result("5D config dimension", config.dimension == 5);
    print_test_result("5D config has sets", config.num_sets > 0);
    print_test_result("5D config depth range", 
                     config.min_depth == 29 && config.max_depth == 59);
    
    printf("  Dimension: %u\n", config.dimension);
    printf("  Sets: %u\n", config.num_sets);
    printf("  Depth: [%u, %u]\n", config.min_depth, config.max_depth);
}

/**
 * @brief Test 6D discovery preset
 */
static void test_6d_discovery_preset(void) {
    printf("\n" COLOR_BLUE "=== Test: 6D Discovery Preset ===" COLOR_RESET "\n");
    
    DiscoveryConfig config = tetration_discovery_config_6d();
    
    print_test_result("6D config dimension", config.dimension == 6);
    print_test_result("6D config has sets", config.num_sets > 0);
    print_test_result("6D config depth range", 
                     config.min_depth == 29 && config.max_depth == 59);
    
    printf("  Dimension: %u\n", config.dimension);
    printf("  Sets: %u\n", config.num_sets);
    printf("  Depth: [%u, %u]\n", config.min_depth, config.max_depth);
}

/**
 * @brief Test 7D discovery preset
 */
static void test_7d_discovery_preset(void) {
    printf("\n" COLOR_BLUE "=== Test: 7D Discovery Preset ===" COLOR_RESET "\n");
    
    DiscoveryConfig config = tetration_discovery_config_7d();
    
    print_test_result("7D config dimension", config.dimension == 7);
    print_test_result("7D config has sets", config.num_sets > 0);
    print_test_result("7D config depth range", 
                     config.min_depth == 29 && config.max_depth == 59);
    
    printf("  Dimension: %u\n", config.dimension);
    printf("  Sets: %u\n", config.num_sets);
    printf("  Depth: [%u, %u]\n", config.min_depth, config.max_depth);
}

/**
 * @brief Run small-scale discovery experiment
 */
static void test_small_scale_discovery(void) {
    printf("\n" COLOR_BLUE "=== Test: Small-Scale Discovery Experiment ===" COLOR_RESET "\n");
    printf(COLOR_YELLOW "This may take 10-30 seconds..." COLOR_RESET "\n\n");
    
    // Create a small-scale config for testing
    DiscoveryConfig config = {
        .dimension = 5,
        .num_sets = 2,
        .min_depth = 29,
        .max_depth = 35,  // Smaller range for faster testing
        .cluster_threshold = 0.1,
        .regularity_threshold = 0.8
    };
    
    PolytopeCandidate* candidate = tetration_discovery_run(&config);
    
    print_test_result("Small-scale discovery completed", candidate != NULL);
    
    if (candidate) {
        printf("\n" COLOR_GREEN "Discovery Results:" COLOR_RESET "\n");
        tetration_discovery_print_candidate(candidate);
        tetration_discovery_free_candidate(candidate);
    }
}

/**
 * @brief Main test runner
 */
int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     TETRATION DISCOVERY SYSTEM - COMPREHENSIVE TESTS      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Run all tests
    test_discovery_system_creation();
    test_attractor_clustering();
    test_triadic_pattern_detection();
    test_polytope_verification();
    test_5d_discovery_preset();
    test_6d_discovery_preset();
    test_7d_discovery_preset();
    test_small_scale_discovery();
    
    // Print summary
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                      TEST SUMMARY                          ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Total Tests:  %3d                                         ║\n", tests_run);
    printf("║  " COLOR_GREEN "Passed:       %3d" COLOR_RESET "                                         ║\n", tests_passed);
    printf("║  " COLOR_RED "Failed:       %3d" COLOR_RESET "                                         ║\n", tests_failed);
    printf("║  Success Rate: %3d%%                                        ║\n", 
           tests_run > 0 ? (tests_passed * 100 / tests_run) : 0);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}