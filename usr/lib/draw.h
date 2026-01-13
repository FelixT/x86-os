#ifndef DRAW_H
#define DRAW_H

#include "../prog.h"

void draw_line(surface_t *surface, uint16_t colour, int x, int y, bool vertical, int length);
void draw_rect_gradient(surface_t *surface, uint16_t color1, uint16_t color2, int x, int y, int width, int height, int direction);
void draw_unfilledrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_rect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_pixel(surface_t *surface, uint16_t colour, int x, int y);
void draw_checkeredrect(surface_t *surface, uint16_t colour1, uint16_t colour2, int x, int y, int width, int height);

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