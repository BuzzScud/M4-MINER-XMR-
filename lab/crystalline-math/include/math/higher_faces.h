#ifndef MATH_HIGHER_FACES_H
#define MATH_HIGHER_FACES_H

/**
 * @file higher_faces.h
 * @brief Higher-dimensional face generation for polytopes
 * 
 * This module generates k-faces (k-dimensional faces) for polytopes
 * of any dimension. A k-face is a k-dimensional polytope that forms
 * part of the boundary of a higher-dimensional polytope.
 * 
 * Examples:
 * - 0-faces: Vertices (points)
 * - 1-faces: Edges (line segments)
 * - 2-faces: Faces (polygons)
 * - 3-faces: Cells (polyhedra) in 4D+
 * - 4-faces: 4-cells (polychora) in 5D+
 * 
 * For a 4D tesseract:
 * - 0-faces: 16 vertices
 * - 1-faces: 32 edges
 * - 2-faces: 24 square faces
 * - 3-faces: 8 cubic cells
 */

#include "math/polytope.h"
#include "math/schlafli.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FACE STRUCTURES
// ============================================================================

/**
 * @brief Represents a k-dimensional face
 */
typedef struct {
    uint32_t dimension;              /**< Dimension of this face (k) */
    uint32_t index;                  /**< Index of this face */
    
    uint32_t* vertex_indices;        /**< Indices of vertices in this face */
    uint32_t num_vertices;           /**< Number of vertices */
    
    uint32_t* subface_indices;       /**< Indices of (k-1)-faces in this face */
    uint32_t num_subfaces;           /**< Number of (k-1)-faces */
    
    SchlafliSymbol* symbol;          /**< Schläfli symbol (if regular) */
    
    bool is_regular;                 /**< Is this face regular? */
} KFace;

/**
 * @brief Collection of all k-faces for a specific dimension
 */
typedef struct {
    uint32_t dimension;              /**< Dimension k */
    KFace** faces;                   /**< Array of k-faces */
    uint32_t count;                  /**< Number of k-faces */
} KFaceSet;

/**
 * @brief Complete face hierarchy for a polytope
 */
typedef struct {
    uint32_t polytope_dimension;     /**< Dimension of the polytope */
    KFaceSet** face_sets;            /**< Array of face sets (one per dimension) */
    uint32_t num_dimensions;         /**< Number of dimensions (0 to n-1) */
} FaceHierarchy;

// ============================================================================
// FACE GENERATION
// ============================================================================

/**
 * @brief Generate all k-faces for a polytope
 * 
 * @param solid Platonic solid
 * @param k Dimension of faces to generate (0 ≤ k < dimension)
 * @return Set of k-faces, or NULL on error
 */
KFaceSet* higher_faces_generate_k_faces(const PlatonicSolid* solid, uint32_t k);

/**
 * @brief Generate complete face hierarchy for a polytope
 * 
 * Generates all k-faces from 0-faces (vertices) to (n-1)-faces
 * where n is the polytope dimension.
 * 
 * @param solid Platonic solid
 * @return Complete face hierarchy, or NULL on error
 */
FaceHierarchy* higher_faces_generate_hierarchy(const PlatonicSolid* solid);

/**
 * @brief Generate 3-faces (cells) for 4D+ polytopes
 * 
 * For 4D polytopes, 3-faces are polyhedra (cubes, tetrahedra, etc.)
 * 
 * @param solid Platonic solid (must be 4D+)
 * @return Set of 3-faces, or NULL on error
 */
KFaceSet* higher_faces_generate_cells(const PlatonicSolid* solid);

/**
 * @brief Generate 4-faces for 5D+ polytopes
 * 
 * For 5D polytopes, 4-faces are polychora
 * 
 * @param solid Platonic solid (must be 5D+)
 * @return Set of 4-faces, or NULL on error
 */
KFaceSet* higher_faces_generate_4_faces(const PlatonicSolid* solid);

// ============================================================================
// FACE PROPERTIES
// ============================================================================

/**
 * @brief Get vertices of a k-face
 * 
 * @param face k-face
 * @param solid Parent polytope
 * @param vertices Output array of vertex coordinates
 * @param max_vertices Maximum number of vertices to return
 * @return Number of vertices returned
 */
uint32_t higher_faces_get_vertices(const KFace* face,
                                    const PlatonicSolid* solid,
                                    double** vertices,
                                    uint32_t max_vertices);

/**
 * @brief Get Schläfli symbol of a k-face (if regular)
 * 
 * @param face k-face
 * @return Schläfli symbol, or NULL if not regular
 */
SchlafliSymbol* higher_faces_get_symbol(const KFace* face);

/**
 * @brief Check if k-face is regular
 * 
 * @param face k-face
 * @return true if regular
 */
bool higher_faces_is_regular(const KFace* face);

// ============================================================================
// FACE CONNECTIVITY
// ============================================================================

/**
 * @brief Get adjacent k-faces
 * 
 * Two k-faces are adjacent if they share a (k-1)-face.
 * 
 * @param face k-face
 * @param hierarchy Face hierarchy
 * @param adjacent Output array of adjacent face indices
 * @param max_adjacent Maximum number of adjacent faces to return
 * @return Number of adjacent faces
 */
uint32_t higher_faces_get_adjacent(const KFace* face,
                                    const FaceHierarchy* hierarchy,
                                    uint32_t* adjacent,
                                    uint32_t max_adjacent);

/**
 * @brief Get incident (k+1)-faces
 * 
 * Returns all (k+1)-faces that contain this k-face.
 * 
 * @param face k-face
 * @param hierarchy Face hierarchy
 * @param incident Output array of incident face indices
 * @param max_incident Maximum number of incident faces to return
 * @return Number of incident faces
 */
uint32_t higher_faces_get_incident(const KFace* face,
                                    const FaceHierarchy* hierarchy,
                                    uint32_t* incident,
                                    uint32_t max_incident);

// ============================================================================
// FACE ENUMERATION
// ============================================================================

/**
 * @brief Enumerate all k-faces using callback
 * 
 * @param hierarchy Face hierarchy
 * @param k Dimension of faces to enumerate
 * @param callback Function to call for each face
 * @param user_data User data passed to callback
 */
typedef void (*KFaceCallback)(const KFace* face, void* user_data);

void higher_faces_enumerate(const FaceHierarchy* hierarchy,
                             uint32_t k,
                             KFaceCallback callback,
                             void* user_data);

/**
 * @brief Count k-faces
 * 
 * @param hierarchy Face hierarchy
 * @param k Dimension of faces to count
 * @return Number of k-faces
 */
uint32_t higher_faces_count(const FaceHierarchy* hierarchy, uint32_t k);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Free k-face
 * 
 * @param face k-face to free
 */
void higher_faces_free_face(KFace* face);

/**
 * @brief Free k-face set
 * 
 * @param set k-face set to free
 */
void higher_faces_free_set(KFaceSet* set);

/**
 * @brief Free face hierarchy
 * 
 * @param hierarchy Face hierarchy to free
 */
void higher_faces_free_hierarchy(FaceHierarchy* hierarchy);

// ============================================================================
// PRINTING & DEBUGGING
// ============================================================================

/**
 * @brief Print k-face information
 * 
 * @param face k-face to print
 */
void higher_faces_print_face(const KFace* face);

/**
 * @brief Print k-face set
 * 
 * @param set k-face set to print
 */
void higher_faces_print_set(const KFaceSet* set);

/**
 * @brief Print complete face hierarchy
 * 
 * @param hierarchy Face hierarchy to print
 */
void higher_faces_print_hierarchy(const FaceHierarchy* hierarchy);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * @brief Validate face hierarchy
 * 
 * Checks that:
 * - Face counts match f-vector
 * - Connectivity is consistent
 * - Euler characteristic is correct
 * 
 * @param hierarchy Face hierarchy
 * @return true if valid
 */
bool higher_faces_validate_hierarchy(const FaceHierarchy* hierarchy);

#ifdef __cplusplus
}
#endif

#endif /* MATH_HIGHER_FACES_H */