// handles 8 bit bmps and 16 bit R5G6B5 bmps

#include <stdint.h>
#include <stdbool.h>

#include "bmp.h"
#include "gui.h"

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

void bmp_draw(uint8_t *bmp, uint16_t* framebuffer, int screenWidth, int screenHeight, int x, int y, bool whiteIsTransparent, int scale) {
   if(scale < 1) return;

   bmp_header_t *header = (bmp_header_t*)(&bmp[0]);
   bmp_info_t *info = (bmp_info_t*)(&bmp[sizeof(bmp_header_t)]);
   
   if(!bmp_check(info)) return;

   if(info->colourPaletteLength == 0)
      info->colourPaletteLength = 1 << info->bpp; // 2^n

   bmp_colour_t *colours = (bmp_colour_t*)(&bmp[0]+sizeof(bmp_header_t)+info->headerSize); // colour table for 8 bit bmps
   uint8_t *pixels8bit = (uint8_t*)(&bmp[0] + header->dataOffset);
   uint16_t *pixels16bit = (uint16_t*)(&bmp[0] + header->dataOffset);

   uint32_t rowSize = ((info->bpp * info->width+(32-1))/32)*4; // has to be multiple of 4 bytes, bytes per row

   // pixels starts at bottom, works up left to right
   for(int yi = 0; yi < info->height; yi++) {
      for(int xi = 0; xi < info->width; xi++) {
         int index = (info->height-(yi+1))*rowSize+xi;
         int index16 = ((info->height-(yi+1))*rowSize)/2+xi;

         // 8bit
         uint16_t colour = gui_rgb16(colours[pixels8bit[index]].red, colours[pixels8bit[index]].green, colours[pixels8bit[index]].blue);
         // 16 bit
         if(info->bpp == 16)
            colour = pixels16bit[index16];

         if(!whiteIsTransparent || colour != COLOUR_WHITE) {
            for(int dy = 0; dy < scale; dy++) {
               for(int dx = 0; dx < scale; dx++) {
                  int sx = x + xi * scale + dx;
                  int sy = y + yi * scale + dy;
                  int i = sy * screenWidth + sx;
                  if(sx >= 0 && sx < screenWidth && sy >= 0 && sy < screenHeight) {
                     framebuffer[i] = colour;
                  }
               }
            }
         }
      }

   }

   
}