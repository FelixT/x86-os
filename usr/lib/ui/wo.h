// usemode port of windowobjects

#ifndef WO_H
#define WO_H

#include <stdbool.h>
#include "../../prog.h"

typedef enum {
   WO_DISABLED,
   WO_INPUT,
   WO_LABEL,
   WO_BUTTON,
   WO_CANVAS,
   WO_MENU,
   WO_SCROLLBAR,
   WO_SCROLLER,
   WO_GRID,
   WO_GROUPBOX,
   WO_IMAGE,
   WO_CHECKBOX
} wo_type_t;

typedef struct wo_t {
   wo_type_t type;
   int x, y;
   int width, height;
   bool visible;
   bool enabled;
   bool fixed; // fixed position during scroll

   bool hovering;
   bool clicked;
   bool selected; // focused
   bool needs_redraw;
   void *data; // actual object e.g. button_t, label_t, etc.
   
   void (*draw_func)(struct wo_t *wo, surface_t *surface, int window, int offsetX, int offsetY);
   void (*click_func)(struct wo_t *wo, surface_t *surface, int window, int x, int y, int offsetX, int offsetY);
   void (*release_func)(struct wo_t *wo, surface_t *surface, int window, int x, int y, int offsetX, int offsetY);
   void (*unfocus_func)(struct wo_t *wo, surface_t *surface, int window, int offsetX, int offsetY);
   void (*hover_func)(struct wo_t *wo, surface_t *surface, int window, int x, int y, int offsetX, int offsetY);
   void (*mousein_func)(struct wo_t *wo, surface_t *surface, int window, int x, int y, int offsetX, int offsetY);
   void (*unhover_func)(struct wo_t *wo, surface_t *surface, int window, int offsetX, int offsetY); // mouseout
   void (*drag_func)(struct wo_t *wo, int x, int y);
   void (*keypress_func)(struct wo_t *wo, uint16_t c, int window);
   void (*destroy_func)(struct wo_t *wo);
} wo_t;

wo_t *create_wo(int x, int y, int width, int height);
void destroy_wo(wo_t *wo);

#endif