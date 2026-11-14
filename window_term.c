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
#include "msg.h"
#include "fs.h"
#include "time.h"

// default terminal behaviour
// implement kernel mode terminal for debugging

void window_term_printf(char *format, ...) {
   char buffer[512];
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   gui_window_t *selected = getSelectedWindow();
   gui_printf(buffer, selected->txtcolour);
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

void window_term_keypress(uint16_t key, void *window) {
   if(key == 8) {
      window_term_backspace(window);
      return;
   } else if(key == 0x0D) {
      window_term_return(window);
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

      window_draw(selected, true);

   }
}

void window_term_return(void *window) {
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

   // read callback i.e. reading from stdin
   if(selected->read_task > 0) {
      fs_read_window_callback(selected, false);
   } else if(selected->checkcmd_func != &window_term_checkcmd) {
      // E_CHECKCMD override
      if(selected->checkcmd_func != NULL) {
         int taskIndex = get_task_from_window(getSelectedWindowIndex());
         if(taskIndex < 0) return;
         task_state_t *task = &gettasks()[taskIndex];

         char *buffer = (char*)page_alloc(TEXT_BUFFER_LENGTH);
         strcpy_fixed(buffer, selected->text_buffer, selected->text_index);
         buffer[selected->text_index] = '\0';
         uint32_t args[1] = { (uint32_t)buffer };
         map(task->process->page_dir, (uint32_t)buffer, (uint32_t)buffer, 1, 1, 0); // todo: replace

         if(!wm_call_subroutine(task, E_CHECKCMD, (uint32_t)selected->checkcmd_func, args, 1)) {
            unshare_heap_page(task->process->page_dir, (uint32_t)buffer);
            kfree(buffer);
         }
      }
   } else {
      window_term_checkcmd(selected); // default term behaviour
   }

   // windowbuffer has changed (i.e. resized)
   if(windowBuffer != (void *)getWindow(0)) {
      selected = getWindow(0);
      window = selected;
   }

   selected->text_index = 0;
   selected->text_buffer[selected->text_index] = '\0';
   selected->needs_redraw = true;

   window_draw_content(selected, true);
}

void window_term_backspace(void *window) {
   gui_window_t *selected = (gui_window_t*)window;
   if(selected->text_index > 0) {
      selected->text_index--;
      selected->text_x-=getFont()->width+getFont()->padding;
      selected->text_buffer[selected->text_index] = '\0';
   }

   window_draw(window, true);
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
   window_draw(window, true);
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
   window_draw(window, true);
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
   window_draw_content(selected, true);
}

void term_cmd_help() {
   window_term_printf("\n");
   window_term_printf("  Kernel mode terminal. Built in commands are:\n\n");
   window_term_printf("  CLEAR, MOUSE, TASKS, LAUNCH path\n");
   window_term_printf("  PROG1, PROG2\n");
   window_term_printf("  TEST, MTRR, DESKTOP\n");
   window_term_printf("  MEM <page>, DMPMEM addr <bytes>\n");
   window_term_printf("  BG colour, BGIMG path\n");
   window_term_printf("  PADDING size, REDRAWALL\n");
   window_term_printf("  PCI, TASKI <task>, CHANNELS\n");
   window_term_printf("  SIGNAL <port> <flag>\n");
   window_term_printf("  HANDLES <task>\n");
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
      task_state_t *task = &tasks[i];
      if(!task->enabled) continue;
      process_t *process = task->process;
      int w = process->window;
      window_term_printf("\n%i: ", i);
      window_term_printf("<w%i:%s>", w, w > -1 ? getWindow(w)->title : process->exe_path);
      if(is_kthread(task)) {
         window_term_printf(" kthread");
         if(i == 0) {
            int tmp = getSelectedWindow()->txtcolour;
            getSelectedWindow()->txtcolour = rgb16(140, 140, 140);
            window_term_printf(" %i ticks", tasks[i].ticks);
            getSelectedWindow()->txtcolour = tmp;
         }
         continue;
      }

      if(task->routine != E_NONE)
         window_term_printf(" <routine %i>", task->routine);
      if(process->privileged)
         window_term_printf(" privileged");
      if(task->kernel_esp)
         window_term_printf(" parked");
      if(task->kill_pending)
         window_term_printf(" killpending");
      if(task->paused)
         window_term_printf(" paused (%u)", task->pause_reason);

      if(process->threads[0] == task) {
         // main thread

         if(process->no_threads > 1) {
            window_term_printf(" (%i children: ", tasks[i].process->no_threads-1);

            int disp = 0;
            for(int t = 1; t < MAX_TASK_THREADS; t++) {
               if(!tasks[i].process->threads[t]) continue;               
               window_term_printf("%s%i", disp > 0 ? ", " : "", tasks[i].process->threads[t]->task_id);
               disp++;
            }

            window_term_printf(")");
         }
      } else {
         window_term_printf(" child");
      }

      int tmp = getSelectedWindow()->txtcolour;
      getSelectedWindow()->txtcolour = rgb16(140, 140, 140);
      window_term_printf(" %i ticks", tasks[i].ticks);
      getSelectedWindow()->txtcolour = tmp;
   }
}

void term_cmd_prog1() {
   wm_queue_launch("/sys/prog1.bin", false, NULL, 0, true, false);
}

void term_cmd_prog2() {
   wm_queue_launch("/sys/prog2.bin", false, NULL, 0, true, false);
}

void term_cmd_test() {
   uint32_t framebuffer = (uint32_t)gui_get_framebuffer();
   
   window_term_printf("\nKernel: 0x%h - 0x%h <size 0x%h>", KERNEL_START, KERNEL_END, KERNEL_END - KERNEL_START);
   window_term_printf("\nKernel stack 0x%h - 0x%h <size 0x%h>", KSTACK_START, TOS_KERNEL, TOS_KERNEL - KSTACK_START);
   window_term_printf("\nHeap 0x%h - 0x%h <size 0x%h>", HEAP_KERNEL, HEAP_KERNEL_END, HEAP_KERNEL_END - HEAP_KERNEL);
   window_term_printf("\nFramebuffer 0x%h - 0x%h <size 0x%h>", framebuffer, framebuffer + gui_get_framebuffer_size(), gui_get_framebuffer_size());
}

void term_cmd_fat() {
   if(fat_setup())
      window_term_printf("\nFAT re-initialised");
   else
      window_term_printf("\nFAT init failed");
}

void term_cmd_desktop() {
   if(windowmgr_get_settings()->desktop_enabled)
      windowmgr_get_settings()->desktop_enabled = false;
   else
      desktop_init();
}

void term_cmd_launch(char *arg) {
   wm_queue_launch(arg, true, NULL, 0, true, false);
}

void term_cmd_bg(char *arg) {
   int bg = stoi(arg);
   gui_writenum(bg, 0);
   extern int gui_bg;
   gui_bg = bg;
   gui_redrawall();
}

void term_cmd_bgimg(char *arg) {
   int fsize;
   uint8_t *bgimg = fs_read_file_kernel(arg, &fsize);
   if(!bgimg) {
      debug_printf("Image not found\n");
      return;
   }
   desktop_setbgimg(bgimg, fsize);

   gui_redrawall();
}

void term_cmd_mem(char *arg) {
   if(strlen(arg) > 0) {
      int offset = (arg[0]-'0')*256;
      window_term_printf("Start %i / 0x%h\n", offset, HEAP_KERNEL+offset*PAGE_SIZE);

      for(int i = offset; i < offset+256 && i < KERNEL_HEAP_SIZE/PAGE_SIZE; i++) {
         bool alloc = (memory_get_bitmap()[i/32] >> (i%32)) & 0x1;
         window_term_printf("%c", alloc?'1':'0');
      }
   } else {
      window_term_printf("%i/%i pages (%i kb / %i kb) allocated\n", allocated_blocks(), KERNEL_HEAP_SIZE/PAGE_SIZE, allocated_blocks()*PAGE_SIZE/1000, KERNEL_HEAP_SIZE/PAGE_SIZE*PAGE_SIZE/1000);
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

   char buf[9];
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
}

void term_cmd_redrawall() {
   gui_redrawall();
}

void term_cmd_padding(char *arg) {
   int padding = stoi((char*)arg);
   getFont()->padding = padding;
}

void term_cmd_pci() {
   int c;
   pci_device_t *devices = get_pci_devices(&c);
   for(int i = 0; i < c; i++) {
      pci_device_t *device = &devices[i];
      window_term_printf("Found device 0x%h vendor 0x%h\n", device->device, device->vendor);
   }
}

void term_cmd_taski(char *arg) {
   int id = strtoint(arg);
   if(id < 0 || id >= TOTAL_TASKS) {
      debug_printf("Task not found\n");
      return;
   }
   task_state_t *task = &gettasks()[id];
   window_term_printf("\nTask %i uid %u\nEnabled %i paused %i reason %i crashed %i in routine %i\n\n", id, task->task_uid, task->enabled, task->paused, task->pause_reason, task->crashed, task->routine);
   window_term_printf("eip: 0x%h, esp: 0x%h\n", task->registers.eip, task->registers.useresp);
   window_term_printf("Stack: 0x%h-0x%h\n", task->stack_base, task->stack_base + TASK_STACK_SIZE - 0x1000);
   window_term_printf("kstack: 0x%h-0x%h\n", task->kernel_stack_top - KSTACK_SIZE, task->kernel_stack_top);

   process_t *process = task->process;
   if(process) {
      window_term_printf("Allocated: %i (%ikb)\n", task->process->no_allocated, task->process->no_allocated*PAGE_SIZE/1000);
      window_term_printf("Event queue size: %i\n", kqueue_count(&task->event_queue));
      window_term_printf("Process uid: %u, threads: %i, privileged: %i\n", process->uid, process->no_threads, process->privileged);
      window_term_printf("Working dir: %s\n", process->working_dir);
      window_term_printf("Prog mem: 0x%h - 0x%h\n", process->vmem_start, process->vmem_end);
      window_term_printf("Heap: 0x%h - 0x%h\n", process->heap_start, process->heap_end);
      window_term_printf("Exe path: %s\n", process->exe_path);
      window_term_printf("Window: %i\n", process->window);
      window_term_printf("kobjs: %i\n", process->kobj_count);
   }
}

void term_cmd_channels() {
   extern msg_port_t *ports;
   msg_port_t *cur = ports;
   while(cur) {
      window_term_printf("Port %s (owner process %u), %i channels\n", cur->name, cur->owner->uid, cur->channel_count);
      for(int i = 0; i < cur->channel_count; i++) {
         msg_channel_t *channel = cur->channels[i];
         if(!channel) continue;
         window_term_printf("Channel (handles %u/%u) (server %s / client %s)\n", channel->conns[0].handle, channel->conns[1].handle, channel->conns[0].connected ? "connected" : "disconnected", channel->conns[1].connected ? "connected" : "disconnected");
      }
      cur = cur->next;
   }
}

void term_cmd_signal(char *arg) {
   char name[32], c_flag[9];
   if(!strsplit(name, c_flag, arg, ' ')) {
      window_term_printf("Invalid args\n");
      return;
   }
   uint32_t flag = window_term_argtouint(c_flag);
   msg_port_t *port = find_port_by_name(name);
   if(!port) {
      window_term_printf("Port %s not found\n", name);
      return;
   }
   int r = port_signal(port, flag);
   window_term_printf("Sent signal %u to port %s - status %i\n", flag, name, r);
}

void term_cmd_mtrr() {
   mtrr_print();
}

void term_cmd_handles(char *arg) {
   if(strlen(arg) == 0) {
      for(int i = 0; i < TOTAL_TASKS; i++) {
         task_state_t *task = &gettasks()[i];
         if(!task->enabled || task != task->process->threads[0])
            continue;
         window_term_printf("Task %i - %i handles\n", i, task->process->kobj_count);
      }
      return;
   }
   int id = strtoint(arg);
   if(id < 0 || id >= TOTAL_TASKS) {
      window_term_printf("Task not found\n");
      return;
   }
   task_state_t *task = &gettasks()[id];
   if(!task->enabled) return;
   process_t *process = task->process;
   window_term_printf("Process %u - %i handles\n", process->uid, task->process->kobj_count);
   for(int i = 0; i < PROCESS_MAX_KOBJ; i++) {
      kobj_ref_t *ref = task->process->kobj[i];
      if(!ref || !ref->inst) continue;
      window_term_printf("%i (%i) - inst %u type %i refcount %i\n", i, i | (KOBJ_HANDLE_CHECK(ref->uid) << 16), ref->inst->uid, ref->inst->obj->type, ref->inst->ref_count);
   }

   vm_region_t *region = process->regions;
   while(region) {
      window_term_printf("Region 0x%h size 0x%h inst %u type %u\n", region->vaddr, region->size, region->inst->uid, region->inst->obj->type);
      region = region->next;
   }
}

void term_cmd_time() {
   char names[7][12] = {
      "Monday",
      "Tuesday",
      "Wednesday",
      "Thursday",
      "Friday",
      "Saturday",
      "Sunday"
   };
   time_t t = get_time();
   window_term_printf("%s %i/%i/%i %i:%i:%i\n", names[(t.weekday+5)%7], t.day, t.month, t.year, t.hours, t.minutes, t.seconds);
}

void term_cmd_default(char *command) {
   gui_drawchar('\'', 1);
   window_term_printf(command, 1);
   gui_drawchar('\'', 1);
   window_term_printf(": UNRECOGNISED", 4);
}

void window_term_checkcmd(void *window) {
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
      term_cmd_prog1();
   else if(strequ(command, "PROG2"))
      term_cmd_prog2();
   else if(strequ(command, "TEST"))
      term_cmd_test();
   else if(strequ(command, "DESKTOP"))
      term_cmd_desktop();
   else if(strequ(command, "REDRAWALL"))
      term_cmd_redrawall();
   else if(strequ(command, "PADDING"))
      term_cmd_padding((char*)arg);
   else if(strequ(command, "LAUNCH"))
      term_cmd_launch((char*)arg);
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
   else if(strequ(command, "CHANNELS"))
      term_cmd_channels();
   else if(strequ(command, "SIGNAL"))
      term_cmd_signal(arg);
   else if(strequ(command, "MTRR"))
      term_cmd_mtrr();
   else if(strequ(command, "HANDLES"))
      term_cmd_handles((char*)arg);
   else if(strequ(command, "TIME"))
      term_cmd_time();
   else
      term_cmd_default((char*)command);
   
   if(!strequ(command, "CLEAR"))
      gui_drawchar('\n', 0);
   gui_draw();
}
