#ifndef UI_LABEL_H
#define UI_LABEL_H

#include <stdint.h>
#include "wo.h"

typedef struct label_t {
   char label[128];
   uint16_t colour_txt;
   uint16_t colour_txt_hover;
   uint16_t colour_txt_clicked;
   uint16_t colour_border_light;
   uint16_t colour_border_dark;
   uint16_t colour_bg;
   bool bordered;
   bool filled;
   bool valign;
   bool halign;
   int padding_left; // if halign = false
   void (*release_func)(wo_t *wo);
} label_t;

wo_t *create_label(int x, int y, int width, int height, char *text);

#endif