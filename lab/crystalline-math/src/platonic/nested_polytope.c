/**
 * @file nested_polytope.c
 * @brief Implementation of hierarchical polytope nesting
 */

#include "math/nested_polytope.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Create identity transformation
 */
static NestedTransform* create_identity_transform(uint32_t dimension) {
    NestedTransform* transform = calloc(1, sizeof(NestedTransform));
    if (!transform) return NULL;
    
    transform->dimension = dimension;
    transform->scale = 1.0;
    
    transform->translation = calloc(dimension, sizeof(double));
    if (!transform->translation) {
        free(transform);
        return NULL;
    }
    
    transform->rotation = calloc(dimension * dimension, sizeof(double));
    if (!transform->rotation) {
        free(transform->translation);
        free(transform);
        return NULL;
    }
    
    // Identity rotation matrix
    for (uint32_t i = 0; i < dimension; i++) {
        transform->rotation[i * dimension + i] = 1.0;
    }
    
    return transform;
}

/**
 * @brief Compute depth of a node
 */
static uint32_t compute_node_depth(const NestedPolytopeNode* node) {
    uint32_t depth = 0;
    const NestedPolytopeNode* current = node;
    while (current->parent) {
        depth++;
        current = current->parent;
    }
    return depth;
}

// ============================================================================
// TREE CREATION
// ============================================================================

NestedPolytopeTree* nested_polytope_create_tree(PlatonicSolid* root_polytope) {
    if (!root_polytope) return NULL;
    
    NestedPolytopeTree* tree = calloc(1, sizeof(NestedPolytopeTree));
    if (!tree) return NULL;
    
    tree->root = nested_polytope_create_node(root_polytope);
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    
    tree->dimension = root_polytope->dimension;
    tree->total_nodes = 1;
    tree->max_depth = 0;
    tree->preserves_symmetry = true;
    tree->is_balanced = true;
    
    tree->num_levels = 1;
    tree->nodes_per_level = malloc(sizeof(uint32_t));
    if (tree->nodes_per_level) {
        tree->nodes_per_level[0] = 1;
    }
    
    return tree;
}

NestedPolytopeNode* nested_polytope_create_node(PlatonicSolid* polytope) {
    if (!polytope) return NULL;
    
    NestedPolytopeNode* node = calloc(1, sizeof(NestedPolytopeNode));
    if (!node) return NULL;
    
    node->polytope = polytope;
    node->parent = NULL;
    node->children = NULL;
    node->num_children = 0;
    node->max_children = 0;
    node->depth = 0;
    node->index = 0;
    node->strategy = NEST_AT_CENTER;
    
    node->transform = create_identity_transform(polytope->dimension);
    if (!node->transform) {
        free(node);
        return NULL;
    }
    
    node->world_position = calloc(polytope->dimension, sizeof(double));
    if (!node->world_position) {
        nested_polytope_free_transform(node->transform);
        free(node);
        return NULL;
    }
    
    node->world_scale = 1.0;
    node->user_data = NULL;
    strcpy(node->label, "");
    
    return node;
}

// ============================================================================
// NESTING OPERATIONS
// ============================================================================

NestedPolytopeNode* nested_polytope_add_child(
    NestedPolytopeNode* parent,
    PlatonicSolid* child,
    NestingStrategy strategy,
    double scale)
{
    if (!parent || !child) return NULL;
    
    // Create child node
    NestedPolytopeNode* child_node = nested_polytope_create_node(child);
    if (!child_node) return NULL;
    
    // Set parent relationship
    child_node->parent = parent;
    child_node->depth = parent->depth + 1;
    child_node->index = parent->num_children;
    child_node->strategy = strategy;
    
    // Update transform
    child_node->transform->scale = scale;
    
    // Expand parent's children array if needed
    if (parent->num_children >= parent->max_children) {
        uint32_t new_capacity = parent->max_children == 0 ? 4 : parent->max_children * 2;
        NestedPolytopeNode** new_children = realloc(parent->children, 
                                                     new_capacity * sizeof(NestedPolytopeNode*));
        if (!new_children) {
            nested_polytope_free_node(child_node);
            return NULL;
        }
        parent->children = new_children;
        parent->max_children = new_capacity;
    }
    
    // Add child to parent
    parent->children[parent->num_children] = child_node;
    parent->num_children++;
    
    return child_node;
}

uint32_t nested_polytope_add_children_strategy(
    NestedPolytopeNode* parent,
    const PlatonicSolid* child_template,
    NestingStrategy strategy,
    double scale)
{
    if (!parent || !child_template) return 0;
    
    uint32_t num_positions = 0;
    double** positions = NULL;
    
    // Determine positions based on strategy
    switch (strategy) {
        case NEST_AT_VERTICES:
            num_positions = parent->polytope->num_vertices;
            positions = malloc(num_positions * sizeof(double*));
            if (!positions) return 0;
            
            for (uint32_t i = 0; i < num_positions; i++) {
                positions[i] = &parent->polytope->vertex_coords[i * parent->polytope->dimension];
            }
            break;
            
        case NEST_AT_CENTER:
            num_positions = 1;
            positions = malloc(sizeof(double*));
            if (!positions) return 0;
            
            // Compute center (average of vertices)
            positions[0] = calloc(parent->polytope->dimension, sizeof(double));
            if (!positions[0]) {
                free(positions);
                return 0;
            }
            
            for (uint32_t i = 0; i < parent->polytope->num_vertices; i++) {
                for (uint32_t d = 0; d < parent->polytope->dimension; d++) {
                    positions[0][d] += parent->polytope->vertex_coords[i * parent->polytope->dimension + d];
                }
            }
            for (uint32_t d = 0; d < parent->polytope->dimension; d++) {
                positions[0][d] /= parent->polytope->num_vertices;
            }
            break;
            
        default:
            return 0;
    }
    
    // Create children at each position
    uint32_t created = 0;
    for (uint32_t i = 0; i < num_positions; i++) {
        // Clone the template polytope
        PlatonicSolid* child = calloc(1, sizeof(PlatonicSolid));
        if (!child) continue;
        
        memcpy(child, child_template, sizeof(PlatonicSolid));
        
        // Allocate and copy dynamic arrays
        if (child_template->schlafli_symbol) {
            child->schlafli_symbol = malloc(child_template->symbol_length * sizeof(uint32_t));
            if (child->schlafli_symbol) {
                memcpy(child->schlafli_symbol, child_template->schlafli_symbol,
                       child_template->symbol_length * sizeof(uint32_t));
            }
        }
        
        if (child_template->vertex_coords) {
            size_t coord_size = child_template->num_vertices * child_template->dimension * sizeof(double);
            child->vertex_coords = malloc(coord_size);
            if (child->vertex_coords) {
                memcpy(child->vertex_coords, child_template->vertex_coords, coord_size);
            }
        }
        
        // Add child at position
        NestedPolytopeNode* child_node = nested_polytope_add_child_at_position(
            parent, child, positions[i], scale);
        
        if (child_node) {
            created++;
        }
    }
    
    // Cleanup
    if (strategy == NEST_AT_CENTER && positions && positions[0]) {
        free(positions[0]);
    }
    free(positions);
    
    return created;
}

NestedPolytopeNode* nested_polytope_add_child_at_position(
    NestedPolytopeNode* parent,
    PlatonicSolid* child,
    const double* position,
    double scale)
{
    if (!parent || !child || !position) return NULL;
    
    NestedPolytopeNode* child_node = nested_polytope_add_child(
        parent, child, NEST_CUSTOM, scale);
    
    if (child_node && child_node->transform) {
        memcpy(child_node->transform->translation, position,
               parent->polytope->dimension * sizeof(double));
    }
    
    return child_node;
}

bool nested_polytope_remove_child(NestedPolytopeNode* parent, uint32_t child_index) {
    if (!parent || child_index >= parent->num_children) return false;
    
    // Free the child node
    nested_polytope_free_node(parent->children[child_index]);
    
    // Shift remaining children
    for (uint32_t i = child_index; i < parent->num_children - 1; i++) {
        parent->children[i] = parent->children[i + 1];
        parent->children[i]->index = i;
    }
    
    parent->num_children--;
    return true;
}

// ============================================================================
// TREE TRAVERSAL
// ============================================================================

void nested_polytope_traverse_dfs(
    NestedPolytopeNode* node,
    NestedPolytopeCallback callback,
    void* user_data)
{
    if (!node || !callback) return;
    
    // Visit this node
    callback(node, user_data);
    
    // Recursively visit children
    for (uint32_t i = 0; i < node->num_children; i++) {
        nested_polytope_traverse_dfs(node->children[i], callback, user_data);
    }
}

void nested_polytope_traverse_bfs(
    NestedPolytopeTree* tree,
    NestedPolytopeCallback callback,
    void* user_data)
{
    if (!tree || !tree->root || !callback) return;
    
    // Simple queue using array
    uint32_t queue_capacity = 1024;
    NestedPolytopeNode** queue = malloc(queue_capacity * sizeof(NestedPolytopeNode*));
    if (!queue) return;
    
    uint32_t front = 0;
    uint32_t back = 0;
    
    // Enqueue root
    queue[back++] = tree->root;
    
    while (front < back) {
        NestedPolytopeNode* node = queue[front++];
        
        // Visit node
        callback(node, user_data);
        
        // Enqueue children
        for (uint32_t i = 0; i < node->num_children; i++) {
            if (back < queue_capacity) {
                queue[back++] = node->children[i];
            }
        }
    }
    
    free(queue);
}

void nested_polytope_traverse_level(
    NestedPolytopeTree* tree,
    uint32_t depth,
    NestedPolytopeCallback callback,
    void* user_data)
{
    if (!tree || !tree->root || !callback) return;
    
    // Helper function to traverse at specific depth
    void traverse_at_depth(NestedPolytopeNode* node, uint32_t target_depth, uint32_t current_depth) {
        if (!node) return;
        
        if (current_depth == target_depth) {
            callback(node, user_data);
        } else if (current_depth < target_depth) {
            for (uint32_t i = 0; i < node->num_children; i++) {
                traverse_at_depth(node->children[i], target_depth, current_depth + 1);
            }
        }
    }
    
    traverse_at_depth(tree->root, depth, 0);
}

// ============================================================================
// COORDINATE TRANSFORMATIONS
// ============================================================================

bool nested_polytope_compute_world_coords(NestedPolytopeNode* node) {
    if (!node || !node->polytope) return false;
    
    uint32_t dim = node->polytope->dimension;
    
    if (!node->parent) {
        // Root node: world coords = local coords
        node->world_scale = 1.0;
        memset(node->world_position, 0, dim * sizeof(double));
        return true;
    }
    
    // Compute parent's world coords first
    if (!nested_polytope_compute_world_coords(node->parent)) {
        return false;
    }
    
    // Accumulate scale
    node->world_scale = node->parent->world_scale * node->transform->scale;
    
    // Transform position: world_pos = parent_world_pos + parent_scale * local_translation
    for (uint32_t d = 0; d < dim; d++) {
        node->world_position[d] = node->parent->world_position[d] +
                                  node->parent->world_scale * node->transform->translation[d];
    }
    
    return true;
}

bool nested_polytope_local_to_world(
    const NestedPolytopeNode* node,
    const double* local_point,
    double* world_point)
{
    if (!node || !local_point || !world_point) return false;
    
    uint32_t dim = node->polytope->dimension;
    
    // Apply scale and translation
    for (uint32_t d = 0; d < dim; d++) {
        world_point[d] = node->world_position[d] + 
                        node->world_scale * local_point[d];
    }
    
    return true;
}

bool nested_polytope_world_to_local(
    const NestedPolytopeNode* node,
    const double* world_point,
    double* local_point)
{
    if (!node || !world_point || !local_point) return false;
    if (node->world_scale == 0.0) return false;
    
    uint32_t dim = node->polytope->dimension;
    
    // Inverse transform
    for (uint32_t d = 0; d < dim; d++) {
        local_point[d] = (world_point[d] - node->world_position[d]) / node->world_scale;
    }
    
    return true;
}

bool nested_polytope_update_all_coords(NestedPolytopeTree* tree) {
    if (!tree || !tree->root) return false;
    
    // Update root
    if (!nested_polytope_compute_world_coords(tree->root)) {
        return false;
    }
    
    // Recursively update all children
    void update_recursive(NestedPolytopeNode* node) {
        for (uint32_t i = 0; i < node->num_children; i++) {
            nested_polytope_compute_world_coords(node->children[i]);
            update_recursive(node->children[i]);
        }
    }
    
    update_recursive(tree->root);
    return true;
}

// ============================================================================
// TREE PROPERTIES
// ============================================================================

static void count_nodes_callback(NestedPolytopeNode* node, void* user_data) {
    (void)node;
    uint32_t* count = (uint32_t*)user_data;
    (*count)++;
}

uint32_t nested_polytope_count_nodes(const NestedPolytopeTree* tree) {
    if (!tree || !tree->root) return 0;
    
    uint32_t count = 0;
    nested_polytope_traverse_dfs(tree->root, count_nodes_callback, &count);
    return count;
}

static void max_depth_callback(NestedPolytopeNode* node, void* user_data) {
    uint32_t* max_depth = (uint32_t*)user_data;
    if (node->depth > *max_depth) {
        *max_depth = node->depth;
    }
}

uint32_t nested_polytope_get_max_depth(const NestedPolytopeTree* tree) {
    if (!tree || !tree->root) return 0;
    
    uint32_t max_depth = 0;
    nested_polytope_traverse_dfs(tree->root, max_depth_callback, &max_depth);
    return max_depth;
}

uint32_t nested_polytope_count_at_depth(const NestedPolytopeTree* tree, uint32_t depth) {
    if (!tree) return 0;
    
    uint32_t count = 0;
    void count_at_depth(NestedPolytopeNode* node, void* user_data) {
        (void)node;
        uint32_t* c = (uint32_t*)user_data;
        (*c)++;
    }
    
    nested_polytope_traverse_level((NestedPolytopeTree*)tree, depth, count_at_depth, &count);
    return count;
}

bool nested_polytope_is_balanced(const NestedPolytopeTree* tree) {
    if (!tree || !tree->root) return true;
    
    // Check if all leaves are at the same depth or differ by at most 1
    uint32_t min_leaf_depth = UINT32_MAX;
    uint32_t max_leaf_depth = 0;
    
    void check_leaf(NestedPolytopeNode* node, void* user_data) {
        (void)user_data;
        if (node->num_children == 0) {
            if (node->depth < min_leaf_depth) min_leaf_depth = node->depth;
            if (node->depth > max_leaf_depth) max_leaf_depth = node->depth;
        }
    }
    
    nested_polytope_traverse_dfs(tree->root, check_leaf, NULL);
    
    return (max_leaf_depth - min_leaf_depth) <= 1;
}

uint32_t nested_polytope_get_nodes_at_depth(
    const NestedPolytopeTree* tree,
    uint32_t depth,
    NestedPolytopeNode** nodes,
    uint32_t max_nodes)
{
    if (!tree || !nodes) return 0;
    
    uint32_t count = 0;
    
    void collect_node(NestedPolytopeNode* node, void* user_data) {
        uint32_t* c = (uint32_t*)user_data;
        if (*c < max_nodes) {
            nodes[*c] = node;
            (*c)++;
        }
    }
    
    nested_polytope_traverse_level((NestedPolytopeTree*)tree, depth, collect_node, &count);
    return count;
}

// ============================================================================
// SYMMETRY PRESERVATION
// ============================================================================

bool nested_polytope_preserves_12fold_symmetry(const NestedPolytopeTree* tree) {
    if (!tree || !tree->root) return false;
    
    // Check if all nodes preserve symmetry
    bool preserves = true;
    
    void check_symmetry(NestedPolytopeNode* node, void* user_data) {
        bool* p = (bool*)user_data;
        if (!nested_polytope_verify_node_symmetry(node)) {
            *p = false;
        }
    }
    
    nested_polytope_traverse_dfs(tree->root, check_symmetry, &preserves);
    return preserves;
}

bool nested_polytope_verify_node_symmetry(const NestedPolytopeNode* node) {
    if (!node || !node->polytope) return false;
    
    // Check if polytope has 12-fold symmetry properties
    // For now, check if num_heads is 12 (from CLLM integration)
    return node->polytope->num_heads == 12;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool nested_polytope_validate_tree(const NestedPolytopeTree* tree) {
    if (!tree || !tree->root) return false;
    
    // Validate all nodes
    bool valid = true;
    
    void validate_node_callback(NestedPolytopeNode* node, void* user_data) {
        bool* v = (bool*)user_data;
        if (!nested_polytope_validate_node(node)) {
            *v = false;
        }
    }
    
    nested_polytope_traverse_dfs(tree->root, validate_node_callback, &valid);
    return valid;
}

bool nested_polytope_validate_node(const NestedPolytopeNode* node) {
    if (!node) return false;
    if (!node->polytope) return false;
    if (!node->transform) return false;
    if (!node->world_position) return false;
    
    // Check parent-child consistency
    if (node->parent) {
        bool found = false;
        for (uint32_t i = 0; i < node->parent->num_children; i++) {
            if (node->parent->children[i] == node) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    // Check depth consistency
    uint32_t computed_depth = compute_node_depth(node);
    if (computed_depth != node->depth) return false;
    
    return true;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void nested_polytope_free_node(NestedPolytopeNode* node) {
    if (!node) return;
    
    // Free children recursively
    for (uint32_t i = 0; i < node->num_children; i++) {
        nested_polytope_free_node(node->children[i]);
    }
    free(node->children);
    
    // Free polytope (note: this assumes ownership)
    if (node->polytope) {
        free(node->polytope->schlafli_symbol);
        free(node->polytope->vertex_coords);
        if (node->polytope->edge_indices) {
            for (uint64_t i = 0; i < node->polytope->num_edges; i++) {
                free(node->polytope->edge_indices[i]);
            }
            free(node->polytope->edge_indices);
        }
        if (node->polytope->face_indices) {
            for (uint64_t i = 0; i < node->polytope->num_faces; i++) {
                free(node->polytope->face_indices[i]);
            }
            free(node->polytope->face_indices);
        }
        free(node->polytope->face_sizes);
        free(node->polytope);
    }
    
    nested_polytope_free_transform(node->transform);
    free(node->world_position);
    free(node);
}

void nested_polytope_free_tree(NestedPolytopeTree* tree) {
    if (!tree) return;
    
    nested_polytope_free_node(tree->root);
    free(tree->nodes_per_level);
    free(tree);
}

void nested_polytope_free_transform(NestedTransform* transform) {
    if (!transform) return;
    
    free(transform->translation);
    free(transform->rotation);
    free(transform);
}

// ============================================================================
// PRINTING & DEBUGGING
// ============================================================================

void nested_polytope_print_node(const NestedPolytopeNode* node) {
    if (!node) return;
    
    printf("Node: %s (depth=%u, index=%u)\n",
           node->label[0] ? node->label : node->polytope->name,
           node->depth, node->index);
    printf("  Polytope: %s (%uD)\n", node->polytope->name, node->polytope->dimension);
    printf("  Children: %u\n", node->num_children);
    printf("  Scale: %.3f (world: %.3f)\n", node->transform->scale, node->world_scale);
    printf("  Position: [");
    for (uint32_t d = 0; d < node->polytope->dimension; d++) {
        printf("%.3f", node->world_position[d]);
        if (d < node->polytope->dimension - 1) printf(", ");
    }
    printf("]\n");
}

void nested_polytope_print_tree(const NestedPolytopeTree* tree) {
    if (!tree) return;
    
    printf("\n");
    printf("========================================\n");
    printf("Nested Polytope Tree\n");
    printf("========================================\n");
    printf("Dimension: %u\n", tree->dimension);
    printf("Total nodes: %u\n", tree->total_nodes);
    printf("Max depth: %u\n", tree->max_depth);
    printf("Balanced: %s\n", tree->is_balanced ? "yes" : "no");
    printf("Preserves symmetry: %s\n", tree->preserves_symmetry ? "yes" : "no");
    printf("\n");
}

void nested_polytope_print_hierarchy(const NestedPolytopeNode* node, uint32_t indent) {
    if (!node) return;
    
    // Print indentation
    for (uint32_t i = 0; i < indent; i++) {
        printf("  ");
    }
    
    // Print node info
    printf("├─ %s (scale=%.2f, children=%u)\n",
           node->polytope->name, node->transform->scale, node->num_children);
    
    // Print children
    for (uint32_t i = 0; i < node->num_children; i++) {
        nested_polytope_print_hierarchy(node->children[i], indent + 1);
    }
}

void nested_polytope_print_statistics(const NestedPolytopeTree* tree) {
    if (!tree) return;
    
    nested_polytope_print_tree(tree);
    
    printf("Nodes per level:\n");
    for (uint32_t i = 0; i <= tree->max_depth; i++) {
        uint32_t count = nested_polytope_count_at_depth(tree, i);
        printf("  Level %u: %u nodes\n", i, count);
    }
    printf("\n");
    
    printf("Tree structure:\n");
    nested_polytope_print_hierarchy(tree->root, 0);
    printf("\n");
}

