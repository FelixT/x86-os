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

bool mouse_held = false;

typedef struct gui_window_t {
   char title[20];
   int x;
   int y;
   int width;
   int height;
   char text_buffer[40];
   int text_index;
   int text_x;
   int text_y;
   bool needs_redraw;
   bool active;
   bool minimised;
   int toolbar_pos;
} gui_window_t;

gui_window_t gui_windows[4];
int gui_selected_window = 0;

// colours: https://www.fountainware.com/EXPL/vga_color_palettes.htm , 8bit

uint8_t cursor_buffer[7*5]; // store whats behind cursor so it can be restored

extern bool strcmp(char* str1, char* str2);

void strcpy(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      dest[i] = src[i];
      i++;
   }
}

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

   if(gui_selected_window < 0)
      return;

   gui_window_t *selected = &gui_windows[gui_selected_window];
   if(c == '\n') {
      selected->text_x = 0;
      selected->text_y += 8;
      return;
   }

   // overflow
   if(selected->text_x + 6 >= selected->width) {
      gui_drawcharat('-', colour, selected->x + selected->text_x-2, selected->y + selected->text_y);
      selected->text_x = 0;
      selected->text_y += 8;
   }

   gui_drawcharat(c, colour, selected->x + selected->text_x, selected->y + selected->text_y);
   selected->text_x+=6;

   if(gui_text_y > (int)gui_height) {
      selected->text_y = 12;
      selected->text_x = 0;
      gui_clear(3);
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

void gui_window_init(gui_window_t *window) {
   strcpy(window->title, "TERMINAL");
   window->x = 15;
   window->y = 15;
   window->width = 120;
   window->height = 120;
   window->text_buffer[0] = '\0';
   window->text_index = 0;
   window->text_x = 0;
   window->text_y = 12;
   window->needs_redraw = true;
   window->active = false;
   window->minimised = true;
}

void gui_init(void) {
   videomode = 1;

   gui_clear(3);

   for(int i = 0; i < 4; i++) {
      gui_window_init(&gui_windows[i]);
   }

   gui_windows[3].active = true;
   gui_windows[3].minimised = false;
   gui_selected_window = 3;
}

void gui_drawwindow(gui_window_t *window);
void gui_draw(void) {
   //gui_clear(3);
   
   // make sure to draw selected last
   for(int i = 3; i >= 0; i--) {
      if(i != gui_selected_window)
         gui_drawwindow(&gui_windows[i]);
   }
   if(gui_selected_window >= 0)
      gui_drawwindow(&gui_windows[gui_selected_window]);

   // draw toolbar
   gui_drawrect(0x07, 0, gui_height-12, gui_width, 20);

   int toolbarPos = 0;
   for(int i = 0; i < 4; i++) {
      if(gui_windows[i].minimised) {
         gui_drawrect(4, toolbarPos*18+2, gui_height-10, 16, 8);
         gui_drawcharat(gui_windows[i].title[0], 15, toolbarPos*18+2, gui_height-10);
         gui_windows[i].toolbar_pos = toolbarPos;
         toolbarPos++;
      }
   }
}

void gui_keypress(char key) {
   if(((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')) || (key == ' ')) {

      // write to current window
      if(gui_selected_window >= 0) {
         gui_window_t *selected = &gui_windows[gui_selected_window];
         if(selected->text_index < 40-1) {
            selected->text_buffer[selected->text_index] = key;
            selected->text_buffer[selected->text_index+1] = '\0';
            selected->text_index++;
            selected->text_x = (selected->text_index)*6;
         }
      }

      gui_draw();
   }
}

void mouse_enable();

void gui_checkcmd() {
   char *command = gui_windows[gui_selected_window].text_buffer;

   if(strcmp(command, "CLEAR")) {
      gui_windows[gui_selected_window].text_y = 12;
      gui_clear(3);
      gui_draw();
   }
   if(strcmp(command, "MOUSE")) {
      mouse_enable();
   }
   if(strcmp(command, "INT")) {
      char* test = "ABCDEF";

      asm(
         "int $0x30"
         :: "a" (1), "b" (test) // put operands in eax, ebx
      );
   }
   if(strcmp(command, "INT2")) {
      asm(
         "int $0x30"
         :: "a" (2), "b" (428) // put operands in eax, ebx
      );
   }
   if(strcmp(command, "PROG1")) {
      int progAddr = 15000+0x7c00;
      asm(
         "call *%0"
         : : "r" (progAddr)
      );
   }
}

void gui_return() {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];

      gui_drawchar('\n', 0);

      gui_writestrat(selected->text_buffer, 1, selected->x + selected->text_x + 1, selected->y + selected->text_y);
      
      gui_drawchar('\n', 0);

      gui_checkcmd();

      gui_drawchar('\n', 0);

      selected->text_index = 0;
      selected->text_buffer[selected->text_index] = '\0';
   }

   //gui_draw();
}

void gui_backspace() {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->text_index > 0) {
         selected->text_index--;
         selected->text_x-=6;
         selected->text_buffer[selected->text_index] = '\0';
      }
   }


   gui_draw();
}

void gui_drawwindow(gui_window_t *window) {
   if(window->minimised)
      return;

   int bg = 15;
   if(!window->active)
      bg = 7;

   if(window->needs_redraw) {
      // background
      gui_drawrect(bg, window->x, window->y, window->width, window->height);

      // titlebar
      gui_drawrect(7, window->x+1, window->y+1, window->width-2, 10);
      gui_writestrat(window->title, 0, window->x+2, window->y+2);
      // titlebar buttons
      gui_drawcharat('x', 0, window->x+window->width-8, window->y+2);
      gui_drawcharat('-', 0, window->x+window->width-16, window->y+2);

      window->needs_redraw = false;
   }

   // text content
   gui_drawrect(bg, window->x+1, window->y+window->text_y, window->width-2, 7);
   gui_writestrat(window->text_buffer, 0, window->x + 1, window->y + window->text_y);

   // prompt
   gui_drawcharat('_', 0, window->x + window->text_x + 1, window->y + window->text_y);
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

void gui_cursor_save_bg() {
   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;
   for(int y = gui_mouse_y; y < gui_mouse_y + 7; y++) {
      for(int x = gui_mouse_x; x < gui_mouse_x + 5; x++) {
         cursor_buffer[(y-gui_mouse_y)*5+(x-gui_mouse_x)] = terminal_buffer[y*(int)gui_width+x];
      }
   }
}

void gui_cursor_restore_bg(int old_x, int old_y) {
   uint8_t *terminal_buffer = (uint8_t*) 0xA0000;
   for(int y = old_y; y < old_y + 7; y++) {
      for(int x = old_x; x < old_x + 5; x++) {
         terminal_buffer[y*(int)gui_width+x] = cursor_buffer[(y-old_y)*5+(x-old_x)];
      }
   }
}

void mouse_update(int relX, int relY) {
   int old_x = gui_mouse_x;
   int old_y = gui_mouse_y;

   gui_mouse_x += relX;
   gui_mouse_y -= relY;

   gui_mouse_x %= gui_width;
   gui_mouse_y %= gui_height;

   gui_cursor_restore_bg(old_x, old_y); // restore pixels under old cursor location

   gui_cursor_save_bg(); // save pixels at new cursor location

   // draw cursor
   gui_drawcharat(27, 0, gui_mouse_x, gui_mouse_y); // outline
   gui_drawcharat(28, 15, gui_mouse_x, gui_mouse_y); // fill
}

void mouse_leftclick(int relX, int relY) {
   // dragging windows
   if(mouse_held && gui_selected_window >= 0) {
      gui_window_t *window = &gui_windows[gui_selected_window];
      if(window->active) {
         window->x += relX;
         window->y -= relY;
         if(window->x < 0)
            window->x = 0;
         if(window->x + window->width > (int)gui_width)
            window->x = gui_width - window->width;
         if(window->y < 0)
            window->y = 0;
         if(window->y + window->height > (int)gui_height)
            window->y = gui_height - window->height;

         window->needs_redraw = true;
         gui_draw();
      }
      return;
   }
   
   mouse_held = true;

   gui_selected_window = -1;

   for(int i = 0; i < 4; i++) {
      gui_window_t *window = &gui_windows[i];
      // check if clicked on a window
      if(window->minimised) {
         if(gui_mouse_x >= window->toolbar_pos*18+2 && gui_mouse_x <= window->toolbar_pos*18+2+16
            && gui_mouse_y >= (int)gui_height-10 && gui_mouse_y <= (int)gui_height-1) {
               window->minimised = false;
               window->active = true;
               window->needs_redraw = true;
               gui_selected_window = i;
               break;
            }
      }
      
      else if(gui_mouse_x >= window->x && gui_mouse_x <= window->x + window->width
         && gui_mouse_y >= window->y && gui_mouse_y <= window->y + window->height) {
            window->active = true;
            window->needs_redraw = true;
            gui_selected_window = i;
            break;
      }
   }

   for(int i = 0; i < 4; i++) {
      if(i != gui_selected_window) {
         gui_window_t *window = &gui_windows[i];
         window->active = false;
         window->needs_redraw = true;
      }
   }

   gui_draw();

}

void mouse_leftrelease() {
   if(mouse_held) {
      // redraw all
      for(int i = 0; i < 4; i++) {
         gui_window_t *window = &gui_windows[i];
         window->needs_redraw = true;
      }
      gui_clear(3);
      gui_draw();
      gui_cursor_save_bg();
   }

   mouse_held = false;
}