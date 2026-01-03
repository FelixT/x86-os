#include "ui_image.h"

#include "../draw.h"
#include "../../prog.h"
#include "../stdio.h"
#include "../draw.h"
#include "wo.h"

void draw_image(wo_t *image, surface_t *surface, int window, int offsetX, int offsetY) {
   (void)window;
   if(image == NULL || image->data == NULL) return;

   image_t *img_data = (image_t *)image->data;
   if(!img_data->data) return;
   for(int y = 0; y < image->height; y++) {
      for(int x = 0; x < image->width; x++) {
         uint16_t colour = img_data->data[y * image->width + x];
         //if(colour != 0xFFFF) { // transparent
            draw_pixel(surface, colour, image->x + x + offsetX, image->y + y + offsetY);
         //}
      }
   }
}

wo_t *create_image(int x, int y, int width, int height, uint16_t *data) {
   wo_t *image = create_wo(x, y, width, height);
   image_t *img_data = malloc(sizeof(image_t));
   img_data->data = data;

   image->data = img_data;
   image->draw_func = &draw_image;
   //button->click_func = &click_button;
   //button->release_func = &release_button;
   image->destroy_func = &destroy_image;
   image->type = WO_IMAGE;

   return image;
}

void destroy_image(wo_t *image) {
   if(image == NULL) return;
   free(image->data, sizeof(image_t));
   image->data = NULL;
}