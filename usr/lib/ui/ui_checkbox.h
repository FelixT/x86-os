#ifndef UI_CHECKBOX_H
#define UI_CHECKBOX_H

#include "wo.h"

typedef struct checkbox_t {
   bool checked;
   void (*release_func)(wo_t *wo, int window);
} checkbox_t;

wo_t *create_checkbox(int x, int y, bool checked);
void destroy_checkbox(wo_t *checkbox);
void draw_checkbox(wo_t *checkbox, draw_context_t context);
void set_checkbox_release(wo_t *checkbox, void(*release_func)(wo_t *wo, int window));

#endif