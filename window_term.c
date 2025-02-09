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

void window_term_keypress(char key, void *window) {
   if(((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')) || (key == ' ')
   || (key == '/') || (key == '.')) {

      // write to current window
      gui_window_t *selected = (gui_window_t*)window;
      if(selected->text_index < TEXT_BUFFER_LENGTH-1) {
         selected->text_buffer[selected->text_index] = key;
         selected->text_buffer[selected->text_index+1] = '\0';
         selected->text_index++;
         selected->text_x = (selected->text_index)*(FONT_WIDTH+FONT_PADDING);
      }

      window_draw(selected);

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

void window_term_backspace(void *window) {
   gui_window_t *selected = (gui_window_t*)window;
   if(selected->text_index > 0) {
      selected->text_index--;
      selected->text_x-=FONT_WIDTH+FONT_PADDING;
      selected->text_buffer[selected->text_index] = '\0';
   }

   window_draw(window);
}

void window_term_uparrow(void *window) {
   gui_window_t *selected = (gui_window_t*)window;

   selected->cmd_history_pos++;
   if(selected->cmd_history_pos == CMD_HISTORY_LENGTH)
      selected->cmd_history_pos = CMD_HISTORY_LENGTH - 1;

   int len = strlen(selected->cmd_history[selected->cmd_history_pos]);
   strcpy_fixed(selected->text_buffer, selected->cmd_history[selected->cmd_history_pos], len);
   selected->text_index = len;
   selected->text_x=len*(FONT_WIDTH+FONT_PADDING);
   window_draw(window);
}

void window_term_downarrow(void *window) {
   gui_window_t *selected = (gui_window_t*)window;

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
   window_draw(window);
}

void window_term_draw(void *window) {
   gui_window_t *selected = (gui_window_t*)window;
   surface_t *surface = gui_get_surface();

   // current text content/buffer
   draw_rect(surface, selected->colour_bg, selected->x+1, selected->y+selected->text_y+TITLEBAR_HEIGHT, selected->width-2, FONT_HEIGHT);
   draw_char(surface, '>', 8, selected->x + 1, selected->y + selected->text_y+TITLEBAR_HEIGHT);
   draw_string(surface, selected->text_buffer, 0, selected->x + 1 + FONT_WIDTH + FONT_PADDING, selected->y + selected->text_y+TITLEBAR_HEIGHT);
   // prompt
   draw_char(surface, '_', 0, selected->x + selected->text_x + 1 + FONT_WIDTH + FONT_PADDING, selected->y + selected->text_y+TITLEBAR_HEIGHT);

   // drop shadow if selected
   draw_line(surface, COLOUR_DARK_GREY, selected->x+selected->width+1, selected->y+3, true, selected->height-1);
   draw_line(surface, COLOUR_DARK_GREY, selected->x+3, selected->y+selected->height+1, false, selected->width-1);

   draw_unfilledrect(surface, gui_rgb16(80,80,80), selected->x - 1, selected->y - 1, selected->width + 2, selected->height + 2);
}

void term_cmd_help() {
   gui_writestr("HELP, INIT, CLEAR, MOUSE, TASKS, VIEWTASKS, PROG1, PROG2, PROG3,\n", 0);
   gui_writestr("FILES, PAGE, TEST, ATA, FAT, DESKTOP, FATPATH path, PROGC addr, BMP addr,\n", 0);
   gui_writestr("VIEWBMP path, ELF addr, FATDIR clusterno, FATFILE clusterno, READ addr,\n", 0);
   gui_writestr("BG colour, MEM <x>, DMPMEM x <y>, REDRAWALL", 0);
}

void term_cmd_init(void *regs) {
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
   desktop_init();

   gui_writestr("\nEnabling events\n", COLOUR_ORANGE);
   events_add(40, NULL, -1);

   gui_redrawall();
}

void term_cmd_clear(gui_window_t *selected) {
   window_clearbuffer(selected, COLOUR_WHITE);
   selected->text_x = FONT_PADDING;
   selected->text_y = FONT_PADDING;
   selected->text_index = 0;
   selected->text_buffer[0] = '\0';
   selected->needs_redraw = true;
   window_draw_content(selected);
}

void term_cmd_mouse() {
   mouse_enable();
   gui_writestr("Enabled\n", 0);
}

void term_cmd_tasks(void *regs) {
   tasks_init(regs);
}

void term_cmd_viewtasks() {
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

void term_cmd_prog1(void *regs) {
   tasks_launch_binary(regs, "/sys/prog1.bin");
}

void term_cmd_prog2(void *regs) {
   tasks_launch_binary(regs, "/sys/prog2.bin");
}

void term_cmd_prog3(void *regs) {
   tasks_launch_elf(regs, "/sys/prog3.elf", 0, NULL);
}

void term_cmd_files(void *regs) {
   tasks_launch_elf(regs, "/sys/files.elf", 0, NULL);
}

void term_cmd_viewbmp(void *regs, char *arg) {
   gui_writestr(arg, 0);
   gui_writestr("\n", 0);

   int argc = 1;
   char **args = NULL;
   args = malloc(sizeof(char*) * 1);
   char *path = malloc(strlen(arg));
   strcpy(path, arg);
   args[0] = path;
   tasks_launch_elf(regs, "/sys/bmpview.elf", argc, args);
}

void term_cmd_page() {
   page_init();
}

void term_cmd_test() {
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

void term_cmd_ata() {
   ata_identify(true, true); 
}

void term_cmd_fat() {
   fat_setup();
}

extern bool desktop_enabled;
void term_cmd_desktop() {
   if(desktop_enabled)
      desktop_enabled = false;
   else
      desktop_init();
}

void term_cmd_fatpath(char *arg) {
   if(fat_parse_path(arg) == NULL) {
      gui_writestr("File not found\n", 0);
   }
}

void term_cmd_progc(void *regs, char *arg) {
   int addr = stoi(arg);
   gui_writeuint_hex(addr, 0);
   create_task_entry(3, addr, 0, false);
   launch_task(3, regs, true);
}

void term_cmd_bmp(gui_window_t *selected, char *arg) {
   int addr = stoi(arg);
   uint8_t *bmp = (uint8_t*)addr;
   uint16_t *buffer = selected->framebuffer;
   bmp_draw(bmp, buffer, selected->width, selected->height - TITLEBAR_HEIGHT, 0, 0, false);
   selected->needs_redraw = true;
   gui_draw();
}

void term_cmd_elf(registers_t *regs, char *arg) {
      int addr = stoi(arg);
      uint8_t *prog = (uint8_t*)addr;
      elf_run(regs, prog, 0, NULL);
}

void term_cmd_fatdir(char *arg) {
   int cluster = stoi(arg);
   int size = fat_get_dir_size((uint16_t) cluster);

   fat_dir_t *items = malloc(32 * size);
   fat_read_dir((uint16_t)cluster, items);

   for(int i = 0; i < size; i++) {
      if(items[i].filename[0] == 0) break;
      fat_parse_dir_entry(&items[i]);
   }

   free((uint32_t)items, 32 * size);
}

void term_cmd_fatfile(char *arg) {
   int cluster = stoi(arg);
   fat_read_file((uint16_t)cluster, 0);
}

void term_cmd_read(char *arg) {
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

void term_cmd_bg(char *arg) {
   int bg = stoi(arg);
   gui_writenum(bg, 0);
   extern int gui_bg;
   gui_bg = bg;
   gui_redrawall();
}

void term_cmd_mem(char *arg) {
   mem_segment_status_t *status = memory_get_table();

   if(strlen(arg) > 0) {
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

void term_cmd_dmpmem(char *arg) {
   //arg1 = addr
   //arg2 = bytes

   int bytes = 32;
   int rowlen = 8;
   char arg2[10];
   gui_writestr(arg, 0);
   gui_writestr("\n", 0);

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

void term_cmd_redrawall() {
   gui_redrawall();
}

void term_cmd_default(char *command) {
   gui_drawchar('\'', 1);
   gui_writestr(command, 1);
   gui_drawchar('\'', 1);
   gui_writestr(": UNRECOGNISED", 4);
}

void window_checkcmd(void *regs, gui_window_t *selected) {
   //char *command = selected->text_buffer;
   selected->text_buffer[selected->text_index] = '\0';

   char command[10];
   char arg[30];
   strsplit((char*)command, (char*)arg, selected->text_buffer, ' ');

   if(strcmp(command, "HELP"))
      term_cmd_help();
   else if(strcmp(command, "INIT"))
      term_cmd_init(regs);
   else if(strcmp(command, "CLEAR"))
      term_cmd_clear(selected);
   else if(strcmp(command, "MOUSE"))
      term_cmd_mouse();
   else if(strcmp(command, "TASKS"))
      term_cmd_tasks(regs);
   else if(strcmp(command, "VIEWTASKS"))
      term_cmd_viewtasks();
   else if(strcmp(command, "PROG1"))
      term_cmd_prog1(regs);
   else if(strcmp(command, "PROG2"))
      term_cmd_prog2(regs);
   else if(strcmp(command, "PROG3"))
      term_cmd_prog3(regs);
   else if(strcmp(command, "FILES"))
      term_cmd_files(regs);
   else if(strcmp(command, "PAGE"))
      term_cmd_page();
   else if(strcmp(command, "TEST"))
      term_cmd_test();
   else if(strcmp(command, "ATA"))
      term_cmd_ata();
   else if(strcmp(command, "FAT"))
      term_cmd_fat();
   else if(strcmp(command, "DESKTOP"))
      term_cmd_desktop();
   else if(strcmp(command, "FATPATH"))
      term_cmd_fatpath((char*)arg);
   else if(strstartswith(command, "PROGC"))
      term_cmd_progc(regs, (char*)arg);
   else if(strstartswith(command, "BMP"))
      term_cmd_bmp(selected, (char*)arg);
   else if(strstartswith(command, "VIEWBMP"))
      term_cmd_viewbmp(regs, (char*)arg);
   else if(strstartswith(command, "ELF"))
      term_cmd_elf(regs, (char*)arg);
   else if(strstartswith(command, "FATDIR"))
      term_cmd_fatdir((char*)arg);
   else if(strstartswith(command, "FATFILE"))
      term_cmd_fatfile((char*)arg);
   else if(strstartswith(command, "READ"))
      term_cmd_read((char*)arg);
   else if(strstartswith(command, "BG"))
      term_cmd_bg((char*)arg);
   else if(strstartswith(command, "MEM"))
      term_cmd_mem((char*)arg);
   else if(strstartswith(command, "DMPMEM"))
      term_cmd_dmpmem((char*)arg);
   else if(strstartswith(command, "REDRAWALL")) {
      term_cmd_redrawall();
   }
   else
      term_cmd_default((char*)command);
   
   gui_drawchar('\n', 0);
}