#ifndef BMP_H
#define BMP_H

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

void bmp_draw(uint8_t *bmp, uint16_t* framebuffer, int screenWidth, int screenHeight, int x, int y, bool whiteIsTransparent);
uint16_t bmp_get_colour(uint8_t *bmp, int x, int y);
int32_t bmp_get_width(uint8_t *bmp);
int32_t bmp_get_height(uint8_t *bmp);

#endif