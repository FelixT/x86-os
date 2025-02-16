#ifndef DRAW_H
#define DRAW_H

#include <stdbool.h>

#include "surface_t.h"

void draw_rect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_unfilledrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_dottedrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height, int *buffer, bool restore);
void draw_line(surface_t *surface, uint16_t colour, int x, int y, bool vertical, int length);
void draw_char(surface_t *surface, char c, uint16_t colour, int x, int y);
void draw_string(surface_t *surface, char *c, uint16_t colour, int x, int y);
uint16_t rgb16(uint8_t r, uint8_t g, uint8_t b);

void setpixel_safe(surface_t *surface, int index, int colour);

#endif