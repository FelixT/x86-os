#include "gui.h"

extern int videomode;

int gui_mouse_x = 0;
int gui_mouse_y = 0;

uint16_t gui_bg;

size_t gui_width = 320;
size_t gui_height = 200;

bool mouse_enabled = false;
bool mouse_held = false;

gui_window_t *gui_windows;
int gui_selected_window = 0;

uint32_t framebuffer;

uint16_t cursor_buffer[FONT_WIDTH*FONT_HEIGHT]; // store whats behind cursor so it can be restored

gui_window_t *gui_windows;
int NUM_WINDOWS;

bool desktop_enabled = false;
uint8_t *icon_window;
uint8_t *gui_bgimage;

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

void gui_window_writeuint(uint32_t num, uint16_t colour, int windowIndex) {
   char out[20];
   terminal_uinttostr(num, out);
   gui_window_writestr(out, colour, windowIndex);
}

void gui_window_clearbuffer(gui_window_t *window) {
   for(int i = 0; i < window->width*window->height; i++) {
      window->framebuffer[i] = COLOUR_WHITE;
   }
}

bool gui_window_init(gui_window_t *window) {
   strcpy(window->title, " TERMINAL");
   window->x = NUM_WINDOWS*8;
   window->y = NUM_WINDOWS*8;
   window->width = 360;
   window->height = 240;
   window->text_buffer[0] = '\0';
   window->text_index = 0;
   window->text_x = FONT_PADDING;
   window->text_y = FONT_PADDING;
   window->needs_redraw = true;
   window->active = false;
   window->minimised = false;
   window->closed = false;
   window->dragged = false;

   // default TERMINAL functions
   window->return_func = &window_term_return;
   window->keypress_func = &window_term_keypress;
   window->backspace_func = &window_term_backspace;
   window->uparrow_func = &window_term_uparrow;
   window->downarrow_func = &window_term_downarrow;
   
   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      window->cmd_history[i] = malloc(TEXT_BUFFER_LENGTH);
      window->cmd_history[i][0] = '\0';
   }
   window->cmd_history_pos = -1;

   window->framebuffer = malloc(window->width*(window->height-TITLEBAR_HEIGHT)*2);
   if(window->framebuffer == NULL) return false;
   gui_window_clearbuffer(window);
   return true;
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

   // init with one terminal window
   NUM_WINDOWS = 1;
   gui_windows = malloc(sizeof(gui_window_t));
   gui_window_init(&gui_windows[0]);
   gui_windows[0].title[0] = '0';
   gui_selected_window = 0;
}

void gui_desktop_draw();
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
   for(int i = 0; i < NUM_WINDOWS; i++) {
      if(gui_windows[i].minimised) {
         int textWidth = gui_gettextwidth(3);
         int textX = TOOLBAR_ITEM_WIDTH/2 - textWidth/2;
         char text[4] = "   ";
         strcpy_fixed(text, gui_windows[i].title, 3);
         int itemX = TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING);
         int itemY = gui_height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING);
         gui_drawrect(COLOUR_TASKBAR_ENTRY, itemX, itemY, TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT);
         gui_writestrat(text, COLOUR_WHITE, itemX+textX, itemY+1);
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
   if(desktop_enabled) gui_desktop_draw();
   gui_draw();
   gui_cursor_save_bg();
   if(mouse_enabled) gui_cursor_draw();
}

void mouse_enable();

extern void ata_identify(bool primaryBus, bool masterDrive);
extern void ata_read(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf);

extern void fat_setup();
extern void fat_read_dir(uint16_t clusterNo);
extern uint8_t *fat_read_file(uint16_t clusterNo, uint32_t size);
extern void fat_test();

extern void bmp_draw(uint8_t *bmp, uint16_t* framebuffer, int screenWidth, int screenHeight, bool whiteIsTransparent);
extern uint16_t bmp_get_colour(uint8_t *bmp, int x, int y);

void gui_checkcmd(void *regs) {
   gui_window_t *selected = &gui_windows[gui_selected_window];
   char *command = selected->text_buffer;

   
   if(strcmp(command, "HELP")) {
      gui_writestr("INIT, CLEAR, MOUSE, TASKS, VIEWTASKS, PROG1, PROG2, TEST, ATA, FAT, FATTEST, DESKTOP, FATPATH path, PROGC addr, BMP addr, FATDIR clusterno, FATFILE clusterno, READ addr, BG colour, MEM <x>, DMPMEM x <y>", 0);
   }
   else if(strcmp(command, "INIT")) {
      gui_writestr("Enabling mouse\n", COLOUR_ORANGE);
      mouse_enable();

      gui_writestr("\nEnabling ATA\n", COLOUR_ORANGE);
      ata_identify(true, true);

      gui_writestr("\nEnabling FAT\n", COLOUR_ORANGE);
      fat_setup();

      gui_writestr("\nEnabling tasks\n", COLOUR_ORANGE);
      tasks_init(regs);

      gui_writestr("\nEnabling desktop\n", COLOUR_ORANGE);
      gui_desktop_init();
   }
   else if(strcmp(command, "CLEAR")) {
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

      extern bool switching;
      gui_writestr("Scheduling ", 0);
      if(switching) gui_writestr("enabled\n", 0);
      else gui_writestr("disabled\n", 0);
      
      for(int i = 0; i < TOTAL_TASKS; i++) {
         gui_drawchar('\n', 0);

         gui_writenum(i, 0);
         gui_writestr(": ", 0);

         if(tasks[i].enabled)
            gui_writestr("ENABLED", 0);
         else
            gui_writestr("DISABLED", 0);

         gui_writestr(" <", 0);
         gui_writenum(tasks[i].window, 0);
         gui_writestr(">", 0);

         if(tasks[i].privileged)
            gui_writestr(" privileged", 0);
      }
   }
   else if(strcmp(command, "PROG1")) {
      fat_dir_t *entry = fat_parse_path("/sys/prog1.bin");
      if(entry == NULL) {
         gui_writestr("Not found\n", 0);
         return;
      }
      uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
      uint32_t progAddr = (uint32_t)prog;
      create_task_entry(1, progAddr, entry->fileSize, false);
      launch_task(1, regs, true);
   }
   else if(strcmp(command, "PROG2")) {
      fat_dir_t *entry = fat_parse_path("/sys/prog2.bin");
      if(entry == NULL) {
         gui_writestr("Not found\n", 0);
         return;
      }
      uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
      uint32_t progAddr = (uint32_t)prog;
      create_task_entry(2, progAddr, entry->fileSize, false);
      launch_task(2, regs, true);
   }
   else if(strcmp(command, "PROG3")) {
      fat_dir_t *entry = fat_parse_path("/sys/prog3.bin");
      if(entry == NULL) {
         gui_writestr("Not found\n", 0);
         return;
      }
      uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
      uint32_t progAddr = (uint32_t)prog;
      create_task_entry(3, progAddr, entry->fileSize, false);
      launch_task(3, regs, true);
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
   else if(strcmp(command, "DESKTOP")) {
      gui_desktop_init();
   }
   else if(strstartswith(command, "FATPATH")) {
      char arg[40];
      if(strsplit(arg, arg, command, ' ')) {
         if(fat_parse_path(arg) == NULL)
            gui_writestr("File not found\n", 0);
      }
   }
   else if(strstartswith(command, "PROGC")) {
      char arg[10];
      if(strsplit(arg, arg, command, ' ')) {
         int addr = stoi((char*)arg);
         gui_writeuint_hex(addr, 0);
         create_task_entry(3, addr, 0, false);
         launch_task(3, regs, true);
      }
   }
   else if(strstartswith(command, "BMP")) {
      char arg[10];
      if(strsplit(arg, arg, command, ' ')) {
         int addr = stoi((char*)arg);
         uint8_t *bmp = (uint8_t*)addr;
         uint16_t *buffer = selected->framebuffer;
         bmp_draw(bmp, buffer, selected->width, selected->height - TITLEBAR_HEIGHT, false);
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
      char arg[8];
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

         for(int i = offset; i < offset+400 && i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
            if(status[i].allocated)
               gui_writenum(1, 0);
            else
               gui_writenum(0, 0);
         }
      } else {
         int used = 0;
         for(int i = 0; i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
            if(status[i].allocated) used++;
         }
         gui_writenum(used, 0);
         gui_drawchar('/', 0);
         gui_writenum(KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE, 0);
         gui_writestr(" ALLOCATED", 0);
      }
   }
   else if(strstartswith(command, "DMPMEM")) {
      char arg[20];
      if(strsplit(arg, arg, command, ' ')) {
         int bytes = 32;
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

void gui_keypress(char key) {

   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->keypress_func != NULL)
         (*(selected->keypress_func))(key, gui_selected_window);
   }
}

void gui_return(void *regs) {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->return_func != NULL)
         (*(selected->return_func))(regs, gui_selected_window);
   }
}

void gui_backspace() {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->backspace_func != NULL)
         (*(selected->backspace_func))(gui_selected_window);
   }
}

void gui_uparrow() {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->uparrow_func != NULL)
         (*(selected->uparrow_func))(gui_selected_window);
   }
}

void gui_downarrow() {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->downarrow_func != NULL)
         (*(selected->downarrow_func))(gui_selected_window);
   }}

void gui_window_draw(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   if(window->closed || window->minimised || window->dragged)
      return;

   uint16_t bg = COLOUR_WHITE;

   if(window->needs_redraw || windowIndex == gui_selected_window) {
      // background
      //gui_drawrect(bg, window->x, window->y, window->width, window->height);

      // titlebar
      gui_drawrect(COLOUR_TITLEBAR, window->x+1, window->y+1, window->width-2, TITLEBAR_HEIGHT);
      // titlebar text, centred
      int titleWidth = gui_gettextwidth(strlen(window->title));
      int titleX = window->x + window->width/2 - titleWidth/2;
      gui_drawrect(COLOUR_WHITE, titleX-4, window->y+1, titleWidth+8, TITLEBAR_HEIGHT);
      gui_writestrat(window->title, 0, titleX, window->y+3);
      // titlebar buttons
      gui_drawcharat('x', 0, window->x+window->width-(FONT_WIDTH+3), window->y+2);
      gui_drawcharat('-', 0, window->x+window->width-(FONT_WIDTH+3)*2, window->y+2);

      uint16_t *terminal_buffer = (uint16_t*)framebuffer;

      if(window->framebuffer != NULL) {
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
      }
      
      if(windowIndex != gui_selected_window) gui_drawdottedrect(0, window->x, window->y, window->width, window->height);

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
      gui_drawline(COLOUR_DARK_GREY, window->x+window->width, window->y+2, true, window->height-1);
      gui_drawline(COLOUR_DARK_GREY, window->x+2, window->y+window->height, false, window->width-1);

      gui_drawunfilledrect(gui_rgb16(80,80,80), window->x, window->y, window->width, window->height);
   } else {
   }
}

void gui_desktop_init() {
   // load add window icon
   fat_dir_t *entry = fat_parse_path("/bmp/window.bmp");
   if(entry == NULL) {
      gui_writestr("ICON not found\n", 0);
      return;
   }

   icon_window = fat_read_file(entry->firstClusterNo, entry->fileSize);

   // load background
   entry = fat_parse_path("/bmp/bg.bmp");
   if(entry == NULL) {
      gui_writestr("BG not found\n", 0);
      return;
   }

   gui_bgimage = fat_read_file(entry->firstClusterNo, entry->fileSize);
   gui_bg = bmp_get_colour(gui_bgimage, 0, 0);

   desktop_enabled = true;
   gui_redrawall();
}

int gui_window_add() {

   int newIndex = -1;

   // find first closed window
   for(int i = 0; i < NUM_WINDOWS; i++) {
      if(gui_windows[i].closed) {
         newIndex = i;
         break;
      }
   }

   // otherwise resize array
   if(newIndex == -1) {
      newIndex = NUM_WINDOWS;
      gui_window_t *windows_new = resize((uint32_t)gui_windows, NUM_WINDOWS*sizeof(gui_window_t), (NUM_WINDOWS+1)*sizeof(gui_window_t));
   
      if(windows_new != NULL) {
         NUM_WINDOWS++;
         gui_windows = windows_new;
      } else {
         gui_window_writestr("Couldn't create window\n", 0, 0);
         return -1;
      }
   }

   if(gui_window_init(&gui_windows[newIndex])) {
      gui_windows[newIndex].title[0] = newIndex+'0';
      gui_selected_window = newIndex;

      return newIndex;
   } else {
      strcpy((char*)gui_windows[newIndex].title, "ERROR");
      gui_windows[newIndex].closed = true;
      gui_window_writestr("Couldn't create window\n", 0, 0);
   }

   return -1;
}

void gui_window_close(void *regs, int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   if(window->closed) return;

   if(windowIndex == gui_selected_window)
      gui_selected_window = -1;

   window->closed = true;

   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      //free((uint32_t)&window->cmd_history[i][0], TEXT_BUFFER_LENGTH); // broken
      window->cmd_history[i] = NULL;
   }
   free((uint32_t)window->framebuffer, window->width*(window->height-TITLEBAR_HEIGHT)*2);
   window->framebuffer = NULL;

   end_task(get_task_from_window(windowIndex), regs);

   gui_redrawall();
}

void gui_desktop_draw() {
   //gui_writeuint((uint32_t)icon_window, 0);
   bmp_draw(gui_bgimage, (uint16_t *)framebuffer, gui_width, gui_height, 0);
   bmp_draw(icon_window, (uint16_t *)framebuffer, gui_width, gui_height, 1);
}

void gui_desktop_click() {
   if(gui_mouse_x < 50 && gui_mouse_y < 50)
      gui_window_add();
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
   
   if(mouse_enabled) return;

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

bool mouse_clicked_on_window(void *regs, int index) {
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
   } else if(!window->closed && gui_mouse_x >= window->x && gui_mouse_x <= window->x + window->width
      && gui_mouse_y >= window->y && gui_mouse_y <= window->y + window->height) {

         int relX = gui_mouse_x - window->x;
         int relY = gui_mouse_y - window->y;

         // minimise
         if(relY < 10 && relX > window->width - (FONT_WIDTH+3)*2 && relX < window->width - (FONT_WIDTH+3))
            window->minimised = true;

         // close
         if(relY < 10 && relX > window->width - (FONT_WIDTH+3)) {
            gui_window_close(regs, index);
            return false;
         }

         window->active = true;
         window->needs_redraw = true;
         gui_selected_window = index;
         return true;
   }

   return false;
}

void mouse_leftclick(void *regs, int relX, int relY) {
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
   if(mouse_clicked_on_window(regs, gui_selected_window)) {
      // 
   } else {
      // check other windows

      gui_selected_window = -1;
      for(int i = 0; i < NUM_WINDOWS; i++) {
         if(mouse_clicked_on_window(regs, i))
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

   if(desktop_enabled)
      gui_desktop_click();

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

uint16_t *gui_get_framebuffer() {
   return (uint16_t*)framebuffer;
}

gui_window_t *gui_get_windows() {
   return gui_windows;
}

int gui_get_selected_window() {
   return gui_selected_window;
}

void gui_set_selected_window(int windowIndex) {
   gui_selected_window = windowIndex;
}

size_t gui_get_width() {
   return gui_width;
}

size_t gui_get_height() {
   return gui_height;
}

int *gui_get_num_windows() {
   return &NUM_WINDOWS;
}

uint32_t gui_get_window_framebuffer(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   return (uint32_t)window->framebuffer;
}

void gui_showtimer(int number) {
   gui_drawrect(gui_bg, -10, 5, 7, 11);
   gui_writenumat(number, COLOUR_WHITE, -10, 5);
}

int gui_gettextwidth(int textlength) {
   return textlength*(FONT_WIDTH+FONT_PADDING);
}