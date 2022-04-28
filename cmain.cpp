#include <stdint.h>

extern "C" {
   uint16_t colour(uint8_t fg, uint8_t bg) {
      return fg | bg << 4;
   }

   uint16_t entry(char c, uint8_t colour) {
      return (uint16_t) c | (uint16_t) colour << 8;
   }

   void cmain(void) {
      //load_gui();
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      for(unsigned i = 0; i < 80*25; i++) {
         terminal_buffer[i] = entry(' ', colour(15, 0));
      }
      return;
   }
}