#include "draw.h"
#include "stdlib.h"
#include "../../lib/string.h"

static inline void setpixel_safe(surface_t *surface, int index, int colour) {
   if(index < 0 || index >= surface->width*surface->height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface->buffer)[index] = colour;
   }
}

void draw_pixel(draw_context_t *ctx, uint16_t colour, int x, int y) {
   // bounds check
   if(x < ctx->clipRect.x || y < ctx->clipRect.y
   || x >= ctx->clipRect.x + ctx->clipRect.width
   || y >= ctx->clipRect.y + ctx->clipRect.height)
      return;
   int index = y * (int)ctx->surface->width + x;
   setpixel_safe(ctx->surface, index, colour);
}

void draw_line(draw_context_t *ctx, uint16_t colour, int x, int y, bool vertical, int length) {
   if(vertical) {
      for(int yi = y; yi < y+length; yi++)
         draw_pixel(ctx, colour, x, yi);
   } else {
      for(int xi = x; xi < x+length; xi++)
         draw_pixel(ctx, colour, xi, y);
   }
}

void draw_rect(draw_context_t *ctx, uint16_t colour, int x, int y, int width, int height) {
   for(int yi = y; yi < y+height; yi++)
      for(int xi = x; xi < x+width; xi++)
         draw_pixel(ctx, colour, xi, yi);
}

void draw_rect_gradient(draw_context_t *ctx, uint16_t color1, uint16_t color2, int x, int y, int width, int height, int direction) {
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
         draw_pixel(ctx, gradient_color, xi, yi);
      }
   }
}

void draw_unfilledrect(draw_context_t *ctx, uint16_t colour, int x, int y, int width, int height) {
   for(int xi = x; xi < x+width; xi++) // top
      draw_pixel(ctx, colour, xi, y);

   for(int xi = x; xi < x+width; xi++) // bottom
      draw_pixel(ctx, colour, xi, y+height-1);

   for(int yi = y; yi < y+height; yi++) // left
      draw_pixel(ctx, colour, x, yi);

   for(int yi = y; yi < y+height; yi++) // right
      draw_pixel(ctx, colour, x+width-1, yi);
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

void draw_string(draw_context_t *ctx, char *str, uint16_t colour, int x, int y) {
   int maxchars = ctx->clipRect.width / (get_font_info().width + get_font_info().padding) - 1;
   if(strlen(str) > maxchars) {
      char *buf = malloc(maxchars+1);
      strncpy(buf, str, maxchars);
      write_strat_w(buf, x, y, colour, ctx->window);
   } else {
      write_strat_w(str, x, y, colour, ctx->window);
   }
}

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

rect_t rect_intersect(rect_t a, rect_t b) {
   int x1 = MAX(a.x, b.x);
   int y1 = MAX(a.y, b.y);
   int x2 = MIN(a.x + a.width, b.x + b.width);
   int y2 = MIN(a.y + a.height, b.y + b.height);
   
   return (rect_t) {
      .x = x1,
      .y = y1,
      .width = MAX(0, x2 - x1),
      .height = MAX(0, y2 - y1)
   };
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