#include <stdint.h>

#include "window_term.h"
#include "window_t.h"
#include "gui.h"
#include "draw.h"
#include "surface_t.h"
#include "window_t.h"
#include "window.h"
#include "events.h"
#include "windowmgr.h"
#include "paging.h"

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

void window_term_return(void *regs, void *window) {
   void *windowBuffer = (void *)getWindow(0);

   gui_window_t *selected = (gui_window_t*)window;

   // write cmd to window framebuffer
   draw_char(&(selected->surface), '>', 8, 1, selected->text_y);
   draw_string(&(selected->surface), selected->text_buffer, 0, 1 + FONT_WIDTH + FONT_PADDING, selected->text_y);
   
   // write cmd to cmdbuffer
   // shift all entries up
   for(int i = CMD_HISTORY_LENGTH-1; i > 0; i--)
      strcpy(selected->cmd_history[i], selected->cmd_history[i-1]);
   // add new entry
   strcpy_fixed(selected->cmd_history[0], selected->text_buffer, selected->text_index);
   selected->cmd_history[0][selected->text_index] = '\0';
   selected->cmd_history_pos = -1;

   window_newline(selected);
   
   window_checkcmd(regs, selected);

   // windowbuffer has changed (i.e. resized)
   if(windowBuffer != (void *)getWindow(0)) {
      selected = getWindow(0);
      window = selected;
   }

   selected->text_index = 0;
   selected->text_buffer[selected->text_index] = '\0';
   selected->needs_redraw = true;

   window_draw_content(selected);
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

void window_term_draw(int windowIndex) {
   gui_window_t *window = &(gui_get_windows()[windowIndex]);
   surface_t *surface = gui_get_surface();

   // current text content/buffer
   draw_rect(surface, window->colour_bg, window->x+1, window->y+window->text_y+TITLEBAR_HEIGHT, window->width-2, FONT_HEIGHT);
   draw_char(surface, '>', 8, window->x + 1, window->y + window->text_y+TITLEBAR_HEIGHT);
   draw_string(surface, window->text_buffer, 0, window->x + 1 + FONT_WIDTH + FONT_PADDING, window->y + window->text_y+TITLEBAR_HEIGHT);
   // prompt
   draw_char(surface, '_', 0, window->x + window->text_x + 1 + FONT_WIDTH + FONT_PADDING, window->y + window->text_y+TITLEBAR_HEIGHT);

   // drop shadow if selected
   draw_line(surface, COLOUR_DARK_GREY, window->x+window->width+1, window->y+3, true, window->height-1);
   draw_line(surface, COLOUR_DARK_GREY, window->x+3, window->y+window->height+1, false, window->width-1);

   draw_unfilledrect(surface, gui_rgb16(80,80,80), window->x - 1, window->y - 1, window->width + 2, window->height + 2);
}

extern void gui_redrawall();

void window_checkcmd(void *regs, gui_window_t *selected) {
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

      gui_writestr("\nEnabling events\n", COLOUR_ORANGE);
      events_add(40, NULL, -1);

   }
   else if(strcmp(command, "CLEAR")) {
      window_clearbuffer(selected, COLOUR_WHITE);
      selected->text_x = FONT_PADDING;
      selected->text_y = FONT_PADDING;
      selected->text_index = 0;
      command[0] = '\0';
      window_draw_content(selected);
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
      fat_dir_t *entry = fat_parse_path("/sys/prog3.elf");
      if(entry == NULL) {
         gui_writestr("Not found\n", 0);
         return;
      }
      uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
      elf_run(regs, prog, 0, NULL);
      free((uint32_t)prog, entry->fileSize);
   }
   else if(strcmp(command, "FILES")) {
      fat_dir_t *entry = fat_parse_path("/sys/files.elf");
      if(entry == NULL) {
         gui_writestr("Not found\n", 0);
         return;
      }
      uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
      elf_run(regs, prog, 0, NULL);
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

      elf_run(regs, prog, argc, args);
      free((uint32_t)prog, entry->fileSize);
   }
   else if(strstartswith(command, "ELF")) {
      char arg[10];
      if(strsplit(arg, arg, command, ' ')) {
         int addr = stoi((char*)arg);
         uint8_t *prog = (uint8_t*)addr;
         elf_run(regs, prog, 0, NULL);
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