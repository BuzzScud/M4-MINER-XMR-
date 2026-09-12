/**
 * @file platonic_vertices.c
 * @brief Platonic Solid Vertex Generation in 13D
 * 
 * Generates vertices of the 5 Platonic solids mapped to 13D clock lattice.
 * Total of 50 vertices used as geometric anchors.
 */

#include "math/platonic_vertices.h"
#include "math/clock_lattice_13d.h"
#include "math/transcendental.h"
#include <string.h>

#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846
#endif

#ifndef MATH_PHI
#define MATH_PHI 1.61803398874989484820
#endif

#ifndef MATH_TWO_PI
#define MATH_TWO_PI (2.0 * MATH_PI)
#endif

/**
 * Generate 50 Platonic solid vertices in 13D clock lattice
 */
void math_generate_platonic_vertices_13d(
    double vertices[][13],
    int* num_vertices
) {
    int idx = 0;
    
    // Tetrahedron: 4 vertices (4-fold symmetry)
    for (int v = 0; v < 4; v++) {
        double angle = v * MATH_TWO_PI / 4.0;
        for (int d = 0; d < 13; d++) {
            double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
            vertices[idx][d] = math_cos(angle * freq) * math_pow(MATH_PHI, d % 3);
        }
        idx++;
    }
    
    // Cube: 8 vertices (8-fold symmetry)
    for (int v = 0; v < 8; v++) {
        double angle = v * MATH_TWO_PI / 8.0;
        for (int d = 0; d < 13; d++) {
            double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
            vertices[idx][d] = math_cos(angle * freq) * math_pow(MATH_PHI, d % 4);
        }
        idx++;
    }
    
    // Octahedron: 6 vertices (6-fold symmetry)
    for (int v = 0; v < 6; v++) {
        double angle = v * MATH_TWO_PI / 6.0;
        for (int d = 0; d < 13; d++) {
            double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
            vertices[idx][d] = math_cos(angle * freq) * math_pow(MATH_PHI, d % 3);
        }
        idx++;
    }
    
    // Dodecahedron: 20 vertices (20-fold symmetry)
    for (int v = 0; v < 20; v++) {
        double angle = v * MATH_TWO_PI / 20.0;
        for (int d = 0; d < 13; d++) {
            double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
            vertices[idx][d] = math_cos(angle * freq) * math_pow(MATH_PHI, d % 5);
        }
        idx++;
    }
    
    // Icosahedron: 12 vertices (12-fold symmetry - Babylonian!)
    for (int v = 0; v < 12; v++) {
        double angle = v * MATH_TWO_PI / 12.0;
        for (int d = 0; d < 13; d++) {
            double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
            vertices[idx][d] = math_cos(angle * freq) * math_pow(MATH_PHI, d % 4);
        }
        idx++;
    }
    
    *num_vertices = idx;  // Should be exactly 50
}

/**
 * Generate vertices for a specific Platonic solid
 */
void math_generate_single_platonic_13d(
    int solid_type,
    double vertices[][13],
    int* num_vertices
) {
    int idx = 0;
    int num_verts = 0;
    int phi_mod = 3;
    
    switch (solid_type) {
        case 0: num_verts = 4; phi_mod = 3; break;   // Tetrahedron
        case 1: num_verts = 8; phi_mod = 4; break;   // Cube
        case 2: num_verts = 6; phi_mod = 3; break;   // Octahedron
        case 3: num_verts = 20; phi_mod = 5; break;  // Dodecahedron
        case 4: num_verts = 12; phi_mod = 4; break;  // Icosahedron
        default: *num_vertices = 0; return;
    }
    
    for (int v = 0; v < num_verts; v++) {
        double angle = v * MATH_TWO_PI / (double)num_verts;
        for (int d = 0; d < 13; d++) {
            double freq = (double)CLOCK_LATTICE_FREQUENCIES_13D[d];
            vertices[idx][d] = math_cos(angle * freq) * math_pow(MATH_PHI, d % phi_mod);
        }
        idx++;
    }
    
    *num_vertices = idx;
}
