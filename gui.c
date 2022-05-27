#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern int videomode;

int gui_text_x = 0;
int gui_text_y = 20;

int gui_mouse_x = 0;
int gui_mouse_y = 0;

const size_t gui_width = 320;
const size_t gui_height = 200;

// colours: https://www.fountainware.com/EXPL/vga_color_palettes.htm , 8bit

int gui_cmd_index = 0;
char gui_cmd_buffer[40];

uint8_t cursor_buffer[7*5]; // store whats behind cursor so it can be restored

extern bool strcmp(char* str1, char* str2);

uint8_t gui_colour8bit(uint8_t row, uint8_t col) {
   return (row << 4) | col;
}

void gui_drawrect(uint8_t colour, int x, int y, int width, int height) {
   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;
   for(int yi = y; yi < y+height; yi++) {
      for(int xi = x; xi < x+width; xi++) {
         terminal_buffer[yi*(int)gui_width+xi] = colour;
      }
   }
   return;
}

void gui_clear(uint8_t colour) {
   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;
   for(int y = 0; y < (int)gui_height; y++) {
      for(int x = 0; x < (int)gui_width; x++) {
         terminal_buffer[y*(int)gui_width+x] = colour;
      }
   }
   return;
}

extern void getFontLetter(char c, int* dest);

int font_letter[35];

void gui_drawcharat(char c, int colour, int x, int y) {
   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;
   // 7*5
   
   getFontLetter(c, font_letter);

   int i = 0;      
   for(int yi = y; yi < y+7; yi++) {
      for(int xi = x; xi < x+5; xi++) {
         if(font_letter[i] == 1)
            terminal_buffer[yi*(int)gui_width+xi] = colour;
         //else
            //terminal_buffer[yi*(int)gui_width+xi] = 0;
         i++;
      }
   }
}

void gui_drawchar(char c, int colour) {
      gui_drawcharat(c, colour, gui_text_x, gui_text_y);
      gui_text_x+=6;
      if(gui_text_x >= (int)gui_width) {
         gui_text_x = 0;
         gui_text_y+=8;
      }
}

void gui_writestr(char *c, int colour) {
   int i = 0;
   while(c[i] != '\0')
      gui_drawchar(c[i++], colour);
}

void gui_writestrat(char *c, int colour, int x, int y) {
   int i = 0;
   while(c[i] != '\0') {
      gui_drawcharat(c[i++], colour, x, y);
      x+=6;
   }
}

extern void terminal_numtostr(int num, char *out);

void gui_writenum(int num, int colour) {
   if(num < 0)
      gui_drawchar('-', colour);

   char out[20];
   terminal_numtostr(num, out);
   gui_writestr(out, colour);
}

void gui_writenumat(int num, int colour, int x, int y) {
   char out[20];
   terminal_numtostr(num, out);
   gui_writestrat(out, colour, x, y);
}

void gui_init(void) {
   videomode = 1;

   gui_clear(3);

   gui_cmd_buffer[0] = '\0';
}

void gui_draw(void) {
   //gui_clear(3);
   
   // draw cmd buffer
   gui_drawrect(3, 0, gui_text_y, gui_width, 7);
   gui_writestrat(gui_cmd_buffer, 0, 0, gui_text_y);

   // prompt
   gui_drawcharat('_', 0, gui_text_x, gui_text_y);

   //gui_writestrat("AA", 0, 0, 20);

   // draw toolbar
   gui_drawrect(0x07, 0, gui_height-12, gui_width, 20);
}

void gui_keypress(char key) {
   if(((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')) || (key == ' ')) {
      int i = gui_cmd_index%39;
      gui_cmd_buffer[i] = key;
      gui_cmd_buffer[i+1] = '\0';
      gui_cmd_index++;
      gui_text_x = (i+1)*6;
      gui_draw();
   }
}

void mouse_enable();

void gui_checkcmd() {
   if(strcmp(gui_cmd_buffer, "CLEAR")) {
      gui_text_y = 20;
      gui_clear(3);
      gui_draw();
   }
   if(strcmp(gui_cmd_buffer, "MOUSE")) {
      mouse_enable();
   }
}

void gui_return() {
   gui_text_x = 0;
   gui_text_y += 8;

   gui_writestrat(gui_cmd_buffer, 1, 0, gui_text_y);
   gui_text_y += 8;

   gui_checkcmd();

   gui_cmd_index = 0;
   gui_cmd_buffer[gui_cmd_index] = '\0';

   gui_draw();
}

void gui_backspace() {
   if(gui_cmd_index > 0) {
      gui_cmd_index--;
      gui_text_x-=6;
      gui_cmd_buffer[gui_cmd_index] = '\0';
   }

   gui_draw();
}

static inline void outb(uint16_t port, uint8_t val) {
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
   uint8_t ret;
   asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}

void mouse_enable() {
   // https://wiki.osdev.org/Mouse_Input
   // https://wiki.osdev.org/PS/2_Mouse
   // enable ps2 mouse
   
   outb(0x64, 0xA8);

   // enable irq12 interrupt by setting status
   outb(0x64, 0x20);
   unsigned char status = (inb(0x60) | 2);
   outb(0x64, 0x60);
   outb(0x60, status);

   // use default mouse settings
   outb(0x64, 0xD4);
   outb(0x60, 0xF6);
   inb(0x60);

   // enable
   outb(0x64, 0xD4);
   outb(0x60, 0xF4);
   inb(0x60);
}

void mouse_update(int relX, int relY) {
   int old_x = gui_mouse_x;
   int old_y = gui_mouse_y;

   gui_mouse_x += relX;
   gui_mouse_y -= relY;

   gui_mouse_x %= gui_width;
   gui_mouse_y %= gui_height;

   //gui_drawrect(5, gui_mouse_x, gui_mouse_y, 2, 2);

   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;

   // restore 
   for(int y = old_y; y < old_y + 7; y++) {
      for(int x = old_x; x < old_x + 5; x++) {
         terminal_buffer[y*(int)gui_width+x] = cursor_buffer[(y-old_y)*5+(x-old_x)];
      }
   }

   // save

   for(int y = gui_mouse_y; y < gui_mouse_y + 7; y++) {
      for(int x = gui_mouse_x; x < gui_mouse_x + 5; x++) {
         cursor_buffer[(y-gui_mouse_y)*5+(x-gui_mouse_x)] = terminal_buffer[y*(int)gui_width+x];
      }
   }

   // draw cursor
   gui_drawcharat(27, 0, gui_mouse_x, gui_mouse_y); // outline
   gui_drawcharat(28, 15, gui_mouse_x, gui_mouse_y); // fill

   //gui_writenum(relY, 5);
   //gui_drawchar(' ', 5);
}