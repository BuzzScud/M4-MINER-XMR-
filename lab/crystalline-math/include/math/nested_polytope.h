#ifndef MATH_NESTED_POLYTOPE_H
#define MATH_NESTED_POLYTOPE_H

/**
 * @file nested_polytope.h
 * @brief Hierarchical nesting of polytopes within polytopes
 * 
 * This module enables polytopes to contain other polytopes in a hierarchical
 * tree structure, creating fractal-like geometric patterns. This is essential
 * for the Crystalline CLLM architecture where geometric structures nest
 * recursively to create multi-scale representations.
 * 
 * Key Features:
 * - Tree structure with parent/child relationships
 * - Coordinate transformations for nested polytopes
 * - Depth tracking and level-order traversal
 * - Scaling and positioning of nested polytopes
 * - Preservation of 12-fold symmetry
 * - Support for arbitrary nesting patterns
 * 
 * Example:
 * - A cube containing 8 tetrahedra at its vertices
 * - A tesseract containing 16 cubes at its vertices
 * - Recursive nesting to arbitrary depth
 */

#include "math/polytope.h"
#include "math/higher_faces.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// NESTING STRUCTURES
// ============================================================================

/**
 * @brief Nesting placement strategy
 */
typedef enum {
    NEST_AT_VERTICES,        /**< Place children at parent vertices */
    NEST_AT_EDGE_CENTERS,    /**< Place children at edge midpoints */
    NEST_AT_FACE_CENTERS,    /**< Place children at face centers */
    NEST_AT_CELL_CENTERS,    /**< Place children at cell centers (4D+) */
    NEST_AT_CENTER,          /**< Place single child at polytope center */
    NEST_CUSTOM              /**< Custom placement positions */
} NestingStrategy;

/**
 * @brief Transformation for nested polytope
 */
typedef struct {
    double* translation;     /**< Translation vector [dimension] */
    double scale;            /**< Uniform scale factor */
    double* rotation;        /**< Rotation matrix [dimension × dimension] */
    uint32_t dimension;      /**< Dimension of transformation */
} NestedTransform;

/**
 * @brief A node in the polytope nesting tree
 */
typedef struct NestedPolytopeNode {
    // Polytope data
    PlatonicSolid* polytope;              /**< The polytope at this node */
    
    // Tree structure
    struct NestedPolytopeNode* parent;    /**< Parent node (NULL for root) */
    struct NestedPolytopeNode** children; /**< Array of child nodes */
    uint32_t num_children;                /**< Number of children */
    uint32_t max_children;                /**< Allocated capacity for children */
    
    // Nesting metadata
    uint32_t depth;                       /**< Depth in tree (0 for root) */
    uint32_t index;                       /**< Index among siblings */
    NestingStrategy strategy;             /**< How this node was nested */
    
    // Transformation
    NestedTransform* transform;           /**< Transform from parent space */
    
    // Computed properties
    double* world_position;               /**< Position in world coordinates */
    double world_scale;                   /**< Cumulative scale from root */
    
    // Metadata
    char label[64];                       /**< Optional label for this node */
    void* user_data;                      /**< User-defined data */
    
} NestedPolytopeNode;

/**
 * @brief Complete hierarchical polytope tree
 */
typedef struct {
    NestedPolytopeNode* root;             /**< Root node */
    uint32_t total_nodes;                 /**< Total nodes in tree */
    uint32_t max_depth;                   /**< Maximum depth */
    uint32_t dimension;                   /**< Dimension of all polytopes */
    
    // Tree properties
    bool preserves_symmetry;              /**< Does nesting preserve symmetry? */
    bool is_balanced;                     /**< Is tree balanced? */
    
    // Statistics
    uint32_t* nodes_per_level;            /**< Nodes at each depth level */
    uint32_t num_levels;                  /**< Number of depth levels */
    
} NestedPolytopeTree;

// ============================================================================
// TREE CREATION
// ============================================================================

/**
 * @brief Create a new nested polytope tree with a root
 * 
 * @param root_polytope Root polytope (ownership transferred)
 * @return New tree, or NULL on error
 */
NestedPolytopeTree* nested_polytope_create_tree(PlatonicSolid* root_polytope);

/**
 * @brief Create a new node
 * 
 * @param polytope Polytope for this node (ownership transferred)
 * @return New node, or NULL on error
 */
NestedPolytopeNode* nested_polytope_create_node(PlatonicSolid* polytope);

// ============================================================================
// NESTING OPERATIONS
// ============================================================================

/**
 * @brief Nest a child polytope within a parent
 * 
 * @param parent Parent node
 * @param child Child polytope (ownership transferred)
 * @param strategy Nesting strategy
 * @param scale Scale factor for child
 * @return Child node, or NULL on error
 */
NestedPolytopeNode* nested_polytope_add_child(
    NestedPolytopeNode* parent,
    PlatonicSolid* child,
    NestingStrategy strategy,
    double scale
);

/**
 * @brief Nest multiple children using a strategy
 * 
 * Creates one child per placement point (e.g., one per vertex)
 * 
 * @param parent Parent node
 * @param child_template Template polytope to replicate
 * @param strategy Nesting strategy
 * @param scale Scale factor for children
 * @return Number of children added
 */
uint32_t nested_polytope_add_children_strategy(
    NestedPolytopeNode* parent,
    const PlatonicSolid* child_template,
    NestingStrategy strategy,
    double scale
);

/**
 * @brief Nest a child at a custom position
 * 
 * @param parent Parent node
 * @param child Child polytope (ownership transferred)
 * @param position Position in parent's coordinate system
 * @param scale Scale factor
 * @return Child node, or NULL on error
 */
NestedPolytopeNode* nested_polytope_add_child_at_position(
    NestedPolytopeNode* parent,
    PlatonicSolid* child,
    const double* position,
    double scale
);

/**
 * @brief Remove a child from parent
 * 
 * @param parent Parent node
 * @param child_index Index of child to remove
 * @return true if removed successfully
 */
bool nested_polytope_remove_child(NestedPolytopeNode* parent, uint32_t child_index);

// ============================================================================
// TREE TRAVERSAL
// ============================================================================

/**
 * @brief Callback for tree traversal
 */
typedef void (*NestedPolytopeCallback)(NestedPolytopeNode* node, void* user_data);

/**
 * @brief Traverse tree in depth-first order (pre-order)
 * 
 * @param node Starting node
 * @param callback Function to call for each node
 * @param user_data User data passed to callback
 */
void nested_polytope_traverse_dfs(
    NestedPolytopeNode* node,
    NestedPolytopeCallback callback,
    void* user_data
);

/**
 * @brief Traverse tree in breadth-first order (level-order)
 * 
 * @param tree Tree to traverse
 * @param callback Function to call for each node
 * @param user_data User data passed to callback
 */
void nested_polytope_traverse_bfs(
    NestedPolytopeTree* tree,
    NestedPolytopeCallback callback,
    void* user_data
);

/**
 * @brief Traverse nodes at a specific depth level
 * 
 * @param tree Tree to traverse
 * @param depth Depth level to traverse
 * @param callback Function to call for each node
 * @param user_data User data passed to callback
 */
void nested_polytope_traverse_level(
    NestedPolytopeTree* tree,
    uint32_t depth,
    NestedPolytopeCallback callback,
    void* user_data
);

// ============================================================================
// COORDINATE TRANSFORMATIONS
// ============================================================================

/**
 * @brief Compute world coordinates for a node
 * 
 * Applies all transformations from root to this node
 * 
 * @param node Node to compute coordinates for
 * @return true if successful
 */
bool nested_polytope_compute_world_coords(NestedPolytopeNode* node);

/**
 * @brief Transform a point from node's local space to world space
 * 
 * @param node Node defining local space
 * @param local_point Point in local coordinates
 * @param world_point Output point in world coordinates
 * @return true if successful
 */
bool nested_polytope_local_to_world(
    const NestedPolytopeNode* node,
    const double* local_point,
    double* world_point
);

/**
 * @brief Transform a point from world space to node's local space
 * 
 * @param node Node defining local space
 * @param world_point Point in world coordinates
 * @param local_point Output point in local coordinates
 * @return true if successful
 */
bool nested_polytope_world_to_local(
    const NestedPolytopeNode* node,
    const double* world_point,
    double* local_point
);

/**
 * @brief Update all world coordinates in tree
 * 
 * @param tree Tree to update
 * @return true if successful
 */
bool nested_polytope_update_all_coords(NestedPolytopeTree* tree);

// ============================================================================
// TREE PROPERTIES
// ============================================================================

/**
 * @brief Get total number of nodes in tree
 * 
 * @param tree Tree to query
 * @return Number of nodes
 */
uint32_t nested_polytope_count_nodes(const NestedPolytopeTree* tree);

/**
 * @brief Get maximum depth of tree
 * 
 * @param tree Tree to query
 * @return Maximum depth
 */
uint32_t nested_polytope_get_max_depth(const NestedPolytopeTree* tree);

/**
 * @brief Get number of nodes at a specific depth
 * 
 * @param tree Tree to query
 * @param depth Depth level
 * @return Number of nodes at that depth
 */
uint32_t nested_polytope_count_at_depth(const NestedPolytopeTree* tree, uint32_t depth);

/**
 * @brief Check if tree is balanced
 * 
 * A tree is balanced if all leaves are at the same depth or differ by at most 1
 * 
 * @param tree Tree to check
 * @return true if balanced
 */
bool nested_polytope_is_balanced(const NestedPolytopeTree* tree);

/**
 * @brief Get all nodes at a specific depth
 * 
 * @param tree Tree to query
 * @param depth Depth level
 * @param nodes Output array of nodes
 * @param max_nodes Maximum nodes to return
 * @return Number of nodes returned
 */
uint32_t nested_polytope_get_nodes_at_depth(
    const NestedPolytopeTree* tree,
    uint32_t depth,
    NestedPolytopeNode** nodes,
    uint32_t max_nodes
);

// ============================================================================
// SYMMETRY PRESERVATION
// ============================================================================

/**
 * @brief Check if nesting preserves 12-fold symmetry
 * 
 * @param tree Tree to check
 * @return true if symmetry is preserved
 */
bool nested_polytope_preserves_12fold_symmetry(const NestedPolytopeTree* tree);

/**
 * @brief Verify symmetry at a specific node
 * 
 * @param node Node to check
 * @return true if node's nesting preserves symmetry
 */
bool nested_polytope_verify_node_symmetry(const NestedPolytopeNode* node);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * @brief Validate tree structure
 * 
 * Checks:
 * - Parent-child relationships are consistent
 * - No cycles in tree
 * - All transformations are valid
 * - Depth values are correct
 * 
 * @param tree Tree to validate
 * @return true if valid
 */
bool nested_polytope_validate_tree(const NestedPolytopeTree* tree);

/**
 * @brief Validate a single node
 * 
 * @param node Node to validate
 * @return true if valid
 */
bool nested_polytope_validate_node(const NestedPolytopeNode* node);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Free a node and all its children recursively
 * 
 * @param node Node to free
 */
void nested_polytope_free_node(NestedPolytopeNode* node);

/**
 * @brief Free entire tree
 * 
 * @param tree Tree to free
 */
void nested_polytope_free_tree(NestedPolytopeTree* tree);

/**
 * @brief Free a transform
 * 
 * @param transform Transform to free
 */
void nested_polytope_free_transform(NestedTransform* transform);

// ============================================================================
// PRINTING & DEBUGGING
// ============================================================================

/**
 * @brief Print node information
 * 
 * @param node Node to print
 */
void nested_polytope_print_node(const NestedPolytopeNode* node);

/**
 * @brief Print tree structure
 * 
 * @param tree Tree to print
 */
void nested_polytope_print_tree(const NestedPolytopeTree* tree);

/**
 * @brief Print tree in hierarchical format
 * 
 * @param node Starting node
 * @param indent Indentation level
 */
void nested_polytope_print_hierarchy(const NestedPolytopeNode* node, uint32_t indent);

/**
 * @brief Print tree statistics
 * 
 * @param tree Tree to analyze
 */
void nested_polytope_print_statistics(const NestedPolytopeTree* tree);

#ifdef __cplusplus
}
#endif

#endif /* MATH_NESTED_POLYTOPE_H */