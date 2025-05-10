#ifndef WINDOWOBJ_H
#define WINDOWOBJ_H

#include <stdbool.h>
#include "surface_t.h"

enum windowobj_type {
   WO_DISABLED,
   WO_TEXT,
   WO_BUTTON,
   WO_CANVAS,
   WO_MENU
};

typedef struct windowobj_menuitem_t {
   char text[20];
   void (*func)();
} windowobj_menu_t;

typedef struct windowobj_t {
   enum windowobj_type type;

   void (*draw_func)(void *windowobj);
   void (*click_func)(void *windowobj);
   void (*hover_func)(void *windowobj, int x, int y);
   void (*return_func)(void *windowobj);
   surface_t *window_surface;

   int x;
   int y;
   int width;
   int height;
   char *text;
   int textpadding;
   bool textvalign;
   bool texthalign;
   bool hovering;
   bool clicked;
   bool visible;
   int textpos;
   int cursorx;
   int cursory;
   int menuselected;
   int menuitem_count;
   windowobj_menu_t *menuitems;
} windowobj_t;

void windowobj_init(windowobj_t *windowobj, surface_t *window_surface);

void windowobj_draw(void *windowobj);
void windowobj_redraw(void *window, void *windowobj);
void windowobj_click(void *regs, void *windowobj);
void windowobj_hover(void *windowobj, int x, int y);
void windowobj_keydown(void *regs, void *windowobj, int scan_code);

#endif