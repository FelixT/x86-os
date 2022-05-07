#include <stdint.h>
#include <stddef.h>

extern int videomode;

int gui_index = 0;

// video mode = 320x200 256 color graphics, set in main_32.asm
const size_t gui_width = 320;
const size_t gui_height = 200;

void gui_drawrect(int colour, int x, int y, int width, int height) {
   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;
   for(int yi = y; yi < y+height; yi++) {
      for(int xi = x; xi < x+width; xi++) {
         terminal_buffer[yi*gui_width+xi] = colour;
      }
   }
   return;
}

void gui_clear(int colour) {
   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;
   for(int y = 0; y < (int)gui_height; y++) {
      for(int x = 0; x < (int)gui_width; x++) {
         terminal_buffer[y*gui_width+x] = colour;
      }
   }
   return;
}

void gui_draw(void) {
   gui_clear(3);
   videomode = 1;
   gui_drawrect(13, gui_index%(gui_width*gui_height/2), 5, 10, 10);
   gui_index++;
}