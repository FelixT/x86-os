#include <stdint.h>
#include <stddef.h>

extern "C" {
   size_t terminal_index = 0;

   uint16_t colour(uint8_t fg, uint8_t bg) {
      return fg | bg << 4;
   }

   uint16_t entry(char c, uint8_t colour) {
      return (uint16_t) c | (uint16_t) colour << 8;
   }

   void terminal_clear(void) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      for(unsigned i = 0; i < 80*25; i++) {
         terminal_buffer[i] = entry(' ', colour(15, 0));
      }
      return;
   }

   void terminal_write(char* str) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      int i = 0;
      while(str[i] != '\0') {
         terminal_buffer[terminal_index++] = entry(str[i++], colour(15, 0));

         if(terminal_index > 100)
            terminal_index = 0;
      }
   }

   void gui_drawrect(int colour, int x, int y, int width, int height) {
      uint8_t *terminal_buffer = (uint8_t*) 0xA8000;
      for(int yi = 0; yi < y+height; yi++) {
         for(int xi = 0; xi < x+width; xi++) {
            terminal_buffer[yi*320+xi] = colour;
         }
      }
      return;
   }

   void gui_draw(void) {
      gui_drawrect(13, 5, 5, 10, 10);
   }
}