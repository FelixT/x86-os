#ifndef FONT_H
#define FONT_H

#define MAX_FONT_WIDTH 14
#define MAX_FONT_HEIGHT 14

typedef struct font_t {
   int width;
   int height;
   int padding;
   uint8_t *fontmap[256]; // 256 possible chars, map from char to pointer to letter
} font_t;

typedef struct fontfile_t {
   uint8_t size;
   uint8_t width;
   uint8_t height;
   uint8_t chars;
} fontfile_t;

font_t *getFont();
void getFontLetter(font_t *font, char c, int* dest);
void font_init();
void font_load(fontfile_t *file);

#endif