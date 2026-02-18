#include "ui_label.h"

#include "../../../lib/string.h"
#include "../draw.h"

int ui_string_width(char *txt) {
   int width = 0;

   for(int i = 0; i < strlen(txt); i++) {
      if(txt[i] == '\n') {
         break; // only return width of first line
      }
      width += get_font_info().width + get_font_info().padding;
   }

   return width;
}

int ui_string_height(char *txt, int width) {
   int x = 0;
   int height = get_font_info().height;
   for(int i = 0; i < strlen(txt); i++) {
      if(txt[i] == '\n') {
         height += get_font_info().height + get_font_info().padding;
         x = 0;
      } else {
         x += get_font_info().width + get_font_info().padding;
         if(x + get_font_info().width + get_font_info().padding > width) {
            height += get_font_info().height + get_font_info().padding;
            x = 0;
         }
      }
   }
   return height;
}

void draw_label(wo_t *label, draw_context_t context) {
   if(label == NULL || label->data == NULL) return;
   label_t *label_data = (label_t *)label->data;

   uint16_t txt = label_data->colour_txt;
   uint16_t light = label_data->colour_border_light;
   uint16_t dark = label_data->colour_border_dark;
   uint16_t bg = label_data->colour_bg;

   if(label->clicked) {
      txt = label_data->colour_txt_clicked;
   } else if(label->hovering) {
      txt = label_data->colour_txt_hover;
   }

   int x = label->x + context.offsetX;
   int y = label->y + context.offsetY;
   int width = label->width;
   int height = label->height;

   if(label_data->bordered) {
      draw_line(&context, light, x, y, true,  height);
      draw_line(&context, light, x, y, false, width);
      draw_line(&context, dark, x, y + height - 1, false, width);
      draw_line(&context, dark, x + width - 1, y, true, height);
   }

   if(label_data->filled) {
      draw_rect(&context, bg, x + 1, y + 1, width - 2, height - 2);
   }

   // text 
   int text_width = ui_string_width(label_data->label);
   int text_height = ui_string_height(label_data->label, width);
   char display_label[sizeof(label_data->label)];

   if(text_width > width) {
      // truncate
      int max_chars = width / (get_font_info().width + get_font_info().padding);
      
      if(max_chars < 4) {
         // no room for '...'
         return; // don't draw any text
      }
      
      strncpy(display_label, label_data->label, max_chars - 3);
      display_label[max_chars - 3] = '\0';
      strcat(display_label, "...");
      
      text_width = ui_string_width(display_label);
   } else {
      // full string
      strncpy(display_label, label_data->label, sizeof(display_label) - 1);
      display_label[sizeof(display_label) - 1] = '\0';
   }

   // center text
   int text_x = x + (label_data->halign ? (width - text_width) / 2 : label_data->padding_left);
   int text_y = y + (label_data->valign ? (height - text_height) / 2 : 0);

   for(int i = 0; i < strlen(display_label); i++) {
      if(display_label[i] == '\n') {
         text_y += get_font_info().height + get_font_info().padding;
         text_width = ui_string_width(display_label+i+1);
         text_x = x + (label_data->halign ? (width - text_width) / 2 : label_data->padding_left);
      } else {
         // somewhat inefficient
         char buf[2] = {display_label[i], '\0'};
         if(label_data->bordered)
            write_strat_w(buf, text_x, text_y+1, light, context.window); // shadow
         write_strat_w(buf, text_x, text_y, txt, context.window);
         text_x += get_font_info().width + get_font_info().padding;
         if(text_x + get_font_info().width + get_font_info().padding > x + width) {
            text_y += get_font_info().height + get_font_info().padding;
            text_x = x;
         }
      }
   }
}

void release_label(wo_t *label, draw_context_t context, int x, int y) {
   (void)x;
   (void)y;
   if(label == NULL || label->data == NULL) return;
   label_t *label_data = (label_t *)label->data;
   if(label_data->release_func)
      label_data->release_func(label, context.window);
}

void destroy_label(wo_t *label) {
   label_t *label_data = label->data;
   free(label_data);
}

wo_t *create_label(int x, int y, int width, int height, char *text) {
   wo_t *label = create_wo(x, y, width, height);
   label_t *label_data = malloc(sizeof(label_t));
   strncpy(label_data->label, text, sizeof(label_data->label) - 1);
   label_data->release_func = NULL;
   label_data->colour_txt = 0x0000;
   label_data->colour_txt_hover = 0x0000;
   label_data->colour_txt_clicked = 0x0000;
   label_data->colour_border_light = rgb16(235, 235, 235);
   label_data->colour_border_dark = rgb16(200, 200, 200);
   label_data->colour_bg = 0xFFFF;
   label_data->bordered = true;
   label_data->filled = false;
   label_data->valign = true;
   label_data->halign = true;
   label_data->padding_left = 0;

   label->data = label_data;
   label->draw_func = &draw_label;
   label->release_func = &release_label;
   label->destroy_func = &destroy_label;
   label->type = WO_LABEL;

   return label;
}

label_t *get_label(wo_t *wo) {
   return wo->data;
}