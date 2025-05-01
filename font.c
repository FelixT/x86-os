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
         int i = font->reversed ? (font->width-1)-x : x;
         int bit = (letter[y] >> i) & 1;
         dest[y*font->width+x] = bit;
      }
   }
}

void getFontLetter(font_t *font, char c, int* dest) {
   if(font->fontmap[(int)c] != NULL)
      copyFont(font, font->fontmap[(int)c], dest);
   else if(c >= 'a' && c <= 'z' && font->fontmap[(int)((c-'a')+'A')] != NULL)
      copyFont(font, font->fontmap[(int)((c-'a')+'A')], dest);
   else
      copyFont(font, font->fontmap[0], dest); // null
}

font_t *getFont() {
   return &default_font;
}

void font_load(fontfile_t *file) {
   default_font.width = file->width;
   default_font.height = file->height;
   default_font.reversed = (bool)file->reversed;
   uint8_t *chars = (uint8_t*)&(file->chars);

   // clear existing pointers
   for(int i = 0; i < 256; i++) {
      default_font.fontmap[i] = NULL;
   }

   for(int i = 0; i < file->size; i++) {
      char c = chars[i];

      int offset = file->size + (file->height * i);
      default_font.fontmap[(int)c] = (uint8_t*)(chars + offset);
   }
}

void font_init() {
   font_load((fontfile_t*)(&font7));
   default_font.padding = 2;
}

int font_width(int len) {
   return len*(getFont()->width+getFont()->padding);
}