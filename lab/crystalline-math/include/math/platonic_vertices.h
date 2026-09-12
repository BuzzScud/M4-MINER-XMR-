/**
 * @file platonic_vertices.h
 * @brief Platonic Solid Vertex Generation in 13D
 * 
 * Generates vertices of the 5 Platonic solids mapped to 13D clock lattice.
 */

#ifndef MATH_PLATONIC_VERTICES_H
#define MATH_PLATONIC_VERTICES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate 50 Platonic solid vertices in 13D clock lattice
 * 
 * Includes all 5 Platonic solids:
 * - Tetrahedron: 4 vertices
 * - Cube: 8 vertices
 * - Octahedron: 6 vertices
 * - Dodecahedron: 20 vertices
 * - Icosahedron: 12 vertices
 * Total: 50 vertices
 * 
 * @param vertices Output array [50][13]
 * @param num_vertices Output: number of vertices generated (should be 50)
 */
void math_generate_platonic_vertices_13d(double vertices[][13], int* num_vertices);

/**
 * Generate vertices for a specific Platonic solid
 * 
 * @param solid_type 0=Tetrahedron, 1=Cube, 2=Octahedron, 3=Dodecahedron, 4=Icosahedron
 * @param vertices Output array
 * @param num_vertices Output: number of vertices
 */
void math_generate_single_platonic_13d(int solid_type, double vertices[][13], int* num_vertices);

#ifdef __cplusplus
}
#endif

#endif /* MATH_PLATONIC_VERTICES_H */
