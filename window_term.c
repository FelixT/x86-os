#include <stdint.h>

#include "window_term.h"
#include "window_settings.h"
#include "window_t.h"
#include "gui.h"
#include "draw.h"
#include "surface_t.h"
#include "window_t.h"
#include "window.h"
#include "events.h"
#include "windowmgr.h"
#include "paging.h"
#include "pci.h"

// default terminal behaviour
// implement kernel mode terminal for debugging

void window_term_printf(char *format, ...) {
   char *buffer = malloc(512);
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   gui_window_t *selected = getSelectedWindow();
   gui_printf(buffer, selected->txtcolour);
   free((uint32_t)buffer, 512);
}

uint32_t window_term_argtouint(char *str) {
   uint32_t num = 0;
   if(strstartswith(str, "0x")) {
      str+=2;
      while(*str != '\0') {
         if(*str >= '0' && *str <= '9')
            num = (num << 4) + (*str - '0');
         else if(*str >= 'A' && *str <= 'F')
            num = (num << 4) + (*str - 'A' + 10);
         else if(*str >= 'a' && *str <= 'f')
            num = (num << 4) + (*str - 'a' + 10);
         str++;
      }
   } else {
      while(*str != '\0') {
         if(*str >= '0' && *str <= '9')
            num = (num * 10) + (*str - '0');
         str++;
      }
   }
   return num;
}

void window_term_keypress(void *regs, uint16_t key, void *window) {
   if(key == 8) {
      window_term_backspace(window);
      return;
   } else if(key == 0x0D) {
      window_term_return(regs, window);
      return;
   } else if(key == 0x100) {
      window_term_uparrow(window);
      return;
   } else if(key == 0x101) {
      window_term_downarrow(window);
      return;
   }

   if((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z') || (key >= '0' && key <= '9')
    || key == ' ' || key == '/' || key == '.' || key == '-' || key == '(' || key == ')' || key == '<' || key == '>'
    || key == '=' || key == '+' || key == '|') {

      // write to current window
      gui_window_t *selected = (gui_window_t*)window;
      if(selected->text_index < TEXT_BUFFER_LENGTH-1) {
         selected->text_buffer[selected->text_index] = key;
         selected->text_buffer[selected->text_index+1] = '\0';
         selected->text_index++;
         selected->text_x = (selected->text_index)*(getFont()->width+getFont()->padding);
      }

      window_draw(selected);

   }
}

void window_term_return(void *regs, void *window) {
   void *windowBuffer = (void *)getWindow(0);

   gui_window_t *selected = (gui_window_t*)window;

   // write cmd to window framebuffer
   draw_char(&(selected->surface), '>', selected->txtcolour, 1, selected->text_y);
   draw_string(&(selected->surface), selected->text_buffer, selected->txtcolour, 1 + getFont()->width + getFont()->padding, selected->text_y);
   
   // write cmd to cmdbuffer
   // shift all entries up
   for(int i = CMD_HISTORY_LENGTH-1; i > 0; i--)
      strcpy(selected->cmd_history[i], selected->cmd_history[i-1]);
   // add new entry
   strcpy_fixed(selected->cmd_history[0], selected->text_buffer, selected->text_index);
   selected->cmd_history[0][selected->text_index] = '\0';
   selected->cmd_history_pos = -1;

   window_newline(selected);

   if(selected->read_func != NULL) {
      // read callback i.e. reading from stdin
      if(selected->read_task > 0) {
         task_state_t *readtask = &gettasks()[selected->read_task];
         if(!readtask->enabled || readtask->task_uid != selected->read_task_uid) {
            debug_printf("read: task %i not enabled or crashed\n", selected->read_task);
            return;
         }
         readtask->paused = false; // force switch
         if(!switch_to_task(selected->read_task, regs)) {
            debug_printf("read: couldn't switch to task %i\n", selected->read_task);
         } else {
            char *buffer = (char*)malloc(selected->text_index+1);
            strcpy_fixed(buffer, selected->text_buffer, selected->text_index);
            buffer[selected->text_index] = '\0';
            selected->read_func(regs, buffer);
         }
      } else {
         debug_printf("read: No read task\n");
      }
   } else if(selected->checkcmd_func != &window_term_checkcmd) {
      // "checkcmd" override
      if(selected->checkcmd_func != NULL) {
         int taskIndex = get_task_from_window(getSelectedWindowIndex());
         if(taskIndex < 0) return;
         task_state_t *task = &gettasks()[taskIndex];

         char *buffer = (char*)malloc(selected->text_index+1);
         strcpy_fixed(buffer, selected->text_buffer, selected->text_index);
         buffer[selected->text_index] = '\0';
         uint32_t *args = malloc(sizeof(uint32_t) * 1);
         args[0] = (uint32_t)buffer;
         // map both to prog
         map(task->process->page_dir, (uint32_t)args, (uint32_t)args, 1, 1, 0);
         map(task->process->page_dir, (uint32_t)buffer, (uint32_t)buffer, 1, 1, 0);

         task_call_subroutine(regs, task, "checkcmd",(uint32_t)selected->checkcmd_func, args, 1);
      }
   } else {
      window_term_checkcmd(regs, selected); // default term behaviour
   }

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
      selected->text_x-=getFont()->width+getFont()->padding;
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
   selected->text_x=len*(getFont()->width+getFont()->padding);
   window_draw(window);
}

void window_term_downarrow(void *window) {
   gui_window_t *selected = (gui_window_t*)window;

   selected->cmd_history_pos--;

   if(selected->cmd_history_pos <= -1) {
      selected->cmd_history_pos = -1;
      selected->text_index = 0;
      selected->text_x = getFont()->padding;
      selected->text_buffer[0] = '\0';
   } else {
      int len = strlen(selected->cmd_history[selected->cmd_history_pos]);
      strcpy_fixed(selected->text_buffer, selected->cmd_history[selected->cmd_history_pos], len);
      selected->text_index = len;
      selected->text_x=len*(getFont()->width+getFont()->padding);
   }
   window_draw(window);
}

void window_term_draw(void *window) {
   gui_window_t *selected = (gui_window_t*)window;
   surface_t *surface = gui_get_surface();

   int maxchars = (selected->width - getFont()->padding*2) / (getFont()->width+getFont()->padding) - 1;
   int startindex = selected->text_index - maxchars;
   if(startindex < 0) startindex = 0;

   int y = selected->y + selected->text_y + TITLEBAR_HEIGHT;
   if(y + getFont()->height > selected->y + selected->height)
      y = selected->y + selected->height - getFont()->height;

   // clear bg
   draw_rect(surface, selected->bgcolour, selected->x+1, y, selected->width-2, getFont()->height);
   // current text content/buffer
   bool showprompt = startindex == 0;
   if(showprompt)
      draw_char(surface, '>', selected->txtcolour, selected->x + 1, y);
   int textx = selected->x + 1 + (showprompt ? (getFont()->width + getFont()->padding) : 0);
   draw_string(surface, selected->text_buffer + startindex, selected->txtcolour, textx, y);
   // cursor
   int cursorx = textx + (selected->text_index - startindex)*(getFont()->width+getFont()->padding);
   draw_char(surface, '_', selected->txtcolour, cursorx, y);
}

void window_term_clear(void *window) {
   gui_window_t *selected = (gui_window_t*)window;
   window_clearbuffer(selected, selected->bgcolour);
   selected->text_x = getFont()->padding;
   selected->text_y = getFont()->padding;
   selected->text_index = 0;
   selected->text_buffer[0] = '\0';
   selected->needs_redraw = true;
   window_draw_content(selected);
}

void term_cmd_help() {
   window_term_printf("\n");
   window_term_printf("  Kernel mode terminal. Built in commands are:\n\n");
   window_term_printf("  CLEAR, MOUSE, TASKS, LAUNCH path\n");
   window_term_printf("  PROG1, PROG2\n");
   window_term_printf("  TEST, DESKTOP\n");
   window_term_printf("  MEM <page>, DMPMEM addr <bytes>\n");
   window_term_printf("  BG colour, BGIMG path\n");
   window_term_printf("  PADDING size, REDRAWALL\n");
   window_term_printf("  PCI, TASKI <task>\n");
}

void term_cmd_clear(gui_window_t *selected) {
   window_term_clear((void*)selected);
}

void term_cmd_mouse() {
   extern bool mouse_enabled;
   mouse_enabled = !mouse_enabled;
   if(mouse_enabled)
      window_term_printf("Enabled\n");
   else
      window_term_printf("Disabled\n");
}

void term_cmd_tasks() {
   task_state_t *tasks = gettasks();

   extern bool switching;
   if(!switching) {
      window_term_printf("Scheduling disabled\n");
      return;
   }

   for(int i = 0; i < TOTAL_TASKS; i++) {
      if(tasks[i].enabled) {
         window_term_printf("\n%i: ", i);
         window_term_printf("<w%i: %s>", tasks[i].process->window, gui_get_windows()[tasks[i].process->window].title);
         if(tasks[i].in_routine)
            window_term_printf(" <routine %s>", tasks[i].routine_name);
         if(tasks[i].process->privileged)
            window_term_printf(" privileged");
         if(tasks[i].paused)
            window_term_printf(" paused");
         if(tasks[i].process->threads[0] == &tasks[i]) {
            // main thread
            window_term_printf(" main thread (%i children", tasks[i].process->no_threads-1);

            for(int t = 1; t < tasks[i].process->no_threads; t++) {
               if(t > 1)
                  window_term_printf(", ");
               else
                  window_term_printf(": ");
               
               window_term_printf("%i", tasks[i].process->threads[t]->task_id);
            }

            window_term_printf(")");
         } else {
            window_term_printf(" child");
         }
         int tmp = getSelectedWindow()->txtcolour;
         getSelectedWindow()->txtcolour = rgb16(140, 140, 140);
         window_term_printf("\n   (eip 0x%h, allc %i/%ikb, heap 0x%h %ib)", tasks[i].registers.eip, tasks[i].process->no_allocated, tasks[i].process->no_allocated*MEM_BLOCK_SIZE/1000, tasks[i].process->heap_start, tasks[i].process->heap_end - tasks[i].process->heap_start);
         window_term_printf("\n   (esp 0x%h, stack 0x%h-0x%h)", tasks[i].registers.useresp, tasks[i].stack_top - TASK_STACK_SIZE, tasks[i].stack_top);
         getSelectedWindow()->txtcolour = tmp;
      } else {
         if(i == 0 || tasks[i-1].enabled) // skip consecutive disabled tasks
            window_term_printf("\n%i: Disabled", i);
      }
   }
   window_term_printf("\nCurrent task is %i\n", get_current_task());
}

void term_cmd_prog1(void *regs) {
   tasks_launch_binary(regs, "/sys/prog1.bin");
}

void term_cmd_prog2(void *regs) {
   tasks_launch_binary(regs, "/sys/prog2.bin");
}

void term_cmd_test() {
   uint32_t framebuffer = (uint32_t)gui_get_framebuffer();
   
   window_term_printf("\nKernel: 0x%h - 0x%h <size 0x%h>", KERNEL_START, KERNEL_END, KERNEL_END - KERNEL_START);
   window_term_printf("\nKernel stack 0x%h - 0x%h <size 0x%h>", KSTACK_START, TOS_KERNEL, TOS_KERNEL - KSTACK_START);
   window_term_printf("\nProgram stack 0x%h - 0x%h <size 0x%h>", STACKS_START, TOS_PROGRAM, TOS_PROGRAM - STACKS_START);
   window_term_printf("\nHeap 0x%h - 0x%h <size 0x%h>", HEAP_KERNEL, HEAP_KERNEL_END, HEAP_KERNEL_END - HEAP_KERNEL);
   window_term_printf("\nFramebuffer 0x%h - 0x%h <size 0x%h>", framebuffer, framebuffer + gui_get_framebuffer_size(), gui_get_framebuffer_size());
}

void term_cmd_fat() {
   fat_setup();
}

void term_cmd_desktop() {
   if(windowmgr_get_settings()->desktop_enabled)
      windowmgr_get_settings()->desktop_enabled = false;
   else
      desktop_init();
}

void term_cmd_launch(registers_t *regs, char *arg) {
   tasks_launch_elf(regs, arg, 0, NULL, true);
}

void term_cmd_bg(char *arg) {
   int bg = stoi(arg);
   gui_writenum(bg, 0);
   extern int gui_bg;
   gui_bg = bg;
   gui_redrawall();
}

void term_cmd_bgimg(char *arg) {

   fat_dir_t *entry = fat_parse_path(arg, true);
   if(entry == NULL) {
      debug_writestr("Image not found\n");
      return;
   }

   uint8_t *gui_bgimage = fat_read_file(entry->firstClusterNo, entry->fileSize);
   desktop_setbgimg(gui_bgimage, entry->fileSize);

   gui_redrawall();
}

void term_cmd_mem(char *arg) {
   mem_segment_status_t *status = memory_get_table();

   if(strlen(arg) > 0) {
      int offset = (arg[0]-'0')*400;

      for(int i = offset; i < offset+400 && i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
         if(status[i].allocated)
            window_term_printf("1");
         else
         window_term_printf("0");
      }
   } else {
      int used = 0;
      for(int i = 0; i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
         if(status[i].allocated) used++;
      }
      window_term_printf("%i/%i pages (%i kb / %i kb) allocated\n", used, KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE, used*MEM_BLOCK_SIZE/1000, KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE*MEM_BLOCK_SIZE/1000);
   }
}

void term_cmd_dmpmem(char *arg) {
   //arg1 = addr
   //arg2 = bytes

   int bytes = 32;
   int rowlen = 8;
   char arg2[10];
   window_term_printf(arg, 0);
   window_term_printf("\n", 0);

   if(strsplit(arg, arg2, arg, ' ')) {
      bytes = stoi((char*)arg2);
   }
   int addr = window_term_argtouint((char*)arg);
   window_term_printf("%i bytes at 0x%h\n", bytes, addr);

   if(page_getphysical(page_get_current(), addr) == (uint32_t)-1) {
      window_term_printf("Address not mapped\n");
      return;
   }
   if(page_getphysical(page_get_current(), addr + bytes - 1) == (uint32_t)-1) {
      window_term_printf("Whole range not mapped\n");
   }

   char *buf = malloc(rowlen+1);
   buf[rowlen] = '\0';
   for(int i = 0; i < bytes; i++) {
      uint8_t *mem = (uint8_t*)addr;
      if(mem[i] <= 0x0F)
         window_term_printf("0");
      window_term_printf("%h ", mem[i]);
      buf[i%rowlen] = mem[i];

      if((i%rowlen) == (rowlen-1) || i==(bytes-1)) {
         for(int x = 0; x < (i%rowlen); x++) {
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

void term_cmd_padding(char *arg) {
   int padding = stoi((char*)arg);
   getFont()->padding = padding;
}

void term_cmd_pci() {
   pci_check_devices();
   int c;
   pci_device_t *devices = get_pci_devices(&c);
   for(int i = 0; i < c; i++) {
      pci_device_t *device = &devices[i];
      window_term_printf("Found device 0x%h vendor 0x%h\n", device->device, device->vendor);
   }
}

void term_cmd_taski(char *arg) {
   int id = strtoint(arg);
   if(id <= 0 || id >= TOTAL_TASKS) {
      debug_printf("Task not found\n");
      return;
   }
   task_state_t *task = &gettasks()[id];
   window_term_printf("Task %i uid %u (enabled %i paused %i unpausable %i crashed %i in routine %i)\n", id, task->task_uid, task->enabled, task->paused, task->unpausable, task->crashed, task->in_routine);
   window_term_printf("Stack top 0x%h\n", task->stack_top);
   process_t *process = task->process;
   if(process) {
      window_term_printf("Process uid %u threads %i privileged %i\n", process->uid, process->no_threads, process->privileged);
      window_term_printf("Working dir %s\n", process->working_dir);
      window_term_printf("Prog mem 0x%h - 0x%h\n", process->vmem_start, process->vmem_end);
      window_term_printf("Heap 0x%h - 0x%h\n", process->heap_start, process->heap_end);
      window_term_printf("MMIO 0x%h - 0x%h\n", V_MMIO_START, process->mmio_end);
      window_term_printf("Exe path %s\n", process->exe_path);
      window_term_printf("Window %i\n", process->window);
      window_term_printf("Event queue size %i\n", process->event_queue_size);
      window_term_printf("DMAs %i mapped devices %i fds %i\n", process->dma_count, process->device_count, process->fd_count);
   }
}

void term_cmd_default(char *command) {
   gui_drawchar('\'', 1);
   window_term_printf(command, 1);
   gui_drawchar('\'', 1);
   window_term_printf(": UNRECOGNISED", 4);
}

void window_term_checkcmd(void *regs, void *window) {
   gui_window_t *selected = (gui_window_t*)window;
   //char *command = selected->text_buffer;
   selected->text_buffer[selected->text_index] = '\0';

   char command[10];
   char arg[40];
   strsplit((char*)command, (char*)arg, selected->text_buffer, ' '); // super unsafe

   strtoupper(command);

   if(strequ(command, "HELP"))
      term_cmd_help();
   else if(strequ(command, "CLEAR"))
      term_cmd_clear(selected);
   else if(strequ(command, "MOUSE"))
      term_cmd_mouse();
   else if(strequ(command, "TASKS"))
      term_cmd_tasks();
   else if(strequ(command, "PROG1"))
      term_cmd_prog1(regs);
   else if(strequ(command, "PROG2"))
      term_cmd_prog2(regs);
   else if(strequ(command, "TEST"))
      term_cmd_test();
   else if(strequ(command, "DESKTOP"))
      term_cmd_desktop();
   else if(strequ(command, "REDRAWALL"))
      term_cmd_redrawall();
   else if(strequ(command, "PADDING"))
      term_cmd_padding((char*)arg);
   else if(strequ(command, "LAUNCH"))
      term_cmd_launch(regs, (char*)arg);
   else if(strequ(command, "BGIMG"))
      term_cmd_bgimg((char*)arg);
   else if(strequ(command, "BG"))
      term_cmd_bg((char*)arg);
   else if(strequ(command, "MEM"))
      term_cmd_mem((char*)arg);
   else if(strequ(command, "DMPMEM"))
      term_cmd_dmpmem((char*)arg);
   else if(strequ(command, "PCI"))
      term_cmd_pci();
   else if(strequ(command, "TASKI"))
      term_cmd_taski((char*)arg);
   else
      term_cmd_default((char*)command);
   
   gui_drawchar('\n', 0);
}