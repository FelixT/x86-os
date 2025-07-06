#ifndef WINDOW_SETTINGS_H
#define WINDOW_SETTINGS_H

#include "window_t.h"

typedef struct window_settings_t {
   gui_window_t *window;
   gui_window_t *selected;
   windowobj_t *d_bgcolour_wo;
   windowobj_t *d_bgimg_wo;
   windowobj_t *w_bgcolour_wo;
   windowobj_t *w_txtcolour_wo;
   windowobj_t *theme_wo; 
   windowobj_t *theme_gradientstyle_wo;
   windowobj_t *theme_colour_wo;
   windowobj_t *theme_colour2_wo;
   windowobj_t *theme_txtpadding_wo;
} window_settings_t;

window_settings_t *window_settings_init(gui_window_t *window, gui_window_t *selected);

#endif