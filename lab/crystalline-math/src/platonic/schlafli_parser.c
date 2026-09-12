/**
 * @file schlafli_parser.c
 * @brief Schläfli symbol parsing and manipulation
 */

#include "math/schlafli.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// CREATION AND PARSING
// ============================================================================

SchlafliSymbol* schlafli_create(const uint32_t* components, uint32_t length) {
    if (!components || length == 0) {
        return NULL;
    }
    
    SchlafliSymbol* symbol = (SchlafliSymbol*)calloc(1, sizeof(SchlafliSymbol));
    if (!symbol) {
        return NULL;
    }
    
    // Allocate and copy components
    symbol->components = (uint32_t*)calloc(length, sizeof(uint32_t));
    if (!symbol->components) {
        free(symbol);
        return NULL;
    }
    
    memcpy(symbol->components, components, length * sizeof(uint32_t));
    symbol->length = length;
    symbol->dimension = length + 1;  // Schläfli length n → dimension n+1
    
    // Set derived properties
    symbol->vertices_per_face = components[0];
    symbol->faces_per_vertex = (length >= 2) ? components[1] : 0;
    symbol->cells_per_edge = (length >= 3) ? components[2] : 0;
    
    // Validate
    symbol->is_valid = schlafli_validate(symbol);
    symbol->is_regular = symbol->is_valid;  // All valid Schläfli symbols are regular
    
    return symbol;
}

SchlafliSymbol* schlafli_parse(const char* symbol_str) {
    if (!symbol_str) {
        return NULL;
    }
    
    // Skip whitespace and find opening brace
    const char* p = symbol_str;
    while (*p && isspace(*p)) p++;
    
    if (*p != '{') {
        fprintf(stderr, "Error: Schläfli symbol must start with '{'\n");
        return NULL;
    }
    p++;
    
    // Parse components
    uint32_t components[16];  // Max 16 components (up to 18D)
    uint32_t count = 0;
    
    while (*p && *p != '}' && count < 16) {
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        
        // Parse number
        if (isdigit(*p)) {
            uint32_t value = 0;
            while (isdigit(*p)) {
                value = value * 10 + (*p - '0');
                p++;
            }
            components[count++] = value;
        }
        
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        
        // Check for comma or closing brace
        if (*p == ',') {
            p++;
        } else if (*p == '}') {
            break;
        } else if (*p != '\0') {
            fprintf(stderr, "Error: Invalid character in Schläfli symbol: '%c'\n", *p);
            return NULL;
        }
    }
    
    if (*p != '}') {
        fprintf(stderr, "Error: Schläfli symbol must end with '}'\n");
        return NULL;
    }
    
    if (count == 0) {
        fprintf(stderr, "Error: Schläfli symbol must have at least one component\n");
        return NULL;
    }
    
    return schlafli_create(components, count);
}

// ============================================================================
// VALIDATION
// ============================================================================

bool schlafli_validate(const SchlafliSymbol* symbol) {
    if (!symbol || !symbol->components || symbol->length == 0) {
        return false;
    }
    
    // All components must be >= 3
    for (uint32_t i = 0; i < symbol->length; i++) {
        if (symbol->components[i] < 3) {
            return false;
        }
    }
    
    // For 3D (length = 2), check if it's one of the 5 Platonic solids
    if (symbol->length == 2) {
        uint32_t p = symbol->components[0];
        uint32_t q = symbol->components[1];
        
        // Valid 3D Platonic solids: {3,3}, {4,3}, {3,4}, {5,3}, {3,5}
        if ((p == 3 && q == 3) ||  // Tetrahedron
            (p == 4 && q == 3) ||  // Cube
            (p == 3 && q == 4) ||  // Octahedron
            (p == 5 && q == 3) ||  // Dodecahedron
            (p == 3 && q == 5)) {  // Icosahedron
            return true;
        }
        
        return false;
    }
    
    // For 4D (length = 3), check if it's one of the 6 regular polychora
    if (symbol->length == 3) {
        uint32_t p = symbol->components[0];
        uint32_t q = symbol->components[1];
        uint32_t r = symbol->components[2];
        
        // Valid 4D polychora: {3,3,3}, {4,3,3}, {3,3,4}, {3,4,3}, {5,3,3}, {3,3,5}
        if ((p == 3 && q == 3 && r == 3) ||  // 5-cell
            (p == 4 && q == 3 && r == 3) ||  // Tesseract
            (p == 3 && q == 3 && r == 4) ||  // 16-cell
            (p == 3 && q == 4 && r == 3) ||  // 24-cell
            (p == 5 && q == 3 && r == 3) ||  // 120-cell
            (p == 3 && q == 3 && r == 5)) {  // 600-cell
            return true;
        }
        
        return false;
    }
    
    // For 5D+ (length >= 4), only simplex, hypercube, and cross-polytope are regular
    if (symbol->length >= 4) {
        // Check if all components are 3 (simplex)
        bool all_three = true;
        for (uint32_t i = 0; i < symbol->length; i++) {
            if (symbol->components[i] != 3) {
                all_three = false;
                break;
            }
        }
        if (all_three) {
            return true;  // Simplex {3,3,...,3}
        }
        
        // Check if it's hypercube {4,3,...,3}
        if (symbol->components[0] == 4) {
            bool rest_three = true;
            for (uint32_t i = 1; i < symbol->length; i++) {
                if (symbol->components[i] != 3) {
                    rest_three = false;
                    break;
                }
            }
            if (rest_three) {
                return true;  // Hypercube
            }
        }
        
        // Check if it's cross-polytope {3,3,...,4}
        if (symbol->components[symbol->length - 1] == 4) {
            bool rest_three = true;
            for (uint32_t i = 0; i < symbol->length - 1; i++) {
                if (symbol->components[i] != 3) {
                    rest_three = false;
                    break;
                }
            }
            if (rest_three) {
                return true;  // Cross-polytope
            }
        }
        
        return false;
    }
    
    return false;
}

bool schlafli_is_regular(const SchlafliSymbol* symbol) {
    // All valid Schläfli symbols represent regular polytopes
    return schlafli_validate(symbol);
}

// ============================================================================
// PROPERTIES
// ============================================================================

uint32_t schlafli_get_dimension(const SchlafliSymbol* symbol) {
    if (!symbol) {
        return 0;
    }
    return symbol->dimension;
}

uint32_t schlafli_get_vertices_per_face(const SchlafliSymbol* symbol) {
    if (!symbol || symbol->length == 0) {
        return 0;
    }
    return symbol->vertices_per_face;
}

uint32_t schlafli_get_faces_per_vertex(const SchlafliSymbol* symbol) {
    if (!symbol || symbol->length < 2) {
        return 0;
    }
    return symbol->faces_per_vertex;
}

// ============================================================================
// STRING CONVERSION
// ============================================================================

char* schlafli_to_string(const SchlafliSymbol* symbol) {
    if (!symbol || !symbol->components || symbol->length == 0) {
        return NULL;
    }
    
    // Allocate buffer (max 10 chars per component + commas + braces + null)
    size_t buffer_size = symbol->length * 12 + 3;
    char* str = (char*)malloc(buffer_size);
    if (!str) {
        return NULL;
    }
    
    // Build string
    char* p = str;
    *p++ = '{';
    
    for (uint32_t i = 0; i < symbol->length; i++) {
        if (i > 0) {
            *p++ = ',';
        }
        p += sprintf(p, "%u", symbol->components[i]);
    }
    
    *p++ = '}';
    *p = '\0';
    
    return str;
}

void schlafli_print(const SchlafliSymbol* symbol) {
    if (!symbol) {
        printf("(null)\n");
        return;
    }
    
    char* str = schlafli_to_string(symbol);
    if (str) {
        printf("%s", str);
        free(str);
    }
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void schlafli_free(SchlafliSymbol* symbol) {
    if (!symbol) {
        return;
    }
    
    if (symbol->components) {
        free(symbol->components);
    }
    
    free(symbol);
}

// ============================================================================
// COMPARISON
// ============================================================================

bool schlafli_equals(const SchlafliSymbol* a, const SchlafliSymbol* b) {
    if (!a || !b) {
        return false;
    }
    
    if (a->length != b->length) {
        return false;
    }
    
    for (uint32_t i = 0; i < a->length; i++) {
        if (a->components[i] != b->components[i]) {
            return false;
        }
    }
    
    return true;
}