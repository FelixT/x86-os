#include "wo.h"
#include <stddef.h>

wo_t *create_wo(int x, int y, int width, int height) {
   wo_t *obj = malloc(sizeof(wo_t));
   obj->x = x;
   obj->y = y;
   obj->width = width;
   obj->height = height;
   obj->visible = true;
   obj->enabled = true;
   obj->hovering = false;
   obj->clicked = false;
   obj->selected = false;
   obj->needs_redraw = false;
   obj->data = NULL;
   obj->draw_func = NULL;
   obj->click_func = NULL;
   obj->release_func = NULL;
   obj->hover_func = NULL;
   obj->drag_func = NULL;
   return obj;
}