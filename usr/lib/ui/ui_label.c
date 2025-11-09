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
}

void draw_label(wo_t *label, surface_t *surface) {
   if(label == NULL || label->data == NULL) return;
   label_t *label_data = (label_t *)label->data;

   uint16_t txt = label_data->colour_txt;
   uint16_t light = label_data->colour_border_light;
   uint16_t dark = label_data->colour_border_dark;

   if(label->clicked) {
      txt = label_data->colour_txt_clicked;
   } else if(label->hovering) {
      txt = label_data->colour_txt_hover;
   }

   int x = label->x;
   int y = label->y;
   int width = label->width;
   int height = label->height;

   if(label_data->bordered) {
      draw_line(surface, light,  x, y, true,  height);
      draw_line(surface, light,  x, y, false, width);
      draw_line(surface, dark, x, y + height - 1, false, width);
      draw_line(surface, dark, x + width - 1, y, true, height);
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
   int text_x = x + (label_data->halign ? (width - text_width) / 2 : 0);
   int text_y = y + (label_data->valign ? (height - text_height) / 2 : 0);

   for(int i = 0; i < strlen(display_label); i++) {
      if(display_label[i] == '\n') {
         text_y += get_font_info().height + get_font_info().padding;
         text_width = ui_string_width(display_label+i+1);
         text_x = x + (label_data->halign ? (width - text_width) / 2 : 0);
      } else {
         // somewhat inefficient
         char buf[2] = {display_label[i], '\0'};
         write_strat(buf, text_x, text_y, txt);
         text_x += get_font_info().width + get_font_info().padding;
         if(text_x + get_font_info().width + get_font_info().padding > x + width) {
            text_y += get_font_info().height + get_font_info().padding;
            text_x = x;
         }
      }
   }
}

void release_label(wo_t *label, surface_t *surface, int x, int y) {
   (void)x;
   (void)y;
   (void)surface;
   if(label == NULL || label->data == NULL) return;
   label_t *label_data = (label_t *)label->data;
   if(label_data->release_func)
      label_data->release_func(label);
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
   label_data->colour_border_dark = rgb16(145, 145, 145);
   label_data->bordered = true;
   label_data->valign = true;
   label_data->halign = true;

   label->data = label_data;
   label->draw_func = &draw_label;
   label->release_func = &release_label;
   label->type = WO_LABEL;

   return label;
}
