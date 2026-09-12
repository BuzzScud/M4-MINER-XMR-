/**
 * @file test_nested_polytope.c
 * @brief Comprehensive test suite for hierarchical polytope nesting
 */

#include "math/nested_polytope.h"
#include "math/polytope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Use our own fabs implementation (no math.h)
static inline double fabs(double x) {
    return x < 0.0 ? -x : x;
}

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
    solid->num_heads = 12;  // For 12-fold symmetry
    
    solid->symbol_length = 2;
    solid->schlafli_symbol = malloc(2 * sizeof(uint32_t));
    solid->schlafli_symbol[0] = 3;
    solid->schlafli_symbol[1] = 3;
    
    solid->vertex_coords = malloc(4 * 3 * sizeof(double));
    double verts[4][3] = {
        {1.0, 1.0, 1.0},
        {1.0, -1.0, -1.0},
        {-1.0, 1.0, -1.0},
        {-1.0, -1.0, 1.0}
    };
    memcpy(solid->vertex_coords, verts, 4 * 3 * sizeof(double));
    
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
    solid->num_heads = 12;
    
    solid->symbol_length = 2;
    solid->schlafli_symbol = malloc(2 * sizeof(uint32_t));
    solid->schlafli_symbol[0] = 4;
    solid->schlafli_symbol[1] = 3;
    
    solid->vertex_coords = malloc(8 * 3 * sizeof(double));
    double verts[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
    };
    memcpy(solid->vertex_coords, verts, 8 * 3 * sizeof(double));
    
    strcpy(solid->name, "Cube");
    solid->is_valid = true;
    solid->is_regular = true;
    
    return solid;
}

// ============================================================================
// TESTS: TREE CREATION
// ============================================================================

TEST(test_create_tree) {
    PlatonicSolid* root = create_test_cube();
    ASSERT(root != NULL, "Created root polytope");
    
    NestedPolytopeTree* tree = nested_polytope_create_tree(root);
    ASSERT(tree != NULL, "Created tree");
    ASSERT(tree->root != NULL, "Tree has root node");
    ASSERT(tree->root->polytope == root, "Root node has correct polytope");
    ASSERT(tree->dimension == 3, "Tree dimension is 3");
    ASSERT(tree->total_nodes == 1, "Tree has 1 node");
    ASSERT(tree->max_depth == 0, "Max depth is 0");
    
    nested_polytope_free_tree(tree);
}

TEST(test_create_node) {
    PlatonicSolid* solid = create_test_tetrahedron();
    ASSERT(solid != NULL, "Created polytope");
    
    NestedPolytopeNode* node = nested_polytope_create_node(solid);
    ASSERT(node != NULL, "Created node");
    ASSERT(node->polytope == solid, "Node has correct polytope");
    ASSERT(node->parent == NULL, "Node has no parent");
    ASSERT(node->num_children == 0, "Node has no children");
    ASSERT(node->depth == 0, "Node depth is 0");
    ASSERT(node->transform != NULL, "Node has transform");
    ASSERT(node->world_position != NULL, "Node has world position");
    
    nested_polytope_free_node(node);
}

// ============================================================================
// TESTS: ADDING CHILDREN
// ============================================================================

TEST(test_add_single_child) {
    PlatonicSolid* parent_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(parent_solid);
    ASSERT(tree != NULL, "Created tree");
    
    PlatonicSolid* child_solid = create_test_tetrahedron();
    NestedPolytopeNode* child = nested_polytope_add_child(
        tree->root, child_solid, NEST_AT_CENTER, 0.5);
    
    ASSERT(child != NULL, "Added child");
    ASSERT(tree->root->num_children == 1, "Parent has 1 child");
    ASSERT(child->parent == tree->root, "Child has correct parent");
    ASSERT(child->depth == 1, "Child depth is 1");
    ASSERT(child->transform->scale == 0.5, "Child scale is 0.5");
    
    nested_polytope_free_tree(tree);
}

TEST(test_add_multiple_children) {
    PlatonicSolid* parent_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(parent_solid);
    ASSERT(tree != NULL, "Created tree");
    
    // Add 3 children
    for (int i = 0; i < 3; i++) {
        PlatonicSolid* child_solid = create_test_tetrahedron();
        NestedPolytopeNode* child = nested_polytope_add_child(
            tree->root, child_solid, NEST_AT_CENTER, 0.3);
        ASSERT(child != NULL, "Added child");
    }
    
    ASSERT(tree->root->num_children == 3, "Parent has 3 children");
    
    // Check child indices
    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(tree->root->children[i]->index == i, "Child index is correct");
    }
    
    nested_polytope_free_tree(tree);
}

TEST(test_add_children_at_vertices) {
    PlatonicSolid* parent_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(parent_solid);
    ASSERT(tree != NULL, "Created tree");
    
    PlatonicSolid* child_template = create_test_tetrahedron();
    uint32_t num_added = nested_polytope_add_children_strategy(
        tree->root, child_template, NEST_AT_VERTICES, 0.2);
    
    ASSERT(num_added == 8, "Added 8 children (one per vertex)");
    ASSERT(tree->root->num_children == 8, "Parent has 8 children");
    
    // Cleanup template
    free(child_template->schlafli_symbol);
    free(child_template->vertex_coords);
    free(child_template);
    
    nested_polytope_free_tree(tree);
}

TEST(test_add_child_at_custom_position) {
    PlatonicSolid* parent_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(parent_solid);
    ASSERT(tree != NULL, "Created tree");
    
    double position[3] = {0.5, 0.5, 0.5};
    PlatonicSolid* child_solid = create_test_tetrahedron();
    NestedPolytopeNode* child = nested_polytope_add_child_at_position(
        tree->root, child_solid, position, 0.3);
    
    ASSERT(child != NULL, "Added child at custom position");
    ASSERT(child->transform->translation[0] == 0.5, "X position correct");
    ASSERT(child->transform->translation[1] == 0.5, "Y position correct");
    ASSERT(child->transform->translation[2] == 0.5, "Z position correct");
    
    nested_polytope_free_tree(tree);
}

// ============================================================================
// TESTS: TREE TRAVERSAL
// ============================================================================

static uint32_t traversal_count = 0;
static uint32_t traversal_sum_depth = 0;

static void count_callback(NestedPolytopeNode* node, void* user_data) {
    (void)user_data;
    traversal_count++;
    traversal_sum_depth += node->depth;
}

TEST(test_traverse_dfs) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Add children at two levels
    for (int i = 0; i < 2; i++) {
        PlatonicSolid* child1 = create_test_tetrahedron();
        NestedPolytopeNode* node1 = nested_polytope_add_child(
            tree->root, child1, NEST_AT_CENTER, 0.5);
        
        PlatonicSolid* child2 = create_test_tetrahedron();
        nested_polytope_add_child(node1, child2, NEST_AT_CENTER, 0.25);
    }
    
    traversal_count = 0;
    nested_polytope_traverse_dfs(tree->root, count_callback, NULL);
    
    ASSERT(traversal_count == 5, "DFS traversed 5 nodes (1 root + 2 level1 + 2 level2)");
    
    nested_polytope_free_tree(tree);
}

TEST(test_traverse_bfs) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Add children
    for (int i = 0; i < 3; i++) {
        PlatonicSolid* child = create_test_tetrahedron();
        nested_polytope_add_child(tree->root, child, NEST_AT_CENTER, 0.4);
    }
    
    traversal_count = 0;
    nested_polytope_traverse_bfs(tree, count_callback, NULL);
    
    ASSERT(traversal_count == 4, "BFS traversed 4 nodes");
    
    nested_polytope_free_tree(tree);
}

TEST(test_traverse_level) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Add 2 children at level 1
    for (int i = 0; i < 2; i++) {
        PlatonicSolid* child = create_test_tetrahedron();
        nested_polytope_add_child(tree->root, child, NEST_AT_CENTER, 0.5);
    }
    
    // Traverse level 0
    traversal_count = 0;
    nested_polytope_traverse_level(tree, 0, count_callback, NULL);
    ASSERT(traversal_count == 1, "Level 0 has 1 node");
    
    // Traverse level 1
    traversal_count = 0;
    nested_polytope_traverse_level(tree, 1, count_callback, NULL);
    ASSERT(traversal_count == 2, "Level 1 has 2 nodes");
    
    nested_polytope_free_tree(tree);
}

// ============================================================================
// TESTS: COORDINATE TRANSFORMATIONS
// ============================================================================

TEST(test_world_coordinates) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    double position[3] = {1.0, 0.0, 0.0};
    PlatonicSolid* child_solid = create_test_tetrahedron();
    NestedPolytopeNode* child = nested_polytope_add_child_at_position(
        tree->root, child_solid, position, 0.5);
    
    bool success = nested_polytope_compute_world_coords(child);
    ASSERT(success, "Computed world coordinates");
    ASSERT(child->world_scale == 0.5, "World scale is 0.5");
    ASSERT(fabs(child->world_position[0] - 1.0) < 0.001, "World X position is 1.0");
    
    nested_polytope_free_tree(tree);
}

TEST(test_local_to_world_transform) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    double position[3] = {2.0, 0.0, 0.0};
    PlatonicSolid* child_solid = create_test_tetrahedron();
    NestedPolytopeNode* child = nested_polytope_add_child_at_position(
        tree->root, child_solid, position, 0.5);
    
    nested_polytope_compute_world_coords(child);
    
    double local[3] = {1.0, 0.0, 0.0};
    double world[3];
    bool success = nested_polytope_local_to_world(child, local, world);
    
    ASSERT(success, "Transformed local to world");
    ASSERT(fabs(world[0] - 2.5) < 0.001, "World X = 2.0 + 0.5*1.0 = 2.5");
    
    nested_polytope_free_tree(tree);
}

TEST(test_world_to_local_transform) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    double position[3] = {1.0, 0.0, 0.0};
    PlatonicSolid* child_solid = create_test_tetrahedron();
    NestedPolytopeNode* child = nested_polytope_add_child_at_position(
        tree->root, child_solid, position, 0.5);
    
    nested_polytope_compute_world_coords(child);
    
    double world[3] = {2.0, 0.0, 0.0};
    double local[3];
    bool success = nested_polytope_world_to_local(child, world, local);
    
    ASSERT(success, "Transformed world to local");
    ASSERT(fabs(local[0] - 2.0) < 0.001, "Local X = (2.0 - 1.0) / 0.5 = 2.0");
    
    nested_polytope_free_tree(tree);
}

// ============================================================================
// TESTS: TREE PROPERTIES
// ============================================================================

TEST(test_count_nodes) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Add 3 children
    for (int i = 0; i < 3; i++) {
        PlatonicSolid* child = create_test_tetrahedron();
        nested_polytope_add_child(tree->root, child, NEST_AT_CENTER, 0.4);
    }
    
    uint32_t count = nested_polytope_count_nodes(tree);
    ASSERT(count == 4, "Tree has 4 nodes");
    
    nested_polytope_free_tree(tree);
}

TEST(test_max_depth) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Create 3-level tree
    PlatonicSolid* child1 = create_test_tetrahedron();
    NestedPolytopeNode* node1 = nested_polytope_add_child(
        tree->root, child1, NEST_AT_CENTER, 0.5);
    
    PlatonicSolid* child2 = create_test_tetrahedron();
    nested_polytope_add_child(node1, child2, NEST_AT_CENTER, 0.25);
    
    uint32_t max_depth = nested_polytope_get_max_depth(tree);
    ASSERT(max_depth == 2, "Max depth is 2");
    
    nested_polytope_free_tree(tree);
}

TEST(test_is_balanced) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Add children at same depth
    for (int i = 0; i < 2; i++) {
        PlatonicSolid* child = create_test_tetrahedron();
        nested_polytope_add_child(tree->root, child, NEST_AT_CENTER, 0.5);
    }
    
    bool balanced = nested_polytope_is_balanced(tree);
    ASSERT(balanced, "Tree is balanced");
    
    nested_polytope_free_tree(tree);
}

TEST(test_count_at_depth) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Add 3 children at level 1
    for (int i = 0; i < 3; i++) {
        PlatonicSolid* child = create_test_tetrahedron();
        nested_polytope_add_child(tree->root, child, NEST_AT_CENTER, 0.4);
    }
    
    uint32_t count0 = nested_polytope_count_at_depth(tree, 0);
    uint32_t count1 = nested_polytope_count_at_depth(tree, 1);
    
    ASSERT(count0 == 1, "Level 0 has 1 node");
    ASSERT(count1 == 3, "Level 1 has 3 nodes");
    
    nested_polytope_free_tree(tree);
}

// ============================================================================
// TESTS: SYMMETRY
// ============================================================================

TEST(test_12fold_symmetry) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    bool preserves = nested_polytope_preserves_12fold_symmetry(tree);
    ASSERT(preserves, "Tree preserves 12-fold symmetry");
    
    nested_polytope_free_tree(tree);
}

TEST(test_node_symmetry) {
    PlatonicSolid* solid = create_test_tetrahedron();
    NestedPolytopeNode* node = nested_polytope_create_node(solid);
    
    bool symmetric = nested_polytope_verify_node_symmetry(node);
    ASSERT(symmetric, "Node has 12-fold symmetry");
    
    nested_polytope_free_node(node);
}

// ============================================================================
// TESTS: VALIDATION
// ============================================================================

TEST(test_validate_tree) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Add some children
    for (int i = 0; i < 2; i++) {
        PlatonicSolid* child = create_test_tetrahedron();
        nested_polytope_add_child(tree->root, child, NEST_AT_CENTER, 0.5);
    }
    
    bool valid = nested_polytope_validate_tree(tree);
    ASSERT(valid, "Tree is valid");
    
    nested_polytope_free_tree(tree);
}

TEST(test_validate_node) {
    PlatonicSolid* solid = create_test_tetrahedron();
    NestedPolytopeNode* node = nested_polytope_create_node(solid);
    
    bool valid = nested_polytope_validate_node(node);
    ASSERT(valid, "Node is valid");
    
    nested_polytope_free_node(node);
}

// ============================================================================
// TESTS: DEEP NESTING
// ============================================================================

TEST(test_deep_nesting) {
    PlatonicSolid* root_solid = create_test_cube();
    NestedPolytopeTree* tree = nested_polytope_create_tree(root_solid);
    
    // Create 10-level deep nesting
    NestedPolytopeNode* current = tree->root;
    for (int i = 0; i < 10; i++) {
        PlatonicSolid* child = create_test_tetrahedron();
        current = nested_polytope_add_child(current, child, NEST_AT_CENTER, 0.8);
        ASSERT(current != NULL, "Added child at depth");
    }
    
    uint32_t max_depth = nested_polytope_get_max_depth(tree);
    ASSERT(max_depth == 10, "Max depth is 10");
    
    uint32_t total = nested_polytope_count_nodes(tree);
    ASSERT(total == 11, "Total nodes is 11");
    
    nested_polytope_free_tree(tree);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Hierarchical Polytope Nesting Tests\n");
    printf("========================================\n");
    
    // Tree creation tests
    RUN_TEST(test_create_tree);
    RUN_TEST(test_create_node);
    
    // Adding children tests
    RUN_TEST(test_add_single_child);
    RUN_TEST(test_add_multiple_children);
    RUN_TEST(test_add_children_at_vertices);
    RUN_TEST(test_add_child_at_custom_position);
    
    // Traversal tests
    RUN_TEST(test_traverse_dfs);
    RUN_TEST(test_traverse_bfs);
    RUN_TEST(test_traverse_level);
    
    // Coordinate transformation tests
    RUN_TEST(test_world_coordinates);
    RUN_TEST(test_local_to_world_transform);
    RUN_TEST(test_world_to_local_transform);
    
    // Tree property tests
    RUN_TEST(test_count_nodes);
    RUN_TEST(test_max_depth);
    RUN_TEST(test_is_balanced);
    RUN_TEST(test_count_at_depth);
    
    // Symmetry tests
    RUN_TEST(test_12fold_symmetry);
    RUN_TEST(test_node_symmetry);
    
    // Validation tests
    RUN_TEST(test_validate_tree);
    RUN_TEST(test_validate_node);
    
    // Deep nesting test
    RUN_TEST(test_deep_nesting);
    
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