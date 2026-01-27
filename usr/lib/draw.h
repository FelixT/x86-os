#ifndef DRAW_H
#define DRAW_H

#include "../prog.h"

typedef struct rect_t {
   int x;
   int y;
   int width;
   int height;
} rect_t;

typedef struct draw_context_t {
   surface_t *surface;
   int window;
   int offsetX;
   int offsetY;
   rect_t clipRect;
} draw_context_t;

void draw_line(draw_context_t *ctx, uint16_t colour, int x, int y, bool vertical, int length);
void draw_rect_gradient(draw_context_t *ctx, uint16_t color1, uint16_t color2, int x, int y, int width, int height, int direction);
void draw_unfilledrect(draw_context_t *ctx, uint16_t colour, int x, int y, int width, int height);
void draw_rect(draw_context_t *ctx, uint16_t colour, int x, int y, int width, int height);
void draw_pixel(draw_context_t *ctx, uint16_t colour, int x, int y);
void draw_checkeredrect(surface_t *surface, uint16_t colour1, uint16_t colour2, int x, int y, int width, int height);
void draw_string(draw_context_t *ctx, char *str, uint16_t colour, int x, int y);
rect_t rect_intersect(rect_t a, rect_t b);

// 5r 6g 5b
static inline int get_r16(uint16_t c) {
   return (c >> 11) << 3;
}

static inline int get_g16(uint16_t c) {
   return ((c >> 5) & 0x3F) << 2;
}

static inline int get_b16(uint16_t c) {
   return (c & 0x1F) << 3;
}

#endif