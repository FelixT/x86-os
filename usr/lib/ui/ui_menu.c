#include "ui_menu.h"
#include "../../../lib/string.h"
#include "../draw.h"
#include "../../prog.h"
#include "wo.h"
#include "../stdio.h"

void draw_menu_item(wo_t *menu, surface_t *surface, int window, int index, int offsetX, int offsetY) {
   if(index < 0) return;

   uint16_t bg_selected = rgb16(222, 222, 222);
   uint16_t bg_hover = rgb16(243, 243, 243);
   uint16_t bg_item = rgb16(250, 250, 250);
   uint16_t border_light = rgb16(235, 235, 235);

   menu_t *menu_data = (menu_t *)menu->data;

   int bgwidth = menu->width - 2;
   if(menu_data->scrolling)
      bgwidth -= 14;

   int item_height = get_font_info().height + 7;

   int x = menu->x + offsetX;
   int y = menu->y + offsetY + ((index - menu_data->offset) * item_height) + 1;

   menu_item_t *item = &menu_data->items[index];
   // highlight if selected
   if(index == menu_data->selected_index) {
      draw_rect(surface, bg_selected, x + 1, y, bgwidth, item_height);
      write_strat_w(item->text, x + 5, y + 5, border_light, window);
   } else if(index == menu_data->hover_index && menu->hovering) {
      draw_rect(surface, bg_hover, x + 1, y, bgwidth, item_height);
   } else {
      draw_rect(surface, bg_item, x + 1, y, bgwidth, item_height);
   }
   // draw text
   uint16_t txtcolour = item->enabled ? 0 : rgb16(200, 200, 200);
   write_strat_w(item->text, x + 5, y + 4, txtcolour, window);
   // border
   draw_line(surface, border_light, x + 1, y + item_height - 1, false, menu->width - 2);

}

void draw_menu(wo_t *menu, surface_t *surface, int window, int offsetX, int offsetY) {
   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;

   uint16_t border_light = rgb16(235, 235, 235);
   uint16_t border_dark = rgb16(165, 165, 165);
   uint16_t bg = rgb16(255, 255, 255);
   uint16_t bg_selected = rgb16(222, 222, 222);
   uint16_t bg_hover = rgb16(243, 243, 243);

   if(menu->selected)
      border_light = rgb16(220, 220, 220);

   if(menu->hovering)
      bg = rgb16(248, 248, 248);

   int x = menu->x + offsetX;
   int y = menu->y + offsetY;

   // draw border
   draw_line(surface, border_dark,  x, y, true,  menu->height);
   draw_line(surface, border_dark,  x, y, false, menu->width);
   draw_line(surface, border_light, x, y + menu->height - 1, false, menu->width);
   draw_line(surface, border_light, x + menu->width - 1, y, true, menu->height);

   int item_height = get_font_info().height + 7;
   int max_items = menu->height/item_height;
   int shown_items = menu_data->item_count;
   if(shown_items > max_items)
      shown_items = max_items;
   int shown_height = shown_items*item_height;

   int bgwidth = menu->width - 2;
   if(menu_data->scrolling)
      bgwidth -= 14;

   // draw background
   draw_rect(surface, bg, x + 1, y + 1 + shown_height, bgwidth, menu->height - 2 - shown_height);

   // draw items
   for(int i = menu_data->offset; i < shown_items + menu_data->offset; i++) {
      draw_menu_item(menu, surface, window, i, offsetX, offsetY);
   }

   if(menu_data->scrolling) {
      // draw 14px wide scrollbar
      int width = 14;
      int scrollbarX = x + menu->width - width - 1;
      int scrollbarY = y + 1;

      draw_rect(surface, bg_hover, scrollbarX, scrollbarY + width, width, menu->height - width*2 - 2); // bg
      draw_rect(surface, menu_data->offset > 0 ? bg_selected : bg_hover, scrollbarX, scrollbarY, width, width); // up
      char buf[2];
      buf[0] = 0x80; // uparrow
      buf[1] = 0;
      write_strat_w(buf, scrollbarX + 4, scrollbarY + 4, 0, window);
      draw_rect(surface, menu_data->offset < menu_data->item_count - max_items ? bg_selected : bg_hover, scrollbarX, scrollbarY + menu->height - width - 4, width, width); // down
      buf[0] = 0x81; // downarrow
      write_strat_w(buf, scrollbarX + 4, scrollbarY + menu->height - width - 1, 0, window);
   }
}

void destroy_menu(wo_t *menu) {
   if(menu == NULL) return;
   if(menu->data != NULL) {
      menu_t *menu_data = (menu_t *)menu->data;
      if(menu_data->items != NULL) {
         free(menu_data->items, sizeof(menu_item_t) * menu_data->item_count);
      }
      free(menu_data, sizeof(menu_t));
   }
   free(menu, sizeof(wo_t));
}

menu_item_t *add_menu_item(wo_t *menu, const char *text, void (*func)(wo_t *item, int index, int window)) {
   if(menu == NULL || menu->data == NULL) return NULL;
   menu_t *menu_data = (menu_t *)menu->data;

   // allocate new item array
   menu_item_t *new_items = malloc(sizeof(menu_item_t) * (menu_data->item_count + 1));
   // copy old items
   for(int i = 0; i < menu_data->item_count; i++) {
      new_items[i] = menu_data->items[i];
   }
   // add new item
   menu_item_t *item = &new_items[menu_data->item_count];
   strncpy(item->text, text, 63);
   item->func = func;
   item->enabled = true;

   // free old items
   if(menu_data->items != NULL) {
      free(menu_data->items, sizeof(menu_item_t) * menu_data->item_count);
   }

   // update menu data
   menu_data->items = new_items;
   menu_data->item_count++;

   int item_height = get_font_info().height + 7;
   if(item_height*menu_data->item_count > menu->height)
      menu_data->scrolling = true;

   return item;
}

void menu_click(wo_t *menu, surface_t *surface, int window, int x, int y, int offsetX, int offsetY) {
   (void)x;
   if(x == 0 && y == 0) return;

   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;
   if(menu_data->scrolling && x > menu->width - 14 - 1) {
      if(y < 15) {
         // up click
         menu_data->offset--;
         if(menu_data->offset < 0)
            menu_data->offset = 0;
         draw_menu(menu, surface, window, offsetX, offsetY);
      }
      if(y > menu->height - 14 - 1) {
         // down click
         menu_data->offset++;
         int item_height = get_font_info().height + 7;
         int max_items = menu->height/item_height;
         if(menu_data->offset > menu_data->item_count - max_items)
            menu_data->offset = menu_data->item_count - max_items;
            
         draw_menu(menu, surface, window, offsetX, offsetY);
      }
      return;
   }

   int item_height = get_font_info().height + 7;
   int index = y / item_height + menu_data->offset;

   if(index < 0 || index >= menu_data->item_count) {
      menu_data->selected_index = -1;
      draw_menu(menu, surface, window, offsetX, offsetY);
      return;
   }

   menu_data->selected_index = index;

   menu_item_t *item = &menu_data->items[index];
   if(item->enabled && item->func != NULL) {
      item->func(menu, index, window);
   }

   draw_menu(menu, surface, window, offsetX, offsetY);
}

void menu_keypress(wo_t *menu, uint16_t c, int window) {
   (void)window;
   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;

   if(c == 0x100) {
      // up
      menu_data->selected_index--;
      if(menu_data->selected_index < 0)
         menu_data->selected_index = menu_data->item_count - 1;
      menu_item_t *item = &menu_data->items[menu_data->selected_index];
      if(item->enabled && item->func != NULL)
         item->func(menu, menu_data->selected_index, window);
   } else if(c == 0x101) {
      // down
      menu_data->selected_index++;
      if(menu_data->selected_index >= menu_data->item_count)
         menu_data->selected_index = 0;
      menu_item_t *item = &menu_data->items[menu_data->selected_index];
      if(item->enabled && item->func != NULL)
         item->func(menu, menu_data->selected_index, window);
   }
}

void menu_hover(wo_t *menu, surface_t *surface, int window, int x, int y, int offsetX, int offsetY) {
   (void)x;
   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;
   int old_index = menu_data->hover_index;
   if(menu_data->scrolling && x > menu->width - 14 - 1) {
      if(menu_data->hover_index == -1) return;
      menu_data->hover_index = -1;
      draw_menu_item(menu, surface, window, old_index, offsetX, offsetY);
      return;
   }

   int item_height = get_font_info().height + 7;
   int max_items = menu->height/item_height;
   int shown_items = menu_data->item_count;
   if(shown_items > max_items)
      shown_items = max_items;
   int index = y / item_height + menu_data->offset;

   menu_data->hover_index = index;
   if(index < menu_data->offset || index >= menu_data->offset + shown_items) {
      menu_data->hover_index = -1;
   }

   if(old_index != menu_data->hover_index) {
      draw_menu_item(menu, surface, window, old_index, offsetX, offsetY);
      draw_menu_item(menu, surface, window, menu_data->hover_index, offsetX, offsetY);
   }
}

wo_t *create_menu(int x, int y, int width, int height) {
   wo_t *menu = create_wo(x, y, width, height);
   menu_t *menu_data = malloc(sizeof(menu_t));
   menu_data->items = NULL;
   menu_data->item_count = 0;
   menu_data->selected_index = -1;
   menu_data->hover_index = -1;
   menu_data->offset = 0;
   menu_data->scrolling = false;

   menu->data = menu_data;
   menu->draw_func = &draw_menu;
   menu->click_func = &menu_click;
   menu->keypress_func = &menu_keypress;
   menu->hover_func = &menu_hover;
   menu->type = WO_MENU;

   return menu;
}
