#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include <stdint.h>
#include "wo.h"

typedef struct button_t {
   char label[64];
   uint16_t colour_txt;
   uint16_t colour_bg;
   uint16_t colour_bg2;
   uint16_t colour_txt_hover;
   uint16_t colour_bg_hover;
   uint16_t colour_border_light;
   uint16_t colour_border_dark;
   void (*click_func)(wo_t *wo);
   void (*release_func)(wo_t *wo);
} button_t;

wo_t *create_button(int x, int y, int width, int height, char *text);
void draw_button(wo_t *button, surface_t *surface);

#endif