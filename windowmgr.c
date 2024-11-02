#include "windowmgr.h"
#include "memory.h"
#include "window.h"
#include "tasks.h"
#include "draw.h"
#include "string.h"
#include "bmp.h"

#include "window_term.h"

int windowCount = 0;
int gui_selected_window = -1;
gui_window_t *selectedWindow = NULL;

gui_window_t *gui_windows;

extern surface_t surface; // screen

bool desktop_enabled = false;
uint8_t *icon_window;
uint8_t *gui_bgimage;

void debug_writestr(char *str) {
   if(windowCount == 0) return;
   window_writestr(str, 0, 0);
}

void debug_writeuint(uint32_t num) {
   if(windowCount == 0) return;
   window_writeuint(num, 0, 0);
}

void debug_writehex(uint32_t num) {
   if(windowCount == 0) return;
   char out[20];
   uinttohexstr(num, out);
   window_writestr("0x", 0, 0);
   window_writestr(out, 0, 0);
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
   if(selectedWindow != NULL)
      selectedWindow->active = false;
   gui_selected_window = index;
   if(index == -1) {
      selectedWindow = NULL;
   } else {
      selectedWindow = &gui_windows[index];
      selectedWindow->active = true;
   }
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

void window_close(void *regs, int windowIndex) {
   gui_window_t *window = getWindow(windowIndex);
   if(window->closed) return;

   if(windowIndex == getSelectedWindowIndex())
      setSelectedWindowIndex(-1);

   end_task(get_task_from_window(windowIndex), regs);

   // re-establish pointer as gui_windows may have been resized in end_task
   window = getWindow(windowIndex);
   window->closed = true;

   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      free((uint32_t)&window->cmd_history[i][0], TEXT_BUFFER_LENGTH);
      window->cmd_history[i] = NULL;
   }
   free((uint32_t)window->framebuffer, window->width*(window->height-TITLEBAR_HEIGHT)*2);
   window->framebuffer = NULL;

   gui_redrawall();
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
   
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height;
   surface.buffer = (uint32_t)window->framebuffer;
   window->surface = surface;

   window_clearbuffer(window, COLOUR_WHITE);

   return true;
}

int windowmgr_add() {
   int index = getFirstFreeIndex();
   if(index == -1) {
   
      if(windowCount < 64) {
         windowCount++;
         index = windowCount - 1;
      } else {
         debug_writestr("Couldn't create window\n");
         return -1;
      }
   }
      
   if(window_init(&gui_windows[index])) {
      gui_windows[index].title[0] = index+'0';
      setSelectedWindowIndex(index);

      return index;
   } else {
      strcpy((char*)gui_windows[index].title, "ERROR");
      gui_windows[index].closed = true;
      debug_writestr("Couldn't create window\n");
      return -1;
   }

}

void window_focus() {

}

void window_draw_outline(gui_window_t *window) {
   // titlebar
   draw_rect(&surface, COLOUR_TITLEBAR, window->x, window->y, window->width, TITLEBAR_HEIGHT);
   // shading
   for(int i = 0; i < (TITLEBAR_HEIGHT-6)/2; i++)
      draw_line(&surface, COLOUR_LIGHT_GREY, window->x+4, window->y+4+(i*2), false, window->width-24);
   // titlebar text, centred
   int titleWidth = gui_gettextwidth(strlen(window->title));
   int titleX = window->x + window->width/2 - titleWidth/2;
   draw_rect(&surface, COLOUR_TITLEBAR, titleX-4, window->y+3, titleWidth+8, FONT_HEIGHT+FONT_PADDING*2+2);
   draw_string(&surface, window->title, 0, titleX, window->y+5);

   // titlebar buttons
   draw_char(&surface, 'x', 0, window->x+window->width-(FONT_WIDTH+3), window->y+2);
   draw_char(&surface, '-', 0, window->x+window->width-(FONT_WIDTH+3)*2, window->y+2);

}

void window_draw_content(gui_window_t *window) {
   if(window->framebuffer == NULL) return;

   gui_window_t *selected = getSelectedWindow();

   for(int y = 0; y < window->height - TITLEBAR_HEIGHT; y++) {
      for(int x = 0; x < window->width; x++) {
         // ignore pixels covered by the currently selected window
         int screenY = (window->y + y + TITLEBAR_HEIGHT);
         int screenX = (window->x + x);
         if(selected != window && selected != NULL
         && screenX >= selected->x
         && screenX < selected->x + selected->width
         && screenY >= selected->y
         && screenY < selected->y + selected->height)
            continue;

         int index = screenY*surface.width + screenX;
         int w_index = y*window->width + x;
         setpixel_safe(&surface, index, window->framebuffer[w_index]);
      }
   }
}

void window_draw(gui_window_t *window) {

   if(window->dragged)
      draw_dottedrect(&surface, COLOUR_WHITE, window->x, window->y, window->width, window->height, NULL, false);

   if(window->closed || window->minimised || window->dragged)
      return;

   if(window->needs_redraw || window == selectedWindow) {
      // draw window content/framebuffer
      window_draw_outline(window);
      window_draw_content(window);

      if(window != selectedWindow)
         draw_dottedrect(&surface, 0, window->x-1, window->y-1, window->width+2, window->height+2, NULL, false);

      window->needs_redraw = false;
   }

   if(window == selectedWindow && window->draw_func != NULL)
      (*(window->draw_func))(window);

}

void windowmgr_init() {
   // assigned fixed memory for 64 windows for now to reduce bugs
   gui_windows = malloc(sizeof(gui_window_t) * 64);
   memset(gui_windows, 0, sizeof(gui_window_t) * 64);
   // Init with one (debug) window
   window_init(&gui_windows[0]);
   strcpy(gui_windows[0].title, "0DEBUG WINDOW");
   gui_windows[0].active = true;
   gui_windows[0].x = 5;
   gui_windows[0].y = 5;
   setSelectedWindowIndex(0);
   windowCount++;
}

void toolbar_draw() {
   gui_drawrect(COLOUR_TOOLBAR, 0, surface.height-TOOLBAR_HEIGHT, surface.width, TOOLBAR_HEIGHT);

   int toolbarPos = 0;
   // padding = 2px
   for(int i = 0; i < getWindowCount(); i++) {
      if(getWindow(i)->closed) continue;
      int bg = COLOUR_TASKBAR_ENTRY;
      if(getWindow(i)->minimised)
         bg = COLOUR_LIGHT_GREY;
      if(getWindow(i)->active)
         bg = COLOUR_DARK_GREY;
      int textWidth = gui_gettextwidth(3);
      int textX = TOOLBAR_ITEM_WIDTH/2 - textWidth/2;
      char text[4] = "   ";
      strcpy_fixed(text, getWindow(i)->title, 3);
      int itemX = TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING);
      int itemY = surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING);
      gui_drawrect(bg, itemX, itemY, TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT);
      gui_writestrat(text, COLOUR_WHITE, itemX+textX, itemY+TOOLBAR_PADDING/2);
      draw_line(&surface, 0, itemX, itemY+TOOLBAR_ITEM_HEIGHT,false,TOOLBAR_ITEM_WIDTH);
      getWindow(i)->toolbar_pos = toolbarPos;
      toolbarPos++;
   }

}

extern char scan_to_char(int scan_code);

void windowmgr_uparrow(registers_t *regs, gui_window_t *window) {

   if(window->uparrow_func == NULL) return;

   int taskIndex = get_current_task();
   task_state_t *task = &gettasks()[taskIndex];

   if(taskIndex == -1 || !task->enabled
   || window->uparrow_func == (void *)window_term_uparrow) {
      // launch into function directly as kernel
      (*(window->uparrow_func))(window);
   } else {
      // run as task
      task_call_subroutine(regs, (uint32_t)(window->uparrow_func), NULL, 0);
   }
   
}

void windowmgr_downarrow(registers_t *regs, gui_window_t *window) {

   if(window->downarrow_func == NULL) return;

   int taskIndex = get_current_task();
   task_state_t *task = &gettasks()[taskIndex];

   if(taskIndex == -1 || !task->enabled
   || selectedWindow->downarrow_func == (void *)window_term_downarrow) {
      // launch into function directly as kernel
      (*(window->downarrow_func))(window);
   } else {
      // run as task
      task_call_subroutine(regs, (uint32_t)(window->downarrow_func), NULL, 0);
   }
   
}

void windowmgr_keypress(void *regs, int scan_code) {
   if(selectedWindow == NULL) return;
   
   switch(scan_code) {
      case 28: // return
         if(selectedWindow->return_func != NULL)
            (*(selectedWindow->return_func))(regs, selectedWindow);
         break;
      case 14: // backspace
         if(selectedWindow->backspace_func != NULL)
            (*(selectedWindow->backspace_func))(selectedWindow);
         break;
      case 72: // up arrow
         windowmgr_uparrow(regs, selectedWindow);
         break;
      case 80: // down arrow
         windowmgr_downarrow(regs, selectedWindow);
         break;
      default:
         char c = scan_to_char(scan_code);
         if(selectedWindow->keypress_func != NULL)
            (*(selectedWindow->keypress_func))(c, selectedWindow);
         break;
   }
}

bool clicked_on_window(void *regs, int index, int x, int y) {
   gui_window_t *window = getWindow(index);
   if(window->closed) return false;

   if(x >= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING) && x <= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING)+TOOLBAR_ITEM_WIDTH
   && y >= (int)surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING) && y <= (int)surface.height-TOOLBAR_PADDING) {
      // clicked on window's icon in toolbar, maximise
      window->minimised = false;
      window->needs_redraw = true;
      setSelectedWindowIndex(index);
      return true;
   } else if(!window->minimised && x >= window->x && x <= window->x + window->width && y >= window->y && y <= window->y + window->height) {
      // clicked within window bounds

      int relX = x - window->x;
      int relY = y - window->y;

      // minimise
      if(relY < TITLEBAR_HEIGHT && relX > window->width - (FONT_WIDTH+3)*2 && relX < window->width - (FONT_WIDTH+3)) {
         window->minimised = true;
         setSelectedWindowIndex(-1);
         return false;
      }

      // close
      if(relY < TITLEBAR_HEIGHT && relX > window->width - (FONT_WIDTH+3)) {
         window_close(regs, index);
         return false;
      }

      window->needs_redraw = true;
      setSelectedWindowIndex(index);
      return true;

   }

   return false;
}


void window_click() {

}

extern uint16_t *draw_buffer;

void windowmgr_dragged(int relX, int relY) {
   gui_window_t *window = getSelectedWindow();
   if(window == NULL || !window->active) return;

   // restore dotted outline
   if(window->dragged)
      draw_dottedrect(&surface, COLOUR_WHITE, window->x, window->y, window->width, window->height, (int*)draw_buffer, true);

   window->x += relX;
   window->y -= relY;
   if(window->x < 0)
      window->x = 0;
   if(window->x + window->width > (int)surface.width)
      window->x = surface.width - window->width;
   if(window->y < 0)
      window->y = 0;
   if(window->y + window->height > (int)surface.height - TOOLBAR_HEIGHT)
      window->y = surface.height - window->height - TOOLBAR_HEIGHT;

   window->dragged = true;
   window->needs_redraw = true;

   // draw dotted outline
   draw_dottedrect(&surface, COLOUR_WHITE, window->x, window->y, window->width, window->height, (int*)draw_buffer, false);
}

bool windowmgr_click(void *regs, int x, int y) {
   gui_window_t *prevSelected = selectedWindow;

   if(!clicked_on_window(regs, getSelectedWindowIndex(), x, y)) {
      // check other windows
      setSelectedWindowIndex(-1);
      for(int i = 0; i < getWindowCount(); i++) {
         if(clicked_on_window(regs, i, x, y)) break;
      }
   }

   if(selectedWindow == NULL) return false;

   // only call click routine when already the focused window
   if(selectedWindow != prevSelected) return true;
   
   // switch to task
   gui_interrupt_switchtask(regs);
   if(get_current_task_window() != getSelectedWindowIndex())
      return true; // no task

   if(selectedWindow->click_func == NULL) return true;

   // calling function as task
   uint32_t *args = malloc(sizeof(uint32_t) * 2);
   args[1] = x - selectedWindow->x;
   args[0] = y - (selectedWindow->y + TITLEBAR_HEIGHT);

   if((y - (selectedWindow->y + TITLEBAR_HEIGHT)) >= 0) {
      task_call_subroutine(regs, (uint32_t)(selectedWindow->click_func), args, 2);
   } else {
      // clicked titlebar, don't call routine
      free((uint32_t)args, sizeof(uint32_t) * 2);
   }

   return true;
}

void windowmgr_draw() {
   // draw windows in reverse index order, then selected on top
   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(i != getSelectedWindowIndex())
         window_draw(getWindow(i));
   }
   if(selectedWindow != NULL)
      window_draw(selectedWindow);

   toolbar_draw();
}

void windowmgr_redrawall() {
   for(int i = 0; i < getWindowCount(); i++)
      getWindow(i)->needs_redraw = true;

   windowmgr_draw();
}

extern uint16_t gui_bg;
void desktop_init() {
   // load add window icon
   debug_writestr("Init desktop\n");
   fat_dir_t *entry = fat_parse_path("/bmp/window.bmp");
   if(entry == NULL) {
      debug_writestr("Icon not found\n");
      return;
   }

   icon_window = fat_read_file(entry->firstClusterNo, entry->fileSize);

   // load background
   entry = fat_parse_path("/bmp/bg16.bmp");
   if(entry == NULL) {
      debug_writestr("BG not found\n");
      return;
   }

   gui_bgimage = fat_read_file(entry->firstClusterNo, entry->fileSize);
   gui_bg = bmp_get_colour(gui_bgimage, 0, 0);

   desktop_enabled = true;
}

void desktop_draw() {
   if(!desktop_enabled) return;

   int32_t width = bmp_get_width(gui_bgimage);
   int32_t height = bmp_get_height(gui_bgimage);
   int x = (surface.width - width) / 2;
   int y = ((surface.height - TOOLBAR_HEIGHT) - height) / 2;
   bmp_draw(gui_bgimage, (uint16_t *)surface.buffer, surface.width, surface.height, x, y, 0);
   bmp_draw(icon_window, (uint16_t *)surface.buffer, surface.width, surface.height, 10, 10, 1);
}

void desktop_click(int x, int y) {
   if(!desktop_enabled) return;

   if(x >= 10 && y >= 10 && x <= 60 && y <= 60)
      windowmgr_add();
}

