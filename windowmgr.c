#include "windowmgr.h"
#include "memory.h"
#include "window.h"
#include "tasks.h"
#include "draw.h"

int windowCount = 0;
int gui_selected_window = 0;

gui_window_t *gui_windows;

extern surface_t surface;

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

bool window_init(gui_window_t *window) {
   strcpy(window->title, " TERMINAL");
   window->x = 0;
   window->y = 0;
   window->width = 440;
   window->height = 320;
   window->text_buffer[0] = '\0';
   window->text_index = 0;
   window->text_x = FONT_PADDING;
   window->text_y = FONT_PADDING;
   window->needs_redraw = true;
   window->active = false;
   window->minimised = false;
   window->closed = false;
   window->dragged = false;
   window->colour_bg = COLOUR_WHITE;

   // default TERMINAL functions
   window->return_func = &window_term_return;
   window->keypress_func = &window_term_keypress;
   window->backspace_func = &window_term_backspace;
   window->uparrow_func = &window_term_uparrow;
   window->downarrow_func = &window_term_downarrow;

   window->draw_func = &window_term_draw;

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

void window_draw(int index) {
   gui_window_t *window = getWindow(index);
   if(window->closed || window->minimised || window->dragged)
      return;

   if(window->needs_redraw || index == getSelectedWindowIndex()) {
      // background
      //gui_drawrect(bg, window->x, window->y, window->width, window->height);

      // titlebar
      draw_rect(&surface, COLOUR_TITLEBAR, window->x+1, window->y+1, window->width-2, TITLEBAR_HEIGHT);
      // titlebar text, centred
      int titleWidth = gui_gettextwidth(strlen(window->title));
      int titleX = window->x + window->width/2 - titleWidth/2;
      draw_rect(&surface, COLOUR_WHITE, titleX-4, window->y+1, titleWidth+8, TITLEBAR_HEIGHT);
      draw_string(&surface, window->title, 0, titleX, window->y+3);
      // titlebar buttons
      draw_char(&surface, 'x', 0, window->x+window->width-(FONT_WIDTH+3), window->y+2);
      draw_char(&surface, '-', 0, window->x+window->width-(FONT_WIDTH+3)*2, window->y+2);

      if(window->framebuffer != NULL) {
         // draw window content/framebuffer
         for(int y = 0; y < window->height - TITLEBAR_HEIGHT; y++) {
            for(int x = 0; x < window->width; x++) {
               // ignore pixels covered by the currently selected window
               int screenY = (window->y + y + TITLEBAR_HEIGHT);
               int screenX = (window->x + x);
               if(index != getSelectedWindowIndex() && getSelectedWindowIndex() >= 0
               && screenX >= getSelectedWindow()->x
               && screenX < getSelectedWindow()->x + getSelectedWindow()->width
               && screenY >= getSelectedWindow()->y
               && screenY < getSelectedWindow()->y + getSelectedWindow()->height)
                  continue;

               int index = screenY*surface.width + screenX;
               int w_index = y*window->width + x;
               setpixel_safe(&surface, index, window->framebuffer[w_index]);
            }
         }
      }
      
      if(index != getSelectedWindowIndex()) gui_drawdottedrect(0, window->x, window->y, window->width, window->height);

      window->needs_redraw = false;
   }


   if(index == getSelectedWindowIndex()) {

      if(window->draw_func != NULL)
         (*(window->draw_func))(getSelectedWindowIndex());

   }
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