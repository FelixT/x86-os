#include "draw.h"
#include "font.h"

int font_letter[FONT_WIDTH*FONT_HEIGHT];

void setpixel_safe(surface_t *surface, int index, int colour) {
   if(index < 0 || index >= surface->width*surface->height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
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

void draw_dottedrect(surface_t *surface, uint16_t colour, int x, int y, int width, int height) {
   for(int xi = x; xi < x+width; xi++) // top
      if((xi%2) == 0)
         setpixel_safe(surface, y*(int)surface->width+xi, colour);

   for(int xi = x; xi < x+width; xi++) // bottom
      if((xi%2) == 0)
         setpixel_safe(surface, (y+height-1)*(int)surface->width+xi, colour);

   for(int yi = y; yi < y+height; yi++) // left
      if((yi%2) == 0)
         setpixel_safe(surface, (yi)*(int)surface->width+x, colour);

   for(int yi = y; yi < y+height; yi++) // right
      if((yi%2) == 0)
         setpixel_safe(surface, (yi)*(int)surface->width+x+width-1, colour);
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
   getFontLetter(c, font_letter);

   int i = 0;      
   for(int yi = y; yi < y+FONT_HEIGHT; yi++) {
      for(int xi = x; xi < x+FONT_WIDTH; xi++) {
         if(font_letter[i] == 1)
            setpixel_safe(surface, yi*(int)surface->width+xi, colour);
         i++;
      }
   }
}

void draw_string(surface_t *surface, char* c, uint16_t colour, int x, int y) {
   int i = 0;
   while(c[i] != '\0') {
      draw_char(surface, c[i++], colour, x, y);
      x+=FONT_WIDTH+FONT_PADDING;
   }
}