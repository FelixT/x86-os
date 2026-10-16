#include <stddef.h>
#include "draw.h"
#include "font.h"
#include "lib/maths.h"

int *font_letter;

static bool clipping = false;
static rect_t clip;

void draw_set_clip(rect_t rect) {
   clip = rect;
   clipping = true;
}

void draw_clear_clip() {
   clipping = false;
}

static inline void setpixel(surface_t *surface, int index, int colour) {
   ((uint16_t*)surface->buffer)[index] = colour;
}

void setpixelb(surface_t *surface, int index, int colour, int *buffer, int count, bool restore) {
   if(buffer != NULL) {
      if(restore)
         colour = buffer[count];
      else
         buffer[count] = ((uint16_t*)(surface->buffer))[index];
   }
   ((uint16_t*)surface->buffer)[index] = colour;
}

// clip to surface + clipping
// false if nothing to draw
bool clip_rect(surface_t *surface, int *x, int *y, int *w, int *h) {
   int x0 = *x, y0 = *y;
   int x1 = *x + *w, y1 = *y + *h;

   if(clipping) {
      x0 = max(x0, clip.x);
      y0 = max(y0, clip.y);
      x1 = min(x1, clip.x + clip.w);
      y1 = min(y1, clip.y + clip.h);
   }
   x0 = max(x0, 0);
   y0 = max(y0, 0);
   x1 = min(x1, surface->width);
   y1 = min(y1, surface->height);
   if(x0 >= x1 || y0 >= y1) return false;
   *x = x0; *y = y0;
   *w = x1 - x0; *h = y1 - y0;
   return true;
}

static bool in_clip(surface_t *surface, int x, int y) {
   if(x < 0 || x >= surface->width || y < 0 || y >= surface->height) return false;
   if(!clipping) return true;
   return x >= clip.x && x < clip.x+clip.w && y >= clip.y && y < clip.y+clip.h;
}

void draw_rect(surface_t *surface, uint16_t colour, int x, int y, int width, int height) {
   if(!clip_rect(surface, &x, &y, &width, &height))
      return;

   for(int yi = y; yi < y+height; yi++)
      for(int xi = x; xi < x+width; xi++)
         setpixel(surface, yi*surface->pitch+xi, colour);
}

void draw_rect_gradient(surface_t *surface, uint16_t color1, uint16_t color2, int x, int y, int width, int height, int direction) {
   int r1 = (color1 >> 11) & 0x1F;
   int g1 = (color1 >> 5) & 0x3F;
   int b1 = color1 & 0x1F;
   
   int r2 = (color2 >> 11) & 0x1F;
   int g2 = (color2 >> 5) & 0x3F;
   int b2 = color2 & 0x1F;
   
   int range = (direction == 0) ? width : height;

   int clipX = x;
   int clipY = y;
   if(!clip_rect(surface, &clipX, &clipY, &width, &height)) return;
   
   for(int yi = clipY; yi < clipY + height; yi++) {
      for(int xi = clipX; xi < clipX + width; xi++) {
         int pos = (direction == 0) ? xi - x : yi - y;
         int factor = (pos * 256) / (range - 1);
         int r = r1 + (((r2 - r1) * factor) >> 8);
         int g = g1 + (((g2 - g1) * factor) >> 8);
         int b = b1 + (((b2 - b1) * factor) >> 8);
         uint16_t gradient_color = (r << 11) | (g << 5) | b;
         setpixel(surface, yi * surface->pitch + xi, gradient_color);
      }
   }
}

void draw_unfilledrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height) {
   draw_line(surface, colour, x, y, false, width); // top
   draw_line(surface, colour, x, y+height-1, false, width); // bottom
   draw_line(surface, colour, x, y, true, height); // left
   draw_line(surface, colour, x+width-1, y, true, height); // right
}

static inline void dottedrect_pixel(surface_t *surface, int x, int y, uint16_t colour, int *buffer, int *count, bool restore) {
   if(!in_clip(surface, x, y)) return;
   setpixelb(surface, y*surface->pitch+x, colour, buffer, (*count)++, restore);
}

void draw_dottedrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height, int *buffer, bool restore) {
   int count = 0;

   for(int xi = x; xi < x+width; xi++) // top
      if((xi%2) == 0)
         dottedrect_pixel(surface, xi, y, colour, buffer, &count, restore);

   for(int xi = x+1; xi < x+width-1; xi++) // bottom
      if((xi%2) == 0)
         dottedrect_pixel(surface, xi, y+height-1, colour, buffer, &count, restore);

   for(int yi = y+1; yi < y+height; yi++) // left
      if((yi%2) == 0)
         dottedrect_pixel(surface, x, yi, colour, buffer, &count, restore);

   for(int yi = y+1; yi < y+height; yi++) // right
      if((yi%2) == 0)
         dottedrect_pixel(surface, x+width-1, yi, colour, buffer, &count, restore);
}

void draw_line(surface_t *surface, uint16_t colour, int x, int y, bool vertical, int length) {
   draw_rect(surface, colour, x, y, vertical?1:length, vertical?length:1);
}

void draw_char(surface_t *surface, char c, uint16_t colour, int x, int y) {
   getFontLetter(getFont(), c, font_letter);

   int oX = x;
   int oY = y;
   int fw = getFont()->width;
   int w = getFont()->width;
   int h = getFont()->height;
   if(!clip_rect(surface, &x, &y, &w, &h))
      return;

   for(int yi = y; yi < y+h; yi++) {
      for(int xi = x; xi < x+w; xi++) {
         if(font_letter[(yi - oY)*fw + (xi - oX)] == 1)
            setpixel(surface, yi*surface->pitch+xi, colour);
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

uint16_t rgb16_lighten(uint16_t color, uint8_t amount) {
   // amount: 0-255 (0 = no change, 255 = full white)
   uint8_t r = (color >> 11) & 0x1F;
   uint8_t g = (color >> 5) & 0x3F;
   uint8_t b = color & 0x1F;
   
   r = r + (((31 - r) * amount) >> 8);
   g = g + (((63 - g) * amount) >> 8);
   b = b + (((31 - b) * amount) >> 8);
   
   return (r << 11) | (g << 5) | b;
}

uint16_t rgb16_darken(uint16_t color, uint8_t amount) {
   // amount: 0-255 (0 = no change, 255 = full black)
   uint8_t r = (color >> 11) & 0x1F;
   uint8_t g = (color >> 5) & 0x3F;
   uint8_t b = color & 0x1F;
   
   r = (r * (255 - amount)) >> 8;
   g = (g * (255 - amount)) >> 8;
   b = (b * (255 - amount)) >> 8;
   
   return (r << 11) | (g << 5) | b;
}
