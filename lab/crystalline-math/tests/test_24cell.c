/**
 * @file test_24cell.c
 * @brief Test 24-cell generator
 */

#include "math/platonic_generator.h"
#include "math/schlafli.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Testing 24-cell generator...\n\n");
    
    // Test 1: Generate via Schläfli symbol
    printf("Test 1: Generate 24-cell via Schläfli symbol {3,4,3}\n");
    uint32_t schlafli[] = {3, 4, 3};
    PlatonicSolid* solid = platonic_generate(4, schlafli, 3);
    
    if (!solid) {
        fprintf(stderr, "FAIL: Could not generate 24-cell\n");
        return 1;
    }
    
    printf("✓ 24-cell generated successfully\n");
    
    // Test 2: Verify properties
    printf("\nTest 2: Verify 24-cell properties\n");
    printf("  Name: %s\n", solid->name);
    printf("  Dimension: %u\n", solid->dimension);
    printf("  Vertices: %lu (expected 24)\n", (unsigned long)solid->num_vertices);
    printf("  Edges: %lu (expected 96)\n", (unsigned long)solid->num_edges);
    printf("  Faces: %lu (expected 96)\n", (unsigned long)solid->num_faces);
    printf("  Cells: %lu (expected 24)\n", (unsigned long)solid->num_cells);
    
    bool pass = true;
    if (solid->num_vertices != 24) {
        fprintf(stderr, "FAIL: Wrong number of vertices\n");
        pass = false;
    }
    if (solid->num_edges != 96) {
        fprintf(stderr, "FAIL: Wrong number of edges\n");
        pass = false;
    }
    if (solid->num_faces != 96) {
        fprintf(stderr, "FAIL: Wrong number of faces\n");
        pass = false;
    }
    if (solid->num_cells != 24) {
        fprintf(stderr, "FAIL: Wrong number of cells\n");
        pass = false;
    }
    
    if (pass) {
        printf("✓ All counts correct\n");
    }
    
    // Test 3: Verify Schläfli symbol
    printf("\nTest 3: Verify Schläfli symbol\n");
    if (solid->symbol_length != 3) {
        fprintf(stderr, "FAIL: Wrong Schläfli symbol length\n");
        pass = false;
    } else if (solid->schlafli_symbol[0] != 3 ||
               solid->schlafli_symbol[1] != 4 ||
               solid->schlafli_symbol[2] != 3) {
        fprintf(stderr, "FAIL: Wrong Schläfli symbol values\n");
        pass = false;
    } else {
        printf("  Schläfli symbol: {%u,%u,%u}\n",
               solid->schlafli_symbol[0],
               solid->schlafli_symbol[1],
               solid->schlafli_symbol[2]);
        printf("✓ Schläfli symbol correct\n");
    }
    
    // Test 4: Verify 12-fold symmetry
    printf("\nTest 4: Verify 12-fold symmetry properties\n");
    printf("  Embedding dim: %u (should be multiple of 12)\n", solid->embedding_dim);
    printf("  Hidden dim: %u (should be multiple of 12)\n", solid->hidden_dim);
    printf("  Num heads: %u (should be 12)\n", solid->num_heads);
    
    if (solid->num_heads != 12) {
        fprintf(stderr, "FAIL: num_heads should be 12\n");
        pass = false;
    }
    if (solid->embedding_dim % 12 != 0) {
        fprintf(stderr, "FAIL: embedding_dim should be multiple of 12\n");
        pass = false;
    }
    if (solid->hidden_dim % 12 != 0) {
        fprintf(stderr, "FAIL: hidden_dim should be multiple of 12\n");
        pass = false;
    }
    
    if (pass) {
        printf("✓ 12-fold symmetry properties correct\n");
    }
    
    // Test 5: Verify self-duality
    printf("\nTest 5: Verify self-duality (24-cell is dual of itself)\n");
    if (solid->num_vertices == solid->num_cells) {
        printf("  Vertices = Cells = %lu ✓\n", (unsigned long)solid->num_vertices);
        printf("✓ Self-dual property confirmed\n");
    } else {
        fprintf(stderr, "FAIL: 24-cell should be self-dual\n");
        pass = false;
    }
    
    // Test 6: Print detailed info
    printf("\nTest 6: Detailed properties\n");
    platonic_print_detailed(solid);
    
    // Cleanup
    platonic_free(solid);
    
    if (pass) {
        printf("\n✅ ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("\n❌ SOME TESTS FAILED\n");
        return 1;
    }
}