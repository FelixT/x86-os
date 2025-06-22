#include "api.h"

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

void api_write_string(registers_t *regs) {
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];
   // write ebx
   if(gettasks()[get_current_task()].vmem_start == 0) // not elf
      window_writestr((char*)(gettasks()[get_current_task()].prog_entry+regs->ebx), window->txtcolour, get_current_task_window());
   else // elf
      window_writestr((char*)regs->ebx, window->txtcolour, get_current_task_window());
}

void api_write_number(registers_t *regs) {
   // write ebx
   window_writenum(regs->ebx, 0, get_current_task_window());
}

void api_write_uint(registers_t *regs) {
   // write ebx
   window_writeuint(regs->ebx, 0, get_current_task_window());
}

void api_write_newline() {
   window_drawchar('\n', 0, get_current_task_window());
}

void api_write_string_at(registers_t *regs) {
      // IN: ebx = string address
      // IN: ecx = x
      // IN: edx = y
      gui_window_t *window = &gui_get_windows()[get_current_task_window()];
      window_writestrat((char*)regs->ebx, window->txtcolour, regs->ecx, regs->edx, get_current_task_window());
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

void api_print_program_stack(registers_t *regs) {
   for(int i = 0; i < 64; i++) {
      gui_writenum(((int*)regs->esp)[i], get_current_task_window());
      gui_writestr(" ", 0);
   }
}

void api_print_stack() {
   // show current stack contents
   int esp;
   asm("movl %%esp, %0" : "=r"(esp));

   for(int i = 0; i < 64; i++) {
      gui_writenum(((int*)esp)[i], 0);
      gui_writestr(" ", 0);
   }
}

void api_return_framebuffer(registers_t *regs) {
   // get framebuffer in ebx
   uint32_t framebuffer = (uint32_t)gui_get_window_framebuffer(get_current_task_window());
   regs->ebx = framebuffer;
   uint32_t size = gui_get_windows()[get_current_task_window()].framebuffer_size;
   // map to task
   for(uint32_t i = framebuffer/0x1000; i < (framebuffer+size+0xFFF)/0x1000; i++) {
      map(gettasks()[get_current_task()].page_dir, i*0x1000, i*0x1000, 1, 1);
   }
}

void api_return_window_width(registers_t *regs) {
   // get window framebuffer width in ebx
   regs->ebx = gui_get_windows()[get_current_task_window()].width;
}

void api_return_window_height(registers_t *regs) {
   // get window framebuffer height in ebx
   regs->ebx = gui_get_windows()[get_current_task_window()].height - TITLEBAR_HEIGHT;
}

void api_redraw_window() {
   // draw
   gui_get_windows()[get_current_task_window()].needs_redraw = true;
   gui_draw_window(get_current_task_window());
}

void api_redraw_pixel(registers_t *regs) {
   // IN: ebx = x
   // IN: ecx = y

   // x, y
   window_draw_content_region(&(gui_get_windows()[get_current_task_window()]), regs->ebx, regs->ecx, 1, 1);
}

void api_end_task(registers_t *regs) {
   // return with status ebx
   window_writestr("Task ended with status ", 0, get_current_task_window());
   window_writenum(regs->ebx, 0, get_current_task_window());
   window_drawchar('\n', 0, get_current_task_window());

   end_task(get_current_task(), regs);
}

void api_override_uparrow(registers_t *regs) {
   // override uparrow window function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].uparrow_func = (void *)(addr);
}

void api_override_checkcmd(registers_t *regs) {
   // override terminal checkcmd function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].checkcmd_func = (void *)(addr);
}

void api_override_downarrow(registers_t *regs) {
   // override downarrow window function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].downarrow_func = (void *)(addr);
}

void api_override_mouseclick(registers_t *regs) {
   // override mouse left click function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].click_func = (void *)(addr);
}

void api_override_draw(registers_t *regs) {
   // override draw function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].draw_func = (void *)(addr);
}

void api_override_resize(registers_t *regs) {
   // override resize function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].resize_func = (void *)(addr);
}

void api_override_drag(registers_t *regs) {
   // override drag function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].drag_func = (void *)(addr);
}

void api_override_mouserelease(registers_t *regs) {
   // override mouserelease function with ebx
   uint32_t addr = regs->ebx;

   gui_get_windows()[get_current_task_window()].mouserelease_func = (void *)(addr);
}

void api_end_subroutine(registers_t *regs) {
   task_subroutine_end(regs) ;
}

void api_malloc(registers_t *regs) {
   // IN: ebx = size
   // OUT: ebx = addr
   uint32_t *mem = malloc(regs->ebx);

   task_state_t *task = &gettasks()[get_current_task()];
   //task->allocated_pages[task->no_allocated] = mem;
   //task->no_allocated++;*/

   // identity map
   for(uint32_t i = (uint32_t)mem/0x1000; i < ((uint32_t)mem+regs->ebx+0xFFF)/0x1000; i++) {
      map(task->page_dir, i*0x1000, i*0x1000, 1, 1);
   }

   regs->ebx = (uint32_t)mem;

   // TODO: use special usermode malloc rather than the kernel malloc
   // keep track of which task each malloc is from
}

void api_free(registers_t *regs) {
   // IN: ebx = addr
   // IN: ecx = size
   free(regs->ebx, regs->ecx);
}

void api_fat_get_bpb(registers_t *regs) {
   // out: ebx
   fat_bpb_t *bpb = malloc(sizeof(fat_bpb_t));
   *bpb = fat_get_bpb(); // refresh fat tables
   map(gettasks()[get_current_task()].page_dir, (uint32_t)bpb, (uint32_t)bpb, 1, 1);

   regs->ebx = (uint32_t)bpb;
}

void api_fat_read_root(registers_t *regs) {
   // out: ebx
   fat_dir_t *items = fat_read_root();
   for(uint32_t i = (uint32_t)items/0x1000; i < ((uint32_t)items+sizeof(fat_dir_t)*fat_get_bpb().noRootEntries+0xFFF)/0x1000; i++) {
      map(gettasks()[get_current_task()].page_dir, i*0x1000, i*0x1000, 1, 1);
   }

   regs->ebx = (uint32_t)items;
}

void api_fat_parse_path(registers_t *regs) {
   // IN: ebx = addr of char* path
   // IN: ebx = isfile
   // OUT: ebx = addr of fat_dir_t entry for path or 0 if doesn't exist

   fat_dir_t *entry = fat_parse_path((char*)regs->ebx, (bool)regs->ecx);
   map(gettasks()[get_current_task()].page_dir, (uint32_t)entry, (uint32_t)entry, 1, 1);
   regs->ebx = (uint32_t)entry;
}

void api_fat_read_file_callback(void *regs, int task) {
   gettasks()[task].paused = false;
   switch_to_task(task, regs);
}

void api_fat_read_file(registers_t *regs) {
   // IN: ebx = path
   // OUT: ebx = addr of file content buffer, null on fail
   fat_dir_t *entry = fat_parse_path((char*)regs->ebx, true);
   if(!entry) {
      regs->ebx = 0;
   } else {
      regs->ebx = (uint32_t)fat_read_file_chunked(entry->firstClusterNo, entry->fileSize, &api_fat_read_file_callback, get_current_task());
      gettasks()[get_current_task()].paused = true;
      switch_task(regs); // yield
   }
}

void api_get_get_dir_size(registers_t *regs) {
   // IN: ebx = directory firstClusterNo
   // OUT: ebx = directory size
   regs->ebx = (uint32_t)fat_get_dir_size(regs->ebx);
}

void api_read_dir(registers_t *regs) {
   // IN: ebx = directory firstClusterNo
   // OUT: ebx
   fat_dir_t *items = malloc(32 * fat_get_dir_size(regs->ebx));
   fat_read_dir((uint16_t)regs->ebx, items);
   map(gettasks()[get_current_task()].page_dir, (uint32_t)items, (uint32_t)items, 1, 1);
   regs->ebx = (uint32_t)items;
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
   (void)regs;
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];
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
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];

   windowobj_t *wo = malloc(sizeof(windowobj_t));
   windowobj_init(wo, &window->surface);
   window->window_objects[window->window_object_count++] = wo;
   map(gettasks()[get_current_task()].page_dir, (uint32_t)wo, (uint32_t)wo, 1, 1);

   regs->ebx = (uint32_t)wo;
}

void api_launch_task(registers_t *regs) {
   // IN: ebx = path
   // IN: ecx = argc
   // IN: edx = args

   char *path = (char*)regs->ebx;
   int argc = (int)regs->ecx;
   char **args = (char**)regs->edx;  
   
   task_state_t *parenttask = &gettasks()[get_current_task()];

   task_subroutine_end(regs);

   tasks_launch_elf(regs, path, argc, args);
   task_state_t *task = &gettasks()[get_current_task()];
   strcpy(task->working_dir, parenttask->working_dir); // inherit working dir

   // map args to new task
   if(argc > 0 && args != NULL) {
      for(int i = 0; i < argc; i++) {
         // map args to task
         map(task->page_dir, (uint32_t)*(args[i]), (uint32_t)*(args[i]), 1, 1);
      }
   }
   map(gettasks()[get_current_task()].page_dir, (uint32_t)args, (uint32_t)args, 1, 1);
}

void api_fat_write_file(registers_t *regs) {
   // IN: ebx = path
   // IN: ecx = buffer
   // IN: edx = size

   fat_write_file((char*)regs->ebx, (uint8_t*)regs->ecx, (uint32_t)regs->edx);
}

void api_fat_new_file(registers_t *regs) {
   // IN: ebx = path
   fat_new_file((char*)regs->ebx, NULL, 0);
}

void api_set_sys_font(registers_t *regs) {
   // IN: ebx = path

   // todo: require privilege
   char *path = (char*)regs->ebx;
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry == NULL || entry->attributes == 0x10) {
      window_writestr("Font not found\n", 0, get_current_task_window());
      return;
   }
   fontfile_t *file = (fontfile_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
   font_load(file);

}

void api_set_window_title(registers_t *regs) {
   // IN: ebx = title
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];
   strcpy(window->title, (char*)regs->ebx);

   // redraw window
   gui_draw_window(get_current_task_window());
   toolbar_draw();
}

void api_set_working_dir(registers_t *regs) {
   // IN: ebx = path
   char *path = (char*)regs->ebx;
   strcpy(gettasks()[get_current_task()].working_dir, path);
}

void api_get_working_dir(registers_t *regs) {
   // IN: ebx = size 256 buf for working dif
   strcpy((char*)regs->ebx, gettasks()[get_current_task()].working_dir);
}

void api_display_popup(registers_t *regs) {
   // IN: ebx = title
   // IN: ecx = message

   // program loses control of dialog after its created

   char *title = (char*)regs->ebx;
   char *message = (char*)regs->ecx;
   gui_window_t *parent = getWindow(get_current_task_window());
   int popup = windowmgr_add();
   window_popup_dialog(getWindow(popup), parent, message);
   strcpy(getWindow(popup)->title, title);
}

void api_display_colourpicker(registers_t *regs) {
   // IN: ebx = callback function (argument is uint16_t colour)

   gui_window_t *parent = getWindow(get_current_task_window());
   int popup = windowmgr_add();
   debug_printf("Displaying colourpicker\n");
   window_popup_colourpicker(getWindow(popup), parent, (void*)regs->ebx);
}

void api_display_filepicker(registers_t *regs) {
   // IN: ebx = callback function (argument is char *path)

   gui_window_t *parent = getWindow(get_current_task_window());
   int popup = windowmgr_add();
   window_popup_filepicker(getWindow(popup), parent, (void*)regs->ebx);
}

void api_read_callback(void *regs, char *buffer) {
   (void)regs;
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];

   window->read_func = NULL;
   strcpy(window->read_buffer, buffer);
   gettasks()[get_current_task()].paused = false;
}

void api_read(registers_t *regs) {
   // IN: ebx = char* buffer
   // OUT: buffer is populated
   gui_window_t *window = &gui_get_windows()[get_current_task_window()];
   window->read_func = &api_read_callback;
   window->read_buffer = (char*)regs->ebx;
   gettasks()[get_current_task()].paused = true;
   switch_task(regs); // yield
}

void api_debug_write_str(registers_t *regs) {
   debug_printf("t%iw%i: ", get_current_task(), get_current_task_window(), getSelectedWindowIndex());
   debug_printf((char*)regs->ebx);
   debug_printf("\n");
}

void api_fat_new_dir(registers_t *regs) {
   // IN: ebx - dir path
   fat_new_dir((char*)regs->ebx);
}