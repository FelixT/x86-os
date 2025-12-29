#ifndef UI_GROUPBOX_H
#define UI_GROUPBOX_H

#include "wo.h"

typedef struct groupbox_t {
   wo_t *canvas;
   char label[32];
   uint16_t colour_label;
   uint16_t colour_border;
} groupbox_t;

wo_t *create_groupbox(int x, int y, int width, int height, char *label);
void groupbox_add(wo_t *groupbox, wo_t *child);

#endif