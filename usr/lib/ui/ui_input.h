#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <stdint.h>
#include "wo.h"

typedef struct input_t {
   char text[128];
   uint16_t colour_txt;
   uint16_t colour_bg;
   uint16_t colour_bg_hover;
   uint16_t colour_bg_clicked;
   uint16_t colour_txt_hover;
   uint16_t colour_txt_clicked;
   uint16_t colour_border_light;
   uint16_t colour_border_dark;
   bool bordered;
   bool valign;
   bool halign;
   int padding_left; // if halign = false
   void (*return_func)(wo_t *wo, int window);
   bool placeholder; // text is placeholder

   int cursor_pos;
} input_t;

wo_t *create_input(int x, int y, int width, int height);
void set_input_text(wo_t *input, char *text);
void keypress_input(wo_t *input, uint16_t c, int window);
void set_input_return(wo_t *input, void(*return_func)(wo_t *wo, int window));
input_t *get_input(wo_t *input);

#endif