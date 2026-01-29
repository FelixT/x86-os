#ifndef UI_TEXTAREA_H
#define UI_TEXTAREA_H

#include <stdint.h>
#include "wo.h"

typedef struct textarea_t {
   char *text;
   int textbuf_size;
   uint16_t colour_txt;
   uint16_t colour_bg;
   uint16_t colour_bg_hover;
   uint16_t colour_bg_clicked;
   uint16_t colour_txt_hover;
   uint16_t colour_txt_clicked;
   uint16_t colour_border_light;
   uint16_t colour_border_dark;
   bool bordered;
   bool focused;
   int padding;

   int cursor_pos;
} textarea_t;

wo_t *create_textarea(int x, int y, int width, int height);
void set_textarea_text(wo_t *textarea, char *text);
void keypress_textarea(wo_t *textarea, uint16_t c, int window);
textarea_t *get_textarea(wo_t *textarea);
int textarea_get_rows(wo_t *textarea);
void textarea_get_pos_from_index(wo_t *textarea, int index, int *row, int *col);

#endif