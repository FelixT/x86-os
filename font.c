#include <stdint.h>
#include <stddef.h>

#include "font.h"
#include "memory.h"
#include "windowmgr.h"

font_t default_font;

extern uint8_t font7;

void copyFont(font_t *font, uint8_t* letter, int* dest) {
   for(int y = 0; y < font->height; y++) {
      //uint8_t fontrow = letter[y];

      for(int x = 0; x < font->width; x++) {
         // get bit in reverse order
         int bit = (letter[y] >> ((font->width-1)-x)) & 1;
         dest[y*font->width+x] = bit;
      }
   }
}

void getFontLetter(font_t *font, char c, int* dest) {
   if(font->fontmap[(int)c] != NULL)
      copyFont(font, font->fontmap[(int)c], dest);
   else
      copyFont(font, font->fontmap[0], dest); // null
}

font_t *getFont() {
   return &default_font;
}

void font_load(fontfile_t *file) {
   default_font.width = file->width;
   default_font.height = file->height;
   uint8_t *chars = (uint8_t*)&(file->chars);

   for(int i = 0; i < file->size; i++) {
      debug_writeuint(i);
      char c = chars[i];
      debug_writestr(":");
      debug_writeuint((uint8_t)c);
      debug_writestr("\n");

      int offset = file->size + (file->height * i);
      default_font.fontmap[(int)c] = (uint8_t*)(chars + offset);
   }
}

void font_init() {
   font_load((fontfile_t*)(&font7));
   default_font.padding = 2;
}