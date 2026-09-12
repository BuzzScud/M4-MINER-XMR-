#ifndef MATH_SCHLAFLI_H
#define MATH_SCHLAFLI_H

/**
 * @file schlafli.h
 * @brief Schläfli symbol parsing and manipulation
 * 
 * Schläfli symbols are a notation for regular polytopes:
 * - {p} = Regular polygon with p sides
 * - {p,q} = Regular polyhedron with p-gonal faces, q meeting at each vertex
 * - {p,q,r} = Regular 4D polychoron
 * - {p,q,r,...} = Regular nD polytope
 * 
 * Examples:
 * - {3,3} = Tetrahedron (3 edges per face, 3 faces per vertex)
 * - {4,3} = Cube (4 edges per face, 3 faces per vertex)
 * - {3,4} = Octahedron (3 edges per face, 4 faces per vertex)
 * - {5,3} = Dodecahedron (5 edges per face, 3 faces per vertex)
 * - {3,5} = Icosahedron (3 edges per face, 5 faces per vertex)
 * - {3,3,3} = 5-cell (4D simplex)
 * - {4,3,3} = Tesseract (4D hypercube)
 * - {3,3,4} = 16-cell (4D cross-polytope)
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CORE STRUCTURE
// ============================================================================

/**
 * @brief Represents a Schläfli symbol {p,q,r,...}
 */
typedef struct {
    uint32_t* components;            /**< Symbol components {p, q, r, ...} */
    uint32_t length;                 /**< Number of components */
    uint32_t dimension;              /**< Dimension of polytope (length + 2) */
    
    // Derived properties
    uint32_t vertices_per_face;      /**< p (first component) */
    uint32_t faces_per_vertex;       /**< q (second component) */
    uint32_t cells_per_edge;         /**< r (third component, 4D+) */
    
    // Validation
    bool is_valid;                   /**< Valid Schläfli symbol */
    bool is_regular;                 /**< Regular polytope */
    
} SchlafliSymbol;

// ============================================================================
// CREATION AND PARSING
// ============================================================================

/**
 * @brief Parse Schläfli symbol from string
 * 
 * @param symbol_str String representation (e.g., "{3,3}", "{4,3,3}")
 * @return Pointer to parsed symbol, or NULL on error
 * 
 * Examples:
 *   schlafli_parse("{3,3}")   → Tetrahedron
 *   schlafli_parse("{4,3}")   → Cube
 *   schlafli_parse("{3,3,3}") → 5-cell
 */
SchlafliSymbol* schlafli_parse(const char* symbol_str);

/**
 * @brief Create Schläfli symbol from components
 * 
 * @param components Array of components {p, q, r, ...}
 * @param length Number of components
 * @return Pointer to created symbol, or NULL on error
 * 
 * Examples:
 *   schlafli_create((uint32_t[]){3,3}, 2)   → Tetrahedron
 *   schlafli_create((uint32_t[]){4,3}, 2)   → Cube
 *   schlafli_create((uint32_t[]){3,3,3}, 3) → 5-cell
 */
SchlafliSymbol* schlafli_create(const uint32_t* components, uint32_t length);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * @brief Validate Schläfli symbol
 * 
 * Checks if the symbol represents a valid regular polytope.
 * 
 * @param symbol Pointer to symbol
 * @return true if valid, false otherwise
 */
bool schlafli_validate(const SchlafliSymbol* symbol);

/**
 * @brief Check if symbol represents a regular polytope
 * 
 * @param symbol Pointer to symbol
 * @return true if regular, false otherwise
 */
bool schlafli_is_regular(const SchlafliSymbol* symbol);

// ============================================================================
// PROPERTIES
// ============================================================================

/**
 * @brief Get dimension from Schläfli symbol
 * 
 * Dimension = length + 2
 * 
 * @param symbol Pointer to symbol
 * @return Dimension of polytope
 */
uint32_t schlafli_get_dimension(const SchlafliSymbol* symbol);

/**
 * @brief Get number of vertices per face
 * 
 * @param symbol Pointer to symbol
 * @return Number of vertices per face (p)
 */
uint32_t schlafli_get_vertices_per_face(const SchlafliSymbol* symbol);

/**
 * @brief Get number of faces per vertex
 * 
 * @param symbol Pointer to symbol
 * @return Number of faces per vertex (q)
 */
uint32_t schlafli_get_faces_per_vertex(const SchlafliSymbol* symbol);

// ============================================================================
// STRING CONVERSION
// ============================================================================

/**
 * @brief Convert Schläfli symbol to string
 * 
 * @param symbol Pointer to symbol
 * @return String representation (e.g., "{3,3}"), must be freed by caller
 */
char* schlafli_to_string(const SchlafliSymbol* symbol);

/**
 * @brief Print Schläfli symbol to stdout
 * 
 * @param symbol Pointer to symbol
 */
void schlafli_print(const SchlafliSymbol* symbol);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Free Schläfli symbol
 * 
 * @param symbol Pointer to symbol to free
 */
void schlafli_free(SchlafliSymbol* symbol);

// ============================================================================
// COMPARISON
// ============================================================================

/**
 * @brief Compare two Schläfli symbols for equality
 * 
 * @param a First symbol
 * @param b Second symbol
 * @return true if equal, false otherwise
 */
bool schlafli_equals(const SchlafliSymbol* a, const SchlafliSymbol* b);

#ifdef __cplusplus
}
#endif

#endif /* MATH_SCHLAFLI_H */