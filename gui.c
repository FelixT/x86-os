#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "gui.h"

extern int videomode;

int gui_mouse_x = 0;
int gui_mouse_y = 0;

uint16_t gui_bg;

size_t gui_width = 320;
size_t gui_height = 200;

bool mouse_enabled = false;
bool mouse_held = false;

gui_window_t gui_windows[NUM_WINDOWS];
int gui_selected_window = 0;

uint32_t framebuffer;

uint16_t cursor_buffer[FONT_WIDTH*FONT_HEIGHT]; // store whats behind cursor so it can be restored

extern bool strcmp(char* str1, char* str2);
extern int strlen(char* str);

void strtoupper(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      if(src[i] >= 'a' && src[i] <= 'z')
         dest[i] = src[i] + ('A' - 'a');
      else
         dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

void strcpy(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

void strcpy_fixed(char* dest, char* src, int length) {
   for(int i = 0; i < length; i++)
      dest[i] = src[i];
   dest[length] = '\0';
}

int stoi(char* str) {
   // convert str to int
   int out = 0;
   int power = 1;
   for(int i = strlen(str) - 1; i >= 0 ; i--) {
      if(str[i] >= '0' && str[i] <= '9') {
         out += power*(str[i]-'0');
         power *= 10;
      }
   }
   return out;
}

// split at first of char
bool strsplit(char* dest1, char* dest2, char* src, char splitat) {
   int i = 0;

   while(src[i] != splitat) {
      if(src[i] == '\0')
         return false;

      if(dest1 != NULL)
         dest1[i] = src[i];
      i++;
   }
   dest1[i] = '\0';
   i++;

   int start = i;
   while(src[i] != '\0') {
      if(dest2 != NULL)
         dest2[i - start] = src[i];
      i++;
   }
   if(dest2 != NULL)
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

uint16_t gui_rgb16(uint8_t r, uint8_t g, uint8_t b) {
   // 5r 6g 5b
   return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void gui_drawrect(uint16_t colour, int x, int y, int width, int height) {
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;
   for(int yi = y; yi < y+height; yi++) {
      for(int xi = x; xi < x+width; xi++) {
         terminal_buffer[yi*(int)gui_width+xi] = colour;
      }
   }
   return;
}

void gui_drawunfilledrect(uint16_t colour, int x, int y, int width, int height) {
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;

   for(int xi = x; xi < x+width; xi++) // top
      terminal_buffer[y*(int)gui_width+xi] = colour;

   for(int xi = x; xi < x+width; xi++) // bottom
      terminal_buffer[(y+height-1)*(int)gui_width+xi] = colour;

   for(int yi = y; yi < y+height; yi++) // left
      terminal_buffer[(yi)*(int)gui_width+x] = colour;

   for(int yi = y; yi < y+height; yi++) // right
      terminal_buffer[(yi)*(int)gui_width+x+width-1] = colour;
}

void gui_drawdottedrect(uint16_t colour, int x, int y, int width, int height) {
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;

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

void gui_drawline(uint16_t colour, int x, int y, bool vertical, int length) {
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;
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

void gui_clear(uint16_t colour) {
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;
   for(int y = 0; y < (int)gui_height; y++) {
      for(int x = 0; x < (int)gui_width; x++) {
         terminal_buffer[y*(int)gui_width+x] = colour;
      }
   }
   return;
}

extern void getFontLetter(char c, int* dest);

int font_letter[FONT_WIDTH*FONT_HEIGHT];

void gui_drawcharat(char c, uint16_t colour, int x, int y) {
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;

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

void gui_window_drawcharat(char c, uint16_t colour, int x, int y, int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   uint16_t *terminal_buffer = window->framebuffer;
   
   if(c >= 'a' && c <= 'z') c = (c - 'a') + 'A'; // convert to uppercase

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

void gui_window_drawrect(uint16_t colour, int x, int y, int width, int height, int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   uint16_t *terminal_buffer = (uint16_t*)window->framebuffer;
   for(int yi = y; yi < y+height; yi++) {
      for(int xi = x; xi < x+width; xi++) {
         terminal_buffer[yi*(int)window->width+xi] = colour;
      }
   }
   return;
}

void gui_window_writestrat(char *c, uint16_t colour, int x, int y, int windowIndex) {
   int i = 0;
   while(c[i] != '\0') {
      gui_window_drawcharat(c[i++], colour, x, y, windowIndex);
      x+=FONT_WIDTH+FONT_PADDING;
   }
}

void gui_window_scroll(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   uint16_t *terminal_buffer = window->framebuffer;

   int scrollY = FONT_HEIGHT+FONT_PADDING;
   for(int y = scrollY; y < window->height - TITLEBAR_HEIGHT; y++) {
      for(int x = window->x; x < window->x + window->width; x++) {
         terminal_buffer[(y-scrollY)*window->width+x] = terminal_buffer[y*(int)window->width+x];
      }
   }
   // clear bottom
   int newY = window->height - (scrollY + TITLEBAR_HEIGHT);
   gui_window_drawrect(COLOUR_WHITE, 0, newY, window->width, scrollY, windowIndex);
   window->text_y = newY;
   window->text_x = FONT_PADDING;
}

void gui_window_drawchar(char c, uint16_t colour, int windowIndex) {

   gui_window_t *selected = &gui_windows[windowIndex];
      
   //if(selected->minimised || !selected->active)
   //   return;

   if(c == '\n') {
      selected->text_x = FONT_PADDING;
      selected->text_y += FONT_HEIGHT + FONT_PADDING;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
         gui_window_scroll(windowIndex);
      }

      // immediately output each line
      selected->needs_redraw=true;
      gui_window_draw(windowIndex);

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

void gui_window_writestr(char *c, uint16_t colour, int windowIndex) {
   int i = 0;
   while(c[i] != '\0')
      gui_window_drawchar(c[i++], colour, windowIndex);
}

extern void terminal_numtostr(int num, char *out);
extern void terminal_uinttostr(uint32_t num, char *out);
void gui_window_writenum(int num, uint16_t colour, int windowIndex) {
   if(num < 0)
      gui_window_drawchar('-', colour, windowIndex);

   char out[20];
   terminal_numtostr(num, out);
   gui_window_writestr(out, colour, windowIndex);
}

void gui_window_clearbuffer(gui_window_t *window) {
   for(int i = 0; i < window->width*window->height; i++) {
      window->framebuffer[i] = COLOUR_WHITE;
   }
}

void gui_window_init(gui_window_t *window) {
   strcpy(window->title, " TERMINAL");
   window->x = 8;
   window->y = 8;
   window->width = 360;
   window->height = 240;
   window->text_buffer[0] = '\0';
   window->text_index = 0;
   window->text_x = FONT_PADDING;
   window->text_y = FONT_PADDING;
   window->needs_redraw = true;
   window->active = false;
   window->minimised = true;
   window->dragged = false;
   
   window->framebuffer = malloc(window->width*(window->height-TITLEBAR_HEIGHT)*2);
   gui_window_clearbuffer(window);
}

void gui_redrawall();
void gui_drawchar(char c, uint16_t colour) {

   if(gui_selected_window < 0)
      return;

   gui_window_drawchar(c, colour, gui_selected_window);

}

void gui_writestr(char *c, uint16_t colour) {
   int i = 0;
   while(c[i] != '\0')
      gui_drawchar(c[i++], colour);
}

void gui_writestrat(char *c, uint16_t colour, int x, int y) {
   int i = 0;
   while(c[i] != '\0') {
      gui_drawcharat(c[i++], colour, x, y);
      x+=FONT_WIDTH+FONT_PADDING;
   }
}

void gui_writenum(int num, uint16_t colour) {
   if(num < 0)
      gui_drawchar('-', colour);

   char out[20];
   terminal_numtostr(num, out);
   gui_writestr(out, colour);
}

extern void terminal_uinttohex(uint32_t num, char* out);
void gui_writeuint_hex(uint32_t num, uint16_t colour) {
   char out[20];
   terminal_uinttohex(num, out);
   gui_writestr(out, colour);
}

void gui_writeuint(uint32_t num, uint16_t colour) {
   char out[20];
   terminal_uinttostr(num, out);
   gui_writestr(out, colour);
}

void gui_writenumat(int num, uint16_t colour, int x, int y) {
   char out[20];
   terminal_numtostr(num, out);
   gui_writestrat(out, colour, x, y);
}

void gui_writeuintat(uint32_t num, uint16_t colour, int x, int y) {
   char out[20];
   terminal_uinttostr(num, out);
   gui_writestrat(out, colour, x, y);
}

extern vbe_mode_info_t vbe_mode_info_structure;
void gui_init(void) {
   videomode = 1;

   gui_bg = COLOUR_CYAN;

   gui_width = vbe_mode_info_structure.width;
   gui_height = vbe_mode_info_structure.height;
   framebuffer = vbe_mode_info_structure.framebuffer;

   // reserve framebuffer memory so malloc can't assign it
   memory_reserve(framebuffer, (int)gui_width*(int)gui_height);
   
   gui_clear(gui_bg);

   // init windows
   for(int i = 0; i < NUM_WINDOWS; i++) {
      gui_window_init(&gui_windows[i]);
      gui_windows[i].title[0] = i+'0';
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
   gui_drawrect(COLOUR_TOOLBAR, 0, gui_height-TOOLBAR_HEIGHT, gui_width, TOOLBAR_HEIGHT);

   int toolbarPos = 0;
   // padding = 2px
   for(int i = 0; i < 4; i++) {
      if(gui_windows[i].minimised) {
         gui_drawrect(COLOUR_TASKBAR_ENTRY, TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING), gui_height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING), TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT);
         gui_drawcharat(gui_windows[i].title[0], COLOUR_WHITE, TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING), gui_height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING));
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
   if(((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')) || (key == ' ')
   || (key == '/') || (key == '.')) {

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

extern void fat_setup();
extern void fat_read_dir(uint16_t clusterNo);
extern void fat_read_file(uint16_t clusterNo, uint32_t size);
extern void fat_test();
extern void *fat_parse_path(char *path);

extern void bmp_draw(uint8_t *bmp, uint16_t* framebuffer, int screenWidth, bool whiteIsTransparent);

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
      gui_writestr("Enabled", 0);
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
      int progAddr = 42000+0x7c00+512;
      create_task_entry(1, progAddr);
      launch_task(1, regs);
   }
   else if(strcmp(command, "PROG2")) {
      int progAddr = 42000+0x7c00+512*2;
      create_task_entry(2, progAddr);
      launch_task(2, regs);
   }
   else if(strcmp(command, "TEST")) {
      extern uint32_t tos_kernel;
      extern uint32_t tos_program;
      extern uint8_t heap_kernel;
      
      gui_drawchar('\n', 0);
      gui_writestr("TOS KERNEL ", 4);
      gui_writeuint((uint32_t)&tos_kernel, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex((uint32_t)&tos_kernel, 0);

      gui_drawchar('\n', 0);
      gui_writestr("TOS PROGRAM ", 4);
      gui_writeuint((uint32_t)&tos_program, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex((uint32_t)&tos_program, 0);

      gui_drawchar('\n', 0);
      gui_writestr("HEAP KERNEL ", 4);
      gui_writeuint((uint32_t)&heap_kernel, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex((uint32_t)&heap_kernel, 0);

      gui_drawchar('\n', 0);
      gui_writestr("HEAP KERNEL END ", 4);
      gui_writeuint((uint32_t)&heap_kernel+0x0100000, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex((uint32_t)&heap_kernel+0x0100000, 0);

   }
   else if(strcmp(command, "ATA")) {
      ata_identify(true, true); 
   }
   else if(strcmp(command, "FAT")) {
      fat_setup();
   }
   else if(strcmp(command, "FATTEST")) {
      fat_test();
   }
   else if(strstartswith(command, "FATPATH")) {
      char arg[40];
      if(strsplit(arg, arg, command, ' ')) {
         if(!fat_parse_path(arg))
            gui_writestr("File not found\n", 0);
      }
   }
   else if(strstartswith(command, "PROGC")) {
      char arg[10];
      if(strsplit(arg, arg, command, ' ')) {
         int addr = stoi((char*)arg);
         gui_writeuint_hex(addr, 0);
         create_task_entry(3, addr);
         launch_task(3, regs);
      }
   }
   else if(strstartswith(command, "BMP")) {
      char arg[10];
      if(strsplit(arg, arg, command, ' ')) {
         int addr = stoi((char*)arg);
         uint8_t *bmp = (uint8_t*)addr;
         uint16_t *buffer = selected->framebuffer;
         bmp_draw(bmp, buffer, selected->width, false);
         selected->needs_redraw = true;
         gui_draw();
      }
   }
   else if(strstartswith(command, "FATDIR")) {
      char arg[5];
      if(strsplit(arg, arg, command, ' ')) {
         int cluster = stoi((char*)arg);
         fat_read_dir((uint16_t)cluster);
      }
   }
   else if(strstartswith(command, "FATFILE")) {
      char arg[5];
      if(strsplit(arg, arg, command, ' ')) {
         int cluster = stoi((char*)arg);
         fat_read_file((uint16_t)cluster, 0);
      }
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
         uint16_t *buf = malloc(512);
         ata_read(true, true, lba, buf);
         for(int i = 0; i < 256; i++) {
            gui_writeuint_hex(buf[i], 0);
            gui_drawchar(' ', 0);
         }
         free((uint32_t)buf, 512);

      }
   }
   else if(strstartswith(command, "BG")) {
      char arg[5];
      if(strsplit(arg, arg, command, ' ')) {
         int bg = stoi((char*)arg);
         gui_writenum(bg, 0);
         gui_bg = bg;
         gui_redrawall();
      }
   }
   else if(strstartswith(command, "MEM")) {
      char arg[5];
      mem_segment_status_t *status = memory_get_table();
      
      if(strsplit(arg, arg, command, ' ')) {
         int offset = (arg[0]-'0')*400;

         for(int i = offset; i < offset+400 && i < 2048; i++) {
            if(status[i].allocated)
               gui_writenum(1, 0);
            else
               gui_writenum(0, 0);
         }
      } else {
         int used = 0;
         for(int i = 0; i < 2048; i++) {
            if(status[i].allocated) used++;
         }
         gui_writenum(used, 0);
         gui_drawchar('/', 0);
         gui_writenum(2048, 0);
         gui_writestr(" ALLOCATED", 0);
      }
   }
   else if(strstartswith(command, "DMPMEM")) {
      char arg[20];
      if(strsplit(arg, arg, command, ' ')) {
         int bytes = 20;
         int rowlen = 8;
         char arg2[10];
         if(strsplit(arg, arg2, arg, ' ')) {
            bytes = stoi((char*)arg2);
         }
         int addr = stoi((char*)arg);
         gui_writeuint_hex(addr, 0);
         gui_drawchar(':', 0);
         gui_writenum(bytes, 0);
         gui_drawchar('\n', 0);

         char *buf = malloc(rowlen);
         buf[rowlen] = '\0';
         for(int i = 0; i < bytes; i++) {
            uint8_t *mem = (uint8_t*)addr;
            if(mem[i] <= 0x0F)
               gui_drawchar('0', 0);
            gui_writeuint_hex(mem[i], 0);
            gui_drawchar(' ', 0);
            buf[i%rowlen] = mem[i];

            if((i%rowlen) == (rowlen-1) || i==(bytes-1)) {
               for(int x = 0; x < rowlen; x++) {
                  if(buf[x] != '\n')
                     gui_drawchar(buf[x], 0x2F);
               }
               gui_drawchar('\n', 0);
            } 
         }
         free((uint32_t)buf, rowlen);
      }
   }
   else {
      gui_drawchar('\'', 1);
      gui_writestr(selected->text_buffer, 1);
      gui_drawchar('\'', 1);
      gui_writestr(": UNRECOGNISED", 4);
   }
   gui_drawchar('\n', 0);
}

void gui_return(void *regs) {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];

      // write cmd to window framebuffer
      gui_window_drawcharat('>', 8, 1, selected->text_y, gui_selected_window);
      gui_window_writestrat(selected->text_buffer, 0, 1 + FONT_WIDTH + FONT_PADDING, selected->text_y, gui_selected_window);
      // write prompt to framebuffer
      //gui_window_drawcharat('_', 0, window->text_x + 1, window->text_y, windowIndex);
      
      gui_drawchar('\n', 0);
      
      gui_checkcmd(regs);

      selected->text_index = 0;
      selected->text_buffer[selected->text_index] = '\0';
      selected->needs_redraw = true;

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

   uint16_t bg = COLOUR_WHITE;

   if(window->needs_redraw || windowIndex == gui_selected_window) {
      // background
      //gui_drawrect(bg, window->x, window->y, window->width, window->height);

      // titlebar
      gui_drawrect(COLOUR_TITLEBAR, window->x+1, window->y+1, window->width-2, TITLEBAR_HEIGHT);
      gui_writestrat(window->title, 0, window->x+2, window->y+3);
      // titlebar buttons
      gui_drawcharat('x', 0, window->x+window->width-(FONT_WIDTH+3), window->y+2);
      gui_drawcharat('-', 0, window->x+window->width-(FONT_WIDTH+3)*2, window->y+2);

      uint16_t *terminal_buffer = (uint16_t*)framebuffer;

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
      gui_drawcharat('>', 8, window->x + 1, window->y + window->text_y+TITLEBAR_HEIGHT);
      gui_writestrat(window->text_buffer, 0, window->x + 1 + FONT_WIDTH + FONT_PADDING, window->y + window->text_y+TITLEBAR_HEIGHT);
      // prompt
      gui_drawcharat('_', 0, window->x + window->text_x + 1 + FONT_WIDTH + FONT_PADDING, window->y + window->text_y+TITLEBAR_HEIGHT);

      // drop shadow if selected
      gui_drawline(COLOUR_DARK_GREY, window->x+window->width, window->y+1, true, window->height);
      gui_drawline(COLOUR_DARK_GREY, window->x+1, window->y+window->height, false, window->width);

      gui_drawunfilledrect(COLOUR_WINDOW_OUTLINE, window->x, window->y, window->width, window->height);
   } else {
      gui_drawdottedrect(0, window->x, window->y, window->width, window->height);
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
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;
   for(int y = gui_mouse_y; y < gui_mouse_y + FONT_HEIGHT; y++) {
      for(int x = gui_mouse_x; x < gui_mouse_x + FONT_WIDTH; x++) {
         cursor_buffer[(y-gui_mouse_y)*FONT_WIDTH+(x-gui_mouse_x)] = terminal_buffer[y*(int)gui_width+x];
      }
   }
}

void gui_cursor_restore_bg(int old_x, int old_y) {
   uint16_t *terminal_buffer = (uint16_t*) framebuffer;
   for(int y = old_y; y < old_y + FONT_HEIGHT; y++) {
      for(int x = old_x; x < old_x + FONT_WIDTH; x++) {
         terminal_buffer[y*(int)gui_width+x] = cursor_buffer[(y-old_y)*FONT_WIDTH+(x-old_x)];
      }
   }
}

void gui_cursor_draw() {
   gui_drawcharat(27, 0, gui_mouse_x, gui_mouse_y); // outline
   gui_drawcharat(28, COLOUR_WHITE, gui_mouse_x, gui_mouse_y); // fill

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
            gui_drawdottedrect(COLOUR_WHITE, window->x, window->y, window->width, window->height);
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