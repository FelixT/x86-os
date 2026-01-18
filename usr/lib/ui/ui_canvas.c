#include "ui_canvas.h"
#include "../draw.h"
#include "wo.h"
#include "../stdio.h"

void draw_canvas(wo_t *canvas, wo_draw_context_t context) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   uint16_t light = canvas_data->colour_border_light;
   uint16_t dark = canvas_data->colour_border_dark;

   context.offsetX += canvas->x;
   context.offsetY += canvas->y;
   int x = context.offsetX;
   int y = context.offsetY;
   int width = canvas->width;
   int height = canvas->height;

   // border
   if(canvas_data->bordered) {
      draw_line(context.surface, dark, x, y, true, height);
      draw_line(context.surface, dark, x, y, false, width);
      draw_line(context.surface, light, x, y + height - 1, false, width);
      draw_line(context.surface, light, x + width - 1, y, true, height);

      if(canvas_data->filled)
         draw_rect(context.surface, canvas_data->colour_bg, x + 1, y + 1, width - 2, height - 2);
   } else {
      if(canvas_data->filled)
         draw_rect(context.surface, canvas_data->colour_bg, x, y, width, height);
   }

   // draw children
   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(child && child->visible && child->draw_func)
         child->draw_func(child, context);
   }
}

void canvas_add(wo_t *canvas, wo_t *child) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;

   if(canvas_data->child_count >= MAX_CANVAS_CHILDREN) return;
   canvas_data->children[canvas_data->child_count++] = child;
}

void canvas_click(wo_t *canvas, wo_draw_context_t context, int x, int y) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;
   context.offsetX += canvas->x;
   context.offsetY += canvas->y;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(!child->visible) continue;
      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height) {
         child->clicked = true;
         if(child->click_func)
            child->click_func(child, context, x - child->x, y - child->y);
         else if(child->draw_func)
            child->draw_func(child, context);
         child->clicked = false;
      }
   }
}

void canvas_release(wo_t *canvas, wo_draw_context_t context, int x, int y) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;
   context.offsetX += canvas->x;
   context.offsetY += canvas->y;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(!child->visible) continue;
      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height) {
         //if(child->type == WO_INPUT)
            child->selected = true;
         if(child->release_func)
            child->release_func(child, context, x - child->x, y - child->y);
         else if(child->draw_func)
            child->draw_func(child, context);
      }
   }
}

void canvas_mousein() {
   // do nothing
}

void canvas_hover(wo_t *canvas, wo_draw_context_t context, int x, int y) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;
   context.offsetX += canvas->x;
   context.offsetY += canvas->y;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(!child->visible) continue;

      bool was_hovering = child->hovering;

      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height) {
         if(child->hover_func)
            child->hover_func(child, context, x - child->x, y - child->y);
         child->hovering = true;
         if(was_hovering) continue;
         // mouse in
         if(child->mousein_func)
            child->mousein_func(child, context, x, y);
         else
            child->draw_func(child, context);
      } else {
         if(!was_hovering) continue;
         // mouse out
         child->hovering = false;
         if(child->unhover_func)
            child->unhover_func(child, context);
         else if(child->draw_func)
            child->draw_func(child, context);
      }
   }
}

void canvas_unfocus(wo_t *canvas, wo_draw_context_t context) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;
   context.offsetX += canvas->x;
   context.offsetY += canvas->y;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(!child->selected) continue;
      child->selected = false;
      if(child->unfocus_func)
         child->unfocus_func(child, context);
      else if(child->draw_func)
         child->draw_func(child, context);
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

void canvas_unhover(wo_t *canvas, wo_draw_context_t context) {
   if(canvas == NULL || canvas->data == NULL) return;
   canvas_t *canvas_data = (canvas_t *)canvas->data;
   context.offsetX += canvas->x;
   context.offsetY += canvas->y;

   for(int i = 0; i < canvas_data->child_count; i++) {
      wo_t *child = canvas_data->children[i];
      if(!child->hovering || !child->visible) continue;
      child->hovering = false;
      if(child->unhover_func)
         child->unhover_func(child, context);
      else if(child->draw_func)
         child->draw_func(child, context);
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
   canvas->unhover_func = &canvas_unhover;
   canvas->mousein_func = (void*)&canvas_mousein;

   return canvas;
}

void canvas_item_fill(wo_t *canvas, wo_t *item) {
   int marginX = item->x;
   int marginY = item->y;
   item->width = canvas->width - marginX*2;
   item->height = canvas->height - marginY*2;
}

void canvas_item_center(wo_t *canvas, wo_t *item) {
   int x = (canvas->width - item->width)/2;
   int y = (canvas->height - item->height)/2;
   item->x = x;
   item->y = y;
}