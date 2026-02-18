#include "ui_button.h"

#include "../draw.h"
#include "../../prog.h"
#include "../../../lib/string.h"
#include "wo.h"

static inline int string_width(char *txt) {
   return strlen(txt)*(get_font_info().width+get_font_info().padding);
}

void click_button(wo_t *button, draw_context_t context, int x, int y) {
   (void)x;
   (void)y;
   if(button == NULL || button->data == NULL) return;
   button_t *btn_data = (button_t *)button->data;
   draw_button(button, context);
   if(btn_data->click_func)
      btn_data->click_func(button, context.window);
}

void release_button(wo_t *button, draw_context_t context, int x, int y) {
   (void)x;
   (void)y;
   if(button == NULL || button->data == NULL) return;
   button_t *btn_data = (button_t *)button->data;
   draw_button(button, context);
   if(btn_data->release_func)
      btn_data->release_func(button, context.window);
}

wo_t *create_button(int x, int y, int width, int height, char *text) {
   wo_t *button = create_wo(x, y, width, height);
   button_t *btn_data = malloc(sizeof(button_t));
   strncpy(btn_data->label, text, 63);
   btn_data->click_func = NULL;
   btn_data->release_func = NULL;
   btn_data->colour_bg = 0xD6BA;
   btn_data->colour_bg2 = 0xDF1B;
   btn_data->colour_txt = 0x0000;
   btn_data->colour_bg_hover = 0xDF1B;
   btn_data->colour_txt_hover = 0x0000;
   btn_data->colour_border_light = rgb16(235, 235, 235);
   btn_data->colour_border_dark = rgb16(145, 145, 145);

   button->data = btn_data;
   button->draw_func = &draw_button;
   button->click_func = &click_button;
   button->release_func = &release_button;
   button->type = WO_BUTTON;

   return button;
}

void destroy_button(wo_t *button) {
   if(button == NULL) return;
   if(button->data != NULL) {
      free(button->data);
      button->data = NULL;
   }
   free(button);
}

void draw_button(wo_t *button, draw_context_t context) {
   if(button == NULL || button->data == NULL) return;
   button_t *btn_data = (button_t *)button->data;

   uint16_t bg = btn_data->colour_bg;
   uint16_t bg2 = btn_data->colour_bg2;
   uint16_t txt = btn_data->colour_txt;
   uint16_t light = btn_data->colour_border_light;
   uint16_t dark = btn_data->colour_border_dark;

   if(button->clicked && button->hovering) {
      uint16_t tmp = light;
      light = dark;
      dark = tmp;
   } else if(button->hovering) {
      bg = btn_data->colour_bg_hover;
      txt = btn_data->colour_txt_hover;
   }

   int x = button->x + context.offsetX;
   int y = button->y + context.offsetY;
   int width = button->width;
   int height = button->height;
   bool gradient = true;
   bool gradientstyle = true;

   if(gradient) {
      if(button->hovering || button->clicked)
         draw_rect_gradient(&context, bg2, bg, x, y, width, height, gradientstyle);
      else
         draw_rect_gradient(&context, bg, bg2, x, y, width, height, gradientstyle);
   } else {
      draw_rect(&context, bg, x, y, width, height);
   }

   draw_line(&context, light,  x, y, true,  height);
   draw_line(&context, light,  x, y, false, width);
   draw_line(&context, dark, x, y + height - 1, false, width);
   draw_line(&context, dark, x + width - 1, y, true, height);

   // text 
   int text_width = string_width(btn_data->label);
   char display_label[sizeof(btn_data->label)];

   if(text_width > width) {
      // truncate
      int max_chars = width / (get_font_info().width + get_font_info().padding);
      
      if(max_chars < 3) {
         // no room for '...'
         return; // don't draw any text
      }
      
      strncpy(display_label, btn_data->label, max_chars - 3);
      display_label[max_chars - 3] = '\0';
      strcat(display_label, "...");
      
      text_width = string_width(display_label);
   } else {
      // full string
      strncpy(display_label, btn_data->label, sizeof(display_label) - 1);
      display_label[sizeof(display_label) - 1] = '\0';
   }

   // center text
   int text_x = x + (width - text_width) / 2;
   int text_y = y + (height - get_font_info().height) / 2;
   write_strat_w(display_label, text_x, text_y + 1, button->clicked ? 0xFFFF : light, context.window); // shadow
   write_strat_w(display_label, text_x, text_y, txt, context.window);
}

void set_button_release(wo_t *button, void(*release_func)(wo_t *wo, int window)) {
   button_t *button_data = button->data;
   button_data->release_func = release_func;
}

button_t *get_button(wo_t *button) {
   return button->data;
}