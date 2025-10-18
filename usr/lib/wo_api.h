// window object specific helper functions

#ifndef WO_API_H
#define WO_API_H

#include "../../windowobj.h"

windowobj_t *create_wo_w(int window, windowobj_t *parent, int type, int x, int y, int width, int height);
windowobj_t *create_wo(windowobj_t *parent, int type, int x, int y, int width, int height);
windowobj_t *create_canvas_w(int window, windowobj_t *parent, int x, int y, int width, int height);
windowobj_t *create_canvas(windowobj_t *parent, int x, int y, int width, int height);
windowobj_t *create_button_w(int window, windowobj_t *parent, int x, int y, char *text);
windowobj_t *create_button(windowobj_t *parent, int x, int y, char *text);
windowobj_t *create_text_w(int window, windowobj_t *parent, int x, int y, char *text);
windowobj_t *create_text(windowobj_t *parent, int x, int y, char *text);
windowobj_t *create_text_static_w(int window, windowobj_t *parent, int x, int y, char *text);
windowobj_t *create_text_static(windowobj_t *parent, int x, int y, char *text);
void set_text(windowobj_t *wo, char *text);

#endif