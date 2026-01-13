#include "draw.h"
#include "../../lib/string.h"

static inline void setpixel_safe(surface_t *surface, int index, int colour) {
   if(index < 0 || index >= surface->width*surface->height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface->buffer)[index] = colour;
   }
}

void draw_pixel(surface_t *surface, uint16_t colour, int x, int y) {
   int index = y * (int)surface->width + x;
   setpixel_safe(surface, index, colour);
}

void draw_line(surface_t *surface, uint16_t colour, int x, int y, bool vertical, int length) {
   if(vertical) {
      for(int yi = y; yi < y+length; yi++)
         setpixel_safe(surface, yi*(int)surface->width+x, colour);
   } else {
      for(int xi = x; xi < x+length; xi++)
         setpixel_safe(surface, y*(int)surface->width+xi, colour);
   }
}

void draw_rect(surface_t *surface, uint16_t colour, int x, int y, int width, int height) {
   for(int yi = y; yi < y+height; yi++)
      for(int xi = x; xi < x+width; xi++)
         setpixel_safe(surface, yi*(int)surface->width+xi, colour);
}

void draw_rect_gradient(surface_t *surface, uint16_t color1, uint16_t color2, int x, int y, int width, int height, int direction) {
   int r1 = (color1 >> 11) & 0x1F;
   int g1 = (color1 >> 5) & 0x3F;
   int b1 = color1 & 0x1F;
   
   int r2 = (color2 >> 11) & 0x1F;
   int g2 = (color2 >> 5) & 0x3F;
   int b2 = color2 & 0x1F;
   
   int range = (direction == 0) ? width : height;
   
   for(int yi = y; yi < y + height; yi++) {
      for(int xi = x; xi < x + width; xi++) {
         int pos = (direction == 0) ? (xi - x) : (yi - y);
         int factor = (pos * 256) / (range - 1);
         int r = r1 + (((r2 - r1) * factor) >> 8);
         int g = g1 + (((g2 - g1) * factor) >> 8);
         int b = b1 + (((b2 - b1) * factor) >> 8);
         uint16_t gradient_color = (r << 11) | (g << 5) | b;
         setpixel_safe(surface, yi * (int)surface->width + xi, gradient_color);
      }
   }
}

void draw_unfilledrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height) {
   for(int xi = x; xi < x+width; xi++) // top
      setpixel_safe(surface, y*(int)surface->width+xi, colour);

   for(int xi = x; xi < x+width; xi++) // bottom
      setpixel_safe(surface, (y+height-1)*(int)surface->width+xi, colour);

   for(int yi = y; yi < y+height; yi++) // left
      setpixel_safe(surface, (yi)*(int)surface->width+x, colour);

   for(int yi = y; yi < y+height; yi++) // right
      setpixel_safe(surface, (yi)*(int)surface->width+x+width-1, colour);
}

void draw_checkeredrect(surface_t *surface, uint16_t colour1, uint16_t colour2, int x, int y, int width, int height) {
    uint32_t pattern1 = ((uint32_t)colour1 << 16) | colour2;
    uint32_t pattern2 = ((uint32_t)colour2 << 16) | colour1;
    
    uint16_t *buffer = (uint16_t*)surface->buffer;
    
    for(int yi = 0; yi < height; yi++) {
      uint32_t *line32 = (uint32_t*)(buffer + (y + yi) * surface->width + x);
      uint32_t pattern = ((yi + y) & 1) ? pattern2 : pattern1;
      
      for(int xi = 0; xi < width / 2; xi++) {
         line32[xi] = pattern;
      }
      
      // handle odd widths
      if(width & 1) {
         uint16_t *line16 = (uint16_t*)line32;
         line16[width - 1] = ((yi + y + width - 1) & 1) ? colour2 : colour1;
      }
   }
}