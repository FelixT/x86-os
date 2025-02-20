#ifndef WINDOWOBJ_H
#define WINDOWOBJ_H

#include <stdbool.h>
#include "surface_t.h"

enum windowobj_type {
   WO_DISABLED,
   WO_TEXT,
   WO_BUTTON,
   WO_CANVAS
};

typedef struct windowobj_t {
   enum windowobj_type type;

   void (*draw_func)(void *windowobj);
   void (*click_func)(void *windowobj);
   void (*hover_func)(void *windowobj);
   surface_t *window_surface;

   int x;
   int y;
   int width;
   int height;
   char *text;
   int textpadding;
   bool hovering;
   bool clicked;

} windowobj_t;

void windowobj_init(windowobj_t *windowobj, surface_t *window_surface);

void windowobj_draw(void *windowobj);
void windowobj_redraw(void *window, void *windowobj);
void windowobj_click(void *regs, void *windowobj);
void windowobj_hover(void *windowobj);

#endif