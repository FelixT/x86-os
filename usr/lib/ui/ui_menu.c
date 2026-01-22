#include "ui_menu.h"
#include "../../../lib/string.h"
#include "../draw.h"
#include "../../prog.h"
#include "wo.h"
#include "../stdio.h"

void draw_menu_item(wo_t *menu, draw_context_t context, int index) {
   if(index < 0) return;

   uint16_t bg_selected = rgb16(222, 222, 222);
   uint16_t bg_hover = rgb16(243, 243, 243);
   uint16_t bg_item = rgb16(250, 250, 250);
   uint16_t border_light = rgb16(235, 235, 235);

   menu_t *menu_data = (menu_t *)menu->data;

   int bgwidth = menu->width - 2;
   if(menu_data->scrollbar_visible)
      bgwidth -= 14;

   int item_height = get_font_info().height + 7;

   int x = menu->x + context.offsetX;
   int y = menu->y + context.offsetY + ((index - menu_data->offset) * item_height) + 1;

   menu_item_t *item = &menu_data->items[index];
   // highlight if selected
   if(index == menu_data->selected_index) {
      draw_rect(&context, bg_selected, x + 1, y, bgwidth, item_height);
      write_strat_w(item->text, x + 5, y + 5, border_light, context.window);
   } else if(index == menu_data->hover_index && menu->hovering) {
      draw_rect(&context, bg_hover, x + 1, y, bgwidth, item_height);
   } else {
      draw_rect(&context, bg_item, x + 1, y, bgwidth, item_height);
   }
   // draw text
   uint16_t txtcolour = item->enabled ? 0 : rgb16(200, 200, 200);
   write_strat_w(item->text, x + 5, y + 4, txtcolour, context.window);
   // border
   draw_line(&context, border_light, x + 1, y + item_height - 1, false, bgwidth);

}

void draw_menu_scrollbar(wo_t *menu, draw_context_t context) {
   menu_t *menu_data = (menu_t *)menu->data;

   int x = menu->x + context.offsetX;
   int y = menu->y + context.offsetY;

   uint16_t border_dark = rgb16(165, 165, 165);
   uint16_t bg_selected = rgb16(222, 222, 222);
   uint16_t bg_hover = rgb16(243, 243, 243);

   int item_height = get_font_info().height + 7;
   int max_items = menu->height/item_height;

   int width = 14;
   int scrollbarX = x + menu->width - width - 1;
   int scrollbarY = y + 1;
   int scrollAreaHeight = menu->height - width*2 - 2;
   int scrollAreaY = scrollbarY + width;
   int scrollerHeight = (scrollAreaHeight * menu_data->shown_items) / (menu_data->item_count ? menu_data->item_count : 1);
   if(scrollerHeight < 4)
      scrollerHeight = 4;
   int scrollerY = scrollAreaY + (scrollAreaHeight * menu_data->offset) / (menu_data->item_count ? menu_data->item_count : 1);
   menu_data->scrollerY = scrollerY - menu->y - context.offsetY;
   menu_data->scrollerHeight = scrollerHeight;

   draw_rect(&context, bg_hover, scrollbarX, scrollAreaY, width, scrollAreaHeight); // bg / scrollarea
   draw_rect(&context, menu_data->scrolling ? 0 : border_dark, scrollbarX+5, scrollerY, 4, scrollerHeight); // 4px scroller
   draw_rect(&context, menu_data->offset > 0 ? bg_selected : bg_hover, scrollbarX, scrollbarY, width, width); // up
   char buf[2];
   buf[0] = 0x80; // uparrow
   buf[1] = 0;
   write_strat_w(buf, scrollbarX + 5, scrollbarY + 4, 0, context.window);
   draw_rect(&context, menu_data->offset < menu_data->item_count - max_items ? bg_selected : bg_hover, scrollbarX, scrollbarY + scrollAreaHeight + width, width, width); // down
   buf[0] = 0x81; // downarrow
   write_strat_w(buf, scrollbarX + 5, scrollbarY + scrollAreaHeight + width + 4, 0, context.window);
}

void draw_menu(wo_t *menu, draw_context_t context) {
   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;

   uint16_t border_light = rgb16(235, 235, 235);
   uint16_t border_dark = rgb16(165, 165, 165);
   uint16_t bg = rgb16(255, 255, 255);

   if(menu->selected)
      border_light = rgb16(220, 220, 220);

   if(menu->hovering)
      bg = rgb16(248, 248, 248);

   int x = menu->x + context.offsetX;
   int y = menu->y + context.offsetY;

   // draw border
   draw_line(&context, border_dark,  x, y, true,  menu->height);
   draw_line(&context, border_dark,  x, y, false, menu->width);
   draw_line(&context, border_light, x, y + menu->height - 1, false, menu->width);
   draw_line(&context, border_light, x + menu->width - 1, y, true, menu->height);

   int item_height = get_font_info().height + 7;
   int max_items = menu->height/item_height;
   int shown_items = menu_data->item_count;
   if(shown_items > max_items)
      shown_items = max_items;
   menu_data->shown_items = shown_items;
   int shown_height = shown_items*item_height;

   int bgwidth = menu->width - 2;
   if(menu_data->scrollbar_visible)
      bgwidth -= 14;

   // draw background
   draw_rect(&context, bg, x + 1, y + 1 + shown_height, bgwidth, menu->height - 2 - shown_height);

   // draw items
   for(int i = menu_data->offset; i < shown_items + menu_data->offset; i++) {
      draw_menu_item(menu, context, i);
   }

   if(menu_data->scrollbar_visible) {
      // draw 14px wide scrollbar
      draw_menu_scrollbar(menu, context);
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
      menu_data->scrollbar_visible = true;

   return item;
}

void menu_click(wo_t *menu, draw_context_t context, int x, int y) {
   if(x == 0 && y == 0) return;

   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;
   // scrollbar click
   if(menu_data->scrollbar_visible && x > menu->width - 14 - 1) {
      if(y < 15) {
         // up click
         menu_data->offset--;
         if(menu_data->offset < 0)
            menu_data->offset = 0;
         draw_menu(menu, context);
      }
      if(y > menu_data->scrollerY && y < menu_data->scrollerY + menu_data->scrollerHeight) {
         // scroller click
         menu_data->scrolling = true;
         draw_menu_scrollbar(menu, context);
      }
      if(y > menu->height - 14 - 1) {
         // down click
         menu_data->offset++;
         int item_height = get_font_info().height + 7;
         int max_items = menu->height/item_height;
         if(menu_data->offset > menu_data->item_count - max_items)
            menu_data->offset = menu_data->item_count - max_items;
            
         draw_menu(menu, context);
      }
      return;
   }

   int item_height = get_font_info().height + 7;
   int index = y / item_height + menu_data->offset;

   if(index < 0 || index >= menu_data->item_count) {
      menu_data->selected_index = -1;
      draw_menu(menu, context);
      return;
   }

   menu_data->selected_index = index;

   menu_item_t *item = &menu_data->items[index];
   if(item->enabled && item->func != NULL) {
      item->func(menu, index, context.window);
   }

   draw_menu(menu, context);
}

void menu_release(wo_t *menu, draw_context_t context, int x, int y) {
   (void)x;
   (void)y;
   menu_t *menu_data = menu->data;
   if(menu_data->scrolling) {
      menu_data->scrolling = false;
      draw_menu_scrollbar(menu, context);
   }
}

void menu_keypress(wo_t *menu, uint16_t c, int window) {
   (void)window;
   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;

   int item_height = get_font_info().height + 7;
   int max_items = menu->height/item_height;
   int shown_items = menu_data->item_count;
   if(shown_items > max_items)
      shown_items = max_items;

   if(c == 0x100) {
      // up
      menu_data->selected_index--;
      if(menu_data->selected_index < 0) {
         menu_data->selected_index = menu_data->item_count - 1;
         menu_data->offset = menu_data->selected_index - shown_items + 1;
      }
      if(menu_data->selected_index < menu_data->offset)
         menu_data->offset = menu_data->selected_index;
      menu_item_t *item = &menu_data->items[menu_data->selected_index];
      if(item->enabled && item->func != NULL)
         item->func(menu, menu_data->selected_index, window);
   } else if(c == 0x101) {
      // down
      menu_data->selected_index++;
      if(menu_data->selected_index >= menu_data->item_count) {
         menu_data->selected_index = 0;
         menu_data->offset = 0;
      }
      if(menu_data->selected_index >= menu_data->offset + shown_items)
         menu_data->offset = menu_data->selected_index - shown_items + 1;
      menu_item_t *item = &menu_data->items[menu_data->selected_index];
      if(item->enabled && item->func != NULL)
         item->func(menu, menu_data->selected_index, window);
   }
}

void menu_hover(wo_t *menu, draw_context_t context, int x, int y) {
   (void)x;
   if(menu == NULL || menu->data == NULL) return;
   menu_t *menu_data = (menu_t *)menu->data;

   if(menu_data->scrolling && menu->clicked) {
      menu_data->scrollerY = y;
      if(menu_data->scrollerY < 14)
         menu_data->scrollerY = 14;
      if(menu_data->scrollerY > menu->height - 14)
         menu_data->scrollerY = menu->height - 14;
      menu_data->offset = ((menu_data->item_count - menu_data->shown_items) * (menu_data->scrollerY-14))/(menu->height - 14*2);
      draw_menu(menu, context);
      return;
   } else if(!menu->clicked) {
      menu_data->scrolling = false;
   }
   int old_index = menu_data->hover_index;
   if(menu_data->scrollbar_visible && x > menu->width - 14 - 1) {
      if(menu_data->hover_index == -1) return;
      menu_data->hover_index = -1;
      draw_menu_item(menu, context, old_index);
      return;
   }

   int item_height = get_font_info().height + 7;
   int index = y / item_height + menu_data->offset;

   menu_data->hover_index = index;
   if(index < menu_data->offset || index >= menu_data->offset + menu_data->shown_items) {
      menu_data->hover_index = -1;
   }

   if(old_index != menu_data->hover_index) {
      draw_menu_item(menu, context, old_index);
      draw_menu_item(menu, context, menu_data->hover_index);
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
   menu_data->scrollbar_visible = false;
   menu_data->scrolling = false;

   menu->data = menu_data;
   menu->draw_func = &draw_menu;
   menu->click_func = &menu_click;
   menu->release_func = &menu_release;
   menu->keypress_func = &menu_keypress;
   menu->hover_func = &menu_hover;
   menu->type = WO_MENU;

   return menu;
}
