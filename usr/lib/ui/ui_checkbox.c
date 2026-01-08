#include "ui_checkbox.h"

#include "../draw.h"
#include "../../prog.h"
#include "../../../lib/string.h"

void release_checkbox(wo_t *checkbox, surface_t *surface, int window, int x, int y, int offsetX, int offsetY) {
   (void)x;
   (void)y;
   (void)surface;
   (void)window;
   (void)offsetX;
   (void)offsetY;
   if(checkbox == NULL || checkbox->data == NULL) return;
   checkbox_t *check_data = (checkbox_t *)checkbox->data;
   check_data->checked = !check_data->checked;
   if(check_data->release_func)
      check_data->release_func(checkbox, window);
   draw_checkbox(checkbox, surface, window, offsetX, offsetY);
}

wo_t *create_checkbox(int x, int y, bool checked) {
   wo_t *checkbox = create_wo(x, y, 20, 20);
   checkbox_t *checkbox_data = malloc(sizeof(checkbox_t));
   checkbox_data->release_func = NULL;
   checkbox_data->checked = checked;

   checkbox->data = checkbox_data;
   checkbox->draw_func = &draw_checkbox;
   checkbox->release_func = &release_checkbox;
   checkbox->type = WO_CHECKBOX;

   return checkbox;
}

void destroy_checkbox(wo_t *checkbox) {
   if(checkbox == NULL) return;
   if(checkbox->data != NULL) {
      free(checkbox->data, sizeof(checkbox_t));
      checkbox->data = NULL;
   }
   free(checkbox, sizeof(wo_t));
}

void draw_check(surface_t *surface, int x, int y, uint16_t colour, bool checked) {
   if(checked) {
      // draw check mark
      draw_pixel(surface, colour, x+6, y+10);
      draw_pixel(surface, colour, x+7, y+11);
      draw_pixel(surface, colour, x+8, y+12);
      draw_pixel(surface, colour, x+9, y+11);
      draw_pixel(surface, colour, x+10, y+10);
      draw_pixel(surface, colour, x+11, y+9);
      draw_pixel(surface, colour, x+12, y+8);
      draw_pixel(surface, colour, x+13, y+7);
   } else {
      // draw cross
      draw_pixel(surface, colour, x+6, y+6);
      draw_pixel(surface, colour, x+6, y+12);
      draw_pixel(surface, colour, x+7, y+7);
      draw_pixel(surface, colour, x+7, y+11);
      draw_pixel(surface, colour, x+8, y+8);
      draw_pixel(surface, colour, x+8, y+10);
      draw_pixel(surface, colour, x+9, y+9);
      draw_pixel(surface, colour, x+10, y+10);
      draw_pixel(surface, colour, x+10, y+8);
      draw_pixel(surface, colour, x+11, y+11);
      draw_pixel(surface, colour, x+11, y+7);
      draw_pixel(surface, colour, x+12, y+12);
      draw_pixel(surface, colour, x+12, y+6);

   }
}

void draw_checkbox(wo_t *checkbox, surface_t *surface, int window, int offsetX, int offsetY) {
   (void)window;
   if(checkbox == NULL || checkbox->data == NULL) return;
   // 20x20

   checkbox_t *check_data = (checkbox_t *)checkbox->data;

   int x = checkbox->x + offsetX;
   int y = checkbox->y + offsetY;

   int w = checkbox->width;
   int h = checkbox->height;

   uint16_t bg = 0xD6BA;
   uint16_t bg2 = 0xDF1B;
   uint16_t txt = 0x0000;
   uint16_t light = rgb16(235, 235, 235);
   uint16_t dark = rgb16(145, 145, 145);

   if(checkbox->clicked) {
      uint16_t tmp = light;
      light = dark;
      dark = tmp;
   } else if(checkbox->hovering) {
      bg = 0xDF1B;
   }

   bool gradient = true;
   bool gradientstyle = true;

   if(gradient) {
      if(checkbox->hovering || checkbox->clicked)
         draw_rect_gradient(surface, bg2, bg, x, y, w, h, gradientstyle);
      else
         draw_rect_gradient(surface, bg, bg2, x, y, w, h, gradientstyle);
   } else {
      draw_rect(surface, bg, x, y, w, h);
   }

   draw_line(surface, light,  x, y, true,  h);
   draw_line(surface, light,  x, y, false, w);
   draw_line(surface, dark, x, y + h - 1, false, w);
   draw_line(surface, dark, x + w - 1, y, true, h);

   draw_check(surface, x, y+1, checkbox->clicked ? 0xFFFF : light, check_data->checked); // shadow
   draw_check(surface, x, y, txt, check_data->checked);
}

void set_checkbox_release(wo_t *checkbox, void(*release_func)(wo_t *wo, int window)) {
   checkbox_t *check_data = checkbox->data;
   check_data->release_func = release_func;
}