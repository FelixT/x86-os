#include "windowmgr.h"
#include "memory.h"
#include "window.h"

int windowCount = 0;
int gui_selected_window = 0;

gui_window_t *gui_windows;

void debug_writestr(char *str) {
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
   gui_windows[0].title[0] = '0';
   gui_selected_window = 0;
}