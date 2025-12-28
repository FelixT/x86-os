#ifndef UI_GRID_H
#define UI_GRID_H

#include <stdint.h>
#include "wo.h"

#define MAX_GRID_CELL_CHILDREN 16

typedef struct grid_cell_t {
   wo_t *children[MAX_GRID_CELL_CHILDREN];
   int child_count;
   bool hovering;
} grid_cell_t;

typedef struct grid_t {
   uint16_t colour_border_light;
   uint16_t colour_border_dark;
   uint16_t colour_bg;
   uint16_t colour_bg_hovered;
   bool bordered;
   bool filled;
   int rows;
   int cols;
   grid_cell_t **cells; // cell_content[rows][cols] -> grid_cell_t
   void (*click_func)(int row, int col);
} grid_t;

wo_t *create_grid(int x, int y, int width, int height, int rows, int cols);
void grid_add(wo_t *grid, wo_t *child, int row, int col);

#endif