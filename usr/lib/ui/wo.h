// usemode port of windowobjects

#ifndef WO_H
#define WO_H

#include <stdbool.h>
#include "../../prog.h"

typedef enum {
   WO_DISABLED,
   WO_TEXTINPUT,
   WO_LABEL,
   WO_BUTTON,
   WO_CANVAS,
   WO_MENU,
   WO_SCROLLBAR,
   WO_SCROLLER
} wo_type_t;

typedef struct wo_t {
   wo_type_t type;
   int x, y;
   int width, height;
   bool visible;
   bool enabled;

   bool hovering;
   bool clicked;
   bool needs_redraw;
   void *data; // actual object e.g. button_t, label_t, etc.
   
   void (*draw_func)(struct wo_t *wo, surface_t *surface);
   void (*click_func)(struct wo_t *wo, surface_t *surface, int x, int y);
   void (*release_func)(struct wo_t *wo, surface_t *surface, int x, int y);
   void (*hover_func)(struct wo_t *wo, int x, int y);
   void (*drag_func)(struct wo_t *wo, int x, int y);
} wo_t;

wo_t *create_wo(int x, int y, int width, int height);
//wo_t *destroy_wo(int x, int y, int width, int height);

#endif