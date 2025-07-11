// window object specific helper functions

#ifndef WO_API_H
#define WO_API_H

#include "../../windowobj.h"

windowobj_t *create_button(int x, int y, char *text);
windowobj_t *create_text(int x, int y, char *text);
void set_text(windowobj_t *wo, char *text);

#endif