#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
   uint32_t buffer;
   int width;
   int height;
} surface_t;

void draw_rect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_unfilledrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_dottedrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height, int *buffer, bool restore);
void draw_line(surface_t *surface, uint16_t colour, int x, int y, bool vertical, int length);
void draw_char(surface_t *surface, char c, uint16_t colour, int x, int y);
void draw_string(surface_t *surface, char *c, uint16_t colour, int x, int y);

void setpixel_safe(surface_t *surface, int index, int colour);

#endif