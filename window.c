// default window behaviour

#include "window.h"
#include "gui.h"
#include "ata.h"
#include "memory.h"
#include "tasks.h"
#include "fat.h"
#include "paging.h"
#include "font.h"
#include "bmp.h"
#include "elf.h"

#include "draw.h"

// default terminal behaviour

void window_term_keypress(char key, int windowIndex) {
   if(((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')) || (key == ' ')
   || (key == '/') || (key == '.')) {

      // write to current window
      gui_window_t *selected = &(gui_get_windows()[windowIndex]);
      if(selected->text_index < TEXT_BUFFER_LENGTH-1) {
         selected->text_buffer[selected->text_index] = key;
         selected->text_buffer[selected->text_index+1] = '\0';
         selected->text_index++;
         selected->text_x = (selected->text_index)*(FONT_WIDTH+FONT_PADDING);
      }

      gui_draw_window(windowIndex);

   }
}

void window_term_return(void *regs, int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   // write cmd to window framebuffer
   window_drawcharat('>', 8, 1, selected->text_y, windowIndex);
   window_writestrat(selected->text_buffer, 0, 1 + FONT_WIDTH + FONT_PADDING, selected->text_y, windowIndex);
   
   // write cmd to cmdbuffer
   // shift all entries up
   for(int i = CMD_HISTORY_LENGTH-1; i > 0; i--)
      strcpy(selected->cmd_history[i], selected->cmd_history[i-1]);
   // add new entry
   strcpy_fixed(selected->cmd_history[0], selected->text_buffer, selected->text_index);
   selected->cmd_history[0][selected->text_index] = '\0';
   selected->cmd_history_pos = -1;

   window_drawchar('\n', 0, windowIndex);
   
   window_checkcmd(regs);
   selected = &(gui_get_windows()[windowIndex]); // in case the buffer has changed during the cmd 

   selected->text_index = 0;
   selected->text_buffer[selected->text_index] = '\0';
   selected->needs_redraw = true;

   gui_draw_window(windowIndex);
}

void window_term_backspace(int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);
   if(selected->text_index > 0) {
      selected->text_index--;
      selected->text_x-=FONT_WIDTH+FONT_PADDING;
      selected->text_buffer[selected->text_index] = '\0';
   }

   gui_draw_window(windowIndex);
}

void window_term_uparrow(int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   selected->cmd_history_pos++;
   if(selected->cmd_history_pos == CMD_HISTORY_LENGTH)
      selected->cmd_history_pos = CMD_HISTORY_LENGTH - 1;

   int len = strlen(selected->cmd_history[selected->cmd_history_pos]);
   strcpy_fixed(selected->text_buffer, selected->cmd_history[selected->cmd_history_pos], len);
   selected->text_index = len;
   selected->text_x=len*(FONT_WIDTH+FONT_PADDING);
   gui_draw_window(windowIndex);
}

void window_term_downarrow(int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   selected->cmd_history_pos--;

   if(selected->cmd_history_pos <= -1) {
      selected->cmd_history_pos = -1;
      selected->text_index = 0;
      selected->text_x = FONT_PADDING;
      selected->text_buffer[0] = '\0';
   } else {
      int len = strlen(selected->cmd_history[selected->cmd_history_pos]);
      strcpy_fixed(selected->text_buffer, selected->cmd_history[selected->cmd_history_pos], len);
      selected->text_index = len;
      selected->text_x=len*(FONT_WIDTH+FONT_PADDING);
   }
   gui_draw_window(windowIndex);
}


// === window actions ===

extern void gui_redrawall();

void window_checkcmd(void *regs) {
   gui_window_t *selected = &(gui_get_windows()[gui_get_selected_window()]);
   char *command = selected->text_buffer;

   
   if(strcmp(command, "HELP")) {
      gui_writestr("INIT, CLEAR, MOUSE, TASKS, VIEWTASKS, PROG1, PROG2, FILES, TEST, ATA, FAT, FATTEST, DESKTOP, FATPATH path, PROGC addr, BMP addr, FATDIR clusterno, FATFILE clusterno, READ addr, ELF addr, BG colour, MEM <x>, DMPMEM x <y>", 0);
   }
   else if(strcmp(command, "INIT")) {
      gui_writestr("Enabling mouse\n", COLOUR_ORANGE);
      mouse_enable();

      gui_writestr("\nEnabling ATA\n", COLOUR_ORANGE);
      ata_identify(true, true);

      gui_writestr("\nEnabling FAT\n", COLOUR_ORANGE);
      fat_setup();

      gui_writestr("\nEnabling paging\n", COLOUR_ORANGE);
      page_init();

      gui_writestr("\nEnabling tasks\n", COLOUR_ORANGE);
      tasks_init(regs);

      gui_writestr("\nEnabling desktop\n", COLOUR_ORANGE);
      gui_desktop_init();

   }
   else if(strcmp(command, "CLEAR")) {
      window_clearbuffer(selected, COLOUR_WHITE);
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
   else if(strcmp(command, "FILES")) {
      fat_dir_t *entry = fat_parse_path("/sys/files.elf");
      if(entry == NULL) {
         gui_writestr("Not found\n", 0);
         return;
      }
      uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
      elf_run(regs, prog, 3, 0, NULL);
      free((uint32_t)prog, entry->fileSize);
   }
   else if(strcmp(command, "PAGE")) {
      page_init();
   }
   else if(strcmp(command, "TEST")) {
      extern uint32_t kernel_end;
      uint32_t framebuffer = (uint32_t)gui_get_framebuffer();
      
      gui_writestr("\nKERNEL ", 4);
      gui_writeuint(0x7e00, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex((uint32_t)0x7e00, 0);

      gui_writestr("\nKERNEL END ", 4);
      gui_writeuint((uint32_t)&kernel_end + 0x17e00, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex((uint32_t)&kernel_end + 0x17e00, 0);

      gui_writestr("\nKERNEL STACK ", 4);
      gui_writeuint(STACKS_START + 0x17e00, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex(STACKS_START + 0x17e00, 0);

      gui_writestr("\nTOS KERNEL ", 4);
      gui_writeuint(TOS_KERNEL, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex(TOS_KERNEL, 0);

      gui_writestr("\nTOS PROGRAM ", 4);
      gui_writeuint(TOS_PROGRAM, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex(TOS_PROGRAM, 0);

      gui_writestr("\nHEAP KERNEL ", 4);
      gui_writeuint(HEAP_KERNEL, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex(HEAP_KERNEL, 0);

      gui_writestr("\nHEAP KERNEL END ", 4);
      gui_writeuint(HEAP_KERNEL_END, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex(HEAP_KERNEL_END, 0);

      gui_writestr("\nframebuffer ", 4);
      gui_writeuint(framebuffer, 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex(framebuffer, 0);

      gui_writestr("\nframebuffer END ", 4);
      gui_writeuint(framebuffer + gui_get_framebuffer_size(), 0);
      gui_writestr(" 0x", 0);
      gui_writeuint_hex(framebuffer + gui_get_framebuffer_size(), 0);

   }
   else if(strcmp(command, "ATA")) {
      ata_identify(true, true); 
   }
   else if(strcmp(command, "FAT")) {
      fat_setup();
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
         bmp_draw(bmp, buffer, selected->width, selected->height - TITLEBAR_HEIGHT, 0, 0, false);
         selected->needs_redraw = true;
         gui_draw();
      }
   }
   else if(strstartswith(command, "VIEWBMP")) {
      char arg[20];

      fat_dir_t *entry = fat_parse_path("/sys/bmpview.elf");
      if(entry == NULL) {
         gui_writestr("Not found\n", 0);
         return;
      }
      uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);

      int argc = 0;
      char **args = NULL;
      if(strsplit(arg, arg, command, ' ')) {
         args = malloc(sizeof(char*) * 1);
         char *path = malloc(strlen(arg));
         strcpy(path, arg);
         args[0] = path;
         argc = 1;
      }

      elf_run(regs, prog, 2, argc, args);
      free((uint32_t)prog, entry->fileSize);
   }
   else if(strstartswith(command, "ELF")) {
      char arg[10];
      if(strsplit(arg, arg, command, ' ')) {
         int addr = stoi((char*)arg);
         uint8_t *prog = (uint8_t*)addr;
         elf_run(regs, prog, 3, 0, NULL);
      }
   }
   else if(strstartswith(command, "FATDIR")) {
      char arg[5];
      if(strsplit(arg, arg, command, ' ')) {
         int cluster = stoi((char*)arg);
         int size = fat_get_dir_size((uint16_t) cluster);

         fat_dir_t *items = malloc(32 * size);
         fat_read_dir((uint16_t)cluster, items);

         for(int i = 0; i < size; i++) {
            if(items[i].filename[0] == 0) break;
            fat_parse_dir_entry(&items[i]);
         }

         free((uint32_t)items, 32 * size);
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
         extern int gui_bg;
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

void window_scroll(int windowIndex) {
   gui_window_t *window = &gui_get_windows()[windowIndex];
   uint16_t *terminal_buffer = window->framebuffer;

   int scrollY = FONT_HEIGHT+FONT_PADDING;
   for(int y = scrollY; y < window->height - TITLEBAR_HEIGHT; y++) {
      for(int x = window->x; x < window->x + window->width; x++) {
         int srcIndex = y*(int)window->width+x;
         int outIndex = (y-scrollY)*window->width+x;
         if(srcIndex >= 0 && srcIndex < (int)gui_get_width()*(int)gui_get_height()
         && outIndex >= 0 && outIndex < window->width*(window->height-TITLEBAR_HEIGHT))
            terminal_buffer[outIndex] = terminal_buffer[srcIndex];
      }
   }
   // clear bottom
   int newY = window->height - (scrollY + TITLEBAR_HEIGHT);
   window_drawrect(COLOUR_WHITE, 0, newY, window->width, scrollY, windowIndex);
   window->text_y = newY;
   window->text_x = FONT_PADDING;
}

bool window_init(gui_window_t *window) {
   strcpy(window->title, " TERMINAL");
   window->x = gui_get_num_windows()*8;
   window->y = gui_get_num_windows()*8;
   window->width = 380;
   window->height = 280;
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

   // other functions without default behaviour
   window->click_func = NULL;
   
   window->cmd_history[0] = malloc(CMD_HISTORY_LENGTH*TEXT_BUFFER_LENGTH);
   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      if(i > 0)
         window->cmd_history[i] = window->cmd_history[0] + TEXT_BUFFER_LENGTH;
      window->cmd_history[i][0] = '\0';
   }
   window->cmd_history_pos = -1;

   window->framebuffer = malloc(window->width*(window->height-TITLEBAR_HEIGHT)*2);
   if(window->framebuffer == NULL) return false;
   window_clearbuffer(window, COLOUR_WHITE);
   return true;
}

surface_t window_getsurface(int windowIndex) {
   gui_window_t *window = &(gui_get_windows()[windowIndex]);
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height;
   surface.buffer = (uint32_t)window->framebuffer;
   return surface;
}

void window_drawcharat(char c, uint16_t colour, int x, int y, int windowIndex) {
   if(c >= 'a' && c <= 'z') c = (c - 'a') + 'A'; // convert to uppercase
   surface_t surface = window_getsurface(windowIndex);
   draw_char(&surface, c, colour, x, y);
}

void window_drawrect(uint16_t colour, int x, int y, int width, int height, int windowIndex) {
   surface_t surface = window_getsurface(windowIndex);
   draw_rect(&surface, colour, x, y, width, height);
}

void window_writestrat(char *c, uint16_t colour, int x, int y, int windowIndex) {
   int i = 0;
   while(c[i] != '\0') {
      window_drawcharat(c[i++], colour, x, y, windowIndex);
      x+=FONT_WIDTH+FONT_PADDING;
   }
}

void window_clearbuffer(gui_window_t *window, uint16_t colour) {
   for(int i = 0; i < window->width*(window->height-TITLEBAR_HEIGHT); i++) {
      window->framebuffer[i] = colour;
   }
}

void window_writenumat(int num, uint16_t colour, int x, int y, int windowIndex) {
   if(num < 0)
      window_drawcharat('-', colour, x+=(FONT_WIDTH+FONT_PADDING), y, windowIndex);

   char out[20];
   inttostr(num, out);
   window_writestrat(out, colour, x, y, windowIndex);
}

void window_drawchar(char c, uint16_t colour, int windowIndex) {

   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   if(c == '\n') {
      selected->text_x = FONT_PADDING;
      selected->text_y += FONT_HEIGHT + FONT_PADDING;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
         window_scroll(windowIndex);
      }

      // immediately output each line
      selected->needs_redraw=true;
      gui_draw_window(windowIndex);

      return;
   }

   // x overflow
   if(selected->text_x + FONT_WIDTH + FONT_PADDING >= selected->width) {
      window_drawcharat('-', colour, selected->text_x-2, selected->text_y, windowIndex);
      selected->text_x = FONT_PADDING;
      selected->text_y += FONT_HEIGHT + FONT_PADDING;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
         window_scroll(windowIndex);
      }
   }

   window_drawcharat(c, colour, selected->text_x, selected->text_y, windowIndex);
   selected->text_x+=FONT_WIDTH+FONT_PADDING;

   if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
      window_scroll(windowIndex);
   }

   selected->needs_redraw = true;

}

void window_writestr(char *c, uint16_t colour, int windowIndex) {
   int i = 0;
   while(c[i] != '\0')
      window_drawchar(c[i++], colour, windowIndex);
}

void window_writenum(int num, uint16_t colour, int windowIndex) {
   if(num < 0)
      window_drawchar('-', colour, windowIndex);

   char out[20];
   inttostr(num, out);
   window_writestr(out, colour, windowIndex);
}

void window_writeuint(uint32_t num, uint16_t colour, int windowIndex) {
   char out[20];
   uinttostr(num, out);
   window_writestr(out, colour, windowIndex);
}
