#ifndef UI_IMAGE_H
#define UI_IMAGE_H

#include <stdint.h>
#include "wo.h"

typedef struct image_t {
   uint16_t *data;
   uint16_t colour_bg;
   uint16_t colour_border;
   //void (*click_func)(wo_t *wo, int window);
   //void (*release_func)(wo_t *wo, int window);
} image_t;

wo_t *create_image(int x, int y, int width, int height, uint16_t *data);
void draw_image(wo_t *button, wo_draw_context_t context);
void destroy_image(wo_t *image);

#endif