#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "gui.h"

extern int videomode;

int gui_mouse_x = 0;
int gui_mouse_y = 0;

int gui_bg = 3;

size_t gui_width = 320;
size_t gui_height = 200;

bool mouse_enabled = false;
bool mouse_held = false;

gui_window_t gui_windows[NUM_WINDOWS];
int gui_selected_window = 0;

// colours: https://www.fountainware.com/EXPL/vga_color_palettes.htm , 8bit

uint32_t framebuffer;

uint8_t cursor_buffer[FONT_WIDTH*FONT_HEIGHT]; // store whats behind cursor so it can be restored

extern bool strcmp(char* str1, char* str2);
extern int strlen(char* str);

void strcpy(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

bool strsplit(char* dest1, char* dest2, char* src, char splitat) {
   int i = 0;

   while(src[i] != splitat) {
      if(src[i] == '\0')
         return false;

      dest1[i] = src[i];
      i++;
   }
   dest1[i] = '\0';
   i++;

   int start = i;
   while(src[i] != '\0') {
      dest2[i - start] = src[i];
      i++;
   }
   dest2[i - start] = '\0';

   return true;
}

bool strstartswith(char* src, char* startswith) {
   int i = 0;
   while(startswith[i] != '\0') {
      if(src[i] != startswith[i])
         return false;
      i++;
   }
   return true;
}

uint8_t gui_colour8bit(uint8_t row, uint8_t col) {
   return (row << 4) | col;
}

void gui_drawrect(uint8_t colour, int x, int y, int width, int height) {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;
   for(int yi = y; yi < y+height; yi++) {
      for(int xi = x; xi < x+width; xi++) {
         terminal_buffer[yi*(int)gui_width+xi] = colour;
      }
   }
   return;
}

void gui_drawunfilledrect(uint8_t colour, int x, int y, int width, int height) {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;

   for(int xi = x; xi < x+width; xi++) // top
      terminal_buffer[y*(int)gui_width+xi] = colour;

   for(int xi = x; xi < x+width; xi++) // bottom
      terminal_buffer[(y+height-1)*(int)gui_width+xi] = colour;

   for(int yi = y; yi < y+height; yi++) // left
      terminal_buffer[(yi)*(int)gui_width+x] = colour;

   for(int yi = y; yi < y+height; yi++) // right
      terminal_buffer[(yi)*(int)gui_width+x+width-1] = colour;
}

void gui_drawdottedrect(uint8_t colour, int x, int y, int width, int height) {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;

   for(int xi = x; xi < x+width; xi++) // top
      if((xi%2) == 0)
         terminal_buffer[y*(int)gui_width+xi] = colour;

   for(int xi = x; xi < x+width; xi++) // bottom
      if((xi%2) == 0)
         terminal_buffer[(y+height-1)*(int)gui_width+xi] = colour;

   for(int yi = y; yi < y+height; yi++) // left
      if((yi%2) == 0)
      terminal_buffer[(yi)*(int)gui_width+x] = colour;

   for(int yi = y; yi < y+height; yi++) // right
      if((yi%2) == 0)
      terminal_buffer[(yi)*(int)gui_width+x+width-1] = colour;
}

void gui_drawline(uint8_t colour, int x, int y, bool vertical, int length) {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;
   if(vertical) {
      for(int yi = y; yi < y+length; yi++) {
         terminal_buffer[yi*(int)gui_width+x] = colour;
      }
   } else {
      for(int xi = x; xi < x+length; xi++) {
         terminal_buffer[y*(int)gui_width+xi] = colour;
      }
   }
}

void gui_clear(uint8_t colour) {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;
   for(int y = 0; y < (int)gui_height; y++) {
      for(int x = 0; x < (int)gui_width; x++) {
         terminal_buffer[y*(int)gui_width+x] = colour;
      }
   }
   return;
}

extern void getFontLetter(char c, int* dest);

int font_letter[FONT_WIDTH*FONT_HEIGHT];

void gui_drawcharat(char c, int colour, int x, int y) {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;
   
   getFontLetter(c, font_letter);

   int i = 0;      
   for(int yi = y; yi < y+FONT_HEIGHT; yi++) {
      for(int xi = x; xi < x+FONT_WIDTH; xi++) {
         if(font_letter[i] == 1)
            terminal_buffer[yi*(int)gui_width+xi] = colour;
         i++;
      }
   }
}

void gui_window_drawcharat(char c, int colour, int x, int y, int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   uint8_t *terminal_buffer = window->framebuffer;
   
   getFontLetter(c, font_letter);

   int i = 0;      
   for(int yi = y; yi < y+FONT_HEIGHT; yi++) {
      for(int xi = x; xi < x+FONT_WIDTH; xi++) {
         if(font_letter[i] == 1)
            terminal_buffer[yi*(int)window->width+xi] = colour;
         i++;
      }
   }
}

void gui_window_drawrect(uint8_t colour, int x, int y, int width, int height, int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   uint8_t *terminal_buffer = (uint8_t*)window->framebuffer;
   for(int yi = y; yi < y+height; yi++) {
      for(int xi = x; xi < x+width; xi++) {
         terminal_buffer[yi*(int)window->width+xi] = colour;
      }
   }
   return;
}

void gui_window_writestrat(char *c, int colour, int x, int y, int windowIndex) {
   int i = 0;
   while(c[i] != '\0') {
      gui_window_drawcharat(c[i++], colour, x, y, windowIndex);
      x+=FONT_WIDTH+FONT_PADDING;
   }
}

void gui_window_scroll(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   uint8_t *terminal_buffer = window->framebuffer;

   int scrollY = FONT_HEIGHT+FONT_PADDING;
   for(int y = scrollY; y < window->height - TITLEBAR_HEIGHT; y++) {
      for(int x = window->x; x < window->x + window->width; x++) {
         terminal_buffer[(y-scrollY)*window->width+x] = terminal_buffer[y*(int)window->width+x];
      }
   }
   // clear bottom
   int newY = window->height - (scrollY + TITLEBAR_HEIGHT);
   gui_window_drawrect(15, 0, newY, window->width, scrollY, windowIndex);
   window->text_y = newY;
   window->text_x = FONT_PADDING;
}

void gui_window_drawchar(char c, int colour, int windowIndex) {

   gui_window_t *selected = &gui_windows[windowIndex];
      
   //if(selected->minimised || !selected->active)
   //   return;

   if(c == '\n') {
      selected->text_x = FONT_PADDING;
      selected->text_y += FONT_HEIGHT + FONT_PADDING;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
         gui_window_scroll(windowIndex);
      }

      return;
   }

   // x overflow
   if(selected->text_x + FONT_WIDTH + FONT_PADDING >= selected->width) {
      gui_window_drawcharat('-', colour, selected->text_x-2, selected->text_y, windowIndex);
      selected->text_x = FONT_PADDING;
      selected->text_y += FONT_HEIGHT + FONT_PADDING;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
         gui_window_scroll(windowIndex);
      }
   }

   gui_window_drawcharat(c, colour, selected->text_x, selected->text_y, windowIndex);
   selected->text_x+=FONT_WIDTH+FONT_PADDING;

   if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
      gui_window_scroll(windowIndex);
   }

   selected->needs_redraw = true;

}

void gui_window_writestr(char *c, int colour, int windowIndex) {
   int i = 0;
   while(c[i] != '\0')
      gui_window_drawchar(c[i++], colour, windowIndex);
}

extern void terminal_numtostr(int num, char *out);
extern void terminal_uinttostr(uint32_t num, char *out);
void gui_window_writenum(int num, int colour, int windowIndex) {
   if(num < 0)
      gui_window_drawchar('-', colour, windowIndex);

   char out[20];
   terminal_numtostr(num, out);
   gui_window_writestr(out, colour, windowIndex);
}

void gui_window_clearbuffer(gui_window_t *window) {
   for(int i = 0; i < window->width*window->height; i++) {
      window->framebuffer[i] = 15;
   }
}

void gui_window_init(gui_window_t *window) {
   strcpy(window->title, "TERMINAL");
   window->x = 8;
   window->y = 8;
   window->width = 300;
   window->height = 180;
   window->text_buffer[0] = '\0';
   window->text_index = 0;
   window->text_x = FONT_PADDING;
   window->text_y = FONT_PADDING;
   window->needs_redraw = true;
   window->active = false;
   window->minimised = true;
   window->dragged = false;
   
   window->framebuffer = malloc(window->width*(window->height-TITLEBAR_HEIGHT));
   gui_window_clearbuffer(window);
}

void gui_redrawall();
void gui_drawchar(char c, int colour) {

   if(gui_selected_window < 0)
      return;

   gui_window_drawchar(c, colour, gui_selected_window);

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
      x+=FONT_WIDTH+FONT_PADDING;
   }
}

void gui_writenum(int num, int colour) {
   if(num < 0)
      gui_drawchar('-', colour);

   char out[20];
   terminal_numtostr(num, out);
   gui_writestr(out, colour);
}

void gui_writeuint(uint32_t num, int colour) {
   char out[20];
   terminal_uinttostr(num, out);
   gui_writestr(out, colour);
}

void gui_writenumat(int num, int colour, int x, int y) {
   char out[20];
   terminal_numtostr(num, out);
   gui_writestrat(out, colour, x, y);
}

void gui_writeuintat(uint32_t num, int colour, int x, int y) {
   char out[20];
   terminal_uinttostr(num, out);
   gui_writestrat(out, colour, x, y);
}

extern vbe_mode_info_t vbe_mode_info_structure;
void gui_init(void) {
   videomode = 1;

   gui_width = vbe_mode_info_structure.width;
   gui_height = vbe_mode_info_structure.height;
   framebuffer = vbe_mode_info_structure.framebuffer;

   // reserve framebuffer memory so malloc can't assign it
   memory_reserve(framebuffer, (int)gui_width*(int)gui_height);
   
   gui_clear(gui_bg);

   // init windows
   for(int i = 0; i < NUM_WINDOWS; i++) {
      gui_window_init(&gui_windows[i]);
   }

   gui_windows[NUM_WINDOWS-1].active = true;
   gui_windows[NUM_WINDOWS-1].minimised = false;
   gui_selected_window = NUM_WINDOWS-1;
}

void gui_window_draw(int windowIndex);
void gui_draw(void) {
   //gui_clear(3);
   
   // make sure to draw selected last
   for(int i = NUM_WINDOWS-1; i >= 0; i--) {
      if(i != gui_selected_window)
         gui_window_draw(i);
   }
   if(gui_selected_window >= 0)
      gui_window_draw(gui_selected_window);

   // draw toolbar
   gui_drawrect(0x07, 0, gui_height-TOOLBAR_HEIGHT, gui_width, TOOLBAR_HEIGHT);

   int toolbarPos = 0;
   // padding = 2px
   for(int i = 0; i < 4; i++) {
      if(gui_windows[i].minimised) {
         gui_drawrect(21, TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING), gui_height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING), TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT);
         gui_drawcharat(gui_windows[i].title[0], 15, TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING), gui_height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING));
         gui_windows[i].toolbar_pos = toolbarPos;
         toolbarPos++;
      }
   }
}

void gui_cursor_draw();
void gui_cursor_save_bg();
void gui_redrawall() {
   for(int i = 0; i < NUM_WINDOWS; i++) {
      gui_window_t *window = &gui_windows[i];
      window->needs_redraw = true;
   }
   gui_clear(gui_bg);
   gui_draw();
   gui_cursor_save_bg();
   if(mouse_enabled) gui_cursor_draw();
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
            selected->text_x = (selected->text_index)*(FONT_WIDTH+FONT_PADDING);
         }

         gui_window_draw(gui_selected_window);
      }

   }
}

void mouse_enable();

extern void ata_identify(bool primaryBus, bool masterDrive);
extern void ata_read(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf);

void gui_checkcmd(void *regs) {
   gui_window_t *selected = &gui_windows[gui_selected_window];
   char *command = selected->text_buffer;

   if(strcmp(command, "CLEAR")) {
      gui_window_clearbuffer(selected);
      selected->text_x = FONT_PADDING;
      selected->text_y = FONT_PADDING;
      selected->text_index = 0;
      command[0] = '\0';
      gui_redrawall();
      selected->needs_redraw = true;
      return;
   }
   else if(strcmp(command, "MOUSE")) {
      mouse_enable();
   }
   else if(strcmp(command, "INT")) {
      char* test = "ABCDEF";

      asm(
         "int $0x30"
         :: "a" (1), "b" (test) // put operands in eax, ebx
      );
   }
   else if(strcmp(command, "INT2")) {
      asm(
         "int $0x30"
         :: "a" (2), "b" (428) // put operands in eax, ebx
      );
   }
   else if(strcmp(command, "INT3")) {
      asm(
         "int $0x30"
         :: "a" (3), "d" (428) // put operands in eax, ebx
      );
   }
   else if(strcmp(command, "INT4")) {
      asm(
         "int $0x30"
         :: "a" (4), "d" (0) // put operands in eax, ebx
      );
   }
   else if(strcmp(command, "INT5")) {
      asm(
         "int $0x30"
         :: "a" (5), "d" (0) // put operands in eax, ebx
      );
   }
   else if(strcmp(command, "TASKS")) {
      tasks_init(regs);
   }
   else if(strcmp(command, "VIEWTASKS")) {
      task_state_t *tasks = gettasks();
      for(int i = 0; i < TOTAL_TASKS; i++) {
         gui_drawchar('\n', 0);

         gui_writenum(i, 0);
         gui_writestr(": ", 0);

         if(tasks[i].enabled)
            gui_writestr("ENABLED", 0);
         else
            gui_writestr("DISABLED", 0);
      }
   }
   else if(strcmp(command, "PROG1")) {
      int progAddr = 31000+0x7c00+512;
      create_task_entry(1, progAddr);
      launch_task(1, regs);
   }
   else if(strcmp(command, "PROG2")) {
      int progAddr = 31000+0x7c00+512*2;
      create_task_entry(2, progAddr);
      launch_task(2, regs);
   }
   else if(strcmp(command, "TEST")) {
      extern uint16_t gdt_tss;
      gui_writenum(gdt_tss, 0);
   }
   else if(strcmp(command, "ATA")) {
      ata_identify(true, true); 
   }
   else if(strstartswith(command, "READ")) {
      char arg[5];
      if(strsplit(arg, arg, command, ' ')) {
         // convert str to int
         uint32_t lba = 0;
         int power = 1;
         for(int i = strlen(arg) - 1; i >= 0 ; i--) {
            if(arg[i] >= '0' && arg[i] <= '9') {
               lba += power*(arg[i]-'0');
               power *= 10;
            }
         }
         gui_writeuint(lba, 0);
         gui_writestr("\n", 0);
         uint16_t buf[256];
         ata_read(true, true, lba, buf);
         gui_writenum(buf[255], 0);

      }
   }
   else if(strstartswith(command, "BG")) {
      char arg[5];
      if(strsplit(arg, arg, command, ' ')) {
         // convert str to int
         int bg = 0;
         int power = 1;
         for(int i = strlen(arg) - 1; i >= 0 ; i--) {
            if(arg[i] >= '0' && arg[i] <= '9') {
               bg += power*(arg[i]-'0');
               power *= 10;
            }

         }
         gui_writenum(bg, 0);
         gui_bg = bg;
         gui_redrawall();
      }
   }
   else if(strstartswith(command, "MEM")) {
      char arg[5];
      if(strsplit(arg, arg, command, ' ')) {
         int offset = (arg[0]-'0')*400;

         for(int i = offset; i < offset+400 && i < 2048; i++) {
            if(memory_status[i].allocated)
               gui_writenum(1, 0);
            else
               gui_writenum(0, 0);
         }
      } else {
         int used = 0;
         for(int i = 0; i < 2048; i++) {
            if(memory_status[i].allocated) used++;
         }
         gui_writenum(used, 0);
         gui_drawchar('/', 0);
         gui_writenum(2048, 0);
         gui_writestr(" ALLOCATED", 0);
      }
   }
   else {
      gui_writestr(": UNRECOGNISED", 4);
   }
   gui_drawchar('\n', 0);
}

void gui_return(void *regs) {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];

      // write cmd to window framebuffer
      gui_window_writestrat(selected->text_buffer, 0, 1, selected->text_y, gui_selected_window);
      // write prompt to framebuffer
      //gui_window_drawcharat('_', 0, window->text_x + 1, window->text_y, windowIndex);
      
      gui_drawchar('\n', 0);

      gui_drawchar('\'', 1);
      gui_writestr(selected->text_buffer, 1);
      gui_drawchar('\'', 1);
      
      gui_checkcmd(regs);

      selected->text_index = 0;
      selected->text_buffer[selected->text_index] = '\0';

      gui_window_draw(gui_selected_window);
   }
}

void gui_backspace() {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->text_index > 0) {
         selected->text_index--;
         selected->text_x-=FONT_WIDTH+FONT_PADDING;
         selected->text_buffer[selected->text_index] = '\0';
      }

      gui_window_draw(gui_selected_window);
   }
}

void gui_window_draw(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   if(window->minimised || window->dragged)
      return;

   int bg = 15;

   if(window->needs_redraw) {
      // background
      //gui_drawrect(bg, window->x, window->y, window->width, window->height);
      gui_drawunfilledrect(bg, window->x, window->y, window->width, window->height);

      // titlebar
      gui_drawrect(7, window->x+1, window->y+1, window->width-2, TITLEBAR_HEIGHT);
      gui_writestrat(window->title, 0, window->x+2, window->y+2);
      // titlebar buttons
      gui_drawcharat('x', 0, window->x+window->width-(FONT_WIDTH+3), window->y+2);
      gui_drawcharat('-', 0, window->x+window->width-(FONT_WIDTH+3)*2, window->y+2);

      if(!window->active)
         gui_drawdottedrect(0, window->x, window->y, window->width, window->height);

      uint8_t *terminal_buffer = (uint8_t*)framebuffer;

      // draw window content/framebuffer
      for(int y = 0; y < window->height - TITLEBAR_HEIGHT; y++) {
         for(int x = 0; x < window->width; x++) {
            // ignore pixels covered by the currently selected window
            int screenY = (window->y + y + TITLEBAR_HEIGHT);
            int screenX = (window->x + x);
            if(windowIndex != gui_selected_window
            && screenX >= gui_windows[gui_selected_window].x
            && screenX < gui_windows[gui_selected_window].x + gui_windows[gui_selected_window].width
            && screenY >= gui_windows[gui_selected_window].y
            && screenY < gui_windows[gui_selected_window].y + gui_windows[gui_selected_window].height)
               continue;

            int index = screenY*gui_width + screenX;
            int w_index = y*window->width + x;
            terminal_buffer[index] = window->framebuffer[w_index];
         }
      }
      window->needs_redraw = false;
   }

   if(windowIndex == gui_selected_window) {

      // current text content/buffer
      gui_drawrect(bg, window->x+1, window->y+window->text_y+TITLEBAR_HEIGHT, window->width-2, FONT_HEIGHT);
      gui_writestrat(window->text_buffer, 0, window->x + 1, window->y + window->text_y+TITLEBAR_HEIGHT);
      // prompt
      gui_drawcharat('_', 0, window->x + window->text_x + 1, window->y + window->text_y+TITLEBAR_HEIGHT);

      // drop shadow if selected
      gui_drawline(24, window->x+window->width, window->y, true, window->height);
      gui_drawline(24, window->x, window->y+window->height, false, window->width);
   }
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

   gui_cursor_save_bg();
   mouse_enabled = true;
}

void gui_cursor_save_bg() {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;
   for(int y = gui_mouse_y; y < gui_mouse_y + FONT_HEIGHT; y++) {
      for(int x = gui_mouse_x; x < gui_mouse_x + FONT_WIDTH; x++) {
         cursor_buffer[(y-gui_mouse_y)*FONT_WIDTH+(x-gui_mouse_x)] = terminal_buffer[y*(int)gui_width+x];
      }
   }
}

void gui_cursor_restore_bg(int old_x, int old_y) {
   uint8_t *terminal_buffer = (uint8_t*) framebuffer;
   for(int y = old_y; y < old_y + FONT_HEIGHT; y++) {
      for(int x = old_x; x < old_x + FONT_WIDTH; x++) {
         terminal_buffer[y*(int)gui_width+x] = cursor_buffer[(y-old_y)*FONT_WIDTH+(x-old_x)];
      }
   }
}

void gui_cursor_draw() {
   gui_drawcharat(27, 0, gui_mouse_x, gui_mouse_y); // outline
   gui_drawcharat(28, 15, gui_mouse_x, gui_mouse_y); // fill

}

void mouse_update(int relX, int relY) {
   int old_x = gui_mouse_x;
   int old_y = gui_mouse_y;

   gui_mouse_x += relX;
   gui_mouse_y -= relY;

   if(gui_mouse_x > (int)gui_width)
      gui_mouse_x %= gui_width;

   if(gui_mouse_y > (int)gui_height)
      gui_mouse_y %= gui_height;

   if(gui_mouse_x < 0)
      gui_mouse_x = gui_width - gui_mouse_x;

   if(gui_mouse_y < 0)
      gui_mouse_y = gui_height - gui_mouse_y;

   gui_cursor_restore_bg(old_x, old_y); // restore pixels under old cursor location

   gui_cursor_save_bg(); // save pixels at new cursor location

   gui_cursor_draw();
}

bool mouse_clicked_on_window(int index) {
   gui_window_t *window = &gui_windows[index];
   // clicked on window's icon in toolbar
   if(window->minimised) {
         if(gui_mouse_x >= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING) && gui_mouse_x <= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING)+TOOLBAR_ITEM_WIDTH
            && gui_mouse_y >= (int)gui_height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING) && gui_mouse_y <= (int)gui_height-TOOLBAR_PADDING) {
               window->minimised = false;
               window->active = true;
               window->needs_redraw = true;
               gui_selected_window = index;
               return true;
            }
      }

   else if(gui_mouse_x >= window->x && gui_mouse_x <= window->x + window->width
      && gui_mouse_y >= window->y && gui_mouse_y <= window->y + window->height) {

         int relX = gui_mouse_x - window->x;
         int relY = gui_mouse_y - window->y;

         // minimise
         if(relY < 10 && relX > window->width - (FONT_WIDTH+3)*2 && relX < window->width - (FONT_WIDTH+3))
            window->minimised = true;

         window->active = true;
         window->needs_redraw = true;
         gui_selected_window = index;
         return true;
   }

   return false;
}

void mouse_leftclick(int relX, int relY) {
   // dragging windows
   if(mouse_held) {
      if(gui_selected_window >= 0) {
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
            if(window->y + window->height > (int)gui_height - TOOLBAR_HEIGHT)
               window->y = gui_height - window->height - TOOLBAR_HEIGHT;

            window->dragged = true;
            window->needs_redraw = true;
            gui_drawdottedrect(15, window->x, window->y, window->width, window->height);
            //gui_draw();
         }
      }
      return;
   }
   
   mouse_held = true;

   // check if clicked on current window
   if(mouse_clicked_on_window(gui_selected_window)) {
      // 
   } else {
      // check other windows

      gui_selected_window = -1;
      for(int i = 0; i < NUM_WINDOWS; i++) {
         if(mouse_clicked_on_window(i))
            break;
      }
   
   }

   // make all other windows inactive
   for(int i = 0; i < NUM_WINDOWS; i++) {
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
      if(gui_selected_window >= 0)
         gui_windows[gui_selected_window].dragged = false;

      gui_redrawall();
   }

   mouse_held = false;
}