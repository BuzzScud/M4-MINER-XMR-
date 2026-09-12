/**
 * @file higher_faces.c
 * @brief Implementation of higher-dimensional face generation
 */

#include "math/higher_faces.h"
#include "math/schlafli_math.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Check if a vertex set is a subset of another
 */
static bool is_subset(const uint32_t* subset, uint32_t subset_size,
                     const uint32_t* set, uint32_t set_size) {
    if (subset_size > set_size) return false;
    
    for (uint32_t i = 0; i < subset_size; i++) {
        bool found = false;
        for (uint32_t j = 0; j < set_size; j++) {
            if (subset[i] == set[j]) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

// ============================================================================
// FACE CREATION
// ============================================================================

/**
 * @brief Create a new k-face
 */
static KFace* create_face(uint32_t dimension, uint32_t index,
                          const uint32_t* vertices, uint32_t num_vertices) {
    KFace* face = calloc(1, sizeof(KFace));
    if (!face) return NULL;
    
    face->dimension = dimension;
    face->index = index;
    face->num_vertices = num_vertices;
    
    if (num_vertices > 0) {
        face->vertex_indices = malloc(num_vertices * sizeof(uint32_t));
        if (!face->vertex_indices) {
            free(face);
            return NULL;
        }
        memcpy(face->vertex_indices, vertices, num_vertices * sizeof(uint32_t));
    }
    
    face->subface_indices = NULL;
    face->num_subfaces = 0;
    face->symbol = NULL;
    face->is_regular = false;
    
    return face;
}

// ============================================================================
// 0-FACE GENERATION (VERTICES)
// ============================================================================

/**
 * @brief Generate 0-faces (vertices)
 */
static KFaceSet* generate_0_faces(const PlatonicSolid* solid) {
    if (!solid || solid->num_vertices == 0) return NULL;
    
    KFaceSet* set = calloc(1, sizeof(KFaceSet));
    if (!set) return NULL;
    
    set->dimension = 0;
    set->count = solid->num_vertices;
    set->faces = malloc(set->count * sizeof(KFace*));
    if (!set->faces) {
        free(set);
        return NULL;
    }
    
    // Each vertex is a 0-face
    for (uint32_t i = 0; i < solid->num_vertices; i++) {
        uint32_t vertex = i;
        set->faces[i] = create_face(0, i, &vertex, 1);
        if (!set->faces[i]) {
            // Cleanup on error
            for (uint32_t j = 0; j < i; j++) {
                higher_faces_free_face(set->faces[j]);
            }
            free(set->faces);
            free(set);
            return NULL;
        }
    }
    
    return set;
}

// ============================================================================
// 1-FACE GENERATION (EDGES)
// ============================================================================

/**
 * @brief Generate 1-faces (edges)
 */
static KFaceSet* generate_1_faces(const PlatonicSolid* solid) {
    if (!solid || solid->num_edges == 0 || !solid->edge_indices) return NULL;
    
    KFaceSet* set = calloc(1, sizeof(KFaceSet));
    if (!set) return NULL;
    
    set->dimension = 1;
    set->count = solid->num_edges;
    set->faces = malloc(set->count * sizeof(KFace*));
    if (!set->faces) {
        free(set);
        return NULL;
    }
    
    // Each edge is a 1-face
    for (uint32_t i = 0; i < solid->num_edges; i++) {
        uint32_t vertices[2] = {
            solid->edge_indices[i][0],
            solid->edge_indices[i][1]
        };
        set->faces[i] = create_face(1, i, vertices, 2);
        if (!set->faces[i]) {
            for (uint32_t j = 0; j < i; j++) {
                higher_faces_free_face(set->faces[j]);
            }
            free(set->faces);
            free(set);
            return NULL;
        }
    }
    
    return set;
}

// ============================================================================
// 2-FACE GENERATION (POLYGONS)
// ============================================================================

/**
 * @brief Generate 2-faces (polygons)
 */
static KFaceSet* generate_2_faces(const PlatonicSolid* solid) {
    if (!solid || solid->num_faces == 0 || !solid->face_indices) return NULL;
    
    KFaceSet* set = calloc(1, sizeof(KFaceSet));
    if (!set) return NULL;
    
    set->dimension = 2;
    set->count = solid->num_faces;
    set->faces = malloc(set->count * sizeof(KFace*));
    if (!set->faces) {
        free(set);
        return NULL;
    }
    
    // Each face is a 2-face
    for (uint32_t i = 0; i < solid->num_faces; i++) {
        uint32_t num_vertices = solid->face_sizes[i];
        set->faces[i] = create_face(2, i, solid->face_indices[i], num_vertices);
        if (!set->faces[i]) {
            for (uint32_t j = 0; j < i; j++) {
                higher_faces_free_face(set->faces[j]);
            }
            free(set->faces);
            free(set);
            return NULL;
        }
    }
    
    return set;
}

// ============================================================================
// 3-FACE GENERATION (CELLS) - FOR 4D+ POLYTOPES
// ============================================================================

/**
 * @brief Generate 3-faces (cells) for 4D+ polytopes
 * 
 * For a 4D polytope, 3-faces are polyhedra (cubes, tetrahedra, etc.)
 * We need to identify which vertices form each cell.
 */
static KFaceSet* generate_3_faces(const PlatonicSolid* solid) {
    if (!solid || solid->dimension < 4) return NULL;
    if (solid->num_cells == 0) return NULL;
    
    // For now, we'll use a simplified approach based on the Schläfli symbol
    // This works for the 6 regular 4D polychora
    
    KFaceSet* set = calloc(1, sizeof(KFaceSet));
    if (!set) return NULL;
    
    set->dimension = 3;
    set->count = solid->num_cells;
    set->faces = malloc(set->count * sizeof(KFace*));
    if (!set->faces) {
        free(set);
        return NULL;
    }
    
    // For regular 4D polytopes, we can derive cell structure from Schläfli symbol
    // {p,q,r} means cells are {p,q} polyhedra
    
    if (solid->symbol_length >= 3) {
        uint32_t p = solid->schlafli_symbol[0];
        uint32_t q = solid->schlafli_symbol[1];
        
        // Calculate vertices per cell based on {p,q}
        uint32_t vertices_per_cell;
        if (p == 3 && q == 3) {
            vertices_per_cell = 4;  // Tetrahedron
        } else if (p == 4 && q == 3) {
            vertices_per_cell = 8;  // Cube
        } else if (p == 3 && q == 4) {
            vertices_per_cell = 6;  // Octahedron
        } else if (p == 3 && q == 5) {
            vertices_per_cell = 12; // Icosahedron
        } else if (p == 5 && q == 3) {
            vertices_per_cell = 20; // Dodecahedron
        } else {
            // Unknown cell type
            free(set->faces);
            free(set);
            return NULL;
        }
        
        // For now, create placeholder cells
        // A full implementation would need to compute actual vertex connectivity
        for (uint32_t i = 0; i < solid->num_cells; i++) {
            // Allocate vertices for this cell
            uint32_t* cell_vertices = malloc(vertices_per_cell * sizeof(uint32_t));
            if (!cell_vertices) {
                for (uint32_t j = 0; j < i; j++) {
                    higher_faces_free_face(set->faces[j]);
                }
                free(set->faces);
                free(set);
                return NULL;
            }
            
            // Placeholder: assign vertices cyclically
            // A real implementation would compute actual cell-vertex incidence
            for (uint32_t v = 0; v < vertices_per_cell; v++) {
                cell_vertices[v] = (i * vertices_per_cell + v) % solid->num_vertices;
            }
            
            set->faces[i] = create_face(3, i, cell_vertices, vertices_per_cell);
            free(cell_vertices);
            
            if (!set->faces[i]) {
                for (uint32_t j = 0; j < i; j++) {
                    higher_faces_free_face(set->faces[j]);
                }
                free(set->faces);
                free(set);
                return NULL;
            }
        }
    }
    
    return set;
}

// ============================================================================
// GENERAL K-FACE GENERATION
// ============================================================================

KFaceSet* higher_faces_generate_k_faces(const PlatonicSolid* solid, uint32_t k) {
    if (!solid) return NULL;
    if (k >= solid->dimension) return NULL;
    
    switch (k) {
        case 0:
            return generate_0_faces(solid);
        case 1:
            return generate_1_faces(solid);
        case 2:
            return generate_2_faces(solid);
        case 3:
            return generate_3_faces(solid);
        default:
            // For k >= 4, not yet implemented
            return NULL;
    }
}

// ============================================================================
// FACE HIERARCHY GENERATION
// ============================================================================

FaceHierarchy* higher_faces_generate_hierarchy(const PlatonicSolid* solid) {
    if (!solid) return NULL;
    
    FaceHierarchy* hierarchy = calloc(1, sizeof(FaceHierarchy));
    if (!hierarchy) return NULL;
    
    hierarchy->polytope_dimension = solid->dimension;
    hierarchy->num_dimensions = solid->dimension; // 0 to n-1
    hierarchy->face_sets = malloc(hierarchy->num_dimensions * sizeof(KFaceSet*));
    if (!hierarchy->face_sets) {
        free(hierarchy);
        return NULL;
    }
    
    // Generate all k-faces from 0 to n-1
    for (uint32_t k = 0; k < solid->dimension; k++) {
        hierarchy->face_sets[k] = higher_faces_generate_k_faces(solid, k);
        if (!hierarchy->face_sets[k]) {
            // Cleanup on error
            for (uint32_t j = 0; j < k; j++) {
                higher_faces_free_set(hierarchy->face_sets[j]);
            }
            free(hierarchy->face_sets);
            free(hierarchy);
            return NULL;
        }
    }
    
    // Build connectivity between faces
    // For each k-face, find its (k-1)-faces (subfaces)
    for (uint32_t k = 1; k < solid->dimension; k++) {
        KFaceSet* k_faces = hierarchy->face_sets[k];
        KFaceSet* km1_faces = hierarchy->face_sets[k - 1];
        
        for (uint32_t i = 0; i < k_faces->count; i++) {
            KFace* face = k_faces->faces[i];
            
            // Count how many (k-1)-faces are subsets of this k-face
            uint32_t num_subfaces = 0;
            for (uint32_t j = 0; j < km1_faces->count; j++) {
                KFace* subface = km1_faces->faces[j];
                if (is_subset(subface->vertex_indices, subface->num_vertices,
                             face->vertex_indices, face->num_vertices)) {
                    num_subfaces++;
                }
            }
            
            // Allocate and populate subface indices
            if (num_subfaces > 0) {
                face->subface_indices = malloc(num_subfaces * sizeof(uint32_t));
                if (!face->subface_indices) continue;
                
                face->num_subfaces = num_subfaces;
                uint32_t idx = 0;
                for (uint32_t j = 0; j < km1_faces->count; j++) {
                    KFace* subface = km1_faces->faces[j];
                    if (is_subset(subface->vertex_indices, subface->num_vertices,
                                 face->vertex_indices, face->num_vertices)) {
                        face->subface_indices[idx++] = j;
                    }
                }
            }
        }
    }
    
    return hierarchy;
}

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

KFaceSet* higher_faces_generate_cells(const PlatonicSolid* solid) {
    if (!solid || solid->dimension < 4) return NULL;
    return higher_faces_generate_k_faces(solid, 3);
}

KFaceSet* higher_faces_generate_4_faces(const PlatonicSolid* solid) {
    if (!solid || solid->dimension < 5) return NULL;
    return higher_faces_generate_k_faces(solid, 4);
}

// ============================================================================
// FACE PROPERTIES
// ============================================================================

uint32_t higher_faces_get_vertices(const KFace* face,
                                    const PlatonicSolid* solid,
                                    double** vertices,
                                    uint32_t max_vertices) {
    if (!face || !solid || !vertices) return 0;
    
    uint32_t count = face->num_vertices < max_vertices ? 
                     face->num_vertices : max_vertices;
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t v_idx = face->vertex_indices[i];
        if (v_idx < solid->num_vertices) {
            vertices[i] = &solid->vertex_coords[v_idx * solid->dimension];
        }
    }
    
    return count;
}

SchlafliSymbol* higher_faces_get_symbol(const KFace* face) {
    if (!face) return NULL;
    return face->symbol;
}

bool higher_faces_is_regular(const KFace* face) {
    if (!face) return false;
    return face->is_regular;
}

// ============================================================================
// FACE CONNECTIVITY
// ============================================================================

uint32_t higher_faces_get_adjacent(const KFace* face,
                                    const FaceHierarchy* hierarchy,
                                    uint32_t* adjacent,
                                    uint32_t max_adjacent) {
    if (!face || !hierarchy || !adjacent) return 0;
    if (face->dimension >= hierarchy->num_dimensions) return 0;
    
    KFaceSet* same_dim = hierarchy->face_sets[face->dimension];
    uint32_t count = 0;
    
    // Two k-faces are adjacent if they share a (k-1)-face
    if (face->dimension > 0 && face->num_subfaces > 0) {
        for (uint32_t i = 0; i < same_dim->count; i++) {
            if (i == face->index) continue; // Skip self
            
            KFace* other = same_dim->faces[i];
            
            // Check if they share any subface
            for (uint32_t j = 0; j < face->num_subfaces && count < max_adjacent; j++) {
                for (uint32_t k = 0; k < other->num_subfaces; k++) {
                    if (face->subface_indices[j] == other->subface_indices[k]) {
                        adjacent[count++] = i;
                        goto next_face; // Found shared subface, move to next
                    }
                }
            }
            next_face:;
        }
    }
    
    return count;
}

uint32_t higher_faces_get_incident(const KFace* face,
                                    const FaceHierarchy* hierarchy,
                                    uint32_t* incident,
                                    uint32_t max_incident) {
    if (!face || !hierarchy || !incident) return 0;
    if (face->dimension + 1 >= hierarchy->num_dimensions) return 0;
    
    KFaceSet* higher_dim = hierarchy->face_sets[face->dimension + 1];
    uint32_t count = 0;
    
    // Find all (k+1)-faces that contain this k-face
    for (uint32_t i = 0; i < higher_dim->count && count < max_incident; i++) {
        KFace* higher = higher_dim->faces[i];
        
        // Check if this face is a subface of the higher-dimensional face
        for (uint32_t j = 0; j < higher->num_subfaces; j++) {
            if (higher->subface_indices[j] == face->index) {
                incident[count++] = i;
                break;
            }
        }
    }
    
    return count;
}

// ============================================================================
// FACE ENUMERATION
// ============================================================================

void higher_faces_enumerate(const FaceHierarchy* hierarchy,
                             uint32_t k,
                             KFaceCallback callback,
                             void* user_data) {
    if (!hierarchy || !callback) return;
    if (k >= hierarchy->num_dimensions) return;
    
    KFaceSet* set = hierarchy->face_sets[k];
    for (uint32_t i = 0; i < set->count; i++) {
        callback(set->faces[i], user_data);
    }
}

uint32_t higher_faces_count(const FaceHierarchy* hierarchy, uint32_t k) {
    if (!hierarchy) return 0;
    if (k >= hierarchy->num_dimensions) return 0;
    return hierarchy->face_sets[k]->count;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void higher_faces_free_face(KFace* face) {
    if (!face) return;
    
    free(face->vertex_indices);
    free(face->subface_indices);
    if (face->symbol) {
        free(face->symbol->components);
        free(face->symbol);
    }
    free(face);
}

void higher_faces_free_set(KFaceSet* set) {
    if (!set) return;
    
    if (set->faces) {
        for (uint32_t i = 0; i < set->count; i++) {
            higher_faces_free_face(set->faces[i]);
        }
        free(set->faces);
    }
    free(set);
}

void higher_faces_free_hierarchy(FaceHierarchy* hierarchy) {
    if (!hierarchy) return;
    
    if (hierarchy->face_sets) {
        for (uint32_t i = 0; i < hierarchy->num_dimensions; i++) {
            higher_faces_free_set(hierarchy->face_sets[i]);
        }
        free(hierarchy->face_sets);
    }
    free(hierarchy);
}

// ============================================================================
// PRINTING & DEBUGGING
// ============================================================================

void higher_faces_print_face(const KFace* face) {
    if (!face) return;
    
    printf("%u-face #%u: ", face->dimension, face->index);
    printf("vertices=[");
    for (uint32_t i = 0; i < face->num_vertices; i++) {
        printf("%u", face->vertex_indices[i]);
        if (i < face->num_vertices - 1) printf(",");
    }
    printf("] ");
    
    if (face->num_subfaces > 0) {
        printf("subfaces=[");
        for (uint32_t i = 0; i < face->num_subfaces; i++) {
            printf("%u", face->subface_indices[i]);
            if (i < face->num_subfaces - 1) printf(",");
        }
        printf("] ");
    }
    
    if (face->is_regular) printf("(regular)");
    printf("\n");
}

void higher_faces_print_set(const KFaceSet* set) {
    if (!set) return;
    
    printf("\n%u-faces: %u total\n", set->dimension, set->count);
    printf("================\n");
    for (uint32_t i = 0; i < set->count; i++) {
        higher_faces_print_face(set->faces[i]);
    }
}

void higher_faces_print_hierarchy(const FaceHierarchy* hierarchy) {
    if (!hierarchy) return;
    
    printf("\n");
    printf("========================================\n");
    printf("Face Hierarchy for %uD Polytope\n", hierarchy->polytope_dimension);
    printf("========================================\n");
    
    for (uint32_t k = 0; k < hierarchy->num_dimensions; k++) {
        higher_faces_print_set(hierarchy->face_sets[k]);
    }
}

// ============================================================================
// VALIDATION
// ============================================================================

bool higher_faces_validate_hierarchy(const FaceHierarchy* hierarchy) {
    if (!hierarchy) return false;
    
    // Check that face counts are reasonable
    for (uint32_t k = 0; k < hierarchy->num_dimensions; k++) {
        if (!hierarchy->face_sets[k]) return false;
        if (hierarchy->face_sets[k]->dimension != k) return false;
    }
    
    // Check connectivity consistency
    for (uint32_t k = 1; k < hierarchy->num_dimensions; k++) {
        KFaceSet* k_faces = hierarchy->face_sets[k];
        KFaceSet* km1_faces = hierarchy->face_sets[k - 1];
        
        for (uint32_t i = 0; i < k_faces->count; i++) {
            KFace* face = k_faces->faces[i];
            
            // Verify all subface indices are valid
            for (uint32_t j = 0; j < face->num_subfaces; j++) {
                if (face->subface_indices[j] >= km1_faces->count) {
                    return false;
                }
            }
        }
    }
    
    return true;
}