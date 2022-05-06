#include <stdint.h>
#include <stddef.h>

extern "C" {
   size_t terminal_index;

   uint16_t colour(uint8_t fg, uint8_t bg) {
      return fg | bg << 4;
   }

   uint16_t entry(char c, uint8_t colour) {
      return (uint16_t) c | (uint16_t) colour << 8;
   }
   
   static inline void outb(uint16_t port, uint8_t val) {
      asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
   }

   void terminal_setcursor(int offset) {
      terminal_index = offset%(80*25);
      outb(0x3D4, 0x0F);
	   outb(0x3D5, (uint8_t) (terminal_index & 0xFF));
	   outb(0x3D4, 0x0E);
	   outb(0x3D5, (uint8_t) ((terminal_index >> 8) & 0xFF));
      
   }

   void terminal_clear(void) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      for(unsigned i = 0; i < 80*25; i++) {
         terminal_buffer[i] = entry(' ', colour(15, 0));
      }

      terminal_setcursor(80);
      return;
   }

   void terminal_write(char* str) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      int i = 0;
      while(str[i] != '\0') {
         terminal_buffer[terminal_index] = entry(str[i++], colour(15, 0));

         terminal_setcursor(terminal_index+1);
         //if(terminal_index > 80*25)
         //   terminal_index = 0;
      }
   }

   void terminal_writeat(char* str, int at) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      int i = 0;
      while(str[i] != '\0') {
         terminal_buffer[at++] = entry(str[i++], colour(15, 0));
      }
   }

   void terminal_writenumat(int num, int at) {
      if(num == 0) {
         char out[2] = "0";
         terminal_writeat(out, at);
         return;
      }

      bool negative = num < 0;
      if(negative)
         num = -num;

      // get number length in digits
      int tmp = num;
      int length = 0;
      while(tmp > 0) {
         length++;
         tmp/=10;
      }

      char out[20]; // allocate more memory than required for int's maxvalue
      
      out[length] = '\0';

      for(int i = 0; i < length; i++) {
         out[length-i-1] = '0' + num%10;
         num/=10;
      }

      terminal_writeat(out, at);
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