#include "api.h"
#include "lib/api.h" // shared header

#include "tasks.h"
#include "window.h"
#include "fat.h"
#include "api.h"
#include "bmp.h"
#include "events.h"
#include "windowobj.h"
#include "font.h"
#include "windowmgr.h"
#include "window_popup.h"
#include "fs.h"

// helper funcs

inline gui_window_t *api_get_window() {
   return &gui_get_windows()[get_current_task_window()];
}

inline void api_write_to_task(char *out) {
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
   if(cindex < 0 || cindex > mainwindow->child_count)
      return NULL;

   return mainwindow->children[cindex];
}

// api funcs

void api_write_string(registers_t *regs) {
   task_state_t *task = get_current_task_state();
   // write ebx
   char *out;
   if(task->process->vmem_start == 0) // not elf
      out = (char*)(task->process->prog_entry + regs->ebx);
   else // elf
      out = (char*)regs->ebx;

   api_write_to_task(out);
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
   gui_window_t *window = api_get_cwindow(regs->edi);
   if(!window) return;
   int colour = regs->esi;
   if(colour == -1)
      colour = window->txtcolour;
   int windowindex = get_window_index_from_pointer(window);
   window_writestrat((char*)regs->ebx, colour, regs->ecx, regs->edx, windowindex);
}

void api_write_number_at(registers_t *regs) {
   // IN: ebx = num
   // IN: ecx = x
   // IN: edx = y
   window_writenumat(regs->ebx, 0, regs->ecx, regs->edx, get_current_task_window());
}

void api_yield(registers_t *regs) {
   switch_task(regs);
   //debug_printf("Switched to task %u\n", get_current_task());
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
      map(get_current_task_pagedir(), i*0x1000, i*0x1000, 1, 1);
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
   uint32_t addr = regs->ebx;
   if(addr != 0) return;
   api_get_window()->draw_func = (void *)(addr);
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
   // IN: ebx = size
   // OUT: ebx = addr
   int size = regs->ebx;
   uint32_t *mem = malloc(size);

   task_state_t *task = get_current_task_state();
   //task->allocated_pages[task->no_allocated] = mem;

   // identity map
   task->process->no_allocated += map_size(task->process->page_dir, (uint32_t)mem, (uint32_t)mem, size, 1, 1);

   regs->ebx = (uint32_t)mem;

   // TODO: use special usermode malloc rather than the kernel malloc
   // keep track of which task each malloc is from
}

void api_free(registers_t *regs) {
   // IN: ebx = addr
   // IN: ecx = size
   free(regs->ebx, regs->ecx);

   uint32_t mem = regs->ebx;
   task_state_t *task = get_current_task_state();

   // unmap from user
   task->process->no_allocated -= map_size(task->process->page_dir, mem, mem, regs->ecx, 1, 0);
}

void api_draw_bmp(registers_t *regs) {
   // IN: ebx = bmp address
   // IN: ecx = x
   // IN: edx = y
   // IN: esi = scale
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];

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
   uint32_t callback = regs->ebx;
   uint32_t delta = regs->ecx;

   events_add(delta, (void *)callback, NULL, get_current_task());
}

void api_register_windowobj(registers_t *regs) {
   // IN: ebx = index of child window or -1 for task's main window
   // OUT: ebx = windowobj
   int index = regs->ebx;

   gui_window_t *window = api_get_window();
   if(index >= 0) {
      if(index < window->child_count) {
         window = window->children[index];
      } else {
         regs->ebx = 0;
         return;
      }
   }

   if(!window || window->closed) {
      regs->ebx = 0;
      return;
   }

   windowobj_t *wo = malloc(sizeof(windowobj_t));
   windowobj_init(wo, &window->surface);
   window->window_objects[window->window_object_count++] = wo;
   map(get_current_task_pagedir(), (uint32_t)wo, (uint32_t)wo, 1, 1);

   regs->ebx = (uint32_t)wo;
}

void api_windowobj_add_child(registers_t *regs) {
   // only works on task's main window

   // IN: ebx = parent windowobj
   // OUT: ebx = child windowobj
   gui_window_t *window = api_get_window();
   windowobj_t *parent = (windowobj_t*)regs->ebx;
   windowobj_t *child = NULL;

   for(int i = 0; i < window->window_object_count; i++) {
      if(window->window_objects[i] == parent) { // only works for 1 level of inheritance
         child = malloc(sizeof(windowobj_t));
         windowobj_init(child, &window->surface);
         parent->children[parent->child_count++] = child;
         child->parent = parent;
         map(get_current_task_pagedir(), (uint32_t)child, (uint32_t)child, 1, 1);
      }
   }

   regs->ebx = (uint32_t)child;
}

void api_launch_task(registers_t *regs) {
   // IN: ebx = path
   // IN: ecx = argc
   // IN: edx = args
   // IN: esi = copy environment of current task (use same wd & file descriptors)

   char *path = (char*)regs->ebx;
   int argc = (int)regs->ecx;
   char **args = (char**)regs->edx;
   bool copy = (bool)regs->esi;  
   
   // copy args
   char **copied_args = NULL;
   if(argc > 0 && args != NULL) {
      copied_args = malloc(sizeof(char*) * argc);
      for(int i = 0; i < argc; i++) {
         if(args[i] != NULL) {
            size_t len = strlen(args[i]);
            copied_args[i] = malloc(len + 1);
            strcpy(copied_args[i], args[i]);
         } else {
            copied_args[i] = NULL;
         }
      }
   }

   task_state_t *parenttask = &gettasks()[get_current_task()];

   task_subroutine_end(regs);

   int oldwindow = getSelectedWindowIndex();

   tasks_launch_elf(regs, path, argc, copied_args, true);
   task_state_t *task = &gettasks()[get_current_task()];
   strcpy(task->process->exe_path, path);

   if(copy) {
      // copy file descriptors and working dir from parent
      for(int i = 0; i < parenttask->process->fd_count; i++) {
         task->process->file_descriptors[i] = parenttask->process->file_descriptors[i];
      }
      task->process->fd_count = parenttask->process->fd_count;
      strcpy(task->process->working_dir, parenttask->process->working_dir); // inherit working dir
      window_close(NULL, getSelectedWindowIndex()); // may get issues from having task without window
      setSelectedWindowIndex(oldwindow);
      gui_redrawall();
   }

   // map args to new task
   if(argc > 0 && args != NULL) {
      for(int i = 0; i < argc; i++) {
         // map args to task
         map(task->process->page_dir, (uint32_t)copied_args[i], (uint32_t)copied_args[i], 1, 1);
      }
      map(task->process->page_dir, (uint32_t)copied_args, (uint32_t)copied_args, 1, 1);
   }
}

void api_set_window_title(registers_t *regs) {
   // IN: ebx = title
   // IN: ecx = window child index
   int cindex = regs->ecx;
   char *title = (char*)regs->ebx;
   if(strlen(title) > 19) return;

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
   strcpy(gettasks()[get_current_task()].process->working_dir, path);
}

void api_get_working_dir(registers_t *regs) {
   // IN: ebx = size 256 buf for working dif
   strcpy((char*)regs->ebx, gettasks()[get_current_task()].process->working_dir);
}

void api_display_colourpicker(registers_t *regs) {
   // IN: ebx = colour
   // IN: ecx = callback function (argument is uint16_t colour)

   gui_window_t *parent = getWindow(get_current_task_window());
   int popup = windowmgr_add();
   debug_printf("Displaying colourpicker\n");
   window_popup_colourpicker(getWindow(popup), parent, (void*)regs->ecx, (uint16_t)regs->ebx);
   window_draw_outline(getWindow(popup), false);
}

void api_display_filepicker(registers_t *regs) {
   // IN: ebx = callback function (argument is char *path)

   gui_window_t *parent = getWindow(get_current_task_window());
   int popup = windowmgr_add();
   window_popup_filepicker(getWindow(popup), parent, (void*)regs->ebx);
   window_draw_outline(getWindow(popup), false);
}

void api_debug_write_str(registers_t *regs) {
   debug_printf("t%iw%i: %s\n", get_current_task(), get_current_task_window(), regs->ebx);
}

void api_sbrk(registers_t *regs) {
   // change heap size

   // IN: ebx - int increment/delta
   int delta = (int)regs->ebx;
   task_state_t *task = &gettasks()[get_current_task()];

   int old_heapsize = (int)(task->process->heap_end - task->process->heap_start);
   int new_heapsize = old_heapsize + delta;
   debug_printf("Resizing task %u heap from %u to %u\n", get_current_task(), old_heapsize, new_heapsize);

   // check if we're moving into a new page directory that needs to be mapped or freed
   uint32_t old_end = page_align_up(task->process->heap_start + old_heapsize);
   uint32_t new_end = page_align_up(task->process->heap_start + new_heapsize);
   if(new_end > old_end) {
      // expand
      int delta_pages = (new_end - old_end)/0x1000;
      debug_printf("Expand by %u pages\n", delta_pages);
      uint32_t physical = (uint32_t)malloc(delta_pages*0x1000); // hmm
      if(!physical) {
         debug_printf("Out of memory\n");
      }
      for(uint32_t i = 0; i < (uint32_t)delta_pages*0x1000; i+=0x1000)
         map(task->process->page_dir, physical+i, old_end+i, 1, 1);

   } else if(new_end < old_end) {
      // shrink
      int delta_pages = (old_end - new_end)/0x1000;

      debug_printf("Shrink by %u pages\n", delta_pages);

      uint32_t *physical_addrs = NULL;
      if (delta_pages > 0) {
         physical_addrs = malloc(delta_pages * sizeof(uint32_t));
         if(physical_addrs) {
            for (int i = 0; i < delta_pages; i++) {
               uint32_t virt = (uint32_t)task->process->heap_end - (i + 1) * 0x1000;
               physical_addrs[i] = page_getphysical(task->process->page_dir, virt);
            }
         }
      }

      for (uint32_t i = 0; i < (uint32_t)delta_pages * 0x1000; i += 0x1000) {
         uint32_t virt_addr = task->process->heap_end - 0x1000;
         unmap(task->process->page_dir, virt_addr);
         task->process->heap_end = virt_addr;
      }
        
        // Free physical memory
      if(physical_addrs) {
         for(int i = 0; i < delta_pages; i++) {
            if(physical_addrs[i]) {
               free((uint32_t)physical_addrs[i], 0x1000);
            }
         }
         free((uint32_t)physical_addrs, sizeof(uint32_t*));
      }
   }

   task->process->heap_end = task->process->heap_start + new_heapsize;
   regs->ebx = old_end;

}

void api_open(registers_t *regs) {
   // IN: ebx - char* path
   // IN: ecx - flag (0 read, 1 write)
   // OUT: ebx - int fd
   fs_file_t *file = fs_open((char*)regs->ebx);
   if(!file) {
      if(regs->ecx == 1) { // write
         debug_printf("api_open: creating new file %s\n", (char*)regs->ebx);
         file = fs_new((char*)regs->ebx);
      }
      if(!file) {
         debug_printf("api_open: could not create new file\n");
         regs->ebx = -1;
         return;
      }
   }
   task_state_t *task = get_current_task_state();
   int fd = task->process->fd_count;
   task->process->file_descriptors[task->process->fd_count++] = file;
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
      r->ebx = strlen(buffer+1);
   } else {
      debug_printf("Couldn't find window\n");
      r->ebx = -1;
   }
}

void api_read_fd_callback(registers_t *regs, int task) {
   debug_printf("Read callback task %i\n", task);
   gettasks()[task].paused = false;
   switch_to_task(task, regs); // wake
}

void api_read(registers_t *regs) {
   // IN: ebx - int fd
   // IN: ecx - char *buf
   // IN: size_t count
   // OUT: size_t bytes read
   task_state_t *task = get_current_task_state();
   int fd = regs->ebx;
   char *buf = (char*)regs->ecx;
   size_t count = regs->edx;

   debug_printf("api_read: fd %i, buf 0x%h, count %u\n", fd, buf, count);
   
   if(fd < 0 || fd >= task->process->fd_count) {
      debug_printf("read: fd not found\n");
      regs->ebx = -1;
      return;
   }
   if(!task->process->file_descriptors[fd]->active) {
      debug_printf("read: fd inactive\n");
      regs->ebx = -1;
      return;
   }

   if(fd == 0) {
      // read from stdin
      // note: only one task can read from a windows stdin at a time as these get overwritten
      gui_window_t *window = getWindow(task->process->file_descriptors[0]->window_index);
      window->read_func = &api_read_stdin_callback;
      window->read_buffer = buf;
      window->read_task = get_current_task();
      task->paused = true;
      switch_task(regs); // yield
   } else if(fd > 0 && fd < 3) {
      // stdout/stderr - can only write to these
   } else if(fd >= 3) {
      // read from file descriptor
      if(fd >= task->process->fd_count) {
         debug_printf("api_read: fd not found\n");
         regs->ebx = -1;
         return;
      }
      regs->ebx = fs_read(task->process->file_descriptors[fd], buf, count, &api_read_fd_callback, get_current_task());
      task->paused = true;
      switch_task(regs); // yield
   }

}

void api_read_dir(registers_t *regs) {
   // IN: ebx - char *path
   // OUT: ebx - fs_dir_content_t *
   fs_dir_content_t *content = fs_read_dir((char*)regs->ebx);
   if(!content) {
      regs->ebx = 0;
      return;
   }
   page_dir_entry_t *page_dir = get_current_task_pagedir();
   map_size(page_dir, (uint32_t)content, (uint32_t)content, sizeof(fs_dir_content_t), 1, 1);
   uint32_t entries_size = sizeof(fs_dir_entry_t)*content->size;
   map_size(page_dir, (uint32_t)content->entries, (uint32_t)content->entries, entries_size, 1, 1);
   regs->ebx = (uint32_t)content;
}

void api_write(registers_t *regs) {
   // IN: ebx - int fd
   // IN: ecx - uint8_t *buffer
   // IN: edx - size_t size
   task_state_t *task = get_current_task_state();
   int fd = regs->ebx;
   uint8_t *buffer = (uint8_t*)regs->ecx;
   size_t size = (size_t)regs->edx;
   if(fd < 0 || fd > task->process->fd_count) {
      debug_printf("api_write: fd not found\n");
      regs->ebx = -1;
   } else {
      fs_write(task->process->file_descriptors[fd], buffer, size);
      regs->ebx = size;
   }
}

void api_fsize(registers_t *regs) {
   // IN: ebx - int fd
   // OUT: ebx - filesize
   task_state_t *task = get_current_task_state();
   int fd = regs->ebx;
   if(fd < 0 || fd > task->process->fd_count) {
      regs->ebx = -1;
   } else {
      regs->ebx = fs_filesize(task->process->file_descriptors[fd]);
   }
}

void api_mkdir(registers_t *regs) {
   // IN: ebx - dir path
   // OUT: ebx - bool success
   regs->ebx = fs_mkdir((char*)regs->ebx);
}

void api_rename(registers_t *regs) {
   // IN: ebx - old path
   // IN: ecx - new name
   // OUT: ebx - bool success
   regs->ebx = fs_rename((char*)regs->ebx, (char*)regs->ecx);
}

void api_new_file(registers_t *regs) {
   // IN: ebx - file path
   // OUT: ebx - int fd / -1 on error
   fs_file_t *file = fs_new((char*)regs->ebx);
   if(!file) {
      regs->ebx = -1;
      debug_printf("api_new_file: error\n");
      return;
   }
   task_state_t *task = get_current_task_state();
   int fd = task->process->fd_count;
   task->process->file_descriptors[task->process->fd_count++] = file;
   regs->ebx = fd;
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
   if(cindex < 0 || cindex > mainwindow->child_count) {
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

   // copy over essential fields
   task_state_t *thread = &gettasks()[task_index];

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

   // todo require priviledge
   windowmgr_settings_t *settings = windowmgr_get_settings();

   switch(regs->ebx) {
      case SETTING_DESKTOP_BGIMG_ENABLED:
         settings->desktop_bgimg_enabled = (bool)regs->ecx;
         gui_redrawall();
         break;
      case SETTING_DESKTOP_BGIMG_PATH:
         if(page_getphysical(get_current_task_pagedir(), regs->ecx) == (uint32_t)-1) {
            regs->ebx = -1; // not mapped
            return;
         }
         fat_dir_t *entry = fat_parse_path((char*)regs->ecx, true);
         if(entry == NULL || entry->attributes == 0x10) {
            regs->ebx = -1; // not found
            return;
         }
         strncpy(settings->desktop_bgimg, (char*)regs->ecx, sizeof(settings->desktop_bgimg));

         uint8_t *img = fat_read_file(entry->firstClusterNo, entry->fileSize);
         desktop_setbgimg(img, entry->fileSize);
         free((uint32_t)entry, sizeof(fat_dir_t));
         break;
      case SETTING_DESKTOP_ENABLED:
         settings->desktop_enabled = (bool)regs->ecx;
         gui_redrawall();
         break;
      case SETTING_SYS_FONT_PATH:
         uint32_t physical = page_getphysical(get_current_task_pagedir(), regs->ecx);
         if(physical == (uint32_t)-1) {
            regs->ebx = -1; // path not mapped
            return;
         }
         char *path = (char*)regs->ecx;
         entry = fat_parse_path(path, true);
         if(entry == NULL || entry->attributes == 0x10) {
            regs->ebx = -1; // not found
            return;
         }
         strncpy(settings->font_path, (char*)regs->ecx, sizeof(settings->font_path));
         fontfile_t *file = (fontfile_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
         font_load(file);
         free((uint32_t)entry, sizeof(fat_dir_t));

         // memory leak as never free prev file
         break;
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
         map_size(get_current_task_pagedir(), (uint32_t)out, (uint32_t)out, sizeof(settings->desktop_bgimg), 1, 1);
         regs->ebx = (uint32_t)out;
         break;
      case SETTING_DESKTOP_ENABLED:
         regs->ebx = (uint32_t)settings->desktop_enabled;
         break;
      case SETTING_SYS_FONT_PATH:
         out = malloc(sizeof(settings->font_path));
         strcpy(out, settings->font_path);
         map_size(get_current_task_pagedir(), (uint32_t)out, (uint32_t)out, sizeof(settings->font_path), 1, 1);
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
   gui_window_t *window = api_get_cwindow(regs->edx);
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
   map_size(get_current_task_pagedir(), (uint32_t)tasks, (uint32_t)tasks, sizeof(api_task_t)*TOTAL_TASKS, 1, 1);
   regs->ebx = (uint32_t)tasks;
   regs->ecx = TOTAL_TASKS;
}