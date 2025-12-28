#include "ui_grid.h"
#include "../draw.h"
#include "wo.h"
#include "../stdio.h"

void draw_grid_cell(wo_t *grid, surface_t *surface, int window, int offsetX, int offsetY, grid_cell_t *cell, int row, int col) {
   grid_t *grid_data = (grid_t *)grid->data;
   int cellOffsetX = col*(grid->width/grid_data->cols);
   int cellOffsetY = row*(grid->height/grid_data->rows);

   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   uint16_t bg = cell->hovering ? grid_data->colour_bg_hovered : grid_data->colour_bg;

   draw_rect(surface, bg, grid->x + cellOffsetX + 1, grid->y + cellOffsetY + 1, cellWidth - 2, cellHeight - 2);
   
   // cell content
   for(int k = 0; k < cell->child_count; k++) {
      wo_t *child = cell->children[k];
      if(child && child->draw_func)
         child->draw_func(child, surface, window, grid->x + offsetX + cellOffsetX, grid->y + offsetY + cellOffsetY);
   }
      
}

void draw_grid(wo_t *grid, surface_t *surface, int window, int offsetX, int offsetY) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   uint16_t light = grid_data->colour_border_light;
   uint16_t dark = grid_data->colour_border_dark;
   uint16_t bg = grid_data->colour_bg;

   int x = grid->x;
   int y = grid->y;
   int width = grid->width;
   int height = grid->height;

   // border
   if(grid_data->bordered) {
      draw_line(surface, dark,  x, y, true, height);
      draw_line(surface, dark,  x, y, false, width);
      draw_line(surface, light, x, y + height - 1, false, width);
      draw_line(surface, light, x + width - 1, y, true, height);

      if(grid_data->filled)
         draw_rect(surface, bg, x + 1, y + 1, width - 2, height - 2);

      // grid lines
      for(int i = 0; i < grid_data->rows - 1; i++) {
         int lineY = (i+1)*(grid->height/grid_data->rows) + y;
         draw_line(surface, light, x + 1, lineY, false, width - 2);
      }
      for(int i = 0; i < grid_data->cols - 1; i++) {
         int lineX = (i+1)*(grid->width/grid_data->cols) + x;
         draw_line(surface, light, lineX, y + 1, true, height - 2);
      }
   } else {
      if(grid_data->filled)
         draw_rect(surface, bg, x, y, width, height);
   }

   // draw cells
   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         draw_grid_cell(grid, surface, window, offsetX, offsetY, cell, i, j);
      }
   }

}

void grid_add(wo_t *grid, wo_t *child, int row, int col) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   grid_cell_t *cell = &grid_data->cells[row][col];
   cell->children[cell->child_count++] = child;
}

void grid_click(wo_t *grid, surface_t *surface, int window, int x, int y) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;


   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         int cellOffsetX = j*(grid->width/grid_data->cols);
         int cellOffsetY = i*(grid->height/grid_data->rows);

         // clicked cell
         if(x >= cellOffsetX && x < cellOffsetX + cellWidth
         && y >= cellOffsetY && y < cellOffsetY + cellHeight) {
            if(grid_data->click_func)
               grid_data->click_func(i, j);
         } else {
            continue;
         }

         // clicked cell elements
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];

            if(x >= child->x + cellOffsetX && x < child->x + child->width + cellOffsetX
            && y >= child->y + cellOffsetY && y < child->y + child->height + cellOffsetY) {
               if(child->click_func)
                  child->click_func(child, surface, window, x - (child->x + grid->x + cellOffsetX), y - (child->y + grid->y + cellOffsetY));
               
               child->clicked = true;
               child->draw_func(child, surface, window, grid->x + cellOffsetX, grid->y + cellOffsetY);
               child->clicked = false;
            }
         }
      }
   }
}

void grid_release(wo_t *grid, surface_t *surface, int window, int x, int y) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];
            int cellOffsetX = j*(grid->width/grid_data->cols);
            int cellOffsetY = i*(grid->height/grid_data->rows);

            if(x >= child->x + cellOffsetX && x < child->x + child->width + cellOffsetX
            && y >= child->y + cellOffsetY && y < child->y + child->height + cellOffsetY) {
               if(child->release_func)
                  child->release_func(child, surface, window, x - (child->x + grid->x + cellOffsetX), y - (child->y + grid->y + cellOffsetY));
               if(child->type == WO_INPUT)
                  child->selected = true;
               child->draw_func(child, surface, window, grid->x + cellOffsetX, grid->y + cellOffsetY);
            }
         }
      }
   }
}

void grid_hover(wo_t *grid, surface_t *surface, int window, int x, int y) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];

         int cellOffsetX = j * cellWidth;
         int cellOffsetY = i * cellHeight;

         // hovering cell?
         if(x >= cellOffsetX && x < cellOffsetX + cellWidth
         && y >= cellOffsetY && y < cellOffsetY + cellHeight) {
            cell->hovering = true;
            draw_grid_cell(grid, surface, window, 0, 0, cell, i, j);
         } else {
            cell->hovering = false;
            draw_grid_cell(grid, surface, window, 0, 0, cell, i, j);
            continue;
         }

         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];

            bool was_hovering = child->hovering;

            if(x >= child->x + cellOffsetX && x < child->x + child->width + cellOffsetX
            && y >= child->y + cellOffsetY && y < child->y + child->height + cellOffsetY) {
               if(was_hovering) continue;
               if(child->hover_func)
                  child->hover_func(child, surface, window, x - (child->x + grid->x + cellOffsetX), y - (child->y + grid->y + cellOffsetY));
               child->hovering = true;
               child->draw_func(child, surface, window, grid->x + cellOffsetX, grid->y + cellOffsetY);
            } else {
               if(!was_hovering) continue;
               child->hovering = false;
               child->draw_func(child, surface, window, grid->x + cellOffsetX, grid->y + cellOffsetY);
            }
         }
      }
   }
}

void grid_unfocus(wo_t *grid, surface_t *surface, int window) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         int cellOffsetX = j * cellWidth;
         int cellOffsetY = i * cellHeight;

         grid_cell_t *cell = &grid_data->cells[i][j];
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];
            if(!child->selected) continue;
            if(child->unfocus_func)
               child->unfocus_func(child, surface, window);
            child->selected = false;
            child->draw_func(child, surface, window, grid->x + cellOffsetX, grid->y + cellOffsetY);
         }
      }
   }
}

void grid_keypress(wo_t *grid, uint16_t c, int window) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];
            if(!child->selected) continue;
            if(child->keypress_func)
               child->keypress_func(child, c, window);
            return;
         }
      }
   }
}

void grid_unhover(wo_t *grid, surface_t *surface, int window) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         if(cell->hovering) {
            cell->hovering = false;
            draw_grid_cell(grid, surface, window, 0, 0, cell, i, j);
         }
      }
   }
}

wo_t *create_grid(int x, int y, int width, int height, int rows, int cols) {
   wo_t *grid = create_wo(x, y, width, height);
   if(!grid) return NULL;

   grid_t *grid_data = malloc(sizeof(grid_t));
   grid_data->colour_border_light = rgb16(200, 200, 200);
   grid_data->colour_border_dark = rgb16(100, 100, 100);
   grid_data->colour_bg = rgb16(252, 252, 252);
   grid_data->colour_bg_hovered = rgb16(244, 244, 244);
   grid_data->bordered = true;
   grid_data->filled = true;
   grid_data->rows = rows;
   grid_data->cols = cols;
   grid_data->click_func = NULL;

   for(int i = 0; i < rows; i++) {
      grid_data->cells[i] = malloc(sizeof(grid_cell_t) * cols);
      for(int j = 0; j < cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         cell->child_count = 0;
         cell->hovering = false;
      }
   }

   grid->type = WO_GRID;
   grid->data = grid_data;
   grid->draw_func = &draw_grid;
   grid->click_func = &grid_click;
   grid->release_func = &grid_release;
   grid->hover_func = &grid_hover;
   grid->unhover_func = &grid_unhover;
   grid->unfocus_func = &grid_unfocus;
   grid->keypress_func = &grid_keypress;

   return grid;
}