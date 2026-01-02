#include "ui_canvas.h"
#include "../draw.h"
#include "wo.h"
#include "../stdio.h"

void draw_canvas(wo_t *canvas, surface_t *surface, int window, int offsetX, int offsetY) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   uint16_t light = canvas_data->colour_border_light;
   uint16_t dark = canvas_data->colour_border_dark;

   int x = canvas->x + offsetX;
   int y = canvas->y + offsetY;
   int width = canvas->width;
   int height = canvas->height;

   // border
   if(canvas_data->bordered) {
      draw_line(surface, dark,  x, y, true, height);
      draw_line(surface, dark,  x, y, false, width);
      draw_line(surface, light, x, y + height - 1, false, width);
      draw_line(surface, light, x + width - 1, y, true, height);

      if(canvas_data->filled)
         draw_rect(surface, canvas_data->colour_bg, x + 1, y + 1, width - 2, height - 2);
   } else {
      if(canvas_data->filled)
         draw_rect(surface, canvas_data->colour_bg, x, y, width, height);
   }

   // draw children
   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(child && child->draw_func)
         child->draw_func(child, surface, window, x, y);
   }
}

void canvas_add(wo_t *canvas, wo_t *child) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   if(canvas_data->child_count >= MAX_CANVAS_CHILDREN) return;
   canvas_data->children[canvas_data->child_count++] = child;
}

void canvas_click(wo_t *canvas, surface_t *surface, int window, int x, int y, int offsetX, int offsetY) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height) {
         if(child->click_func)
            child->click_func(child, surface, window, x - child->x, y - child->y, offsetX + canvas->x, offsetY + canvas->y);
         
         child->clicked = true;
         child->draw_func(child, surface, window, canvas->x + offsetX, canvas->y + offsetY);
         child->clicked = false;
      }
   }
}

void canvas_release(wo_t *canvas, surface_t *surface, int window, int x, int y, int offsetX, int offsetY) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height) {
         if(child->release_func)
            child->release_func(child, surface, window, x - child->x, y - child->y, offsetX + canvas->x, offsetY + canvas->y);
         if(child->type == WO_INPUT)
            child->selected = true;
         child->draw_func(child, surface, window, canvas->x + offsetX, canvas->y + offsetY);
      }
   }
}

void canvas_hover(wo_t *canvas, surface_t *surface, int window, int x, int y, int offsetX, int offsetY) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];

      bool was_hovering = child->hovering;

      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height) {
         if(child->hover_func)
            child->hover_func(child, surface, window, x - child->x, y - child->y, offsetX + canvas->x, offsetY + canvas->y);
         child->hovering = true;
         if(!was_hovering)
            child->draw_func(child, surface, window, canvas->x + offsetX, canvas->y + offsetY);
      } else {
         if(!was_hovering) continue;
         child->hovering = false;
         child->draw_func(child, surface, window, canvas->x + offsetX, canvas->y + offsetY);
      }
   }
}

void canvas_unfocus(wo_t *canvas, surface_t *surface, int window, int offsetX, int offsetY) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(!child->selected) continue;
      if(child->unfocus_func)
         child->unfocus_func(child, surface, window, canvas->x + offsetX, canvas->y + offsetY);
      child->selected = false;
      child->draw_func(child, surface, window, canvas->x + offsetX, canvas->y + offsetY);
   }
}

void canvas_keypress(wo_t *canvas, uint16_t c, int window) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(!child->selected) continue;
      if(child->keypress_func)
         child->keypress_func(child, c, window);
      break;
   }
}

wo_t *create_canvas(int x, int y, int width, int height) {
   wo_t *canvas = create_wo(x, y, width, height);
   if(!canvas) return NULL;

   canvas_t *canvas_data = malloc(sizeof(canvas_t));
   canvas_data->colour_border_light = rgb16(200, 200, 200);
   canvas_data->colour_border_dark = rgb16(100, 100, 100);
   canvas_data->colour_bg = rgb16(252, 252, 252);
   canvas_data->bordered = true;
   canvas_data->filled = true;
   canvas_data->child_count = 0;

   canvas->type = WO_CANVAS;
   canvas->data = canvas_data;
   canvas->draw_func = &draw_canvas;
   canvas->click_func = &canvas_click;
   canvas->release_func = &canvas_release;
   canvas->hover_func = &canvas_hover;
   canvas->unfocus_func = &canvas_unfocus;
   canvas->keypress_func = &canvas_keypress;

   return canvas;
}