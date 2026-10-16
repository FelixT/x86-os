// handles 8 bit bmps and 16 bit R5G6B5 bmps

#include <stdint.h>
#include <stdbool.h>

#include "bmp.h"
#include "gui.h"
#include "lib/maths.h"

bool bmp_check(bmp_info_t *info) {
   // check if supported
   if(info->bpp != 8 && info->bpp != 16) {
      gui_writestr("BPP not 8bit or 16bit\n", 0);// bad
      return false;
   }
   if(info->bpp == 8 && info->compressionMethod != 0) {
      gui_writestr("Compression method not BI_RGB for 8bit bmp\n", 0);
      return false;
   }
   if(info->bpp == 16 && info->compressionMethod != 3) {
      gui_writestr("Compression method not BI_BITFIELDS for 16bit bmp\n", 0);
      return false;
   }
   return true;
}

int32_t bmp_get_width(uint8_t *bmp) {
   bmp_info_t *info = (bmp_info_t*)(&bmp[sizeof(bmp_header_t)]);
   return info->width;
}

int32_t bmp_get_height(uint8_t *bmp) {
   bmp_info_t *info = (bmp_info_t*)(&bmp[sizeof(bmp_header_t)]);
   return info->height;
}

uint16_t bmp_get_colour(uint8_t *bmp, int x, int y) {
   bmp_header_t *header = (bmp_header_t*)(&bmp[0]);
   bmp_info_t *info = (bmp_info_t*)(&bmp[sizeof(bmp_header_t)]);

   if(!bmp_check(info)) return 0;

   bmp_colour_t *colours = (bmp_colour_t*)(&bmp[0]+sizeof(bmp_header_t)+info->headerSize);
   uint8_t *pixels8bit = (uint8_t*)(&bmp[0] + header->dataOffset);
   uint16_t *pixels16bit = (uint16_t*)(&bmp[0] + header->dataOffset);

   uint32_t rowSize = ((info->width+(4-1))/4)*4; // has to be multiple of 4 bytes

   int index = (info->height-(y+1))*rowSize+x;
   if(info->bpp == 8) 
      return gui_rgb16(colours[pixels8bit[index]].red, colours[pixels8bit[index]].green, colours[pixels8bit[index]].blue);
   else
      return pixels16bit[index];
}

void bmp_draw(uint8_t *bmp, surface_t *surface, int x, int y, bool whiteIsTransparent, int scale) {
   if(scale < 1) return;

   uint16_t *framebuffer = (uint16_t*)surface->buffer;
   int minX = 0, minY = 0;
   int screenWidth = surface->width;
   int screenHeight = surface->height;
   if(!clip_rect(surface, &minX, &minY, &screenWidth, &screenHeight)) return;
   screenWidth += minX;
   screenHeight += minY;

   bmp_header_t *header = (bmp_header_t*)(&bmp[0]);
   bmp_info_t *info = (bmp_info_t*)(&bmp[sizeof(bmp_header_t)]);
   
   if(!bmp_check(info)) return;

   if(info->colourPaletteLength == 0)
      info->colourPaletteLength = 1 << info->bpp; // 2^n

   bmp_colour_t *colours = (bmp_colour_t*)(&bmp[0]+sizeof(bmp_header_t)+info->headerSize); // colour table for 8 bit bmps
   uint8_t *pixels8bit = (uint8_t*)(&bmp[0] + header->dataOffset);
   uint16_t *pixels16bit = (uint16_t*)(&bmp[0] + header->dataOffset);

   uint32_t rowSize = ((info->bpp * info->width+(32-1))/32)*4; // has to be multiple of 4 bytes, bytes per row

   int xiStart = minX > x ? (minX - x) / scale : 0;
   int yiStart = minY > y ? (minY - y) / scale : 0;
   int xiEnd = screenWidth > x ? min(info->width, (screenWidth - x + scale - 1) / scale) : 0;
   int yiEnd = screenHeight > y ? min(info->height, (screenHeight - y + scale - 1) / scale) : 0;

   // pixels starts at bottom, works up left to right
   for(int yi = yiStart; yi < yiEnd; yi++) {
      int rowOffset = (info->height - (yi + 1)) * rowSize;
      int scaledY = y + yi * scale;

      for(int xi = xiStart; xi < xiEnd; xi++) {
         int index = rowOffset + xi;
         int index16 = rowOffset / 2 + xi;

         int scaledX = x + xi * scale;

         uint16_t colour;
         if(info->bpp == 8) {
            colour = gui_rgb16(colours[pixels8bit[index]].red, colours[pixels8bit[index]].green, colours[pixels8bit[index]].blue);
         } else { // 16 bit
            colour = pixels16bit[index16];
         }

         if(whiteIsTransparent && colour == COLOUR_WHITE) continue;

         for(int dy = 0; dy < scale; dy++) {
            int sy = scaledY + dy;
            if(sy < minY || sy >= screenHeight) continue;

            for(int dx = 0; dx < scale; dx++) {
               int sx = scaledX + dx;
               if(sx < minX || sx >= screenWidth) continue;

               framebuffer[sy * surface->pitch + sx] = colour;
            }
         }
      }
   }

   
}