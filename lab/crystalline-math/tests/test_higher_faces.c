/**
 * @file test_higher_faces.c
 * @brief Comprehensive test suite for higher-dimensional face generation
 */

#include "math/higher_faces.h"
#include "math/polytope.h"
#include "math/schlafli.h"
#include "math/schlafli_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// TEST UTILITIES
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int assertions_run = 0;

#define TEST(name) \
    static void name(void); \
    static void name##_wrapper(void) { \
        printf("\n[TEST] %s\n", #name); \
        tests_run++; \
        name(); \
        tests_passed++; \
    } \
    static void name(void)

#define ASSERT(condition, message) \
    do { \
        assertions_run++; \
        if (!(condition)) { \
            printf("  ✗ ASSERTION FAILED: %s\n", message); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            exit(1); \
        } else { \
            printf("  ✓ %s\n", message); \
        } \
    } while(0)

#define RUN_TEST(test) test##_wrapper()

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Create a simple 3D tetrahedron for testing
 */
static PlatonicSolid* create_test_tetrahedron(void) {
    PlatonicSolid* solid = calloc(1, sizeof(PlatonicSolid));
    if (!solid) return NULL;
    
    solid->dimension = 3;
    solid->num_vertices = 4;
    solid->num_edges = 6;
    solid->num_faces = 4;
    
    // Schläfli symbol {3,3}
    solid->symbol_length = 2;
    solid->schlafli_symbol = malloc(2 * sizeof(uint32_t));
    solid->schlafli_symbol[0] = 3;
    solid->schlafli_symbol[1] = 3;
    
    // Vertices (unit tetrahedron)
    solid->vertex_coords = malloc(4 * 3 * sizeof(double));
    double verts[4][3] = {
        {1.0, 1.0, 1.0},
        {1.0, -1.0, -1.0},
        {-1.0, 1.0, -1.0},
        {-1.0, -1.0, 1.0}
    };
    memcpy(solid->vertex_coords, verts, 4 * 3 * sizeof(double));
    
    // Edges
    solid->edge_indices = malloc(6 * sizeof(uint32_t*));
    for (int i = 0; i < 6; i++) {
        solid->edge_indices[i] = malloc(2 * sizeof(uint32_t));
    }
    uint32_t edges[6][2] = {
        {0, 1}, {0, 2}, {0, 3},
        {1, 2}, {1, 3}, {2, 3}
    };
    for (int i = 0; i < 6; i++) {
        solid->edge_indices[i][0] = edges[i][0];
        solid->edge_indices[i][1] = edges[i][1];
    }
    
    // Faces (triangles)
    solid->face_indices = malloc(4 * sizeof(uint32_t*));
    solid->face_sizes = malloc(4 * sizeof(uint32_t));
    for (int i = 0; i < 4; i++) {
        solid->face_indices[i] = malloc(3 * sizeof(uint32_t));
        solid->face_sizes[i] = 3;
    }
    uint32_t faces[4][3] = {
        {0, 1, 2},
        {0, 1, 3},
        {0, 2, 3},
        {1, 2, 3}
    };
    for (int i = 0; i < 4; i++) {
        memcpy(solid->face_indices[i], faces[i], 3 * sizeof(uint32_t));
    }
    
    strcpy(solid->name, "Tetrahedron");
    solid->is_valid = true;
    solid->is_regular = true;
    
    return solid;
}

/**
 * @brief Create a simple 3D cube for testing
 */
static PlatonicSolid* create_test_cube(void) {
    PlatonicSolid* solid = calloc(1, sizeof(PlatonicSolid));
    if (!solid) return NULL;
    
    solid->dimension = 3;
    solid->num_vertices = 8;
    solid->num_edges = 12;
    solid->num_faces = 6;
    
    // Schläfli symbol {4,3}
    solid->symbol_length = 2;
    solid->schlafli_symbol = malloc(2 * sizeof(uint32_t));
    solid->schlafli_symbol[0] = 4;
    solid->schlafli_symbol[1] = 3;
    
    // Vertices (unit cube)
    solid->vertex_coords = malloc(8 * 3 * sizeof(double));
    double verts[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
    };
    memcpy(solid->vertex_coords, verts, 8 * 3 * sizeof(double));
    
    // Edges
    solid->edge_indices = malloc(12 * sizeof(uint32_t*));
    for (int i = 0; i < 12; i++) {
        solid->edge_indices[i] = malloc(2 * sizeof(uint32_t));
    }
    uint32_t edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},  // bottom
        {4,5}, {5,6}, {6,7}, {7,4},  // top
        {0,4}, {1,5}, {2,6}, {3,7}   // vertical
    };
    for (int i = 0; i < 12; i++) {
        solid->edge_indices[i][0] = edges[i][0];
        solid->edge_indices[i][1] = edges[i][1];
    }
    
    // Faces (squares)
    solid->face_indices = malloc(6 * sizeof(uint32_t*));
    solid->face_sizes = malloc(6 * sizeof(uint32_t));
    for (int i = 0; i < 6; i++) {
        solid->face_indices[i] = malloc(4 * sizeof(uint32_t));
        solid->face_sizes[i] = 4;
    }
    uint32_t faces[6][4] = {
        {0,1,2,3},  // bottom
        {4,5,6,7},  // top
        {0,1,5,4},  // front
        {2,3,7,6},  // back
        {0,3,7,4},  // left
        {1,2,6,5}   // right
    };
    for (int i = 0; i < 6; i++) {
        memcpy(solid->face_indices[i], faces[i], 4 * sizeof(uint32_t));
    }
    
    strcpy(solid->name, "Cube");
    solid->is_valid = true;
    solid->is_regular = true;
    
    return solid;
}

/**
 * @brief Create a 4D tesseract for testing
 */
static PlatonicSolid* create_test_tesseract(void) {
    PlatonicSolid* solid = calloc(1, sizeof(PlatonicSolid));
    if (!solid) return NULL;
    
    solid->dimension = 4;
    solid->num_vertices = 16;
    solid->num_edges = 32;
    solid->num_faces = 24;
    solid->num_cells = 8;
    
    // Schläfli symbol {4,3,3}
    solid->symbol_length = 3;
    solid->schlafli_symbol = malloc(3 * sizeof(uint32_t));
    solid->schlafli_symbol[0] = 4;
    solid->schlafli_symbol[1] = 3;
    solid->schlafli_symbol[2] = 3;
    
    // Simplified vertex coordinates (just for testing structure)
    solid->vertex_coords = calloc(16 * 4, sizeof(double));
    
    // Simplified edge structure
    solid->edge_indices = malloc(32 * sizeof(uint32_t*));
    for (int i = 0; i < 32; i++) {
        solid->edge_indices[i] = malloc(2 * sizeof(uint32_t));
        solid->edge_indices[i][0] = i % 16;
        solid->edge_indices[i][1] = (i + 1) % 16;
    }
    
    // Simplified face structure
    solid->face_indices = malloc(24 * sizeof(uint32_t*));
    solid->face_sizes = malloc(24 * sizeof(uint32_t));
    for (int i = 0; i < 24; i++) {
        solid->face_indices[i] = malloc(4 * sizeof(uint32_t));
        solid->face_sizes[i] = 4;
        for (int j = 0; j < 4; j++) {
            solid->face_indices[i][j] = (i * 4 + j) % 16;
        }
    }
    
    strcpy(solid->name, "Tesseract");
    solid->is_valid = true;
    solid->is_regular = true;
    
    return solid;
}

static void free_test_solid(PlatonicSolid* solid) {
    if (!solid) return;
    free(solid->schlafli_symbol);
    free(solid->vertex_coords);
    if (solid->edge_indices) {
        for (uint64_t i = 0; i < solid->num_edges; i++) {
            free(solid->edge_indices[i]);
        }
        free(solid->edge_indices);
    }
    if (solid->face_indices) {
        for (uint64_t i = 0; i < solid->num_faces; i++) {
            free(solid->face_indices[i]);
        }
        free(solid->face_indices);
    }
    free(solid->face_sizes);
    free(solid);
}

// ============================================================================
// TESTS: 0-FACE GENERATION (VERTICES)
// ============================================================================

TEST(test_0_faces_tetrahedron) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created tetrahedron");
    
    KFaceSet* vertices = higher_faces_generate_k_faces(solid, 0);
    ASSERT(vertices != NULL, "Generated 0-faces");
    ASSERT(vertices->dimension == 0, "0-faces have dimension 0");
    ASSERT(vertices->count == 4, "Tetrahedron has 4 vertices");
    
    // Check each vertex
    for (uint32_t i = 0; i < vertices->count; i++) {
        KFace* v = vertices->faces[i];
        ASSERT(v->dimension == 0, "Vertex has dimension 0");
        ASSERT(v->num_vertices == 1, "Vertex contains 1 vertex");
        ASSERT(v->vertex_indices[0] == i, "Vertex index is correct");
    }
    
    higher_faces_free_set(vertices);
    free_test_solid(solid);
}

TEST(test_0_faces_cube) {
    PlatonicSolid* solid = create_test_cube();
    ASSERT(solid != NULL, "Created cube");
    
    KFaceSet* vertices = higher_faces_generate_k_faces(solid, 0);
    ASSERT(vertices != NULL, "Generated 0-faces");
    ASSERT(vertices->count == 8, "Cube has 8 vertices");
    
    higher_faces_free_set(vertices);
    free_test_solid(solid);
}

// ============================================================================
// TESTS: 1-FACE GENERATION (EDGES)
// ============================================================================

TEST(test_1_faces_tetrahedron) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created tetrahedron");
    
    KFaceSet* edges = higher_faces_generate_k_faces(solid, 1);
    ASSERT(edges != NULL, "Generated 1-faces");
    ASSERT(edges->dimension == 1, "1-faces have dimension 1");
    ASSERT(edges->count == 6, "Tetrahedron has 6 edges");
    
    // Check each edge
    for (uint32_t i = 0; i < edges->count; i++) {
        KFace* e = edges->faces[i];
        ASSERT(e->dimension == 1, "Edge has dimension 1");
        ASSERT(e->num_vertices == 2, "Edge contains 2 vertices");
    }
    
    higher_faces_free_set(edges);
    free_test_solid(solid);
}

TEST(test_1_faces_cube) {
    PlatonicSolid* solid = create_test_cube();
    ASSERT(solid != NULL, "Created cube");
    
    KFaceSet* edges = higher_faces_generate_k_faces(solid, 1);
    ASSERT(edges != NULL, "Generated 1-faces");
    ASSERT(edges->count == 12, "Cube has 12 edges");
    
    higher_faces_free_set(edges);
    free_test_solid(solid);
}

// ============================================================================
// TESTS: 2-FACE GENERATION (POLYGONS)
// ============================================================================

TEST(test_2_faces_tetrahedron) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created tetrahedron");
    
    KFaceSet* faces = higher_faces_generate_k_faces(solid, 2);
    ASSERT(faces != NULL, "Generated 2-faces");
    ASSERT(faces->dimension == 2, "2-faces have dimension 2");
    ASSERT(faces->count == 4, "Tetrahedron has 4 faces");
    
    // Check each face
    for (uint32_t i = 0; i < faces->count; i++) {
        KFace* f = faces->faces[i];
        ASSERT(f->dimension == 2, "Face has dimension 2");
        ASSERT(f->num_vertices == 3, "Triangular face has 3 vertices");
    }
    
    higher_faces_free_set(faces);
    free_test_solid(solid);
}

TEST(test_2_faces_cube) {
    PlatonicSolid* solid = create_test_cube();
    ASSERT(solid != NULL, "Created cube");
    
    KFaceSet* faces = higher_faces_generate_k_faces(solid, 2);
    ASSERT(faces != NULL, "Generated 2-faces");
    ASSERT(faces->count == 6, "Cube has 6 faces");
    
    // Check each face
    for (uint32_t i = 0; i < faces->count; i++) {
        KFace* f = faces->faces[i];
        ASSERT(f->dimension == 2, "Face has dimension 2");
        ASSERT(f->num_vertices == 4, "Square face has 4 vertices");
    }
    
    higher_faces_free_set(faces);
    free_test_solid(solid);
}

// ============================================================================
// TESTS: 3-FACE GENERATION (CELLS)
// ============================================================================

TEST(test_3_faces_tesseract) {
    PlatonicSolid* solid = create_test_tesseract();
    ASSERT(solid != NULL, "Created tesseract");
    
    KFaceSet* cells = higher_faces_generate_k_faces(solid, 3);
    ASSERT(cells != NULL, "Generated 3-faces");
    ASSERT(cells->dimension == 3, "3-faces have dimension 3");
    ASSERT(cells->count == 8, "Tesseract has 8 cubic cells");
    
    // Check each cell
    for (uint32_t i = 0; i < cells->count; i++) {
        KFace* c = cells->faces[i];
        ASSERT(c->dimension == 3, "Cell has dimension 3");
        ASSERT(c->num_vertices == 8, "Cubic cell has 8 vertices");
    }
    
    higher_faces_free_set(cells);
    free_test_solid(solid);
}

TEST(test_3_faces_invalid_3d) {
    PlatonicSolid* solid = create_test_cube();
    ASSERT(solid != NULL, "Created cube");
    
    // 3D polytope should not have 3-faces
    KFaceSet* cells = higher_faces_generate_k_faces(solid, 3);
    ASSERT(cells == NULL, "3D polytope has no 3-faces");
    
    free_test_solid(solid);
}

// ============================================================================
// TESTS: FACE HIERARCHY
// ============================================================================

TEST(test_hierarchy_tetrahedron) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created tetrahedron");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    ASSERT(hierarchy->polytope_dimension == 3, "Hierarchy dimension is 3");
    ASSERT(hierarchy->num_dimensions == 3, "Has 3 face dimensions (0,1,2)");
    
    // Check 0-faces
    ASSERT(hierarchy->face_sets[0] != NULL, "Has 0-faces");
    ASSERT(hierarchy->face_sets[0]->count == 4, "4 vertices");
    
    // Check 1-faces
    ASSERT(hierarchy->face_sets[1] != NULL, "Has 1-faces");
    ASSERT(hierarchy->face_sets[1]->count == 6, "6 edges");
    
    // Check 2-faces
    ASSERT(hierarchy->face_sets[2] != NULL, "Has 2-faces");
    ASSERT(hierarchy->face_sets[2]->count == 4, "4 faces");
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

TEST(test_hierarchy_cube) {
    PlatonicSolid* solid = create_test_cube();
    ASSERT(solid != NULL, "Created cube");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    
    ASSERT(hierarchy->face_sets[0]->count == 8, "8 vertices");
    ASSERT(hierarchy->face_sets[1]->count == 12, "12 edges");
    ASSERT(hierarchy->face_sets[2]->count == 6, "6 faces");
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

TEST(test_hierarchy_tesseract) {
    PlatonicSolid* solid = create_test_tesseract();
    ASSERT(solid != NULL, "Created tesseract");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    ASSERT(hierarchy->polytope_dimension == 4, "Hierarchy dimension is 4");
    ASSERT(hierarchy->num_dimensions == 4, "Has 4 face dimensions (0,1,2,3)");
    
    ASSERT(hierarchy->face_sets[0]->count == 16, "16 vertices");
    ASSERT(hierarchy->face_sets[1]->count == 32, "32 edges");
    ASSERT(hierarchy->face_sets[2]->count == 24, "24 faces");
    ASSERT(hierarchy->face_sets[3]->count == 8, "8 cells");
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

// ============================================================================
// TESTS: FACE CONNECTIVITY
// ============================================================================

TEST(test_connectivity_subfaces) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created tetrahedron");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    
    // Check that 2-faces have 1-faces as subfaces
    KFaceSet* faces = hierarchy->face_sets[2];
    for (uint32_t i = 0; i < faces->count; i++) {
        KFace* face = faces->faces[i];
        ASSERT(face->num_subfaces == 3, "Triangle has 3 edges as subfaces");
    }
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

TEST(test_connectivity_adjacent) {
    PlatonicSolid* solid = create_test_cube();
    ASSERT(solid != NULL, "Created cube");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    
    // Check adjacent faces (faces that share an edge)
    KFaceSet* faces = hierarchy->face_sets[2];
    uint32_t adjacent[10];
    
    for (uint32_t i = 0; i < faces->count; i++) {
        KFace* face = faces->faces[i];
        uint32_t num_adj = higher_faces_get_adjacent(face, hierarchy, adjacent, 10);
        ASSERT(num_adj == 4, "Each cube face is adjacent to 4 other faces");
    }
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

// ============================================================================
// TESTS: FACE ENUMERATION
// ============================================================================

static uint32_t enumeration_count = 0;

static void count_callback(const KFace* face, void* user_data) {
    (void)face;
    (void)user_data;
    enumeration_count++;
}

TEST(test_enumeration) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created tetrahedron");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    
    // Enumerate vertices
    enumeration_count = 0;
    higher_faces_enumerate(hierarchy, 0, count_callback, NULL);
    ASSERT(enumeration_count == 4, "Enumerated 4 vertices");
    
    // Enumerate edges
    enumeration_count = 0;
    higher_faces_enumerate(hierarchy, 1, count_callback, NULL);
    ASSERT(enumeration_count == 6, "Enumerated 6 edges");
    
    // Enumerate faces
    enumeration_count = 0;
    higher_faces_enumerate(hierarchy, 2, count_callback, NULL);
    ASSERT(enumeration_count == 4, "Enumerated 4 faces");
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

TEST(test_face_count) {
    PlatonicSolid* solid = create_test_cube();
    ASSERT(solid != NULL, "Created cube");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    
    ASSERT(higher_faces_count(hierarchy, 0) == 8, "Count 8 vertices");
    ASSERT(higher_faces_count(hierarchy, 1) == 12, "Count 12 edges");
    ASSERT(higher_faces_count(hierarchy, 2) == 6, "Count 6 faces");
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

// ============================================================================
// TESTS: VALIDATION
// ============================================================================

TEST(test_hierarchy_validation) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created tetrahedron");
    
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    ASSERT(hierarchy != NULL, "Generated face hierarchy");
    
    bool valid = higher_faces_validate_hierarchy(hierarchy);
    ASSERT(valid, "Hierarchy is valid");
    
    higher_faces_free_hierarchy(hierarchy);
    free_test_solid(solid);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Higher-Dimensional Face Generation Tests\n");
    printf("========================================\n");
    
    // 0-face tests
    RUN_TEST(test_0_faces_tetrahedron);
    RUN_TEST(test_0_faces_cube);
    
    // 1-face tests
    RUN_TEST(test_1_faces_tetrahedron);
    RUN_TEST(test_1_faces_cube);
    
    // 2-face tests
    RUN_TEST(test_2_faces_tetrahedron);
    RUN_TEST(test_2_faces_cube);
    
    // 3-face tests
    RUN_TEST(test_3_faces_tesseract);
    RUN_TEST(test_3_faces_invalid_3d);
    
    // Hierarchy tests
    RUN_TEST(test_hierarchy_tetrahedron);
    RUN_TEST(test_hierarchy_cube);
    RUN_TEST(test_hierarchy_tesseract);
    
    // Connectivity tests
    RUN_TEST(test_connectivity_subfaces);
    RUN_TEST(test_connectivity_adjacent);
    
    // Enumeration tests
    RUN_TEST(test_enumeration);
    RUN_TEST(test_face_count);
    
    // Validation tests
    RUN_TEST(test_hierarchy_validation);
    
    printf("\n");
    printf("========================================\n");
    printf("TEST SUMMARY\n");
    printf("========================================\n");
    printf("Tests run:        %d\n", tests_run);
    printf("Tests passed:     %d\n", tests_passed);
    printf("Assertions:       %d\n", assertions_run);
    printf("Success rate:     %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("========================================\n");
    
    if (tests_passed == tests_run) {
        printf("\n✓ ALL TESTS PASSED!\n\n");
        return 0;
    } else {
        printf("\n✗ SOME TESTS FAILED\n\n");
        return 1;
    }
}