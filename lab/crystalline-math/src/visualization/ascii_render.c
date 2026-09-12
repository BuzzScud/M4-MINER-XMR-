/**
 * @file ascii_render.c
 * @brief ASCII art visualization for Crystalline Abacus
 * 
 * ⚠️ CRITICAL: NO math.h - Uses ONLY internal math functions ⚠️
 * 
 * Creates terminal-friendly visualizations of:
 * - Clock lattice structure
 * - Abacus beads on the clock
 * - Arithmetic operations
 */

#include "math/visualization.h"
#include "math/arithmetic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ASCII art dimensions */
#define ASCII_WIDTH 80
#define ASCII_HEIGHT 40
#define CENTER_X 40
#define CENTER_Y 20

/* Ring radii in character units */
#define RING_0_RADIUS 18  /* Outer ring (12 positions) */
#define RING_1_RADIUS 14  /* Ring 1 (60 positions) */
#define RING_2_RADIUS 10  /* Ring 2 (60 positions) */
#define RING_3_RADIUS 6   /* Inner ring (100 positions) */

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Clear ASCII buffer
 */
static void clear_buffer(char* buffer, size_t width, size_t height) {
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            buffer[y * width + x] = ' ';
        }
    }
}

/**
 * @brief Draw a character at position (with bounds checking)
 */
static void draw_char(char* buffer, size_t width, size_t height,
                     int x, int y, char c) {
    if (x >= 0 && x < (int)width && y >= 0 && y < (int)height) {
        buffer[y * width + x] = c;
    }
}

/**
 * @brief Draw a circle using Bresenham's algorithm
 */
static void draw_circle(char* buffer, size_t width, size_t height,
                       int cx, int cy, int radius, char c) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    
    while (x <= y) {
        /* Draw 8 octants */
        draw_char(buffer, width, height, cx + x, cy + y, c);
        draw_char(buffer, width, height, cx - x, cy + y, c);
        draw_char(buffer, width, height, cx + x, cy - y, c);
        draw_char(buffer, width, height, cx - x, cy - y, c);
        draw_char(buffer, width, height, cx + y, cy + x, c);
        draw_char(buffer, width, height, cx - y, cy + x, c);
        draw_char(buffer, width, height, cx + y, cy - x, c);
        draw_char(buffer, width, height, cx - y, cy - x, c);
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/**
 * @brief Draw a line using Bresenham's algorithm
 */
static void draw_line(char* buffer, size_t width, size_t height,
                     int x0, int y0, int x1, int y1, char c) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        draw_char(buffer, width, height, x0, y0, c);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* ============================================================================
 * CLOCK LATTICE VISUALIZATION
 * ============================================================================
 */

/**
 * @brief Render clock lattice structure as ASCII
 */
MathError clock_lattice_to_ascii(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < ASCII_WIDTH * ASCII_HEIGHT) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Clear buffer */
    clear_buffer(buffer, ASCII_WIDTH, ASCII_HEIGHT);
    
    /* Draw the four rings */
    draw_circle(buffer, ASCII_WIDTH, ASCII_HEIGHT, CENTER_X, CENTER_Y, RING_0_RADIUS, 'O');
    draw_circle(buffer, ASCII_WIDTH, ASCII_HEIGHT, CENTER_X, CENTER_Y, RING_1_RADIUS, 'o');
    draw_circle(buffer, ASCII_WIDTH, ASCII_HEIGHT, CENTER_X, CENTER_Y, RING_2_RADIUS, '.');
    draw_circle(buffer, ASCII_WIDTH, ASCII_HEIGHT, CENTER_X, CENTER_Y, RING_3_RADIUS, '*');
    
    /* Draw center point */
    draw_char(buffer, ASCII_WIDTH, ASCII_HEIGHT, CENTER_X, CENTER_Y, '+');
    
    /* Draw cardinal directions (12 positions on outer ring) */
    for (int i = 0; i < 12; i++) {
        double angle = (double)i / 12.0 * 2.0 * MATH_PI;
        int x = CENTER_X + (int)(RING_0_RADIUS * math_cos(angle));
        int y = CENTER_Y + (int)(RING_0_RADIUS * math_sin(angle));
        
        /* Draw tick mark */
        draw_char(buffer, ASCII_WIDTH, ASCII_HEIGHT, x, y, '|');
    }
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * CLOCK POSITION VISUALIZATION
 * ============================================================================
 */

/**
 * @brief Render clock position as ASCII art
 */
MathError clock_position_to_ascii(const ClockPosition* pos,
                                   char* buffer,
                                   size_t buffer_size) {
    if (!pos || !buffer || buffer_size < ASCII_WIDTH * ASCII_HEIGHT) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Start with clock lattice */
    clock_lattice_to_ascii(buffer, buffer_size);
    
    /* Convert position to Cartesian */
    Point2D cart;
    MathError err = clock_to_cartesian(pos, &cart);
    if (err != MATH_SUCCESS) {
        return err;
    }
    
    /* Scale to ASCII coordinates */
    int x = CENTER_X + (int)(cart.x * RING_0_RADIUS);
    int y = CENTER_Y + (int)(cart.y * RING_0_RADIUS);
    
    /* Draw position marker */
    draw_char(buffer, ASCII_WIDTH, ASCII_HEIGHT, x, y, '@');
    
    /* Draw line from center to position */
    draw_line(buffer, ASCII_WIDTH, ASCII_HEIGHT, CENTER_X, CENTER_Y, x, y, '-');
    
    return MATH_SUCCESS;
}

/* ============================================================================
 * ABACUS VISUALIZATION
 * ============================================================================
 */

/**
 * @brief Render abacus as ASCII art
 */
MathError abacus_to_ascii(const CrystallineAbacus* abacus,
                          char* buffer,
                          size_t buffer_size) {
    if (!abacus || !buffer || buffer_size < ASCII_WIDTH * ASCII_HEIGHT) {
        return MATH_ERROR_INVALID_ARG;
    }
    
    /* Start with clock lattice */
    clock_lattice_to_ascii(buffer, buffer_size);
    
    /* Draw each bead */
    for (size_t i = 0; i < abacus->num_beads; i++) {
        const AbacusBead* bead = &abacus->beads[i];
        
        /* Convert position to Cartesian */
        Point2D cart;
        MathError err = clock_to_cartesian(&bead->position, &cart);
        if (err != MATH_SUCCESS) {
            continue;  /* Skip invalid positions */
        }
        
        /* Scale to ASCII coordinates */
        int x = CENTER_X + (int)(cart.x * RING_0_RADIUS);
        int y = CENTER_Y + (int)(cart.y * RING_0_RADIUS);
        
        /* Choose character based on weight exponent */
        char c;
        if (bead->weight_exponent > 0) {
            c = '#';  /* Integer part (high weight) */
        } else if (bead->weight_exponent == 0) {
            c = '@';  /* Ones place */
        } else {
            c = '*';  /* Fractional part (low weight) */
        }
        
        /* Draw bead */
        draw_char(buffer, ASCII_WIDTH, ASCII_HEIGHT, x, y, c);
    }
    
    return MATH_SUCCESS;
}

/**
 * @brief Print abacus as ASCII art to stdout
 */
void abacus_print_ascii(const CrystallineAbacus* abacus) {
    if (!abacus) {
        printf("NULL abacus\n");
        return;
    }
    
    /* Allocate buffer */
    size_t buffer_size = ASCII_WIDTH * ASCII_HEIGHT;
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        printf("ERROR: Memory allocation failed\n");
        return;
    }
    
    /* Render to buffer */
    MathError err = abacus_to_ascii(abacus, buffer, buffer_size);
    if (err != MATH_SUCCESS) {
        printf("ERROR: Rendering failed\n");
        free(buffer);
        return;
    }
    
    /* Print buffer line by line */
    printf("\n");
    printf("Crystalline Abacus (Base %u):\n", abacus->base);
    printf("Sign: %s\n", abacus->negative ? "negative" : "positive");
    printf("Beads: %zu\n", abacus->num_beads);
    printf("\n");
    
    for (size_t y = 0; y < ASCII_HEIGHT; y++) {
        for (size_t x = 0; x < ASCII_WIDTH; x++) {
            putchar(buffer[y * ASCII_WIDTH + x]);
        }
        putchar('\n');
    }
    
    printf("\nLegend:\n");
    printf("  O = Outer ring (12 positions)\n");
    printf("  o = Ring 1 (60 positions)\n");
    printf("  . = Ring 2 (60 positions)\n");
    printf("  * = Inner ring (100 positions)\n");
    printf("  # = Integer bead (weight > 1)\n");
    printf("  @ = Ones place (weight = 1)\n");
    printf("  * = Fractional bead (weight < 1)\n");
    printf("\n");
    
    /* Print bead details */
    printf("Bead Details:\n");
    for (size_t i = 0; i < abacus->num_beads; i++) {
        const AbacusBead* bead = &abacus->beads[i];
        printf("  [%zu] value=%u, exponent=%d, ring=%u, pos=%u\n",
               i, bead->value, bead->weight_exponent,
               bead->position.ring, bead->position.position);
    }
    printf("\n");
    
    free(buffer);
}