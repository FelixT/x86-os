#ifndef WINDOWOBJ_H
#define WINDOWOBJ_H

#include <stdbool.h>
#include "surface_t.h"

enum windowobj_type {
   WO_DISABLED,
   WO_TEXT,
   WO_BUTTON,
   WO_CANVAS,
   WO_MENU,
   WO_SCROLLBAR,
   WO_SCROLLER
};

typedef struct windowobj_menu_t {
   char text[20];
   void (*func)();
   bool disabled;
} windowobj_menu_t;

typedef struct windowobj_t {
   enum windowobj_type type;

   void (*draw_func)(void *windowobj);
   void (*click_func)(void *windowobj, void *regs); // user progs passed dummy NULL 2nd arg
   void (*hover_func)(void *windowobj, int x, int y);
   void (*return_func)(void *windowobj);
   void (*release_func)(void *windowobj, void *regs, int x, int y);
   void (*drag_func)(void *windowobj, int x, int y);
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
   bool disabled;
   bool oneline; // no return behaviour unless overriden
   int textpos; // text length
   int cursor_textpos; // position in text cursor is at
   int cursorx;
   int cursory;
   int menuselected;
   int menuitem_count;
   int menuhovered;
   windowobj_menu_t *menuitems;

   void *children[10];
   int child_count;
   void *parent; // parent windowobj, NULL if this is a top-level object
} windowobj_t;

void windowobj_init(windowobj_t *windowobj, surface_t *window_surface);

void windowobj_draw(void *windowobj);
void windowobj_redraw(void *window, void *windowobj);
void windowobj_click(void *regs, void *windowobj, int relX, int relY);
bool windowobj_release(void *regs, void *windowobj, int relX, int relY);
void windowobj_hover(void *windowobj, int x, int y);
void windowobj_keydown(void *regs, void *windowobj, int scan_code);
void windowobj_free(windowobj_t *wo);
void windowobj_dragged(void *windowobj, int x, int y, int relX, int relY);

#endif