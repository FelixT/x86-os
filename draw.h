#ifndef DRAW_H
#define DRAW_H

#include <stdbool.h>

#include "surface_t.h"

typedef struct rect_t {
   int x, y;
   int w, h;
} rect_t;

static inline uint16_t rgb16(uint8_t r, uint8_t g, uint8_t b) {
   // 5r 6g 5b
   return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static inline void setpixel_safe(surface_t *surface, int index, uint16_t colour) {
   if(index < 0 || index >= surface->pitch*surface->height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface->buffer)[index] = colour;
   }
}

void draw_rect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_rect_gradient(surface_t *surface, uint16_t color1, uint16_t color2, int x, int y, int width, int height, int direction);
void draw_unfilledrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_dottedrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height, int *buffer, bool restore);
void draw_line(surface_t *surface, uint16_t colour, int x, int y, bool vertical, int length);
void draw_char(surface_t *surface, char c, uint16_t colour, int x, int y);
void draw_string(surface_t *surface, char *c, uint16_t colour, int x, int y);
uint16_t rgb16_lighten(uint16_t color, uint8_t amount);
uint16_t rgb16_darken(uint16_t color, uint8_t amount);
void draw_set_clip(rect_t rect);
void draw_clear_clip();
bool clip_rect(surface_t *surface, int *x, int *y, int *w, int *h);

#endif
