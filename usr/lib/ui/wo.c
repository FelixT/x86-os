#include "wo.h"
#include <stddef.h>

wo_t *create_wo(int x, int y, int width, int height) {
   wo_t *wo = malloc(sizeof(wo_t));
   wo->x = x;
   wo->y = y;
   wo->width = width;
   wo->height = height;
   wo->visible = true;
   wo->enabled = true;
   wo->hovering = false;
   wo->clicked = false;
   wo->selected = false;
   wo->needs_redraw = false;

   wo->data = NULL;
   wo->draw_func = NULL;
   wo->click_func = NULL;
   wo->release_func = NULL;
   wo->unfocus_func = NULL;
   wo->hover_func = NULL;
   wo->unhover_func = NULL;
   wo->drag_func = NULL;
   wo->destroy_func = NULL;
   return wo;
}

void destroy_wo(wo_t *wo) {
   if(wo && wo->destroy_func)
      wo->destroy_func(wo);

   wo->enabled = false;
   wo->visible = false;
   
   wo->data = NULL;
   wo->draw_func = NULL;
   wo->click_func = NULL;
   wo->release_func = NULL;
   wo->unfocus_func = NULL;
   wo->hover_func = NULL;
   wo->unhover_func = NULL;
   wo->drag_func = NULL;
   wo->destroy_func = NULL;
}