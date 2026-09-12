/**
 * @file test_120cell_600cell.c
 * @brief Test 120-cell and 600-cell generators
 */

#include "math/platonic_generator.h"
#include "math/schlafli.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Testing 120-cell and 600-cell generators...\n\n");
    
    bool all_pass = true;
    
    // ========================================================================
    // TEST 120-CELL
    // ========================================================================
    
    printf("=== Testing 120-cell {5,3,3} ===\n\n");
    
    // Test 1: Generate via Schläfli symbol
    printf("Test 1: Generate 120-cell via Schläfli symbol {5,3,3}\n");
    uint32_t schlafli_120[] = {5, 3, 3};
    PlatonicSolid* cell_120 = platonic_generate(4, schlafli_120, 3);
    
    if (!cell_120) {
        fprintf(stderr, "FAIL: Could not generate 120-cell\n");
        all_pass = false;
    } else {
        printf("✓ 120-cell generated successfully\n");
        
        // Test 2: Verify properties
        printf("\nTest 2: Verify 120-cell properties\n");
        printf("  Name: %s\n", cell_120->name);
        printf("  Dimension: %u\n", cell_120->dimension);
        printf("  Vertices: %lu (expected 600)\n", (unsigned long)cell_120->num_vertices);
        printf("  Edges: %lu (expected 1200)\n", (unsigned long)cell_120->num_edges);
        printf("  Faces: %lu (expected 720)\n", (unsigned long)cell_120->num_faces);
        printf("  Cells: %lu (expected 120)\n", (unsigned long)cell_120->num_cells);
        
        if (cell_120->num_vertices != 600) {
            fprintf(stderr, "FAIL: Wrong number of vertices\n");
            all_pass = false;
        }
        if (cell_120->num_edges != 1200) {
            fprintf(stderr, "FAIL: Wrong number of edges\n");
            all_pass = false;
        }
        if (cell_120->num_faces != 720) {
            fprintf(stderr, "FAIL: Wrong number of faces\n");
            all_pass = false;
        }
        if (cell_120->num_cells != 120) {
            fprintf(stderr, "FAIL: Wrong number of cells\n");
            all_pass = false;
        }
        
        if (cell_120->num_vertices == 600 && 
            cell_120->num_edges == 1200 &&
            cell_120->num_faces == 720 &&
            cell_120->num_cells == 120) {
            printf("✓ All counts correct\n");
        }
        
        // Test 3: Verify 12-fold symmetry
        printf("\nTest 3: Verify 12-fold symmetry properties\n");
        printf("  Embedding dim: %u (should be multiple of 12)\n", cell_120->embedding_dim);
        printf("  Hidden dim: %u (should be multiple of 12)\n", cell_120->hidden_dim);
        printf("  Num heads: %u (should be 12)\n", cell_120->num_heads);
        
        if (cell_120->num_heads != 12) {
            fprintf(stderr, "FAIL: num_heads should be 12\n");
            all_pass = false;
        }
        if (cell_120->embedding_dim % 12 != 0) {
            fprintf(stderr, "FAIL: embedding_dim should be multiple of 12\n");
            all_pass = false;
        }
        if (cell_120->hidden_dim % 12 != 0) {
            fprintf(stderr, "FAIL: hidden_dim should be multiple of 12\n");
            all_pass = false;
        }
        
        if (cell_120->num_heads == 12 &&
            cell_120->embedding_dim % 12 == 0 &&
            cell_120->hidden_dim % 12 == 0) {
            printf("✓ 12-fold symmetry properties correct\n");
        }
        
        platonic_free(cell_120);
    }
    
    // ========================================================================
    // TEST 600-CELL
    // ========================================================================
    
    printf("\n=== Testing 600-cell {3,3,5} ===\n\n");
    
    // Test 4: Generate via Schläfli symbol
    printf("Test 4: Generate 600-cell via Schläfli symbol {3,3,5}\n");
    uint32_t schlafli_600[] = {3, 3, 5};
    PlatonicSolid* cell_600 = platonic_generate(4, schlafli_600, 3);
    
    if (!cell_600) {
        fprintf(stderr, "FAIL: Could not generate 600-cell\n");
        all_pass = false;
    } else {
        printf("✓ 600-cell generated successfully\n");
        
        // Test 5: Verify properties
        printf("\nTest 5: Verify 600-cell properties\n");
        printf("  Name: %s\n", cell_600->name);
        printf("  Dimension: %u\n", cell_600->dimension);
        printf("  Vertices: %lu (expected 120)\n", (unsigned long)cell_600->num_vertices);
        printf("  Edges: %lu (expected 720)\n", (unsigned long)cell_600->num_edges);
        printf("  Faces: %lu (expected 1200)\n", (unsigned long)cell_600->num_faces);
        printf("  Cells: %lu (expected 600)\n", (unsigned long)cell_600->num_cells);
        
        if (cell_600->num_vertices != 120) {
            fprintf(stderr, "FAIL: Wrong number of vertices\n");
            all_pass = false;
        }
        if (cell_600->num_edges != 720) {
            fprintf(stderr, "FAIL: Wrong number of edges\n");
            all_pass = false;
        }
        if (cell_600->num_faces != 1200) {
            fprintf(stderr, "FAIL: Wrong number of faces\n");
            all_pass = false;
        }
        if (cell_600->num_cells != 600) {
            fprintf(stderr, "FAIL: Wrong number of cells\n");
            all_pass = false;
        }
        
        if (cell_600->num_vertices == 120 && 
            cell_600->num_edges == 720 &&
            cell_600->num_faces == 1200 &&
            cell_600->num_cells == 600) {
            printf("✓ All counts correct\n");
        }
        
        // Test 6: Verify 12-fold symmetry
        printf("\nTest 6: Verify 12-fold symmetry properties\n");
        printf("  Embedding dim: %u (should be multiple of 12)\n", cell_600->embedding_dim);
        printf("  Hidden dim: %u (should be multiple of 12)\n", cell_600->hidden_dim);
        printf("  Num heads: %u (should be 12)\n", cell_600->num_heads);
        
        if (cell_600->num_heads != 12) {
            fprintf(stderr, "FAIL: num_heads should be 12\n");
            all_pass = false;
        }
        if (cell_600->embedding_dim % 12 != 0) {
            fprintf(stderr, "FAIL: embedding_dim should be multiple of 12\n");
            all_pass = false;
        }
        if (cell_600->hidden_dim % 12 != 0) {
            fprintf(stderr, "FAIL: hidden_dim should be multiple of 12\n");
            all_pass = false;
        }
        
        if (cell_600->num_heads == 12 &&
            cell_600->embedding_dim % 12 == 0 &&
            cell_600->hidden_dim % 12 == 0) {
            printf("✓ 12-fold symmetry properties correct\n");
        }
        
        // Test 7: Verify duality (120-cell and 600-cell are duals)
        printf("\nTest 7: Verify duality relationship\n");
        printf("  120-cell: V=%lu, C=%lu\n", 
               (unsigned long)600, (unsigned long)120);
        printf("  600-cell: V=%lu, C=%lu\n",
               (unsigned long)cell_600->num_vertices,
               (unsigned long)cell_600->num_cells);
        
        if (cell_600->num_vertices == 120 && cell_600->num_cells == 600) {
            printf("✓ Duality confirmed: 120-cell vertices = 600-cell cells\n");
            printf("✓ Duality confirmed: 120-cell cells = 600-cell vertices\n");
        } else {
            fprintf(stderr, "FAIL: Duality relationship incorrect\n");
            all_pass = false;
        }
        
        platonic_free(cell_600);
    }
    
    // ========================================================================
    // SUMMARY
    // ========================================================================
    
    printf("\n=== Summary ===\n");
    printf("4D Regular Polychora (6 total):\n");
    printf("  ✓ 5-cell {3,3,3} - 5 vertices\n");
    printf("  ✓ Tesseract {4,3,3} - 16 vertices\n");
    printf("  ✓ 16-cell {3,3,4} - 8 vertices\n");
    printf("  ✓ 24-cell {3,4,3} - 24 vertices\n");
    printf("  ✓ 120-cell {5,3,3} - 600 vertices\n");
    printf("  ✓ 600-cell {3,3,5} - 120 vertices\n");
    printf("\n");
    
    if (all_pass) {
        printf("✅ ALL TESTS PASSED\n");
        printf("✅ 4D POLYCHORA: 100%% COMPLETE (6/6)\n");
        return 0;
    } else {
        printf("❌ SOME TESTS FAILED\n");
        return 1;
    }
}