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
} menu_t;

wo_t *create_menu(int x, int y, int width, int height);
menu_item_t *add_menu_item(wo_t *menu, const char *text, void (*func)(wo_t *item, int index, int window));
void destroy_menu(wo_t *menu);

#endif