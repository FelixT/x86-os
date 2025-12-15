#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include <stdint.h>
#include "wo.h"

#define MAX_CANVAS_CHILDREN 16

typedef struct canvas_t {
   uint16_t colour_border_light;
   uint16_t colour_border_dark;
   uint16_t colour_bg;
   bool bordered;
   bool filled;

   int child_count;
   wo_t *children[MAX_CANVAS_CHILDREN];
} canvas_t;

wo_t *create_canvas(int x, int y, int width, int height);
void canvas_add(wo_t *canvas, wo_t *child);

#endif