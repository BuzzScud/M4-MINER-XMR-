/**
 * @file test_validator_simple.c
 * @brief Simple test for mathematical validation
 */

#include "math/schlafli.h"
#include "math/schlafli_validator.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    int passed = 0;
    int failed = 0;
    
    printf("==============================================\n");
    printf("Schläfli Mathematical Validation Tests\n");
    printf("==============================================\n");
    
    // Test all 5 Platonic solids
    printf("\n=== 3D Platonic Solids ===\n");
    
    const char* solids_3d[] = {"{3,3}", "{4,3}", "{3,4}", "{5,3}", "{3,5}"};
    const char* names_3d[] = {"Tetrahedron", "Cube", "Octahedron", "Dodecahedron", "Icosahedron"};
    uint64_t vertices_3d[] = {4, 8, 6, 20, 12};
    
    for (int i = 0; i < 5; i++) {
        printf("\n%s %s:\n", names_3d[i], solids_3d[i]);
        SchlafliSymbol* sym = schlafli_parse(solids_3d[i]);
        
        if (!sym) {
            printf("  ❌ Failed to parse\n");
            failed++;
            continue;
        }
        
        bool valid = schlafli_validate_mathematical(sym);
        uint64_t v = schlafli_calculate_vertices(sym);
        
        if (valid && v == vertices_3d[i]) {
            printf("  ✓ Valid, %lu vertices\n", v);
            passed++;
        } else {
            printf("  ❌ Invalid or wrong vertex count\n");
            failed++;
        }
        
        schlafli_free(sym);
    }
    
    // Test all 6 regular 4D polychora
    printf("\n=== 4D Regular Polychora ===\n");
    
    const char* solids_4d[] = {"{3,3,3}", "{4,3,3}", "{3,3,4}", "{3,4,3}", "{5,3,3}", "{3,3,5}"};
    const char* names_4d[] = {"5-cell", "Tesseract", "16-cell", "24-cell", "120-cell", "600-cell"};
    uint64_t vertices_4d[] = {5, 16, 8, 24, 600, 120};
    
    for (int i = 0; i < 6; i++) {
        printf("\n%s %s:\n", names_4d[i], solids_4d[i]);
        SchlafliSymbol* sym = schlafli_parse(solids_4d[i]);
        
        if (!sym) {
            printf("  ❌ Failed to parse\n");
            failed++;
            continue;
        }
        
        bool valid = schlafli_validate_mathematical(sym);
        uint64_t v = schlafli_calculate_vertices(sym);
        
        if (valid && v == vertices_4d[i]) {
            printf("  ✓ Valid, %lu vertices\n", v);
            passed++;
        } else {
            printf("  ❌ Invalid or wrong vertex count (got %lu, expected %lu)\n", v, vertices_4d[i]);
            failed++;
        }
        
        schlafli_free(sym);
    }
    
    // Test invalid symbols
    printf("\n=== Invalid Symbols (should fail) ===\n");
    
    const char* invalid[] = {"{6,3}", "{3,6}", "{4,4}", "{7,3}"};
    
    for (int i = 0; i < 4; i++) {
        printf("\n%s:\n", invalid[i]);
        SchlafliSymbol* sym = schlafli_parse(invalid[i]);
        
        if (!sym) {
            printf("  ❌ Failed to parse\n");
            failed++;
            continue;
        }
        
        bool valid = schlafli_validate_mathematical(sym);
        
        if (!valid) {
            printf("  ✓ Correctly rejected as invalid\n");
            passed++;
        } else {
            printf("  ❌ Incorrectly accepted as valid\n");
            failed++;
        }
        
        schlafli_free(sym);
    }
    
    // Test nD polytopes
    printf("\n=== nD Polytopes ===\n");
    
    printf("\n5D Simplex {3,3,3,3}:\n");
    SchlafliSymbol* simplex5d = schlafli_parse("{3,3,3,3}");
    if (simplex5d && schlafli_validate_mathematical(simplex5d) && 
        schlafli_calculate_vertices(simplex5d) == 6) {
        printf("  ✓ Valid, 6 vertices\n");
        passed++;
    } else {
        printf("  ❌ Failed\n");
        failed++;
    }
    if (simplex5d) schlafli_free(simplex5d);
    
    printf("\n5D Hypercube {4,3,3,3}:\n");
    SchlafliSymbol* hypercube5d = schlafli_parse("{4,3,3,3}");
    if (hypercube5d && schlafli_validate_mathematical(hypercube5d) && 
        schlafli_calculate_vertices(hypercube5d) == 32) {
        printf("  ✓ Valid, 32 vertices\n");
        passed++;
    } else {
        printf("  ❌ Failed\n");
        failed++;
    }
    if (hypercube5d) schlafli_free(hypercube5d);
    
    printf("\n==============================================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("==============================================\n");
    
    return failed > 0 ? 1 : 0;
}