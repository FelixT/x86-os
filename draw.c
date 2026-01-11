#include <stddef.h>
#include "draw.h"
#include "font.h"

int *font_letter;

inline uint16_t rgb16(uint8_t r, uint8_t g, uint8_t b) {
   // 5r 6g 5b
   return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

inline void setpixel_safe(surface_t *surface, int index, int colour) {
   if(index < 0 || index >= surface->width*surface->height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface->buffer)[index] = colour;
   }
}

void setpixel_safeb(surface_t *surface, int index, int colour, int *buffer, int count, bool restore) {
   if(index < 0 || index >= surface->width*surface->height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      if(buffer != NULL) {
         if(restore)
            colour = buffer[count];
         else
            buffer[count] = ((uint16_t*)(surface->buffer))[index];
      }
      ((uint16_t*)surface->buffer)[index] = colour;
   }
}

void setpixelcoord_safe(surface_t *surface, int x, int y, int colour) {
   if(x < 0 || y < 0 || x >= surface->width || y >= surface->height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface->buffer)[x+y*surface->width] = colour;
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
   
   for( int yi = y; yi < y + height; yi++) {
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

void draw_dottedrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height, int *buffer, bool restore) {
   int count = 0;

   for(int xi = x; xi < x+width; xi++) // top
      if((xi%2) == 0)
         setpixel_safeb(surface, y*(int)surface->width+xi, colour, buffer, count++, restore);

   for(int xi = x+1; xi < x+width-1; xi++) // bottom
      if((xi%2) == 0)
         setpixel_safeb(surface, (y+height-1)*(int)surface->width+xi, colour, buffer, count++, restore);

   for(int yi = y+1; yi < y+height; yi++) // left
      if((yi%2) == 0)
         setpixel_safeb(surface, (yi)*(int)surface->width+x, colour, buffer, count++, restore);

   for(int yi = y+1; yi < y+height; yi++) // right
      if((yi%2) == 0)
         setpixel_safeb(surface, (yi)*(int)surface->width+x+width-1, colour, buffer, count++, restore);
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

void draw_char(surface_t *surface, char c, uint16_t colour, int x, int y) {
   getFontLetter(getFont(), c, font_letter);

   int i = 0;      
   for(int yi = y; yi < y+getFont()->height; yi++) {
      for(int xi = x; xi < x+getFont()->width; xi++) {
         if(font_letter[i] == 1)
            setpixel_safe(surface, yi*(int)surface->width+xi, colour);
         i++;
      }
   }
}

void draw_string(surface_t *surface, char* c, uint16_t colour, int x, int y) {
   int i = 0;
   while(c[i] != '\0' && c[i] != '\n') {
      draw_char(surface, c[i++], colour, x, y);
      x+=getFont()->width+getFont()->padding;
   }
}