#include <stdint.h>
#include <stddef.h>

extern "C" {
   size_t terminal_index;
   int gui_index = 0;

   // video mode = 320x200 256 color graphics 
   const size_t gui_width = 320;
   const size_t gui_height = 200;

   int videomode; // 0 = cli, 1 = gui

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

   void terminal_scroll(void) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;

      for(int i = 80; i < 80*25; i++)
         terminal_buffer[i-80] = terminal_buffer[i];

      for(int i = 80*24; i < 80*25; i++)
         terminal_buffer[i] = entry(' ', colour(15, 0));
   }

   void terminal_write(char* str) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      int i = 0;
      while(str[i] != '\0') {
         if(str[i] == '\n') {
            if(terminal_index >= 80*24) {
               terminal_scroll();
               terminal_setcursor(80*24);
            } else {
               terminal_setcursor(terminal_index + (80-(terminal_index%80)));
            }
            i++;
         } else {
            terminal_buffer[terminal_index] = entry(str[i++], colour(15, 0));

            terminal_setcursor(terminal_index+1);
         }
      }
   }

   void terminal_clear(void) {
      videomode = 0;
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      for(unsigned i = 0; i < 80*25; i++) {
         terminal_buffer[i] = entry(' ', colour(15, 0));
      }

      terminal_setcursor(0);
      return;
   }

   void terminal_prompt(void) {
      terminal_write("\n>");
   }

   void terminal_writeat(char* str, int at) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      int i = 0;
      while(str[i] != '\0') {
         terminal_buffer[at++] = entry(str[i++], colour(15, 0));
      }
   }

   void terminal_backspace(void) {
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      terminal_index-=1;
      terminal_buffer[terminal_index] = entry(' ', colour(15, 0));
      terminal_setcursor(terminal_index);
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
}