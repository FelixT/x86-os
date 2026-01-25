#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>

#include "wo.h"

typedef struct menu_item_t {
   char text[64];
   void (*func)(wo_t *item, int index, int window);
   bool enabled;
} menu_item_t;

typedef struct menu_t {
   menu_item_t *items;
   int item_count;
   int selected_index;
   int hover_index;
   int offset; // used for scroll
   int shown_items;
   bool scrollbar_visible; // showing scrollbar
   bool scrolling;
   int scrollerHeight;
   int scrollerY;
} menu_t;

wo_t *create_menu(int x, int y, int width, int height);
menu_item_t *add_menu_item(wo_t *menu, const char *text, void (*func)(wo_t *item, int index, int window));
menu_item_t *get_menu_item(wo_t *menu, int index);
void destroy_menu(wo_t *menu);
void resize_menu(wo_t *menu); // resize to fit contents

#endif