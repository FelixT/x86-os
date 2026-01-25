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
   bool fill_hovered_empty_cells;
   int rows;
   int cols;
   grid_cell_t **cells; // cell_content[rows][cols] -> grid_cell_t
   int (*click_func)(wo_t *grid, int window, int row, int col);
   grid_cell_t *hovered;
   int hoveredrow;
   int hoveredcol;
} grid_t;

wo_t *create_grid(int x, int y, int width, int height, int rows, int cols);
void grid_add(wo_t *grid, wo_t *child, int row, int col);
int get_grid_cell_width(wo_t *grid);
int get_grid_cell_height(wo_t *grid);
void grid_item_fill_cell(wo_t *grid, wo_t *item);
void grid_item_center_cell(wo_t *grid, wo_t *item);
void draw_grid_cell(wo_t *grid, draw_context_t context, grid_cell_t *cell, int row, int col);
grid_t *get_grid(wo_t *grid);

#endif