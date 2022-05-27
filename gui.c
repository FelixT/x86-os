#include <stdint.h>
#include <stddef.h>

extern int videomode;

int gui_index = 0;
int gui_text_x = 0;
int gui_text_y = 8;

const size_t gui_width = 320;
const size_t gui_height = 200;

// colours: https://www.fountainware.com/EXPL/vga_color_palettes.htm , 8bit

int gui_cmd_index = 0;
char gui_cmd_buffer[40];

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
   gui_clear(3);

   gui_cmd_buffer[0] = '\0';
}

void gui_draw(void) {
   //gui_clear(3);
   videomode = 1;
   gui_drawrect(0x0F, gui_index%(gui_width*gui_height/2), 50, 10, 10);

   // draw cmd buffer
   //gui_drawrect(3, 0, 8, 320, 10);
   //gui_writestrat("T", 0, 0, 20);
   gui_writestrat(gui_cmd_buffer, 0, 0, 20);

   //gui_writestrat("AA", 0, 0, 20);

   // draw toolbar
   gui_drawrect(0x07, 0, gui_height-12, gui_width, 20);

   gui_index=(gui_index+1)%100;
}

void gui_keypress(char key) {
   int i = gui_cmd_index%39;
   gui_cmd_buffer[i] = key;
   gui_cmd_buffer[i+1] = '\0';
   gui_cmd_index++;
}