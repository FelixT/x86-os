#include "ui_menu.h"
#include "../../../lib/string.h"
#include "../draw.h"
#include "../../prog.h"
#include "wo.h"
#include "../stdio.h"

void draw_menu(wo_t *menu, surface_t *surface, int window) {
   if(menu == NULL || menu->data == NULL) return;
   ui_menu_t *menu_data = (ui_menu_t *)menu->data;

   uint16_t border_light = rgb16(235, 235, 235);
   uint16_t border_dark = rgb16(145, 145, 145);
   uint16_t bg = rgb16(255, 255, 255);
   uint16_t bg_selected = rgb16(230, 230, 230);
   uint16_t bg_hover = rgb16(243, 243, 243);
   uint16_t bg_item = rgb16(250, 250, 250);

   if(menu->selected)
      border_light = rgb16(220, 220, 220);

   if(menu->hovering) {
      bg = rgb16(248, 248, 248);
   }

   // draw border
   draw_line(surface, border_dark,  menu->x, menu->y, true,  menu->height);
   draw_line(surface, border_dark,  menu->x, menu->y, false, menu->width);
   draw_line(surface, border_light, menu->x, menu->y + menu->height - 1, false, menu->width);
   draw_line(surface, border_light, menu->x + menu->width - 1, menu->y, true, menu->height);

   int item_height = get_font_info().height + 8;

   // draw background
   int offset_y = menu_data->item_count * item_height;
   draw_rect(surface, bg, menu->x + 1, menu->y + 1 + offset_y, menu->width - 2, menu->height - 2 - offset_y);

   // draw items
   for(int i = 0; i < menu_data->item_count; i++) {
      ui_menu_item_t *item = &menu_data->items[i];
      int item_y = menu->y + (i * item_height) + 1;
      // highlight if selected
      if(i == menu_data->selected_index) {
         write_strat_w(item->text, menu->x + 5, item_y + 5, 0xFFFF, window);
         draw_rect(surface, bg_selected, menu->x + 1, item_y, menu->width - 2, item_height);
      } else if(i == menu_data->hover_index && menu->hovering) {
         draw_rect(surface, bg_hover, menu->x + 1, item_y, menu->width - 2, item_height);
      } else {
         draw_rect(surface, bg_item, menu->x + 1, item_y, menu->width - 2, item_height);
      }
      // draw text
      write_strat_w(item->text, menu->x + 5, item_y + 4, 0, window);
      // border
      draw_line(surface, border_light, menu->x + 1, item_y + item_height - 1, false, menu->width - 2);
   }
}

void destroy_menu(wo_t *menu) {
   if(menu == NULL) return;
   if(menu->data != NULL) {
      ui_menu_t *menu_data = (ui_menu_t *)menu->data;
      if(menu_data->items != NULL) {
         free(menu_data->items, sizeof(ui_menu_item_t) * menu_data->item_count);
      }
      free(menu_data, sizeof(ui_menu_t));
   }
   free(menu, sizeof(wo_t));
}

void add_menu_item(wo_t *menu, const char *text, void (*func)(wo_t *item, int index, int window)) {
   if(menu == NULL || menu->data == NULL) return;
   ui_menu_t *menu_data = (ui_menu_t *)menu->data;

   // allocate new item array
   ui_menu_item_t *new_items = malloc(sizeof(ui_menu_item_t) * (menu_data->item_count + 1));
   // copy old items
   for(int i = 0; i < menu_data->item_count; i++) {
      new_items[i] = menu_data->items[i];
   }
   // add new item
   strncpy(new_items[menu_data->item_count].text, text, 63);
   new_items[menu_data->item_count].func = func;

   // free old items
   if(menu_data->items != NULL) {
      free(menu_data->items, sizeof(ui_menu_item_t) * menu_data->item_count);
   }

   // update menu data
   menu_data->items = new_items;
   menu_data->item_count++;
}

void menu_click(wo_t *menu, surface_t *surface, int window, int x, int y) {
   (void)x;
   if(menu == NULL || menu->data == NULL) return;
   ui_menu_t *menu_data = (ui_menu_t *)menu->data;

   int item_height = get_font_info().height + 8;
   int index = y / item_height;

   if(index < 0 || index >= menu_data->item_count) {
      menu_data->selected_index = -1;
      draw_menu(menu, surface, window);
      return;
   }

   menu_data->selected_index = index;

   ui_menu_item_t *item = &menu_data->items[index];
   if(item->func != NULL) {
      item->func(menu, index, window);
   }

   draw_menu(menu, surface, window);
}

void menu_keypress(wo_t *menu, uint16_t c) {
   if(menu == NULL || menu->data == NULL) return;
   ui_menu_t *menu_data = (ui_menu_t *)menu->data;

   if(c == 0x100) {
      // up
      menu_data->selected_index--;
      if(menu_data->selected_index < 0)
         menu_data->selected_index = menu_data->item_count - 1;
   } else if(c == 0x101) {
      // down
      menu_data->selected_index++;
      if(menu_data->selected_index >= menu_data->item_count)
         menu_data->selected_index = 0;
   }
}

void menu_hover(wo_t *menu, surface_t *surface, int window, int x, int y) {
   (void)x;
   if(menu == NULL || menu->data == NULL) return;
   ui_menu_t *menu_data = (ui_menu_t *)menu->data;

   int item_height = get_font_info().height + 8;
   int index = y / item_height;

   int old_index = menu_data->hover_index;
   menu_data->hover_index = index;
   if(index < 0 || index >= menu_data->item_count) {
      menu_data->hover_index = -1;
   }

   if(old_index != menu_data->hover_index)
      draw_menu(menu, surface, window);
}

wo_t *create_menu(int x, int y, int width, int height) {
   wo_t *menu = create_wo(x, y, width, height);
   ui_menu_t *menu_data = malloc(sizeof(ui_menu_t));
   menu_data->items = NULL;
   menu_data->item_count = 0;
   menu_data->selected_index = -1;
   menu_data->hover_index = -1;

   menu->data = menu_data;
   menu->draw_func = &draw_menu;
   menu->click_func = &menu_click;
   menu->keypress_func = &menu_keypress;
   menu->hover_func = &menu_hover;
   menu->type = WO_MENU;

   return menu;
}
