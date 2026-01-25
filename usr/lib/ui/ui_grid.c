#include "ui_grid.h"
#include "../draw.h"
#include "wo.h"
#include "../stdio.h"
#include "../../../lib/string.h"

void draw_grid_cell(wo_t *grid, draw_context_t context, grid_cell_t *cell, int row, int col) {
   grid_t *grid_data = (grid_t *)grid->data;
   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   int cellOffsetX = col*cellWidth;
   int cellOffsetY = row*cellHeight;

   context.offsetX += grid->x + cellOffsetX;
   context.offsetY += grid->y + cellOffsetY;
   rect_t cellRect = {
      .x = context.offsetX,
      .y = context.offsetY,
      .width = cellWidth,
      .height = cellHeight
   };
   context.clipRect = rect_intersect(context.clipRect, cellRect);

   uint16_t bg = cell->hovering && (grid_data->fill_hovered_empty_cells || cell->child_count > 0) ? grid_data->colour_bg_hovered : grid_data->colour_bg;

   if(grid_data->filled) {
      int bgx = context.offsetX;
      int bgy = context.offsetY;
      int bgw = cellWidth;
      int bgh = cellHeight;
      if(grid_data->bordered) {
         bgx++;
         bgy++;
         bgw-=2;
         bgh-=2;
      }

      draw_rect(&context, bg, bgx, bgy, bgw, bgh);
   }
   
   // cell content
   for(int k = 0; k < cell->child_count; k++) {
      wo_t *child = cell->children[k];
      if(child && child->draw_func)
         child->draw_func(child, context);
   }
}

void draw_grid(wo_t *grid, draw_context_t context) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   uint16_t light = grid_data->colour_border_light;
   uint16_t dark = grid_data->colour_border_dark;

   int x = grid->x + context.offsetX;
   int y = grid->y + context.offsetY;
   int width = grid->width;
   int height = grid->height;

   // border
   if(grid_data->bordered) {
      draw_line(&context, dark, x, y, true, height);
      draw_line(&context, dark, x, y, false, width);
      draw_line(&context, light, x, y + height - 1, false, width);
      draw_line(&context, light, x + width - 1, y, true, height);

      // grid lines
      for(int i = 0; i < grid_data->rows - 1; i++) {
         int lineY = (i+1)*(grid->height/grid_data->rows) + y;
         draw_line(&context, light, x + 1, lineY, false, width - 2);
      }
      for(int i = 0; i < grid_data->cols - 1; i++) {
         int lineX = (i+1)*(grid->width/grid_data->cols) + x;
         draw_line(&context, light, lineX, y + 1, true, height - 2);
      }
   }

   // draw cells
   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         draw_grid_cell(grid, context, cell, i, j);
      }
   }

}

void grid_add(wo_t *grid, wo_t *child, int row, int col) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   grid_cell_t *cell = &grid_data->cells[row][col];
   if(cell->child_count == MAX_GRID_CELL_CHILDREN) {
      debug_println("Couldn't add child to grid cell %i %i", row, col);
      return;
   }
   cell->children[cell->child_count++] = child;
}

void grid_click(wo_t *grid, draw_context_t context, int x, int y) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   int offsetX = context.offsetX;
   int offsetY = context.offsetY;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         int cellOffsetX = j*cellWidth;
         int cellOffsetY = i*cellHeight;

         // clicked cell
         if(x >= cellOffsetX && x < cellOffsetX + cellWidth
         && y >= cellOffsetY && y < cellOffsetY + cellHeight) {
            if(grid_data->click_func) {
               if(grid_data->click_func(grid, context.window, i, j))
                  return;
            }
         } else {
            continue;
         }

         context.offsetX = grid->x + cellOffsetX + offsetX;
         context.offsetY = grid->y + cellOffsetY + offsetY;
         rect_t cellRect = {
            .x = context.offsetX,
            .y = context.offsetY,
            .width = cellWidth,
            .height = cellHeight
         };
         context.clipRect = rect_intersect(context.clipRect, cellRect);

         // clicked cell elements
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];

            if(x >= child->x + cellOffsetX && x < child->x + child->width + cellOffsetX
            && y >= child->y + cellOffsetY && y < child->y + child->height + cellOffsetY) {
               if(child->click_func)
                  child->click_func(child, context, x - (child->x + cellOffsetX), y - (child->y + cellOffsetY));
               
               child->clicked = true;
               child->draw_func(child, context);
               child->clicked = false;
            }
         }
      }
   }
}

void grid_release(wo_t *grid, draw_context_t context, int x, int y) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;
   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   int offsetX = context.offsetX;
   int offsetY = context.offsetY;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];
            int cellOffsetX = j*cellWidth;
            int cellOffsetY = i*cellHeight;

            if(x >= child->x + cellOffsetX && x < child->x + child->width + cellOffsetX
            && y >= child->y + cellOffsetY && y < child->y + child->height + cellOffsetY) {
               context.offsetX = grid->x + cellOffsetX + offsetX;
               context.offsetY = grid->y + cellOffsetY + offsetY;
               rect_t cellRect = {
                  .x = context.offsetX,
                  .y = context.offsetY,
                  .width = cellWidth,
                  .height = cellHeight
               };
               context.clipRect = rect_intersect(context.clipRect, cellRect);

               if(child->focusable)
                  child->selected = true;

               if(child->release_func)
                  child->release_func(child, context, x - (child->x + cellOffsetX), y - (child->y + cellOffsetY));
               else
                  child->draw_func(child, context);
            }
         }
      }
   }
}

void grid_hover(wo_t *grid, draw_context_t context, int x, int y) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   int hoverCol = x / cellWidth;
   int hoverRow = y / cellHeight;

   int offsetX = context.offsetX;
   int offsetY = context.offsetY;

   if(hoverCol >= 0 && hoverCol < grid_data->cols
   && hoverRow >= 0 && hoverRow < grid_data->rows) {
      grid_cell_t *cell = &grid_data->cells[hoverRow][hoverCol];
      grid_cell_t *oldCell = grid_data->hovered;

      if(cell != oldCell) {
         // unhover old
         if(oldCell) {
            oldCell->hovering = false;
            draw_grid_cell(grid, context, oldCell, grid_data->hoveredrow, grid_data->hoveredcol);
         }

         // hover new
         cell->hovering = true;
         draw_grid_cell(grid, context, cell, hoverRow, hoverCol);
        
         grid_data->hovered = cell;
         grid_data->hoveredrow = hoverRow;
         grid_data->hoveredcol = hoverCol;
      }

      int cellOffsetX = hoverCol*cellWidth;
      int cellOffsetY = hoverRow*cellHeight;
      for(int k = 0; k < cell->child_count; k++) {
         wo_t *child = cell->children[k];

         bool was_hovering = child->hovering;

         context.offsetX = grid->x + cellOffsetX + offsetX;
         context.offsetY = grid->y + cellOffsetY + offsetY;
         rect_t cellRect = {
            .x = context.offsetX,
            .y = context.offsetY,
            .width = cellWidth,
            .height = cellHeight
         };
         context.clipRect = rect_intersect(context.clipRect, cellRect);

         if(x >= child->x + cellOffsetX && x < child->x + child->width + cellOffsetX
         && y >= child->y + cellOffsetY && y < child->y + child->height + cellOffsetY) {
            if(was_hovering) continue;
            if(child->hover_func)
               child->hover_func(child, context, x - (child->x + cellOffsetX), y - (child->y + cellOffsetY));
            child->hovering = true;
            child->draw_func(child, context);
         } else {
            if(!was_hovering) continue;
            child->hovering = false;
            child->draw_func(child, context);
         }
      }
   } else {
      // unhover old
      if(grid_data->hovered) {
         grid_data->hovered->hovering = false;
         draw_grid_cell(grid, context, grid_data->hovered, grid_data->hoveredrow, grid_data->hoveredcol);
         grid_data->hovered = NULL;
      }
   }
}

void grid_unfocus(wo_t *grid, draw_context_t context) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   int cellWidth = grid->width/grid_data->cols;
   int cellHeight = grid->height/grid_data->rows;

   int offsetX = context.offsetX;
   int offsetY = context.offsetY;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         int cellOffsetX = j * cellWidth;
         int cellOffsetY = i * cellHeight;
         context.offsetX = grid->x + cellOffsetX + offsetX;
         context.offsetY = grid->y + cellOffsetY + offsetY;
         rect_t cellRect = {
            .x = context.offsetX,
            .y = context.offsetY,
            .width = cellWidth,
            .height = cellHeight
         };
         context.clipRect = rect_intersect(context.clipRect, cellRect);

         grid_cell_t *cell = &grid_data->cells[i][j];
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];
            if(!child->selected) continue;
            if(child->unfocus_func)
               child->unfocus_func(child, context);
            child->selected = false;
            child->draw_func(child, context);
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

void grid_unhover(wo_t *grid, draw_context_t context) {
   if(grid == NULL || grid->data == NULL) return;
   grid_t *grid_data = (grid_t *)grid->data;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         if(!cell->hovering) continue;

         cell->hovering = false;
         // unhover children
         for(int k = 0; k < cell->child_count; k++) {
            wo_t *child = cell->children[k];
            if(!child->hovering) continue;
            child->hovering = false;
         }
         draw_grid_cell(grid, context, cell, i, j);

         return;
      }
   }
}

void grid_mousein() {
   // do nothing
}

void destroy_grid(wo_t *grid) {
   grid_t *grid_data = grid->data;

   for(int i = 0; i < grid_data->rows; i++) {
      for(int j = 0; j < grid_data->cols; j++) {
         for(int k = 0; k < grid_data->cells[i][j].child_count; k++) {
         wo_t *child = grid_data->cells[i][j].children[k];
            if(child)
               destroy_wo(child);
         }
      }

      free(grid_data->cells[i], sizeof(grid_cell_t) * grid_data->cols);
   }
   free(grid_data->cells, sizeof(grid_cell_t*) * grid_data->rows);

   free(grid_data, sizeof(grid_t));
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
   grid_data->hovered = NULL;
   grid_data->fill_hovered_empty_cells = true;

   grid_data->cells = malloc(sizeof(grid_cell_t*) * rows);
   for(int i = 0; i < rows; i++) {
      grid_data->cells[i] = malloc(sizeof(grid_cell_t) * cols);
      
      for(int j = 0; j < cols; j++) {
         grid_cell_t *cell = &grid_data->cells[i][j];
         memset(cell, 0, sizeof(grid_cell_t));
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
   grid->mousein_func = &grid_mousein;
   grid->destroy_func = &destroy_grid;
   grid->focusable = true;

   return grid;
}

grid_t *get_grid(wo_t *grid) {
   return grid->data;
}

int get_grid_cell_width(wo_t *grid) {
   grid_t *grid_data = grid->data;
   return grid->width/grid_data->cols;
}

int get_grid_cell_height(wo_t *grid) {
   grid_t *grid_data = grid->data;
   return grid->height/grid_data->rows;
}

void grid_item_fill_cell(wo_t *grid, wo_t *item) {
   int marginX = item->x;
   int marginY = item->y;
   item->width = get_grid_cell_width(grid) - marginX*2;
   item->height = get_grid_cell_height(grid) - marginY*2;
}

void grid_item_center_cell(wo_t *grid, wo_t *item) {
   int x = (get_grid_cell_width(grid) - item->width)/2;
   int y = (get_grid_cell_height(grid) - item->height)/2;
   item->x = x;
   item->y = y;
}