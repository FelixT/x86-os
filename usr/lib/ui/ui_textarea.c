#include "ui_textarea.h"

#include "../../../lib/string.h"
#include "../stdio.h"
#include "../stdlib.h"
#include "../draw.h"

// 'one-line' text input

static inline int ui_string_width(char *txt) {
   return strlen(txt)*(get_font_info().width+get_font_info().padding);
}

void textarea_get_pos_from_index(wo_t *textarea, int index, int *row, int *col) {
   textarea_t *textarea_data = textarea->data;
   int max_line_len = textarea->width / (get_font_info().width + get_font_info().padding);

   int line = 0;
   int line_pos = 0;
   for(int i = 0; i < index; i++) {
      char c = textarea_data->text[i];

      // newline
      if(c == '\n') {
         line++;
         line_pos = 0;
         continue;
      }

      // wrap around
      if(line_pos == max_line_len) {
         line++;
         line_pos = 1;
         continue;
      }
      line_pos++;
   }
   *row = line;
   *col = line_pos;
}

int textarea_get_index_from_pos(wo_t *textarea, int row, int col) {
   textarea_t *textarea_data = textarea->data;
   int max_line_len = textarea->width / (get_font_info().width + get_font_info().padding);

   int line = 0;
   int line_pos = 0;
   for(int i = 0; i < strlen(textarea_data->text); i++) {
      if(line == row && line_pos == col)
         return i;
         
      char c = textarea_data->text[i];

      // newline
      if(c == '\n') {
         line++;
         line_pos = 0;
         continue;
      }

      // wrap around
      if(line_pos == max_line_len) {
         line++;
         line_pos = 1;
         continue;
      }
      line_pos++;
   }
   return -1;
}

int textarea_get_rows(wo_t *textarea) {
   textarea_t *textarea_data = textarea->data;
   int max_line_len = textarea->width / (get_font_info().width + get_font_info().padding);

   int line = 0;
   int line_pos = 0;
   for(int i = 0; i < strlen(textarea_data->text); i++) {
      char c = textarea_data->text[i];

      // newline
      if(c == '\n') {
         line++;
         line_pos = 0;
         continue;
      }

      // wrap around
      if(line_pos == max_line_len) {
         line++;
         line_pos = 1;
         continue;
      }
      line_pos++;
   }
   return line + 1;
}

void draw_textarea(wo_t *textarea, draw_context_t context) {
   if(textarea == NULL || textarea->data == NULL) return;
   textarea_t *textarea_data = textarea->data;

   uint16_t bg = textarea_data->colour_bg;
   uint16_t txt = textarea_data->colour_txt;
   uint16_t light = textarea_data->colour_border_light;
   uint16_t dark = textarea_data->colour_border_dark;

   if(textarea->clicked) {
      bg = textarea_data->colour_bg_clicked;
      txt = textarea_data->colour_txt_clicked;

      light = rgb16(180, 180, 180);
   } else if(textarea->selected) {
      light = rgb16(100, 149, 237); // cornflower blue
      dark = rgb16(100, 149, 237);
   } else if(textarea->hovering) {
      bg = textarea_data->colour_bg_hover;
      txt = textarea_data->colour_txt_hover;

      light = rgb16(210, 210, 210);
   }

   int x = textarea->x + context.offsetX;
   int y = textarea->y + context.offsetY;
   int width = textarea->width;
   int height = textarea->height;

   // bg
   if(textarea_data->bordered)
      draw_rect(&context, bg, x+1, y+1, width-2, height-2);
   else
      draw_rect(&context, bg, x, y, width, height);

   // border
   if(textarea_data->bordered) {
      draw_line(&context, dark,  x, y, true,  height);
      draw_line(&context, dark,  x, y, false, width);
      draw_line(&context, light, x, y + height - 1, false, width);
      draw_line(&context, light, x + width - 1, y, true, height);
   }

   width -= 4;

   // text 
   int max_line_len = width / (get_font_info().width + get_font_info().padding);
   char *display_text = malloc(max_line_len+1);
   display_text[0] = '\0';
   display_text[max_line_len+1] = '\0';

   int text_x = x + get_font_info().padding + textarea_data->padding;
   int start_y = y + get_font_info().padding + textarea_data->padding;
   int text_y = start_y;
   int cursor_x = text_x;
   int cursor_y = text_y - 1;

   int line_pos = 0;
   for(int i = 0; i < strlen(textarea_data->text); i++) {
      if(i < textarea_data->cursor_pos) {
         cursor_x += get_font_info().width + get_font_info().padding;
         cursor_y = text_y - 1;
      }

      char c = textarea_data->text[i];

      // newline
      if(c == '\n') {
         display_text[line_pos] = '\0';
         write_strat_w(display_text, text_x, text_y, txt, context.window);
         text_y += get_font_info().height + get_font_info().padding;
         display_text[0] = '\0';
         line_pos = 0;
         if(i < textarea_data->cursor_pos) {
            cursor_x = text_x;
            cursor_y = text_y - 1;
         }
         continue;
      }

      // wrap around
      if(line_pos == max_line_len) {
         write_strat_w(display_text, text_x, text_y, txt, context.window);
         text_y += get_font_info().height + get_font_info().padding;
         display_text[0] = c;
         display_text[1] = '\0';
         line_pos = 1;
         if(i < textarea_data->cursor_pos) {
            cursor_x = text_x + get_font_info().width + get_font_info().padding;
            cursor_y = text_y - 1;
         }
         continue;
      }

      display_text[line_pos] = c;
      display_text[line_pos+1] = '\0';
      line_pos++;
   }

   write_strat_w(display_text, text_x, text_y, txt, context.window); // write remaining text

   // draw cursor
   if(textarea->selected || textarea->clicked || textarea->hovering)
      draw_line(&context, !textarea->selected && textarea->hovering ? rgb16(200, 200, 200) : dark, cursor_x, cursor_y, true, get_font_info().height+2);

   free(display_text);
}

void keypress_textarea(wo_t *textarea, uint16_t c, int window) {
   (void)window;
   if(textarea == NULL || textarea->data == NULL) return;
   textarea_t *textarea_data = textarea->data;

   int len = strlen(textarea_data->text);

   if(c == 8) {
      // backspace
      if(len > 0 && textarea_data->cursor_pos > 0) {
         // remove char before cursor
         for(int i = textarea_data->cursor_pos - 1; i < len; i++) {
            textarea_data->text[i] = textarea_data->text[i + 1];
         }
         textarea_data->cursor_pos--;
      }
   } else if(c == 0x100) {
      // up arrow
      int row, col;
      textarea_get_pos_from_index(textarea, textarea_data->cursor_pos, &row, &col);
      if(row == 0) {
         textarea_data->cursor_pos = 0;
         return;
      }
      row--;
      int index = textarea_get_index_from_pos(textarea, row, col);
      if(index == -1)
         index = textarea_get_index_from_pos(textarea, row+1, 0) - 1;
      textarea_data->cursor_pos = index;
   } else if(c == 0x101) {
      // down arrow
      int row, col;
      textarea_get_pos_from_index(textarea, textarea_data->cursor_pos, &row, &col);
      if(row == textarea_get_rows(textarea)) {
         textarea_data->cursor_pos = strlen(textarea_data->text);
         return;
      }
      row++;
      int index = textarea_get_index_from_pos(textarea, row, col);
      if(index == -1) {
         index = textarea_get_index_from_pos(textarea, row+1, 0);
         if(index == -1)
            index = strlen(textarea_data->text);
         else
            index--;
      }
      textarea_data->cursor_pos = index;
   } else if(c == 0x102) {
      // left arrow
      textarea_data->cursor_pos--;
      if(textarea_data->cursor_pos < 0)
         textarea_data->cursor_pos = 0;
   } else if(c == 0x103) {
      // right arrow
      textarea_data->cursor_pos++;
      if(textarea_data->cursor_pos > len)
         textarea_data->cursor_pos = len;
   } else if(c > 0) {
      // add char at cursor position
      if(c == 0x0D)
         c = 0x0A; // convert \r to \n
   
      if(len < textarea_data->textbuf_size - 1) {
         for(int i = len; i >= textarea_data->cursor_pos; i--) {
            textarea_data->text[i + 1] = textarea_data->text[i];
         }
         textarea_data->text[textarea_data->cursor_pos++] = (char)c;
      }
   } else {
      // resize
      
   }

}

typedef struct cursor_event_t {
   wo_t *wo; // textarea
   bool blinking;
   draw_context_t draw_context;
} cursor_event_t;

void blink_cursor(cursor_event_t *event) {
   if(!event) return;
   wo_t *wo = event->wo;
   if(!wo || !wo->enabled || !wo->data) return;

   if(wo->selected) {
      uint16_t colour = get_textarea(wo)->colour_border_dark;
      colour = event->blinking ? rgb16_lighten(colour, 100) : colour;
      int row, col;
      textarea_get_pos_from_index(wo, get_textarea(wo)->cursor_pos, &row, &col);
      int cursor_x = get_textarea(wo)->padding + col*(get_font_info().width+get_font_info().padding) + wo->x + event->draw_context.offsetX + get_font_info().padding;
      int cursor_y = get_textarea(wo)->padding + row*(get_font_info().height+get_font_info().padding) + wo->y + event->draw_context.offsetY + get_font_info().padding - 1;
      draw_line(&event->draw_context, colour, cursor_x, cursor_y, true, get_font_info().height+2);

      event->blinking = !event->blinking;
      queue_event((uint32_t)&blink_cursor, 200, event);
   }
   end_subroutine();
}

void click_textarea(wo_t *wo, draw_context_t draw_context, int x, int y) {
   textarea_t *textarea_data = wo->data;
   if(!textarea_data->focused) {
      draw_textarea(wo, draw_context);
      return;
   }
   x -= textarea_data->padding;
   y -= textarea_data->padding;
   int row = y / (get_font_info().height + get_font_info().padding);
   int col = x / (get_font_info().width + get_font_info().padding);
   int index = textarea_get_index_from_pos(wo, row, col);
   if(index == -1) {
      index = textarea_get_index_from_pos(wo, row+1, 0);
      if(index == -1)
         index = strlen(textarea_data->text);
      else
         index--;
   }
   textarea_data->cursor_pos = index;
   draw_textarea(wo, draw_context);
}

void release_textarea(wo_t *wo, draw_context_t draw_context, int x, int y) {
   (void)x;
   (void)y;
   get_textarea(wo)->focused = true;
   draw_textarea(wo, draw_context);
   // set up blinking cursor
   cursor_event_t *event = malloc(sizeof(cursor_event_t));
   event->wo = wo;
   event->blinking = true;
   event->draw_context = draw_context;
   queue_event((uint32_t)&blink_cursor, 200, event);
}

void unfocus_textarea(wo_t *wo, draw_context_t draw_context) {
   get_textarea(wo)->focused = false;
   draw_textarea(wo, draw_context);
}

wo_t *create_textarea(int x, int y, int width, int height) {
   wo_t *textarea = create_wo(x, y, width, height);
   textarea_t *textarea_data = malloc(sizeof(textarea_t));
   textarea_data->textbuf_size = 0x1000; // start with 4k bytes
   textarea_data->text = malloc(textarea_data->textbuf_size);
   textarea_data->text[0] = '\0';

   textarea_data->colour_bg = rgb16(255, 255, 255);
   textarea_data->colour_bg_hover = rgb16(248, 248, 248);
   textarea_data->colour_bg_clicked = rgb16(255, 255, 255);
   textarea_data->colour_txt = rgb16(40, 40, 40);
   textarea_data->colour_txt_hover = rgb16(0, 0, 0);
   textarea_data->colour_txt_clicked = rgb16(0, 0, 0);
   textarea_data->colour_border_light = rgb16(235, 235, 235);
   textarea_data->colour_border_dark = rgb16(145, 145, 145);
   textarea_data->bordered = true;
   textarea_data->cursor_pos = 0;
   textarea_data->padding = 2;

   textarea->type = WO_TEXTAREA;
   textarea->data = textarea_data;
   textarea->draw_func = &draw_textarea;
   textarea->keypress_func = &keypress_textarea;
   textarea->click_func = &click_textarea;
   textarea->release_func = &release_textarea;
   textarea->unfocus_func = &unfocus_textarea;
   textarea->focusable = true;
   return textarea;
}

void set_textarea_text(wo_t *textarea, char *text) {
   if(textarea == NULL || textarea->data == NULL) return;
   textarea_t *textarea_data = textarea->data;

   // todo: resize
   strncpy(textarea_data->text, text, textarea_data->textbuf_size);
   textarea_data->text[strlen(textarea_data->text)] = '\0';
   textarea_data->cursor_pos = strlen(textarea_data->text);
}

textarea_t *get_textarea(wo_t *textarea) {
   return textarea->data;
}