/**
 * @file test_discovery_debug.c
 * @brief Debug polytope discovery system
 */

#include "math/polytope_discovery.h"
#include "math/schlafli.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Polytope Discovery Debug ===\n\n");
    
    // Test 1: Parse Schläfli symbol
    printf("Test 1: Parsing {3,3,5}...\n");
    SchlafliSymbol* symbol = schlafli_parse("{3,3,5}");
    if (!symbol) {
        printf("FAIL: Could not parse symbol\n");
        return 1;
    }
    printf("SUCCESS: Parsed symbol\n");
    printf("  Dimension: %u\n", symbol->dimension);
    printf("  Length: %u\n", symbol->length);
    printf("  Components: ");
    for (uint32_t i = 0; i < symbol->length; i++) {
        printf("%u ", symbol->components[i]);
    }
    printf("\n\n");
    
    // Test 2: Get discovery config
    printf("Test 2: Getting discovery config for dimension %u...\n", symbol->dimension);
    DiscoveryConfig config = discovery_config_for_dimension(symbol->dimension);
    printf("Config min_dimension: %u\n", config.min_dimension);
    printf("Config max_dimension: %u\n", config.max_dimension);
    printf("Config min_component: %u\n", config.min_component);
    printf("Config max_component: %u\n", config.max_component);
    printf("Config search_exceptional: %s\n\n", config.search_exceptional ? "true" : "false");
    
    // Test 3: Run discovery
    printf("Test 3: Running discovery search...\n");
    DiscoveryResults* results = discovery_search(&config);
    if (!results) {
        printf("FAIL: discovery_search returned NULL\n");
        schlafli_free(symbol);
        return 1;
    }
    printf("SUCCESS: Discovery completed\n");
    printf("  Found %u polytopes\n\n", results->count);
    
    // Test 4: List all discovered polytopes
    printf("Test 4: Listing discovered polytopes...\n");
    for (uint32_t i = 0; i < results->count; i++) {
        DiscoveredPolytope* p = results->polytopes[i];
        printf("  [%u] Dimension: %u, Symbol: {", i, p->dimension);
        for (uint32_t j = 0; j < p->symbol->length; j++) {
            printf("%u", p->symbol->components[j]);
            if (j < p->symbol->length - 1) printf(",");
        }
        printf("}, Vertices: %lu\n", p->vertices);
    }
    printf("\n");
    
    // Test 5: Search for {3,3,5}
    printf("Test 5: Searching for {3,3,5}...\n");
    bool found = false;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->polytopes[i]->symbol->length == symbol->length) {
            bool match = true;
            for (uint32_t j = 0; j < symbol->length; j++) {
                if (results->polytopes[i]->symbol->components[j] != symbol->components[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                printf("FOUND at index %u!\n", i);
                printf("  Vertices: %lu\n", results->polytopes[i]->vertices);
                printf("  Edges: %lu\n", results->polytopes[i]->edges);
                printf("  Faces: %lu\n", results->polytopes[i]->faces);
                printf("  Cells: %lu\n", results->polytopes[i]->cells);
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        printf("NOT FOUND: {3,3,5} not in discovery results\n");
    }
    
    // Cleanup
    schlafli_free(symbol);
    discovery_results_free(results);
    
    printf("\n=== Debug Complete ===\n");
    return 0;
}