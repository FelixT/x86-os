#include "windowmgr.h"
#include "memory.h"
#include "window.h"
#include "tasks.h"

int windowCount = 0;
int gui_selected_window = 0;

gui_window_t *gui_windows;

void debug_writestr(char *str) {
   if(windowCount == 0) return;
   window_writestr(str, 0, 0);
}

int getFirstFreeIndex() {
   for(int i = 0; i < windowCount; i++) {
      if(gui_windows[i].closed)
        return i;
   }

   return -1;
}

int getSelectedWindowIndex() {
   return gui_selected_window;
}

void setSelectedWindowIndex(int index) {
   gui_selected_window = index;
}

int getWindowCount() {
   return windowCount;
}

gui_window_t *getWindow(int index) {
   return &gui_windows[index];
}

gui_window_t *getSelectedWindow() {
   if(gui_selected_window == -1) return NULL;
   return &gui_windows[gui_selected_window];
}

int gui_window_add() {
   int index = getFirstFreeIndex();

   if(index == -1) {
      index = windowCount;
      gui_window_t *windows_new = resize((uint32_t)gui_windows, windowCount*sizeof(gui_window_t), (windowCount+1)*sizeof(gui_window_t));
   
      if(windows_new != NULL) {
         windowCount++;
         gui_windows = windows_new;
      } else {
         debug_writestr("Couldn't create window\n");
         return -1;
      }
   }

   if(window_init(&gui_windows[index])) {
      gui_windows[index].title[0] = index+'0';
      gui_selected_window = index;

      return index;
   } else {
      strcpy((char*)gui_windows[index].title, "ERROR");
      gui_windows[index].closed = true;
      debug_writestr("Couldn't create window\n");
   }

   return -1;
}

void windowmgr_init() {
   // Init with one (debug) window
   gui_windows = malloc(sizeof(gui_window_t));
   window_init(&gui_windows[0]);
   strcpy(gui_windows[0].title, "0DEBUG WINDOW");
   gui_selected_window = 0;
}

extern char scan_to_char(int scan_code);

void gui_uparrow(registers_t *regs) {

   if(getSelectedWindowIndex() >= 0) {
      gui_window_t *selected = getSelectedWindow();
      if(selected->uparrow_func != NULL) {
         int taskIndex = get_current_task();
         task_state_t *task = &gettasks()[taskIndex];

         if(taskIndex == -1 || !task->enabled || get_current_task_window() != getSelectedWindowIndex() || selected->uparrow_func == (void *)window_term_uparrow) {
            // launch into function directly as kernel
            //window_writestr("\nCalling function as kernel\n", 0, getSelectedWindowIndex());

            (*(selected->uparrow_func))(getSelectedWindowIndex());
         } else {
            // run as task
            //window_writestr("\nCalling function as task\n", 0, getSelectedWindowIndex());

            task_call_subroutine(regs, (uint32_t)(selected->uparrow_func), NULL, 0);
         }
      }
   }
   
}

void gui_downarrow(registers_t *regs) {

   if(getSelectedWindowIndex() >= 0) {
      gui_window_t *selected = getSelectedWindow();
      if(selected->downarrow_func != NULL) {
         int taskIndex = get_current_task();
         task_state_t *task = &gettasks()[taskIndex];

         if(taskIndex == -1 || !task->enabled || get_current_task_window() != getSelectedWindowIndex() || selected->downarrow_func == (void *)window_term_downarrow) {
            // launch into function directly as kernel
            //window_writestr("\nCalling function as kernel\n", 0, getSelectedWindowIndex());

            (*(selected->downarrow_func))(getSelectedWindowIndex());
         } else {
            // run as task
            //window_writestr("\nCalling function as task\n", 0, getSelectedWindowIndex());

            task_call_subroutine(regs, (uint32_t)(selected->downarrow_func), NULL, 0);
         }
      }
   }
   
}

void windowmgr_keypress(void *regs, int scan_code) {
   gui_window_t *selected = getSelectedWindow();
   if(!selected) return;
   
   if(scan_code == 28) {
      if(selected->return_func != NULL)
         (*(selected->return_func))(regs, getSelectedWindowIndex());
   }

   else if(scan_code == 14) { // backspace
      if(selected->backspace_func != NULL)
         (*(selected->backspace_func))(getSelectedWindowIndex());
   }

   else if(scan_code == 72) { // up arrow
      gui_uparrow(regs);
   }

   else if(scan_code == 80) { // down arrow
      gui_downarrow(regs);
   } 
   
   else {
      char c = scan_to_char(scan_code);
      if(selected->keypress_func != NULL)
         (*(selected->keypress_func))(c, getSelectedWindowIndex());
   }
}