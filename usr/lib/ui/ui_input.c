#include "ui_input.h"

#include "../../../lib/string.h"
#include "../stdio.h"
#include "../draw.h"

// 'one-line' text input

static inline int ui_string_width(char *txt) {
   return strlen(txt)*(get_font_info().width+get_font_info().padding);
}

void draw_input(wo_t *input, surface_t *surface, int window) {
   if(input == NULL || input->data == NULL) return;
   input_t *input_data = (input_t *)input->data;

   uint16_t bg = input_data->colour_bg;
   uint16_t txt = input_data->colour_txt;
   uint16_t light = input_data->colour_border_light;
   uint16_t dark = input_data->colour_border_dark;

   if(input->clicked) {
      bg = input_data->colour_bg_clicked;
      txt = input_data->colour_txt_clicked;

      light = rgb16(220, 220, 220);
   } else if(input->selected) {
      light = rgb16(100, 149, 237); // cornflower blue
      dark = rgb16(100, 149, 237);
   } else if(input->hovering) {
      bg = input_data->colour_bg_hover;
      txt = input_data->colour_txt_hover;
   }

   int x = input->x;
   int y = input->y;
   int width = input->width;
   int height = input->height;

   // bg
   if(input_data->bordered)
      draw_rect(surface, bg, x+1, y+1, width-2, height-2);
   else
      draw_rect(surface, bg, x, y, width, height);

   // border
   if(input_data->bordered) {
      draw_line(surface, dark,  x, y, true,  height);
      draw_line(surface, dark,  x, y, false, width);
      draw_line(surface, light, x, y + height - 1, false, width);
      draw_line(surface, light, x + width - 1, y, true, height);
   }

   width -= 4;

   // text 
   char display_text[sizeof(input_data->text)];
   int max_chars = width / (get_font_info().width + get_font_info().padding);
   int txt_offset = 0;
   if(input_data->cursor_pos > max_chars-1) {
      txt_offset = input_data->cursor_pos - (max_chars-1);
   }

   int text_width = ui_string_width(input_data->text) - txt_offset*(get_font_info().width+get_font_info().padding);
   int text_height = get_font_info().height + get_font_info().padding;

   if(text_width > width) {
      // truncate

      if(max_chars < 2) {
         // no room for '-'
         return; // don't draw any text
      }
      
      strncpy(display_text, input_data->text+txt_offset, max_chars - 1);
      strcat(display_text, "-");
      
      text_width = ui_string_width(display_text);
   } else {
      // full string
      strncpy(display_text, input_data->text+txt_offset, sizeof(display_text) - 1);
   }

   // center text
   int text_x = x + (input_data->halign ? (width - text_width) / 2 : get_font_info().padding+1);
   int text_y = y + (input_data->valign ? (height - text_height) / 2 : get_font_info().padding+1);

   write_strat_w(display_text, text_x, text_y, txt, window);

   // draw cursor
   int cursor_x = x + (input_data->halign ? (width - text_width) / 2 : get_font_info().padding) + (input_data->cursor_pos-txt_offset) * (get_font_info().padding+get_font_info().width) + 1;
   if(input->selected && cursor_x <= x + width) {
      draw_line(surface, light, cursor_x, text_y, true, get_font_info().height);
   }
}

void keypress_input(wo_t *input, uint16_t c) {
   if(input == NULL || input->data == NULL) return;
   input_t *input_data = (input_t *)input->data;

   int len = strlen(input_data->text);

   if(c == 8) {
      // backspace
      if(len > 0 && input_data->cursor_pos > 0) {
         // remove char before cursor
         for(int i = input_data->cursor_pos - 1; i < len; i++) {
            input_data->text[i] = input_data->text[i + 1];
         }
         input_data->cursor_pos--;
      }
   } else if(c == 0x102) {
      // left arrow
      input_data->cursor_pos--;
      if(input_data->cursor_pos < 0)
         input_data->cursor_pos = 0;
   } else if(c == 0x103) {
      // right arrow
      input_data->cursor_pos++;
      if(input_data->cursor_pos > len)
         input_data->cursor_pos = len;
   } else if(c == 0x0D) {
      // enter
      if(input_data->return_func != NULL)
         input_data->return_func(input);
   } else if(c > 0) {
      // add char at cursor position
      if((unsigned)len < sizeof(input_data->text) - 1) {
         for(int i = len; i >= input_data->cursor_pos; i--) {
            input_data->text[i + 1] = input_data->text[i];
         }
         input_data->text[input_data->cursor_pos] = (char)c;
         input_data->cursor_pos++;
      }
   }

}

wo_t *create_input(int x, int y, int width, int height) {
   wo_t *input = create_wo(x, y, width, height);
   input_t *input_data = malloc(sizeof(input_t));

   input_data->colour_bg = rgb16(255, 255, 255);
   input_data->colour_bg_hover = rgb16(248, 248, 248);
   input_data->colour_bg_clicked = rgb16(255, 255, 255);
   input_data->colour_txt = rgb16(40, 40, 40);
   input_data->colour_txt_hover = rgb16(0, 0, 0);
   input_data->colour_txt_clicked = rgb16(0, 0, 0);
   input_data->colour_border_light = rgb16(235, 235, 235);
   input_data->colour_border_dark = rgb16(145, 145, 145);
   input_data->bordered = true;
   input_data->valign = false;
   input_data->halign = false;
   input_data->return_func = NULL;
   input_data->text[0] = '\0';
   input_data->cursor_pos = 0;

   input->type = WO_INPUT;
   input->data = input_data;
   input->draw_func = &draw_input;
   input->keypress_func = &keypress_input;
   return input;
}