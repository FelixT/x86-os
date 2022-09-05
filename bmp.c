#include <stdint.h>

#include "gui.h"

typedef struct {
   uint16_t identifier; // 'BM'
   uint32_t size; // bytes
   uint32_t reserved;
   uint32_t dataOffset; // pixel data
} __attribute__((packed)) bmp_header_t;

typedef struct {
   uint32_t headerSize; // size of this header
   int32_t width;
   int32_t height;
   uint16_t colourPlanes;
   uint16_t bpp;
   uint32_t compressionMethod;
   uint32_t dataSize;
   uint32_t horizontalRes;
   uint32_t verticalRes;
   uint32_t colourPaletteLength;
   uint32_t importantColours;
} __attribute__((packed)) bmp_info_t;

typedef struct {
   uint8_t blue;
   uint8_t green;
   uint8_t red;
   uint8_t zero;
} __attribute__((packed)) bmp_colour_t;

void bmp_draw(uint8_t *bmp, uint16_t* framebuffer, int screenWidth, bool whiteIsTransparent) {
   bmp_header_t *header = (bmp_header_t*)(&bmp[0]);
   bmp_info_t *info = (bmp_info_t*)(&bmp[sizeof(bmp_header_t)]);
   
   //gui_writenum(info->width, 0);
   //gui_drawchar('\n', 0);

   if(info->bpp != 8) {
      gui_writestr("BPP not 8bit\n", 0);// bad
      return;
   }
   if(info->compressionMethod != 0) {
      gui_writestr("Compression method not BI_RGB\n", 0);
      return;
   }

   if(info->colourPaletteLength == 0)
      info->colourPaletteLength = 1 << info->bpp; // 2^n

   bmp_colour_t *colours = (bmp_colour_t*)(&bmp[0]+sizeof(bmp_header_t)+info->headerSize);

   uint8_t *pixels = (uint8_t*)(&bmp[0] + header->dataOffset);

   uint32_t rowSize = ((info->width+(4-1))/4)*4; // has to be multiple of 4 bytes

   // pixels starts at bottom, works up left to right
   for(int y = 0; y < info->height; y++) {
      for(int x = 0; x < info->width; x++) {
         int index = (info->height-(y+1))*rowSize+x;
         uint16_t colour = gui_rgb16(colours[pixels[index]].red, colours[pixels[index]].green, colours[pixels[index]].blue);
         if(!whiteIsTransparent || colour != COLOUR_WHITE)
            framebuffer[y*screenWidth+x] = colour;
      }

   }

   
}