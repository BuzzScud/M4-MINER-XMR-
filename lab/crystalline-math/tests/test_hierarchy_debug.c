#include <stdio.h>
#include <stdlib.h>
#include "math/higher_faces.h"
#include "math/platonic_generator.h"

int main() {
    printf("Testing face hierarchy generation...\n");
    
    // Create a simple tetrahedron
    PlatonicSolid* solid = platonic_generate_simplex(3);
    if (!solid) {
        printf("Failed to create tetrahedron\n");
        return 1;
    }
    
    printf("Tetrahedron: %lu vertices, %lu edges\n", 
           (unsigned long)solid->num_vertices, (unsigned long)solid->num_edges);
    
    // Generate face hierarchy
    FaceHierarchy* hierarchy = higher_faces_generate_hierarchy(solid);
    if (!hierarchy) {
        printf("Failed to generate hierarchy\n");
        platonic_free(solid);
        return 1;
    }
    
    printf("Face hierarchy generated successfully\n");
    printf("Polytope dimension: %u\n", hierarchy->polytope_dimension);
    printf("Number of dimensions: %u\n", hierarchy->num_dimensions);
    
    // Print face counts
    for (uint32_t k = 0; k < hierarchy->num_dimensions; k++) {
        if (hierarchy->face_sets[k]) {
            printf("  %u-faces: %u\n", k, hierarchy->face_sets[k]->count);
        }
    }
    
    // Cleanup
    if (hierarchy) {
        for (uint32_t k = 0; k < hierarchy->num_dimensions; k++) {
            if (hierarchy->face_sets[k]) {
                if (hierarchy->face_sets[k]->faces) {
                    for (uint32_t i = 0; i < hierarchy->face_sets[k]->count; i++) {
                        if (hierarchy->face_sets[k]->faces[i]) {
                            if (hierarchy->face_sets[k]->faces[i]->vertex_indices) {
                                free(hierarchy->face_sets[k]->faces[i]->vertex_indices);
                            }
                            if (hierarchy->face_sets[k]->faces[i]->subface_indices) {
                                free(hierarchy->face_sets[k]->faces[i]->subface_indices);
                            }
                            free(hierarchy->face_sets[k]->faces[i]);
                        }
                    }
                    free(hierarchy->face_sets[k]->faces);
                }
                free(hierarchy->face_sets[k]);
            }
        }
        free(hierarchy->face_sets);
        free(hierarchy);
    }
    
    platonic_free(solid);
    
    printf("Test completed successfully\n");
    return 0;
}
