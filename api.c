#include "api.h"
#include "lib/api.h" // shared header

#include "tasks.h"
#include "window.h"
#include "fat.h"
#include "api.h"
#include "bmp.h"
#include "events.h"
#include "font.h"
#include "windowmgr.h"
#include "window_popup.h"
#include "fs.h"
#include "time.h"
#include "futex.h"
#include "shared.h"
#include "pci.h"

// helper funcs

static inline gui_window_t *api_get_window() {
   if(get_current_task_window() < 0) return NULL;
   return &gui_get_windows()[get_current_task_window()];
}

static inline void api_write_to_task(char *out) {
   task_write_to_window(get_current_task(), out, false);
}

void api_printf(char *format, ...) {
   char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   api_write_to_task(buffer);
   free((uint32_t)buffer, 512);
}

gui_window_t *api_get_cwindow(int cindex) {
   gui_window_t *mainwindow = api_get_window();
   if(!mainwindow)
      return NULL;
   if(cindex == -1)
      return mainwindow;
   // get child window
   if(cindex < 0 || cindex >= mainwindow->child_count)
      return NULL;

   return mainwindow->children[cindex];
}

static inline int api_validate_str(char *str, int maxlen) {
   return task_validate_str(get_current_task_state(), str, maxlen);
}

static inline bool api_validate_mem(void *mem, int len, bool rw) {
   return task_validate_mem(get_current_task_state(), mem, len, rw);
}

static inline int api_validate_maxsize(void *mem, int max, bool rw) {
   return task_validate_maxsize(get_current_task_state(), mem, max, rw);
}

// api funcs

#define API_WRITESTR_MAX_LENGTH 0x2000

void api_write_string(registers_t *regs) {
   task_state_t *task = get_current_task_state();
   // write ebx
   // IN ebx str
   // IN ecx window/-1
   char *out;
   if(task->process->vmem_start == 0) // not elf
      out = (char*)(task->process->prog_entry + regs->ebx);
   else // elf
      out = (char*)regs->ebx;
   
   int len = api_validate_str(out, API_WRITESTR_MAX_LENGTH);
   if(len == -1) return; // invalid buffer
   bool truncate = false;
   // truncate long writes
   if(len == -2) {
      truncate = true;
      char *newout = malloc(API_WRITESTR_MAX_LENGTH+1);
      if(!newout) return;
      memcpy(newout, out, API_WRITESTR_MAX_LENGTH);
      newout[API_WRITESTR_MAX_LENGTH] = '\0';
      out = newout;
   }
   // write to task's main window
   if((int)regs->ecx == -1) {
      api_write_to_task(out);
      if(truncate)
         free((uint32_t)out, API_WRITESTR_MAX_LENGTH+1);
      return;
   }
   // write to specified window
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) {
      debug_printf("api_write_string: invalid window index %i\n", regs->ecx);
      api_write_to_task(out);
      if(truncate)
         free((uint32_t)out, API_WRITESTR_MAX_LENGTH+1);
      return;
   }
   int windowindex = get_window_index_from_pointer(window);
   window_writestr(out, window->txtcolour, windowindex);
   if(truncate)
      free((uint32_t)out, API_WRITESTR_MAX_LENGTH+1);
}

void api_write_number(registers_t *regs) {
   // write ebx
   api_printf("%i", regs->ebx);
}

void api_write_uint(registers_t *regs) {
   // write ebx
   api_printf("%u", regs->ebx);
}

void api_write_newline() {
   api_printf("\n");
}

void api_write_string_at(registers_t *regs) {
   // IN: ebx = string address
   // IN: ecx = x
   // IN: edx = y
   // IN: esi = colour (-1 for window colour)
   // IN: edi = cindex (-1 for main window)
   char *str = (char*)regs->ebx;
   if(api_validate_str(str, 256) < 0) return;
   gui_window_t *window = api_get_cwindow(regs->edi);
   if(!window) return;
   int colour = regs->esi;
   if(colour == -1)
      colour = window->txtcolour;
   int windowindex = get_window_index_from_pointer(window);
   window_writestrat(str, colour, regs->ecx, regs->edx, windowindex);
}

void api_write_number_at(registers_t *regs) {
   // IN: ebx = num
   // IN: ecx = x
   // IN: edx = y
   window_writenumat(regs->ebx, 0, regs->ecx, regs->edx, get_current_task_window());
}

void api_yield(registers_t *regs) {
   switch_task(regs);
}

// potentially broken
void api_print_program_stack(registers_t *regs) {
   for(int i = 0; i < 64; i++)
      api_printf("%i ", ((int*)regs->esp)[i]);
}

void api_print_stack() {
   // show current stack contents
   int esp;
   asm("movl %%esp, %0" : "=r"(esp));

   for(int i = 0; i < 64; i++)
      api_printf("%i ", ((int*)esp)[i]);
}

void api_return_framebuffer(registers_t *regs) {
   // IN: ebx = window index or -2 for screen surface
   // OUT: ebx = framebuffer address (NULL for screen surface)
   // OUT: ecx = width
   // OUT: edx = height
   // NULL ebx on fail
   if(regs->ebx == (uint32_t)-2) {
      // screen surface
      regs->ebx = 0;
      regs->ecx = gui_get_width();
      regs->edx = gui_get_height();
      return;
   }

   gui_window_t *window = api_get_cwindow(regs->ebx);
   if(!window) {
      regs->ebx = (uint32_t)NULL;
      return;
   }
   uint32_t framebuffer = (uint32_t)window->framebuffer;
   uint32_t size = window->framebuffer_size;
   // map to task
   for(uint32_t i = framebuffer/0x1000; i < (framebuffer+size+0xFFF)/0x1000; i++) {
      map(get_current_task_pagedir(), i*0x1000, i*0x1000, 1, 1, 0);
   }
   regs->ebx = framebuffer;
   regs->ecx = window->surface.width;
   regs->edx = window->surface.height;
}

void api_return_window_width(registers_t *regs) {
   // IN: ebx = window index
   // OUT: ebx = framebuffer width
   gui_window_t *window = api_get_cwindow(regs->ebx);
   if(!window) {
      regs->ebx = 0;
      return;
   }
   regs->ebx = window->width - (window->scrollbar && window->scrollbar->visible ? 14 : 0);
}

void api_return_window_height(registers_t *regs) {
   // IN: ebx = window index
   // OUT: ebx = framebuffer height
   gui_window_t *window = api_get_cwindow(regs->ebx);
   if(!window) {
      regs->ebx = 0;
      return;
   }
   regs->ebx = window->height - TITLEBAR_HEIGHT;
}

void api_redraw_window(registers_t *regs) {
   // IN: ebx = window index
   // check if selected?
   gui_window_t *window = api_get_cwindow(regs->ebx);
   if(!window) return;
   window->needs_redraw = true;
   gui_draw_window(get_window_index_from_pointer(window));
}

void api_redraw_pixel(registers_t *regs) {
   // IN: ebx = x
   // IN: ecx = y

   // x, y
   window_draw_content_region(api_get_window(), regs->ebx, regs->ecx, 1, 1);
}

void api_end_task(registers_t *regs) {
   // return with status ebx
   api_printf("Ending with status %i\n", regs->ebx);

   end_task(get_current_task(), regs);
}

void api_override_checkcmd(registers_t *regs) {
   // override terminal checkcmd function with ebx
   if(!api_get_window()) return;
   uint32_t addr = regs->ebx;
   api_get_window()->checkcmd_func = (void *)(addr);
}

void api_override_mouseclick(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->click_func = (void *)(regs->ebx);
}

void api_override_draw(registers_t *regs) {
   // override draw function with ebx
   // IN: ebx = func
   // IN: ecx = window
   uint32_t addr = regs->ebx;
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(addr != 0 || !window) return;
   window->draw_func = (void *)(addr);
}

void api_override_resize(registers_t *regs) {
   // override resize function with ebx
   // IN: ebx = function address
   // IN: ecx = window cindex
   uint32_t addr = regs->ebx;
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->resize_func = (void *)(addr);
}

void api_override_drag(registers_t *regs) {
   // override drag function with ebx
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   uint32_t addr = regs->ebx;
   window->drag_func = (void *)(addr);
}

void api_override_release(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->release_func = (void *)(regs->ebx);
}

void api_override_keypress(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->keypress_func = (void *)(regs->ebx);
}

void api_override_keyrelease(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->keyrelease_func = (void *)(regs->ebx);
}

void api_override_hover(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->hover_func = (void *)(regs->ebx);
}

void api_override_close(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   if(regs->ecx == (uint32_t)-1) {
      debug_printf("Cannot override close on main window\n");
      return;
   }
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->close_func = (void *)(regs->ebx);
}

void api_override_rightclick(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->rightclick_func = (void *)(regs->ebx);
}

void api_override_mouseout(registers_t *regs) {
   // IN: ebx = function address
   // IN: ecx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->mouseout_func = (void *)(regs->ebx);
}

void api_end_subroutine(registers_t *regs) {
   task_subroutine_end(regs) ;
}

void api_malloc(registers_t *regs) {
   // kmalloc
   // IN: ebx = size
   // OUT: ebx = addr
   int size = regs->ebx;
   uint32_t *mem = malloc(size);

   task_state_t *task = get_current_task_state();

   // identity map
   task->process->no_allocated += map_size(task->process->page_dir, (uint32_t)mem, (uint32_t)mem, size, 1, 1, 0);

   regs->ebx = (uint32_t)mem;
}

void api_free(registers_t *regs) {
   // IN: ebx = addr
   // IN: ecx = size
   uint32_t mem = regs->ebx;
   uint32_t size = regs->ecx;
   free(mem, size);

   // unmap from user
   task_state_t *task = get_current_task_state();
   task->process->no_allocated -= map_size(task->process->page_dir, mem, mem, size, 0, 1, 0);
}

void api_draw_bmp(registers_t *regs) {
   // IN: ebx = bmp address
   // IN: ecx = x
   // IN: edx = y
   // IN: esi = scale
   if(get_current_task_window() < 0) return;
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];
   if(window->closed) return;

   bmp_draw((uint8_t*)regs->ebx, window->framebuffer, window->width, window->height - TITLEBAR_HEIGHT, regs->ecx, regs->edx, regs->edi, regs->esi);
}

void api_clear_window(registers_t *regs) {
   // IN: ebx = window cindex
   gui_window_t *window = api_get_cwindow(regs->ebx);
   if(!window || window->closed) return;
   window_clearbuffer(window, window->bgcolour);
   window->text_index = 0;
   window->text_x = getFont()->padding;
   window->text_y = getFont()->padding;
}

void api_queue_event(registers_t *regs) {
   // IN: ebx = callback function
   // IN: ecx = how long to wait
   // IN: edx = msg
   uint32_t callback = regs->ebx;
   uint32_t delta = regs->ecx;

   events_add(delta, (void *)callback, (void*)regs->edx, get_current_task());
}

#define API_LAUNCH_MAX_ARGS 32
void api_launch_task(registers_t *regs) {
   // IN: ebx = path
   // IN: ecx = argc
   // IN: edx = args
   // IN: esi = copy environment of current task (use same wd & file descriptors)
   // IN: edi = paused
   // OUT: ebx = pid (-1 for failure)

   char *path = (char*)regs->ebx;
   int argc = (int)regs->ecx;
   char **args = (char**)regs->edx;
   bool copy = (bool)regs->esi;
   bool paused = (bool)regs->edi;

   if(argc > API_LAUNCH_MAX_ARGS) {
      debug_printf("api_launch_task: max %u args\n", API_LAUNCH_MAX_ARGS);
      regs->ebx = -1;
      return;
   }

   if(api_validate_str(path, 256) < 0) {
      debug_printf("api_launch_task: invalid path\n");
      regs->ebx = -1;
      return;
   }

   // copy args
   // todo: handle malloc failures
   char **copied_args = NULL;
   if(argc > 0 && args != NULL) {
      if(!api_validate_mem(args, argc*sizeof(char*), false)) {
         debug_printf("api_launch_task: invalid args\n");
         regs->ebx = -1;
         return;
      }

      copied_args = malloc(sizeof(char*) * (argc+1)); // args includes training null str
      if(!copied_args) { regs->ebx = -1; return; }
      memset(copied_args, 0, sizeof(char*) * (argc+1));
      for(int i = 0; i < argc; i++) {
         if(args[i] != NULL) {
            int len = api_validate_str(args[i], 0x1000);
            if(len < 0) {
               debug_printf("api_launch_task: arg %u is invalid\n", i);
               copied_args[i] = NULL;
               continue;
            }
            copied_args[i] = malloc(len + 1);
            if(!copied_args[i]) {
               free_launch_args(copied_args, argc);
               regs->ebx = -1;
               return;
            }
            strcpy(copied_args[i], args[i]);
         } else {
            copied_args[i] = NULL;
         }
      }
      copied_args[argc] = NULL;
   }

   // copy path
   char pathbuf[256];
   strcpy(pathbuf, path);

   int calling_task = get_current_task();
   task_state_t *parenttask = &gettasks()[calling_task];
   int oldwindow = getSelectedWindowIndex();

   // set up the new task without switching context
   int new_task = tasks_setup_elf(regs, pathbuf, argc, copied_args, !copy, copy);
   if(new_task < 0) {
      free_launch_args(copied_args, argc);
      debug_printf("api_launch_task failed\n");
      regs->ebx = -1;
      return;
   }

   task_state_t *task = &gettasks()[new_task];

   if(copy) {
      // duplicate parent's fds
      for(int i = 0; i < parenttask->process->fd_count; i++)
         task->process->file_descriptors[i] = fs_dup(parenttask->process->file_descriptors[i]);
      task->process->fd_count = parenttask->process->fd_count;
      strcpy(task->process->working_dir, parenttask->process->working_dir);
      int child_win = task->process->window;
      window_close(NULL, child_win);
      task->process->window = -1;
      setSelectedWindowIndex(oldwindow);
      gui_redrawall();
   }

   // map args to new task
   if(argc > 0 && copied_args != NULL) {
      for(int i = 0; i < argc; i++) {
         if(copied_args[i] != NULL)
            map(task->process->page_dir, (uint32_t)copied_args[i], (uint32_t)copied_args[i], 1, 1, 0);
      }
      map(task->process->page_dir, (uint32_t)copied_args, (uint32_t)copied_args, 1, 1, 0);
   }

   task->enabled = true;
   task->paused = paused;
   task->unpausable = paused;
   if(paused) {
      if(get_current_task() == new_task)
         switch_task(regs); // yield
   }
   regs->ebx = new_task;
}

void api_set_window_title(registers_t *regs) {
   // IN: ebx = title
   // IN: ecx = window child index
   int cindex = regs->ecx;
   char *title = (char*)regs->ebx;
   if(api_validate_str(title, 20) < 0) return;

   if(get_current_task_window() < 0) return;
   gui_window_t *mainwindow = &gui_get_windows()[get_current_task_window()];
   if(cindex == -1) {
      strcpy(mainwindow->title, title);
      window_draw_outline(mainwindow, true);
      toolbar_draw();

      return;
   }
   if(cindex >= 0 && cindex < mainwindow->child_count) {
      gui_window_t *window = mainwindow->children[cindex];
      int windex = get_window_index_from_pointer(window);
      if(windex) {
         strcpy(window->title, title);
         window_draw_outline(window, true);
         toolbar_draw();
      }
      return;
   }
}

void api_set_working_dir(registers_t *regs) {
   // IN: ebx = path
   char *path = (char*)regs->ebx;
   if(api_validate_str(path, 256) < 0)
      return;
   strcpy(gettasks()[get_current_task()].process->working_dir, path);
}

void api_get_working_dir(registers_t *regs) {
   // IN: ebx = size 256 buf for working dif
   // OUT: ebx = success
   char *buf = (char*)regs->ebx;
   if(!api_validate_mem(buf, 256, true)) {
      regs->ebx = 0;
      return;
   }
   strcpy((char*)regs->ebx, gettasks()[get_current_task()].process->working_dir);
   regs->ebx = 1;
}

void api_debug_write_str(registers_t *regs) {
   debug_printf("t%iw%i: %s\n", get_current_task(), get_current_task_window(), regs->ebx);
}

void api_sbrk(registers_t *regs) {
   // change heap size

   // IN: ebx - int increment/delta
   int delta = (int)regs->ebx;
   task_state_t *task = &gettasks()[get_current_task()];

   uint32_t old_heap_end = (uint32_t)task->process->heap_end; // saved for return
   int old_heapsize = (int)(task->process->heap_end - task->process->heap_start);
   int new_heapsize = old_heapsize + delta;

   if(new_heapsize < 0) {
      regs->ebx = (uint32_t)-1;  // error
      return;
   }

   uint32_t old_end = page_align_up(task->process->heap_start + old_heapsize);
   uint32_t new_end = page_align_up(task->process->heap_start + new_heapsize);

   if(new_end < old_end) {
      // shrink
      int delta_pages = (old_end - new_end) / 0x1000;
      debug_printf("Shrink by %u pages\n", delta_pages);
      
      uint32_t *physical_addrs = NULL;
      if(delta_pages > 0) {
         physical_addrs = malloc(delta_pages * sizeof(uint32_t));
         if(physical_addrs) {
            for(int i = 0; i < delta_pages; i++) {
               uint32_t virt = old_end - (i + 1) * 0x1000;
               physical_addrs[i] = page_getphysical(task->process->page_dir, virt);
            }
         }
      }
      
      // unmap pages from old_end downward
      for(int i = 0; i < delta_pages; i++) {
         uint32_t virt_addr = old_end - (i + 1) * 0x1000;
         unmap(task->process->page_dir, virt_addr);
      }
      
      // free physical memory
      if(physical_addrs) {
         for(int i = 0; i < delta_pages; i++) {
            if(physical_addrs[i]) {
               free(physical_addrs[i], 0x1000);
            }
         }
         free((uint32_t)physical_addrs, 0x1000);
      }
   }

   task->process->heap_end = task->process->heap_start + new_heapsize;
   regs->ebx = old_heap_end;

}

static int alloc_fd(process_t *process) {
   for(int i = 0; i < TASK_MAX_FDS; i++) {
      if(process->file_descriptors[i] == NULL) {
         if(i >= process->fd_count) process->fd_count = i + 1;
         return i;
      }
   }
   return -1;
}

void api_open(registers_t *regs) {
   // IN: ebx - char* path
   // IN: ecx - flag (0 read, 1 write)
   // OUT: ebx - int fd
   char *path = (char*)regs->ebx;
   if(api_validate_str(path, 256) < 0) {
      debug_printf("api_open: couldn't parse filename\n");
      regs->ebx = -1;
      return;
   }

   task_state_t *task = get_current_task_state();
   fs_file_t *file = fs_open(path);
   if(!file) {
      if(regs->ecx == 1) { // write
         debug_printf("api_open: creating new file %s\n", path);
         file = fs_new(path);
      }
      if(!file) {
         debug_printf("api_open: could not create new file\n");
         regs->ebx = -1;
         return;
      }
   }
   int fd = alloc_fd(task->process);
   if(fd < 0) {
      debug_printf("api_open: too many open files\n");
      fs_close(file);
      regs->ebx = -1;
      return;
   }
   task->process->file_descriptors[fd] = file;
   regs->ebx = fd;
}

void api_read_stdin_callback(void *regs, char *buffer) {
   task_state_t *task = get_current_task_state();
   int w = task->process->file_descriptors[0]->window_index;
   registers_t *r = (registers_t*)regs;
   if(w > 0 && w < getWindowCount() && getWindow(w) && !getWindow(w)->closed) {
      gui_window_t *window = getWindow(w);
      strcpy(window->read_buffer, buffer);
      task->paused = false;
      r->ebx = strlen(buffer);
      switch_to_task(task->task_id, regs); // wake
      task_execute_queued_subroutine(regs, (void*)task->task_id); // check for queued events while task was paused
   } else {
      debug_printf("Couldn't find window\n");
      r->ebx = -1;
   }
}

void api_read_fd_callback(registers_t *regs, int task, int size) {
   gettasks()[task].paused = false;
   switch_to_task(task, regs); // wake
   regs->ebx = size;
   task_execute_queued_subroutine(regs, (void*)task); // check for queued events while task was paused
}

void api_read(registers_t *regs) {
   // IN: ebx - int fd
   // IN: ecx - char *buf
   // IN: size_t count
   // OUT: size_t bytes read
   task_state_t *task = get_current_task_state();
   int fd = regs->ebx;
   char *buf = (char*)regs->ecx;
   int count = regs->edx;

   if(count == 0) { regs->ebx = 0; return; }
   if(count < 0) { regs->ebx = -1; return; }
   
   if(fd < 0 || fd >= task->process->fd_count) {
      debug_printf("read: fd not found\n");
      regs->ebx = -1;
      return;
   }

   fs_file_t *file = task->process->file_descriptors[fd];
   if(!file || !file->active) {
      debug_printf("read: fd inactive\n");
      regs->ebx = -1;
      return;
   }

   void *callback;
   if(file->type == FS_TYPE_TERM) {
      callback = &api_read_stdin_callback;
   } else if(file->type == FS_TYPE_FILE) {
      int maxsize = api_validate_maxsize(buf, count, 1);
      if(maxsize < 0 || (count > 0 && maxsize == 0)) {
         debug_printf("api_read: invalid buffer\n");
         regs->ebx = -1;
         return;
      }
      count = maxsize;
      callback = &api_read_fd_callback;
   } else if(file->type == FS_TYPE_PIPE) {
      callback = NULL;
   } else {
      debug_printf("api_read: invalid fd type\n");
      regs->ebx = -1;
      return;
   }

   int result = fs_read(file, buf, count, callback, get_current_task());
   regs->ebx = result;
   if(result == FS_BLOCKING) {
      if(file->type == FS_TYPE_PIPE && file->pipe) {
         if(count >= FS_PIPE_BUF_SIZE)
            count = FS_PIPE_BUF_SIZE;
         int maxsize = api_validate_maxsize(buf, count, 1);
         if(maxsize < 0 || (count > 0 && maxsize == 0)) {
            debug_printf("api_read: invalid buffer\n");
            regs->ebx = -1;
            return;
         }
         file->pipe->read_buf = buf;
         file->pipe->read_size = maxsize;
      }
      task->paused = true;
      switch_task(regs); // yield
   }
   if(result > 0 && file->type == FS_TYPE_PIPE && file->pipe) {
      int writer_task = file->pipe->write_waiting_task;
      if(writer_task >= 0) {
         if(fs_pipe_wake_writer(file->pipe))
            switch_to_task(writer_task, regs);
      }
   }
}

void api_read_dir(registers_t *regs) {
   // IN: ebx - char *path
   // OUT: ebx - fs_dir_content_t *
   char *path = (char*)regs->ebx;
   if(api_validate_str(path, 256) < 0) {
      regs->ebx = 0;
      return;
   }
   fs_dir_content_t *content = fs_read_dir(path);
   if(!content) {
      regs->ebx = 0;
      return;
   }
   page_dir_entry_t *page_dir = get_current_task_pagedir();
   map_size(page_dir, (uint32_t)content, (uint32_t)content, sizeof(fs_dir_content_t), 1, 1, 0);
   uint32_t entries_size = sizeof(fs_dir_entry_t)*content->size;
   map_size(page_dir, (uint32_t)content->entries, (uint32_t)content->entries, entries_size, 1, 1, 0);
   regs->ebx = (uint32_t)content;
}

void api_write(registers_t *regs) {
   // IN: ebx - int fd
   // IN: ecx - uint8_t *buffer
   // IN: edx - size_t size
   // OUT: ebx - size_t bytes written
   task_state_t *task = get_current_task_state();
   int fd = regs->ebx;
   uint8_t *buffer = (uint8_t*)regs->ecx;
   int size = (int)regs->edx;
   int maxsize = api_validate_maxsize(buffer, size, 0);
   if(maxsize < 0 || (size > 0 && maxsize == 0)) {
      debug_printf("api_write: invalid buffer\n");
      regs->ebx = -1;
      return;
   }
   if(fd < 0 || fd >= task->process->fd_count) {
      debug_printf("api_write: fd not found\n");
      regs->ebx = -1;
      return;
   }

   fs_file_t *file = task->process->file_descriptors[fd];
   if(!file || !file->active) {
      debug_printf("api_write: fd inactive\n");
      regs->ebx = -1;
      return;
   }

   int result = fs_write(file, buffer, maxsize, task->task_id);
   regs->ebx = result;

   if(result == FS_WRITE_WAIT && file->type == FS_TYPE_PIPE && file->pipe) {
      file->pipe->write_buf = buffer;
      file->pipe->write_size = maxsize;
      task->paused = true;
      switch_task(regs); // yield
   }

   if(result > 0 && file->type == FS_TYPE_PIPE && file->pipe) {
      int reader_task = file->pipe->read_waiting_task;
      if(reader_task >= 0) {
         if(fs_pipe_wake_reader(file->pipe))
            switch_to_task(reader_task, regs);
      }
   }
}

void api_fsize(registers_t *regs) {
   // IN: ebx - int fd
   // OUT: ebx - filesize
   task_state_t *task = get_current_task_state();
   int fd = regs->ebx;
   if(fd < 0 || fd >= task->process->fd_count || !task->process->file_descriptors[fd]) {
      regs->ebx = -1;
   } else {
      regs->ebx = fs_filesize(task->process->file_descriptors[fd]);
   }
}

void api_mkdir(registers_t *regs) {
   // IN: ebx - dir path
   // OUT: ebx - bool success
   char *path = (char*)regs->ebx;
   if(api_validate_str(path, 256) < 0) {
      regs->ebx = 0;
      return;
   }
   regs->ebx = fs_mkdir(path);
}

void api_unlink(registers_t *regs) {
   // IN: ebx - file path
   // OUT: ebx - bool success
   char *path = (char*)regs->ebx;
   if(api_validate_str(path, 256) < 0) {
      regs->ebx = 0;
      return;
   }
   regs->ebx = fs_unlink(path);
}

void api_rmdir(registers_t *regs) {
   // IN: ebx - dir path
   // OUT: ebx - bool success
   char *path = (char*)regs->ebx;
   if(api_validate_str(path, 256) < 0) {
      regs->ebx = 0;
      return;
   }
   regs->ebx = fs_rmdir(path);
}

void api_rename(registers_t *regs) {
   // IN: ebx - old path
   // IN: ecx - new name
   // OUT: ebx - bool success
   char *old_path = (char*)regs->ebx;
   char *new_path = (char*)regs->ecx;
   if(api_validate_str(old_path, 256) < 0
   || api_validate_str(new_path, 256) < 0) {
      regs->ebx = 0;
      return;
   }
   regs->ebx = fs_rename(old_path, new_path);
}

void api_new_file(registers_t *regs) {
   // IN: ebx - file path
   // OUT: ebx - int fd / -1 on error
   char *path = (char*)regs->ebx;
   if(api_validate_str(path, 256) < 0) {
      regs->ebx = -1;
      debug_printf("api_new_file: invalid path\n");
      return;
   }
   fs_file_t *file = fs_new(path);
   if(!file) {
      regs->ebx = -1;
      debug_printf("api_new_file: error\n");
      return;
   }
   task_state_t *task = get_current_task_state();
   int fd = alloc_fd(task->process);
   if(fd < 0) {
      debug_printf("api_new_file: too many open files\n");
      fs_close(file);
      regs->ebx = -1;
      return;
   }
   task->process->file_descriptors[fd] = file;
   regs->ebx = fd;
}

void api_seek(registers_t *regs) {
   // IN: ebx - int fd
   // IN: ecx - int offset
   // IN: edx - int type
   // OUT: pos
   task_state_t *task = get_current_task_state();
   int fd = regs->ebx;
   int offset = regs->ecx;
   int type = regs->edx;
   if(fd < 0 || fd >= task->process->fd_count || !task->process->file_descriptors[fd]) {
      regs->ebx = -1;
   } else {
      regs->ebx = fs_seek(task->process->file_descriptors[fd], offset, type);
   }
}

void api_create_scrollbar(registers_t *regs) {
   // IN: ebx - callback (int deltaY, int offsetY)
   // IN: ecx - window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window_create_scrollbar(window, (void*)regs->ebx);
}

void api_set_scrollable_height(registers_t *regs) {
   // IN: ebx - height
   // IN: ecx - window cindex
   // OUT: ebx - new window width (not including scrollbar)
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window_set_scrollable_height(regs, window, regs->ebx);
   regs->ebx = window->width - (window->scrollbar && window->scrollbar->visible ? 14 : 0);
}

void api_scroll_to(registers_t *regs) {
   // IN: ebx - y
   // IN: ecx - window cindex
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   setSelectedWindowIndex(get_window_index_from_pointer(window));
   window_scroll_to(regs, regs->ebx);
}

void api_set_window_size(registers_t *regs) {
   // IN: ebx width
   // IN: ecx height
   // doesn't call resize function
   int width = regs->ebx;
   int height = regs->ecx + TITLEBAR_HEIGHT;

   if(width < 20 || height < 20) return;

   gui_window_t *window = api_get_window();
   if(!window) return;
   int maxwidth = gui_get_surface()->width - 5;
   int maxheight = gui_get_surface()->height - TOOLBAR_HEIGHT;
   if(width > maxwidth)
      width = maxwidth;
   if(height > maxheight)
      height = maxheight;
   if(window->x + width > maxwidth)
      window->x = 0;
   if(window->y + height > maxheight)
      window->y = 0;
   window_resize(NULL, window, width, height);
   gui_redrawall();
}

void api_get_font_info(registers_t *regs) {
   // OUT: ebx: system font width
   // OUT: ecx: system font height
   // OUT: edx: system font padding
   regs->ebx = getFont()->width;
   regs->ecx = getFont()->height;
   regs->edx = getFont()->padding;
}

void api_create_window(registers_t *regs) {
   // IN ebx: width
   // IN ecx: height
   // OUT ebx: child index
   int width = regs->ebx;
   int height = regs->ecx;
   if(width < 50) width = 50;
   if(height < 50) height = 50;

   gui_window_t *parent = api_get_window();
   if(parent->child_count == W_CHILDCOUNT) {
      regs->ebx = -1;
      return;
   }
   int i = windowmgr_add();
   if(i <= 0) {
      regs->ebx = -1;
      return;
   }
   gui_window_t *newwindow = getWindow(i);
   window_removefuncs(newwindow);
   int c = parent->child_count;
   parent->children[c] = newwindow;
   parent->child_count++;
   newwindow->parent = parent;
   window_resize(regs, newwindow, width, height);
   regs->ebx = c;
}

void api_close_window(registers_t *regs) {
   // IN: ebx: child index
   // OUT ebx: bool success
   int cindex = regs->ebx;
   gui_window_t *mainwindow = api_get_window();
   if(!mainwindow) return;
   if(cindex == -1) {
      // close main window and all children without killing task
      window_close(regs, get_current_task_state()->process->window);
      regs->ebx = 0;
      return;
   }
   // close child window
   if(cindex < 0 || cindex >= mainwindow->child_count) {
      regs->ebx = 0;
      return;
   }
   gui_window_t *window = mainwindow->children[cindex];
   int index = get_window_index_from_pointer(window);
   debug_printf("Closing window %i\n", index);
   window_close(NULL, index);
   mainwindow->children[cindex] = NULL;
   if(getSelectedWindow() == NULL)
      setSelectedWindowIndex(get_current_task_state()->process->window);
}

void api_create_thread(registers_t *regs) {
   // IN: ebx: location of routine to invoke for new thread
   // OUT: ebx: task index, -1 on failure

   int task_index = get_free_task_index();
   if(task_index == -1) {
      regs->ebx = -1;
      return;
   }

   task_state_t *parent = get_current_task_state();

   create_task_entry(task_index, regs->ebx, parent->process->prog_size, parent->process->privileged, parent->process);
   task_state_t *thread = &gettasks()[task_index];
   map_size(thread->process->page_dir, thread->stack_top - TASK_STACK_SIZE, thread->stack_top - TASK_STACK_SIZE, TASK_STACK_SIZE, 1, 1, 0);

   // copy over essential fields
   thread->registers.ds = USR_DATA_SEG | 3;
   thread->registers.cs = USR_CODE_SEG | 3; // user code segment
   thread->registers.ss = USR_DATA_SEG | 3;
   thread->registers.eflags = regs->eflags;
   thread->registers.useresp = thread->stack_top;
   thread->enabled = true;

   regs->ebx = task_index;
}

void api_kill_task(registers_t *regs) {
   // todo: check permission
   // IN: ebx: task index
   end_task(regs->ebx, regs);
}

void api_set_setting(registers_t *regs) {
   // IN: ebx: int setting
   // IN: ecx: value
   // OUT: ebx: 0 = success, -1 failure

   // todo require privilege
   windowmgr_settings_t *settings = windowmgr_get_settings();

   switch(regs->ebx) {
      case SETTING_DESKTOP_BGIMG_ENABLED:
         settings->desktop_bgimg_enabled = (bool)regs->ecx;
         gui_redrawall();
         break;
      case SETTING_DESKTOP_BGIMG_PATH : {
         char *path = (char*)regs->ecx;
         if(api_validate_str(path, 256) < 0) {
            regs->ebx = -1; // invalid path
            return;
         }
         fat_dir_t *entry = fat_parse_path(path, true);
         if(entry == NULL || entry->attributes == 0x10) {
            regs->ebx = -1; // not found
            return;
         }
         strncpy(settings->desktop_bgimg, path, sizeof(settings->desktop_bgimg));

         uint8_t *img = fat_read_file(entry->firstClusterNo, entry->fileSize);
         desktop_setbgimg(img, entry->fileSize);
         free((uint32_t)entry, sizeof(fat_dir_t));
         break;
      }
      case SETTING_DESKTOP_ENABLED:
         settings->desktop_enabled = (bool)regs->ecx;
         gui_redrawall();
         break;
      case SETTING_SYS_FONT_PATH: {
         char *path = (char*)regs->ecx;
         if(api_validate_str(path, 256) < 0) {
            regs->ebx = -1; // invalid path
            return;
         }
         fat_dir_t *entry = fat_parse_path(path, true);
         if(entry == NULL || entry->attributes == 0x10) {
            regs->ebx = -1; // not found
            return;
         }
         strncpy(settings->font_path, path, sizeof(settings->font_path));
         fontfile_t *file = (fontfile_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
         font_load(file);
         free((uint32_t)entry, sizeof(fat_dir_t));

         // memory leak as never free prev file
         break;
      }
      case SETTING_THEME_TYPE:
         settings->theme = (int)regs->ecx;
         break;
      case SETTING_WIN_BGCOLOUR:
         settings->default_window_bgcolour = (uint16_t)regs->ecx;
         break;
      case SETTING_WIN_TITLEBARCOLOUR2:
         settings->titlebar_colour2 = (uint16_t)regs->ecx;
         break;
      case SETTING_WIN_TITLEBARCOLOUR:
         settings->titlebar_colour = (uint16_t)regs->ecx;
         break;
      case SETTING_WIN_TXTCOLOUR:
         settings->default_window_txtcolour = (uint16_t)regs->ecx;
         break;
      case SETTING_BGCOLOUR:
         extern uint16_t gui_bg;
         gui_bg = (uint16_t)regs->ecx;
         gui_redrawall();
         break;
      case SETTINGS_SYS_FONT_PADDING:
         getFont()->padding = (int)regs->ecx;
         break;
      case SETTING_THEME_GRADIENTSTYLE:
         settings->titlebar_gradientstyle = (int)regs->ecx;
         break;
      default:
         regs->ebx = -1;
         return;
   }

   regs->ebx = 0;
}

void api_get_setting(registers_t *regs) {
   // IN ebx - setting
   // OUT ebx - value
   windowmgr_settings_t *settings = windowmgr_get_settings();

   switch(regs->ebx) {
      case SETTING_DESKTOP_BGIMG_PATH:
         char *out = malloc(sizeof(settings->desktop_bgimg));
         strcpy(out, settings->desktop_bgimg);
         map_size(get_current_task_pagedir(), (uint32_t)out, (uint32_t)out, sizeof(settings->desktop_bgimg), 1, 1, 0);
         regs->ebx = (uint32_t)out;
         break;
      case SETTING_DESKTOP_ENABLED:
         regs->ebx = (uint32_t)settings->desktop_enabled;
         break;
      case SETTING_SYS_FONT_PATH:
         out = malloc(sizeof(settings->font_path));
         strcpy(out, settings->font_path);
         map_size(get_current_task_pagedir(), (uint32_t)out, (uint32_t)out, sizeof(settings->font_path), 1, 1, 0);
         regs->ebx = (uint32_t)out;
         break;
      case SETTING_THEME_TYPE:
         regs->ebx = (uint32_t)settings->theme;
         break;
      case SETTING_WIN_BGCOLOUR:
         regs->ebx = (uint32_t)settings->default_window_bgcolour;
         break;
      case SETTING_WIN_TITLEBARCOLOUR2:
         regs->ebx = (uint32_t)settings->titlebar_colour2;
         break;
      case SETTING_WIN_TITLEBARCOLOUR:
         regs->ebx = (uint32_t)settings->titlebar_colour;
         break;
      case SETTING_WIN_TXTCOLOUR:
         regs->ebx = (uint32_t)settings->default_window_txtcolour;
         break;
      case SETTING_BGCOLOUR:
         extern uint16_t gui_bg;
         regs->ebx = (uint32_t)gui_bg;
         break;
      case SETTINGS_SYS_FONT_PADDING:
         regs->ebx = (uint32_t)getFont()->padding;
         break;
      case SETTING_THEME_GRADIENTSTYLE:
         regs->ebx = (uint32_t)settings->titlebar_gradientstyle;
         break;
      default:
         regs->ebx = -1;
         return;
   }
}

void api_get_window_setting(registers_t *regs) {
   // IN: ebx setting index
   // IN: ecx window
   // OUT: ebx value
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   switch(regs->ebx) {
      case W_SETTING_BGCOLOUR:
         regs->ebx = window->bgcolour;
         break;
      case W_SETTING_TXTCOLOUR:
         regs->ebx = window->txtcolour;
         break;
   }
}

void api_set_window_setting(registers_t *regs) {
   // IN: ebx setting index
   // IN: ecx value
   // IN: edx window
   gui_window_t *window = api_get_cwindow(regs->edx);
   if(!window) return;
   switch(regs->ebx) {
      case W_SETTING_BGCOLOUR:
         window->bgcolour = regs->ecx;
         break;
      case W_SETTING_TXTCOLOUR:
         window->txtcolour = regs->ecx;;
         break;
   }
}

void api_set_window_position(registers_t *regs) {
   // IN: ebx x
   // IN: ecx y
   // IN: edx window
   int margin = 3;
   gui_window_t *window = api_get_cwindow(regs->edx);
   if(!window) return;
   int x = regs->ebx;
   int y = regs->ecx;
   if(x < margin) x = margin;
   if(y < margin) y = margin;
   if(x > (int)gui_get_width() - window->width - margin)
      x = (int)gui_get_width() - window->width - margin;
   if(y > (int)gui_get_height() - window->height - TOOLBAR_HEIGHT - margin)
      y = (int)gui_get_height() - window->height - TOOLBAR_HEIGHT - margin;
   window->x = x;
   window->y = y;
   gui_redrawall();
}

void api_get_window_position(registers_t *regs) {
   // IN: ebx window
   // OUT: ebx x
   // OUT: ecx y
   gui_window_t *window = api_get_cwindow(regs->ebx);
   if(!window) return;
   regs->ebx = window->x;
   regs->ecx = window->y;
}

void api_set_window_minimised(registers_t *regs) {
   // IN: ebx bool minimised
   // IN: ecx window
   gui_window_t *window = api_get_cwindow(regs->ecx);
   if(!window) return;
   window->minimised = regs->ebx;
   if(window == getSelectedWindow()) {
      getSelectedWindow()->active = false;
      setSelectedWindowIndex(-1);
   } else {
      if(!window->minimised)
         setSelectedWindowIndex(get_window_index_from_pointer(window));
   }
   gui_redrawall();
}

void api_get_tasks(registers_t *regs) {
   // OUT: ebx api_task_t *
   // OUT: ecx size
   api_task_t *tasks = malloc(sizeof(api_task_t)*TOTAL_TASKS);
   for(int i = 0; i < TOTAL_TASKS; i++) {
      task_state_t *task_state = &gettasks()[i];
      api_task_t *api_task = &tasks[i];
      api_task->id = task_state->task_id;
      api_task->enabled = task_state->enabled;
      api_task->paused = task_state->paused;
      if(!api_task->enabled || !task_state->process || task_state->process->no_threads == 0)
         continue;
      api_task->parentid = task_state->process->threads[0]->task_id;
      if(api_task->id != api_task->parentid)
         continue; // child process
      // parent thread
      process_t *process = task_state->process;
      api_task->heap_start = process->heap_start;
      api_task->heap_end = process->heap_end;
      api_task->prog_start = process->prog_start;
      api_task->prog_entry = process->prog_entry;
      api_task->prog_size = process->prog_size;
      api_task->vmem_start = process->vmem_start;
      api_task->vmem_end = process->vmem_start;
      api_task->no_allocated = process->no_allocated;
      api_task->privileged = process->privileged;
      strcpy(api_task->working_dir, process->working_dir);
      strcpy(api_task->exe_path, process->exe_path);
      if(process->window > -1 && !getWindow(process->window)->closed)
         strcpy(api_task->main_window_name, getWindow(process->window)->title);
      else
         strcpy(api_task->main_window_name, "");
   }
   map_size(get_current_task_pagedir(), (uint32_t)tasks, (uint32_t)tasks, sizeof(api_task_t)*TOTAL_TASKS, 1, 1, 0);
   regs->ebx = (uint32_t)tasks;
   regs->ecx = TOTAL_TASKS;
}

typedef struct {
   int task_id;
   uint32_t task_uid;
} api_sleep_msg_t;

void api_sleep_callback(void *regs, void *msg) {
   api_sleep_msg_t sleep_msg = *(api_sleep_msg_t*)msg;
   free((uint32_t)msg, sizeof(api_sleep_msg_t));
   task_state_t *task = &gettasks()[sleep_msg.task_id];
   if(!task->enabled || task->task_uid != sleep_msg.task_uid) return;
   task->paused = false;
   switch_to_task(task->task_id, regs); // wake
   task_execute_queued_subroutine(regs, (void*)task->task_id); // check for queued events while task was paused
}

void api_sleep(registers_t *regs) {
   // IN: ebx - ms
   extern int timer_hz;
   uint32_t ms = regs->ebx;
   int ticks = (timer_hz * ms) / 1000;
   task_state_t *task = get_current_task_state();
   task->paused = true;
   api_sleep_msg_t *msg = malloc(sizeof(api_sleep_msg_t));
   msg->task_id = task->task_id;
   msg->task_uid = task->task_uid;
   events_add(ticks, &api_sleep_callback, msg, -1);
   switch_task(regs); // yield
}

void api_get_timer_tick(registers_t *regs) {
   // OUT: ebx - tick
   regs->ebx = get_timer_tick();
}

void api_pipe(registers_t *regs) {
   // OUT: ebx - read fd
   // OUT: ecx - write fd
   task_state_t *task = get_current_task_state();
   fs_file_t *read_file = NULL, *write_file = NULL;
   fs_create_pipe(&read_file, &write_file);
   int read_fd = alloc_fd(task->process);
   if(read_fd == -1) {
      debug_printf("api_pipe: couldn't create fds\n");
      regs->ebx = -1;
      regs->ecx = -1;
      fs_close(read_file);
      fs_close(write_file);
      return;
   }
   task->process->file_descriptors[read_fd] = read_file;
   int write_fd = alloc_fd(task->process);
   if(write_fd == -1) {
      debug_printf("api_pipe: couldn't create fds\n");
      regs->ebx = -1;
      regs->ecx = -1;
      fs_close(read_file);
      task->process->file_descriptors[read_fd] = NULL;
      fs_close(write_file);
      return;
   }
   task->process->file_descriptors[write_fd] = write_file;
   regs->ebx = read_fd;
   regs->ecx = write_fd;
}

void api_unpause(registers_t *regs) {
   // IN: ebx - task id
   // todo: check if child of calling process or called by root process
   int task_id = regs->ebx;
   if(task_id < 0 || task_id >= TOTAL_TASKS) {
      debug_printf("api_unpause: invalid task id\n");
      return;
   }
   task_state_t *task = &gettasks()[task_id];
   if(task->paused) {
      if(!task->unpausable) {
         debug_printf("Couldn't unpause task %i", task->task_id);
         return;
      }
      task->paused = false;
      task->unpausable = false;
      switch_to_task(task_id, regs);
   }
}

void api_dup(registers_t *regs) {
   // IN: ebx - old fd
   // OUT: ebx - new fd / -1 on error
   task_state_t *task = get_current_task_state();
   int old_fd = regs->ebx;
   if(old_fd < 0 || old_fd >= task->process->fd_count) {
      debug_printf("api_dup: invalid fd\n");
      regs->ebx = -1;
      return;
   }
   fs_file_t *old_file = task->process->file_descriptors[old_fd];
   if(!old_file || !old_file->active) {
      debug_printf("api_dup: old fd inactive\n");
      regs->ebx = -1;
      return;
   }
   int new_fd = alloc_fd(task->process);
   if(new_fd < 0) {
      debug_printf("api_dup: too many open files\n");
      regs->ebx = -1;
      return;
   }
   task->process->file_descriptors[new_fd] = fs_dup(old_file);
   regs->ebx = new_fd;
}

void api_dup2(registers_t *regs) {
   // IN: ebx - old fd
   // IN: ecx - new fd
   // OUT: ebx - new fd / -1 on error
   task_state_t *task = get_current_task_state();
   int old_fd = regs->ebx;
   int new_fd = regs->ecx;
   if(old_fd < 0 || old_fd >= task->process->fd_count || new_fd < 0 || new_fd >= TASK_MAX_FDS) {
      debug_printf("api_dup2: invalid fd\n");
      regs->ebx = -1;
      return;
   }
   if(old_fd == new_fd) { regs->ebx = new_fd; return; }
   fs_file_t *old_file = task->process->file_descriptors[old_fd];
   if(!old_file || !old_file->active) {
      debug_printf("api_dup2: old fd inactive\n");
      regs->ebx = -1;
      return;
   }
   if(new_fd >= task->process->fd_count) {
      for(int i = task->process->fd_count; i <= new_fd; i++)
         task->process->file_descriptors[i] = NULL;
      task->process->fd_count = new_fd + 1;
   }
   fs_file_t *existing = task->process->file_descriptors[new_fd];
   if(existing) fs_close(existing);
   task->process->file_descriptors[new_fd] = fs_dup(old_file);
   regs->ebx = new_fd;
}

void api_close(registers_t *regs) {
   // IN: ebx - fd
   int fd = regs->ebx;
   task_state_t *task = get_current_task_state();
   if(fd < 0 || fd >= task->process->fd_count) {
      debug_printf("api_close: invalid fd\n");
      return;
   }
   fs_file_t *file = task->process->file_descriptors[fd];
   fs_close(file);
   task->process->file_descriptors[fd] = NULL;
}

void api_get_time(registers_t *regs) {
   // OUT: ebx - seconds since midnight
   regs->ebx = get_seconds();
}

void api_futex_wait(registers_t *regs) {
   // IN: ebx - addr
   // IN: ecx - expected value at addr
   // OUT: ebx - 0 on wake, 1 for no match/no block, -1 on error
   void *addr = (void*)regs->ebx;
   uint32_t expected = regs->ecx;
   int result = futex_wait(addr, expected);
   if(result == FUTEX_WAIT_BLOCK) {
      get_current_task_state()->paused = true;
      regs->ebx = 0;
      switch_task(regs);
   }
   if(result == FUTEX_WAIT_FAIL) {
      debug_printf("futex_wait: failed to read futex value\n");
      regs->ebx = -1;
   }
   if(result == FUTEX_WAIT_NOBLOCK) {
      regs->ebx = 1;
   }
}

void api_futex_wake(registers_t *regs) {
   // IN: ebx - addr
   futex_wake(regs, (void*)regs->ebx);
}

void api_shared_create(registers_t *regs) {
   // IN: ebx - size
   // OUT: ebx - addr, NULL on fail
   // OUT: ecx - block uid
   shared_block_t *block = shared_create(get_current_task_state()->process, regs->ebx);
   if(!block) {
      regs->ebx = 0;
   } else {
      regs->ebx = block->vaddr;
      regs->ecx = block->uid;
   }
}

void api_shared_grant(registers_t *regs) {
   // IN: ebx - task id
   // IN: ecx - block uid
   int task_id = regs->ebx;
   if(task_id < 0 || task_id >= TOTAL_TASKS) {
      debug_printf("api_shared_grant: invalid task id\n");
      return;
   }
   task_state_t *task = &gettasks()[task_id];
   if(!task->enabled || !task->process) {
      debug_printf("api_shared_grant: task disabled\n");
      return;
   }
   uint32_t target_uid = task->process->uid;
   shared_grant_access(get_current_task_state()->process, regs->ecx, target_uid);
}

void api_shared_map(registers_t *regs) {
   // IN: ebx - block uid
   // OUT: ebx - addr or NULL
   regs->ebx = shared_map_uid(get_current_task_state()->process, regs->ebx);
}

void api_shared_close(registers_t *regs) {
   // IN: ebx - block uid
   // OUT: ebx 1 success 0 fail
   regs->ebx = shared_close(get_current_task_state()->process, regs->ebx);
}

void api_pci_map(registers_t *regs) {
   // IN: ebx - vendor
   // IN: ecx - device id
   // OUT: ebx - vaddr (0 on failure)
   // mapping device MMIO enables bus-master DMA, granting the task access to
   // arbitrary physical memory - restrict to privileged tasks only
   process_t *process = get_current_task_state()->process;
   if(!process->privileged) {
      debug_printf("api_pci_map: denied for unprivileged task\n");
      regs->ebx = 0;
      return;
   }
   regs->ebx = pci_map_device(process, regs->ebx, regs->ecx);
}

void api_pci_exists(registers_t *regs) {
   // IN: ebx - vendor
   // IN: ecx - device id
   // OUT: bool exists
   process_t *process = get_current_task_state()->process;
   if(!process->privileged) {
      debug_printf("api_pci_exists: denied for unprivileged task\n");
      regs->ebx = 0;
      return;
   }

   regs->ebx = (pci_find_device(regs->ebx, regs->ecx) != NULL);
}

void api_dma(registers_t *regs) {
   // IN: ebx - size
   // OUT: ebx - physical addr (0 on failure)
   // todo: on newer processors use iommu
   process_t *process = get_current_task_state()->process;
   if(!process->privileged) {
      debug_printf("api_dma: denied for unprivileged task\n");
      regs->ebx = 0;
      return;
   }
   if(process->dma_count >= TASK_MAX_DMA) {
      debug_printf("api_dma: dma table full\n");
      regs->ebx = 0;
      return;
   }
   // dma requires physical address of continuous memory
   // kmalloc identity maps and marks physical memory as used
   uint32_t size = regs->ebx;
   api_malloc(regs); // ebx = addr
   if(regs->ebx == 0)
      return;

   // track in process
   process->dma_allocs[process->dma_count].addr = regs->ebx;
   process->dma_allocs[process->dma_count].size = size;
   process->dma_count++;
}

void api_dma_free(registers_t *regs) {
   // IN: ebx - addr
   // IN: ecx - size
   process_t *process = get_current_task_state()->process;
   if(!process->privileged) {
      debug_printf("api_dma_free: denied for unprivileged task\n");
      regs->ebx = 0;
      return;
   }
   // drop from the tracking table
   for(int i = 0; i < process->dma_count; i++) {
      if(process->dma_allocs[i].addr == regs->ebx) {
         process->dma_allocs[i] = process->dma_allocs[process->dma_count - 1];
         process->dma_count--;
         break;
      }
   }
   // free physical memory and unmap from process
   api_free(regs);
}

void api_escalate_do(void *dialog, void *regs) {
   // OUT: ebx - bool success
   window_popup_dialog_t *d = (window_popup_dialog_t*)dialog;

   task_state_t *task = &gettasks()[d->task_id];
   if(task->enabled && task->process && task->process->uid == d->process_uid) {
      debug_printf("Escalating process %u\n", d->process_uid);
      task->process->privileged = true;
      task->registers.ebx = 1;
      task->paused = false;
      switch_to_task(task->task_id, regs);
   } else {
      debug_printf("Couldn't escalate: requesting task ended\n");
   }
}

// escalate close action - deny request by default
void api_escalate_dismiss(void *dialog) {
   window_popup_dialog_t *d = (window_popup_dialog_t*)dialog;
   task_state_t *task = &gettasks()[d->task_id];
   if(task->enabled && task->process && task->process->uid == d->process_uid) {
      debug_printf("Escalation dismissed - denying process %u\n", d->process_uid);
      task->registers.ebx = 0;
      task->paused = false;
   } else {
      debug_printf("Couldn't deny: requesting task ended\n");
   }
}

void api_escalate_cancel(void *window, void *regs) {
   (void)window;
   window_popup_dialog_t *dialog = getSelectedWindow()->state;
   debug_printf("Denied escalating process %u\n", dialog->process_uid);
   dialog->answered = true;

   task_state_t *task = &gettasks()[dialog->task_id];
   if(task->enabled && task->process && task->process->uid == dialog->process_uid) {
      task->registers.ebx = 0;
      task->paused = false;
      switch_to_task(task->task_id, regs);
   } else {
      debug_printf("Couldn't escalate: requesting task ended\n");
   }
   window_close(regs, getSelectedWindowIndex());
}

void api_escalate(registers_t *regs) {
   // escalate process privilege
   // used for trusted drivers as this gives access to:
   // api_pci_map, api_pci_exists, api_dma, api_dma_free
   // OUT: ebx - bool success
   debug_printf("api_escalate\n");
   task_state_t *task = get_current_task_state();
   if(task->process->privileged) {
      debug_printf("already privileged\n");
      regs->ebx = 1;
      return;
   }
   
   // show dialog
   int popup = windowmgr_add();
   char buffer[512];
   sprintf(buffer, "Process %u wants privilege escalation", task->process->uid);
   window_popup_dialog_t *dialog = window_popup_dialog(getWindow(popup), NULL, buffer);
   dialog->callback_func = &api_escalate_do;
   dialog->dismiss_func = &api_escalate_dismiss; // closing dialog window = deny
   dialog->wo_okbtn->x = 75;
   dialog->process_uid = task->process->uid;
   dialog->task_id = task->task_id;
   window_create_button(getWindow(popup), 135, 45, "Deny", &api_escalate_cancel);
   strcpy(getWindow(popup)->title, "Escalate Process");
   strcpy(dialog->wo_okbtn->text, "Grant");
   toolbar_draw();
   window_draw_outline(getWindow(popup), false);

   task->paused = true;
   switch_task(regs); // yield
}
