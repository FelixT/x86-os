#ifndef DRAW_H
#define DRAW_H

#include "../prog.h"

void draw_line(surface_t *surface, uint16_t colour, int x, int y, bool vertical, int length);
void draw_rect_gradient(surface_t *surface, uint16_t color1, uint16_t color2, int x, int y, int width, int height, int direction);
void draw_unfilledrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);
void draw_rect(surface_t *surface, uint16_t colour, int x, int y, int width, int height);

#endif