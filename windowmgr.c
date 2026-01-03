#include "windowmgr.h"
#include "memory.h"
#include "window.h"
#include "tasks.h"
#include "draw.h"
#include "lib/string.h"
#include "bmp.h"
#include "windowobj.h"
#include "events.h"

#include "window_term.h"
#include "window_settings.h"
#include "window_popup.h"
#include <stdarg.h>

int windowCount = 0;
int gui_selected_window = -1;
gui_window_t *selectedWindow = NULL;

gui_window_t *gui_windows;

extern surface_t surface; // screen

windowmgr_settings_t wm_settings = {
   .default_window_bgcolour = 0xFFFF,
   .default_window_txtcolour = 0x0000,
   .desktop_enabled = false,
   .titlebar_colour = COLOUR_TITLEBAR_CLASSIC,
   .titlebar_colour2 = COLOUR_TITLEBAR_COLOUR2,
   .titlebar_gradientstyle = 1, // vertical
   .theme = 0
};

uint8_t *icon_window = NULL;
uint8_t *gui_bgimage = NULL;
uint8_t *icon_files = NULL;
int gui_bgimage_size = 0;

extern bool mouse_heldright;
windowobj_t *default_menu; // right click menu
windowobj_t *app_button; // toolbar app button

// would be much better as a linked link
gui_window_t *render_order[100];

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

void debug_printf(char *format, ...) {
   char *buffer = malloc(512);
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   debug_writestr(buffer);
   free((uint32_t)buffer, 512);
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

// move selected window to front of render order
void update_render_order() {
   if(selectedWindow == NULL) return;

   int found = -1;
   for(int i = 0; i < windowCount; i++) {
      if(render_order[i] == selectedWindow) {
         found = i;
         break;
      }
   }

   if(found == -1) {
      for(int i = windowCount; i > 0; i--) {
         render_order[i] = render_order[i-1];
      }
      render_order[0] = selectedWindow;
   } else if(found > 0) {
      gui_window_t *temp = render_order[found];
      for(int i = found; i > 0; i--) {
         render_order[i] = render_order[i-1];
      }
      render_order[0] = temp;
   }
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

   update_render_order();
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

   if(windowIndex == 0) {
      // debug window
      int popup = windowmgr_add();
      window_popup_dialog(getWindow(popup), getWindow(windowIndex), "Can't close debug window");
      window_draw_outline(getSelectedWindow(), false);
      getWindow(windowIndex)->minimised = true;
      getWindow(windowIndex)->active = false;
      setSelectedWindowIndex(popup);
      return;
   }

   if(regs) {
      int task = get_task_from_window(windowIndex);
      if(task > -1 && gettasks()[task].process->window == windowIndex) {
         end_task(task, regs); // don't kill task if closing child window
         // re-establish pointer as gui_windows may have been resized in end_task
         window = getWindow(windowIndex);
      } else {
         // call close func if set
         debug_printf("Calling close func for window %i\n", windowIndex);
         if(task > -1 && window->close_func != NULL) {
            if(gui_interrupt_switchtask(regs)) {
               uint32_t *args = malloc(sizeof(uint32_t) * 1);
               args[0] = get_cindex();
               task_call_subroutine(regs, "close", (uint32_t)(window->close_func), args, 1);
            }
         }
      }
   }

   if(windowIndex == getSelectedWindowIndex())
      setSelectedWindowIndex(-1);

   window->closed = true;

   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      free((uint32_t)&window->cmd_history[i][0], TEXT_BUFFER_LENGTH);
      window->cmd_history[i] = NULL;
   }
   free((uint32_t)window->framebuffer, window->width*(window->height-TITLEBAR_HEIGHT)*2);
   window->framebuffer = NULL;

   // free window objects
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_free(window->window_objects[i]);
      window->window_objects[i] = NULL;
   }

   // free state
   if(window->state != NULL) {
      if(window->state_free != NULL) {
         window->state_free(window);
      } else {
         free((uint32_t)window->state, window->state_size);
         window->state = NULL;
      }
   }

   // remove from render order
   for(int i = 0; i < windowCount; i++) {
      if(render_order[i] == window) {
         for(int j = i; j < windowCount-1; j++) {
            render_order[j] = render_order[j+1];
         }
         render_order[windowCount-1] = NULL;
         break;
      }
   }

   // take children with them
   for(int i = 0; i < window->child_count; i++) {
      if(window->children[i] != NULL) {
         window_close(regs, get_window_index_from_pointer(window->children[i]));
      }
   }

   gui_redrawall();
}

bool window_init(gui_window_t *window) {
   strcpy(window->title, "KTerminal");
   window->x = 20;
   window->y = 20;
   window->width = 360;
   window->height = 280;
   window->text_buffer[0] = '\0';
   window->text_index = 0;
   window->text_x = getFont()->padding;
   window->text_y = getFont()->padding;
   window->needs_redraw = true;
   window->active = false;
   window->minimised = false;
   window->closed = false;
   window->dragged = false;
   window->bgcolour = wm_settings.default_window_bgcolour;
   window->txtcolour = wm_settings.default_window_txtcolour;
   window->state = NULL;
   window->state_free = NULL;
   window->resizable = true;
   window->resized = false;
   window->disabled = false;
   window->toolbar_pos = -1;
   window->scrolledY = 0;
   window->scrollable_content_height = 0;
   window->scrollbar = NULL;
   window->child_count = 0;

   window_resetfuncs(window);
   // no window objects
   window->window_object_count = 0;
   for(int i = 0; i < 20; i++)
      window->window_objects[i] = NULL;
   
   window->cmd_history[0] = malloc(CMD_HISTORY_LENGTH*TEXT_BUFFER_LENGTH);
   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      if(i > 0)
         window->cmd_history[i] = window->cmd_history[0] + TEXT_BUFFER_LENGTH*i;
      window->cmd_history[i][0] = '\0';
   }
   window->cmd_history_pos = -1;

   window->framebuffer_size = window->width*(window->height-TITLEBAR_HEIGHT)*2;
   window->framebuffer = malloc(window->framebuffer_size);
   if(window->framebuffer == NULL) return false;
   
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height - TITLEBAR_HEIGHT;
   surface.buffer = (uint32_t)window->framebuffer;
   window->surface = surface;

   window_clearbuffer(window, wm_settings.default_window_bgcolour);

   return true;
}

void window_resetfuncs(gui_window_t *window) {
   // reset all functions
   window->keypress_func = &window_term_keypress;
   window->draw_func = &window_term_draw;
   window->checkcmd_func = &window_term_checkcmd;

   // other functions without default behaviour
   window->click_func = NULL;
   window->drag_func = NULL;
   window->resize_func = NULL;
   window->release_func = NULL;
   window->hover_func = NULL;
   window->read_func = NULL;
   window->close_func = NULL;
}

void window_removefuncs(gui_window_t *window) {
   // reset all functions
   window->keypress_func = NULL;
   window->draw_func = NULL;
   window->checkcmd_func = NULL;

   // other functions without default behaviour
   window->click_func = NULL;
   window->drag_func = NULL;
   window->resize_func = NULL;
   window->release_func = NULL;
   window->hover_func = NULL;
   window->read_func = NULL;
   window->close_func = NULL;
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
      setSelectedWindowIndex(index);
      gui_windows[index].x = index * 20;
      gui_windows[index].y = index * 20;

      toolbar_draw();

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

void window_draw_outline(gui_window_t *window, bool occlude) {
   if(window->minimised) return;

   if(occlude) {
      for(int i = 0; i < getWindowCount(); i++) {
         if(render_order[i] == NULL)
            continue;
         if(render_order[i] == window)
            break;
         if(render_order[i]->minimised)
            continue;
         
         gui_window_t *win = render_order[i];
         if(window->x - 1 < win->x + win->width
         && window->x + window->width + 1 > win->x
         && window->y - 1 < win->y + win->height
         && window->y + window->height + 1 > win->y)
            return;
      }
   }

   // titlebar

   // centered text
   int titleWidth = gui_gettextwidth(strlen(window->title));
   int titleX = window->x + window->width/2 - titleWidth/2;

   if(wm_settings.theme == 1) {
      // gradient titlebar
      draw_rect_gradient(&surface, wm_settings.titlebar_colour, wm_settings.titlebar_colour2, window->x, window->y, window->width, TITLEBAR_HEIGHT, wm_settings.titlebar_gradientstyle);
   } else {
      // classic titlebar
      draw_rect(&surface, wm_settings.titlebar_colour, window->x, window->y, window->width, TITLEBAR_HEIGHT);
      // shading
      for(int i = 0; i < (TITLEBAR_HEIGHT-6)/2; i++)
         draw_line(&surface, COLOUR_LIGHT_GREY, window->x+4, window->y+4+(i*2), false, window->width-26);
      // rectangle behind title
      draw_rect(&surface, wm_settings.titlebar_colour, titleX-6, window->y+3, titleWidth+12, getFont()->height+getFont()->padding*2+2);
   }
   // draw text
   draw_string(&surface, window->title, rgb16(210,210,210), titleX, window->y+6);
   draw_string(&surface, window->title, 0, titleX, window->y+5);

   // titlebar buttons
   draw_char(&surface, 0, 0, window->x+window->width-(getFont()->width+3), window->y+2);
   draw_char(&surface, '-', 0, window->x+window->width-(getFont()->width+3)*2, window->y+2);

   draw_line(&surface, rgb16(170,170,170), window->x, window->y+TITLEBAR_HEIGHT-1, false, window->width);

   if(window != selectedWindow) {
      draw_dottedrect(&surface, rgb16(80, 80, 80), window->x-1, window->y-1, window->width+2, window->height+2, NULL, false);
   } else {
      // drop shadow if selected
      draw_line(&surface, COLOUR_DARK_GREY, window->x+window->width+1, window->y+3, true, window->height-1);
      draw_line(&surface, COLOUR_DARK_GREY, window->x+3, window->y+window->height+1, false, window->width-1);

      // outline
      draw_unfilledrect(&surface, gui_rgb16(80,80,80), window->x - 1, window->y - 1, window->width + 2, window->height + 2);
   }
}

extern bool gui_cursor_shown;
extern int gui_mouse_x;
extern int gui_mouse_y;
void window_draw_content_region(gui_window_t *window, int offsetX, int offsetY, int width, int height) {
   if(window->framebuffer == NULL) return;

   int winX = window->x;
   int winY = window->y + TITLEBAR_HEIGHT;
   int winW = window->width;
   int winH = window->height - TITLEBAR_HEIGHT;

   // clamp drawing to window content bounds
   int maxY = offsetY + height;
   int maxX = offsetX + width;
   if(maxY > winH) maxY = winH;
   if(maxX > winW) maxX = winW;

   uint16_t *fb = gui_get_framebuffer();
   uint16_t *winfb = window->framebuffer;

   bool occlude = false;
   for(int i = 0; i < 100 && render_order[i] != NULL; i++) {
      if(render_order[i] == window)
         break;
      
      gui_window_t *win = render_order[i];
      if(win->minimised) continue;
      if(!(window->x >= win->x + win->width
         || window->x + window->width + 2 <= win->x
         || window->y >= win->y + win->height
         || window->y + window->height + 2 <= win->y)) {
         occlude = true;
         break;
      }
   }

   if(!occlude && offsetX == 0 && width == winW) {
      // fast path: copy entire row
      for(int y = offsetY; y < maxY; y++) {
         int screenY = winY + y;
         memcpy_fast(&fb[screenY * surface.width + winX], &winfb[y * winW], width * sizeof(uint16_t));
      }
   } else {
      // general case: copy pixel by pixel
      for(int y = offsetY; y < maxY; y++) {
         int screenY = winY + y;
         for(int x = offsetX; x < maxX; x++) {
            int screenX = winX + x;

            bool occluded = false;
            for(int i = 0; i < 100 && render_order[i] != NULL; i++) {
               if(render_order[i] == window) {
                  break;
               }
               
               gui_window_t *win = render_order[i];
               if(win->minimised) continue;
               if(screenX >= win->x - 1 && screenX < win->x + win->width + 1
               && screenY >= win->y - 1 && screenY < win->y + win->height + 1) {
                  occluded = true;
                  break;
               }
            }

            if(occluded)
               continue;

            int screenIndex = screenY * surface.width + screenX;
            int winIndex = y * winW + x;

            fb[screenIndex] = winfb[winIndex];
         }
      }
   }

   extern uint16_t *draw_buffer;
   // redraw cursor if it disappeared - unless redrawing all
   if(surface.buffer != (uint32_t)draw_buffer && width > getFont()->width && height > getFont()->height
   && gui_mouse_x >= window->x + offsetX && gui_mouse_x <= window->x + offsetX + width && gui_mouse_x + getFont()->width <= window->width
   && gui_mouse_y >= window->y + TITLEBAR_HEIGHT + offsetY && gui_mouse_y <= window->y + TITLEBAR_HEIGHT + offsetY + height && gui_mouse_y + getFont()->height <= window->height) {
      gui_cursor_shown = false;
      gui_cursor_save_bg();
      gui_cursor_draw();
   }

}

void window_draw_content(gui_window_t *window) {
   window_draw_content_region(window, 0, 0, window->width, window->height - TITLEBAR_HEIGHT);
}

void window_disable(gui_window_t *window) {
   for(int y = 0; y < window->height - TITLEBAR_HEIGHT; y+=2) {
      for(int x = 0; x < window->width; x+=2) {
         window->framebuffer[y * window->width + x] = rgb16(150, 150, 150);
      }
   }
   window->disabled = true;
}
 
void window_draw(gui_window_t *window) {

   if(window->dragged || window->resized)
      draw_dottedrect(&surface, COLOUR_LIGHT_GREY, window->x, window->y, window->width, window->height, NULL, false);

   if(window->closed || window->minimised || window->dragged || window->resized)
      return;

   if(window->needs_redraw || window == selectedWindow) {
      window_draw_outline(window, true);
      // call window objects draw funcs
      for(int i = 0; i < window->window_object_count; i++) {
         windowobj_t *wo = window->window_objects[i];
         if(wo != NULL) wo->draw_func(wo);
      }
      // draw window content/framebuffer
      window_draw_content(window);

      window->needs_redraw = false;
   }

   if(window == selectedWindow && window->draw_func != NULL)
      (*(window->draw_func))(window);
}

void windowmgr_getproperties() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   // show selected window settings 
   gui_window_t *selected = getSelectedWindow();
   int new = windowmgr_add();
   gui_window_t *window = getWindow(new);
   window_draw_outline(window, false);
   window->state = (void*)window_settings_init(window, selected);
   window->state_size = sizeof(window_settings_t);
}

void windowmgr_closeselected_callback(registers_t *regs, void *msg) {
   int index = (int)msg;
   debug_printf("Closing window %i\n", index);
   window_close(regs, index);
}

void windowmgr_closeselected() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   // close selected window
   int index = getSelectedWindowIndex();
   if(index == -1) return; // no window selected
   events_add(1, &windowmgr_closeselected_callback, (void*)index, -1);
   setSelectedWindowIndex(-1);
   gui_redrawall();
}

void windowmgr_init() {
   // settings
   strcpy(wm_settings.desktop_bgimg, "/bmp/bg16.bmp");
   strcpy(wm_settings.font_path, "/font/7.fon");

   // assigned fixed memory for 64 windows for now to reduce bugs
   // bug is just that resize func doesn't work i think
   gui_windows = malloc(sizeof(gui_window_t) * 64);
   memset(gui_windows, 0, sizeof(gui_window_t) * 64);
   for(int i = 0; i < 100; i++)
      render_order[i] = NULL;
   // Init with one (debug) window
   window_init(&gui_windows[0]);
   strcpy(gui_windows[0].title, "Debug Log");
   gui_windows[0].active = true;
   gui_windows[0].x = 100;
   gui_windows[0].y = 100;
   setSelectedWindowIndex(0);
   windowCount++;
   render_order[0] = &gui_windows[0];
   window_draw(&gui_windows[0]);

   // set up default menu
   default_menu = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(default_menu, &surface);
   default_menu->type = WO_MENU;
   default_menu->visible = false;
   default_menu->width = 80;
   default_menu->height = 40;
   default_menu->menuitems = malloc(sizeof(windowobj_menu_t) * 10);

   strcpy(default_menu->menuitems[0].text, "Settings");
   default_menu->menuitems[0].func = &windowmgr_getproperties;
   default_menu->menuitems[0].disabled = false;

   strcpy(default_menu->menuitems[1].text, "Close");
   default_menu->menuitems[1].func = &windowmgr_closeselected;
   default_menu->menuitems[1].disabled = false;

   default_menu->menuitem_count = 2;

   // set up app button
   app_button = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(app_button, &surface);
   app_button->type = WO_BUTTON;
   app_button->x = surface.width - 55;
   app_button->y = surface.height - 17;
   app_button->text = "Apps";
}

void toolbar_draw() {
   if(wm_settings.theme == 1) {
      // gradient toolbar
      draw_rect_gradient(&surface, wm_settings.titlebar_colour, wm_settings.titlebar_colour2, 0, surface.height-TOOLBAR_HEIGHT, surface.width, TOOLBAR_HEIGHT, wm_settings.titlebar_gradientstyle);
   } else {
      // classic toolbar
      gui_drawrect(COLOUR_TOOLBAR, 0, surface.height-TOOLBAR_HEIGHT, surface.width, TOOLBAR_HEIGHT);
   }

   int toolbarPos = 0;
   // padding = 2px
   for(int i = 0; i < getWindowCount(); i++) {
      if(getWindow(i)->closed) continue;

      int bg = COLOUR_LIGHT_GREY;
      int bg2 = rgb16(175, 175, 175);
      int fg = COLOUR_DARK_GREY;
      if(getWindow(i)->minimised) {
         bg = COLOUR_TOOLBAR_ENTRY;
         bg2 = rgb16(100, 100, 100);
         fg = COLOUR_WHITE;
      }
      if(getWindow(i)->active) {
         bg = COLOUR_WHITE;
         bg2 = COLOUR_LIGHTLIGHT_GREY;
         fg = COLOUR_BLACK;
      }
      int textWidth = gui_gettextwidth(10);
      int textX = TOOLBAR_ITEM_WIDTH/2 - textWidth/2;
      char text[11] = "       ";
      strcpy_fixed(text, getWindow(i)->title, 10);
      int itemX = TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING);
      int itemY = surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING);
      if(wm_settings.theme == 1) {
         draw_rect_gradient(&surface, bg, bg2, itemX, itemY, TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT, windowmgr_get_settings()->titlebar_gradientstyle);
      } else {
         gui_drawrect(bg, itemX, itemY, TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT);
      }
      gui_writestrat(text, fg, itemX+textX, itemY+TOOLBAR_PADDING/2);
      draw_line(&surface, COLOUR_TOOLBAR_BORDER, itemX, itemY+TOOLBAR_ITEM_HEIGHT,false,TOOLBAR_ITEM_WIDTH);
      getWindow(i)->toolbar_pos = toolbarPos;
      toolbarPos++;
   }

}

extern char scan_to_char(int scan_code, bool shift, bool caps);

void windowmgr_swap_window() {
   if(selectedWindow != NULL) {
      // find next in render order
      bool foundsel = false;
      bool found = false;
      int first = -1;
      for(int i = 0; i < windowCount; i++) {
         if(render_order[i] != NULL && first == -1)
            first = i;
         if(render_order[i] == selectedWindow) {
            foundsel = true;
         } else if(foundsel && render_order[i] != NULL) {
            setSelectedWindowIndex(get_window_index_from_pointer(render_order[i]));
            found = true;
            break;
         }
      }
      if(!found && first != -1)
         setSelectedWindowIndex(get_window_index_from_pointer(render_order[first]));
   } else {
      for(int i = 0; i < windowCount; i++) {
         if(render_order[i] != NULL) {
            setSelectedWindowIndex(get_window_index_from_pointer(render_order[i]));
            break;
         }
      }
   }

   if(selectedWindow)
      selectedWindow->minimised = false;
}


bool keyboard_shift = false;
bool keyboard_caps = false;
bool keyboard_alt = false;
bool keyboard_control = false;

void windowmgr_keypress(void *regs, int scan_code) {
   // check desktop window objects
   if(default_menu->visible) {
      windowobj_keydown(regs, &default_menu, scan_code);
      return;
   }

   bool released = scan_code & 0x80;
   scan_code = scan_code & 0x7F;

   // check modifiers
   if(scan_code == 0x38) {
      // alt
      keyboard_alt = !released;
   } else if(scan_code == 0x0F) {
      // tab
      if(keyboard_alt) {
         // alt+tab
         windowmgr_swap_window();
      }
   } else if(keyboard_alt) {
      // alt+w
      if(scan_to_char(scan_code, false, false) == 'w') {
         window_close(regs, gui_selected_window);
      }
      // alt+space
      if(scan_to_char(scan_code, false, false) == ' ') {
         windowmgr_launch_apps((registers_t*)regs);
         return;
      }
   }

   if(selectedWindow == NULL) return;

   if(scan_code == 0x2A || scan_code == 0x36)
      keyboard_shift = !released;
   
   if(released) return; // don't handle these currently
   
   if(scan_code == 0x3A)
      keyboard_caps = true;

   // check if we have a window object selected
   for(int i = 0; i < selectedWindow->window_object_count; i++) {
      windowobj_t *wo = selectedWindow->window_objects[i];
      if(!wo->clicked) continue;

      windowobj_keydown(regs, (void*)wo, scan_code);
      windowobj_redraw((void*)selectedWindow, (void*)wo);
      return;
   }

   // otherwise, default behaviour
   switch(scan_code) {
      case 0x3A: // caps
         keyboard_caps = !keyboard_caps;
         break;
      default:
         // call window keypress func with char
         uint16_t c = (uint16_t)scan_to_char(scan_code, keyboard_shift, keyboard_caps);
         if(c == 0) { // special chars
            if(scan_code == 14) // backspace
               c = 0x08;
            if(scan_code == 1) // escape
               c = 0x1B;
            if(scan_code == 28) // return
               c = 0x0D;
            if(scan_code == 72) // uparrow
               c = 0x100;
            if(scan_code == 80) // downarrow
               c = 0x101;
            if(scan_code == 75) // leftarrow
               c = 0x102;
            if(scan_code == 77) // rightarrow
               c = 0x103;
         }
         // keypress func
         if(!selectedWindow->keypress_func) break;

         gui_interrupt_switchtask(regs);
         int taskIndex = get_current_task();

         if(taskIndex == -1 || selectedWindow->keypress_func == &window_term_keypress) {
            // launch into function directly as kernel
            (*(selectedWindow->keypress_func))(c, selectedWindow);
         } else {
            // run as task
            uint32_t *args = malloc(sizeof(uint32_t) * 2);
            args[1] = c;
            args[0] = get_cindex();

            task_call_subroutine(regs, "keypress", (uint32_t)(selectedWindow->keypress_func), args, 2);
         }

         break;
   }
}

bool clicked_on_window(void *regs, int index, int x, int y) {
   gui_window_t *window = getWindow(index);
   if(!window || window->closed) return false;

   if(x >= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING) && x <= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING)+TOOLBAR_ITEM_WIDTH
   && y >= (int)surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING) && y <= (int)surface.height-TOOLBAR_PADDING) {
      // clicked on window's icon in toolbar, maximise
      if(window->minimised) {
         window->minimised = false;
         window->needs_redraw = true;
         setSelectedWindowIndex(index);
      } else if(window->active) {
         window->minimised = true;
         window->active = false;
         setSelectedWindowIndex(-1);
         gui_redrawall();
      } else {
         window->needs_redraw = true;
         setSelectedWindowIndex(index);
      }
      return true;
   } else if(!window->minimised && x >= window->x && x <= window->x + window->width && y >= window->y && y <= window->y + window->height) {
      // clicked within window bounds

      int relX = x - window->x;
      int relY = y - window->y;

      // minimise
      if(relY < TITLEBAR_HEIGHT && relX > window->width - (getFont()->width+3)*2 && relX < window->width - (getFont()->width+3)) {
         window->minimised = true;
         setSelectedWindowIndex(-1);
         gui_redrawall();
         return true;
      }

      // close
      if(relY < TITLEBAR_HEIGHT && relX > window->width - (getFont()->width+3)) {
         window_close(regs, index);
         return true;
      }

      window->needs_redraw = true;
      bool alreadyselected = window == selectedWindow;
      setSelectedWindowIndex(index);

      if(relY < TITLEBAR_HEIGHT) {
         window->dragged = true;
         return true;
      }

      if(!alreadyselected) return true;

      // check if we clicked on window object
      relY -= TITLEBAR_HEIGHT;

      int clicked_index = -1;
      for(int i = 0; i < selectedWindow->window_object_count; i++) {
         windowobj_t *wo = selectedWindow->window_objects[i];
         if(relX >= wo->x && relX < wo->x + wo->width
         && relY >= wo->y && relY < wo->y + wo->height) {
            windowobj_click(regs, (void*)wo, relX - wo->x, relY - wo->y);
            windowobj_redraw((void*)selectedWindow, (void*)wo);
            clicked_index = i;
            break;
         }
      }
      
      // unclick others
      for(int i = 0; i < selectedWindow->window_object_count; i++) {
         if(i == clicked_index) continue;
         windowobj_t *wo = selectedWindow->window_objects[i];
         wo->clicked = false;
         for(int x = 0; x < wo->child_count; x++) {
            windowobj_t *child = wo->children[x];
            child->clicked = false;
         }
      }

      return true;

   }

   return false;
}

extern uint16_t *draw_buffer;

extern int gui_mouse_x;
extern int gui_mouse_y;

int clicked_x;
int clicked_y;
void windowmgr_dragged(registers_t *regs, int relX, int relY) {
   gui_window_t *window = getSelectedWindow();
   if(selectedWindow == NULL || !selectedWindow->active) return;

   if(!selectedWindow->dragged && !selectedWindow->resized) {
      if(relX == 0 && relY == 0) return;
   
      int windowX = gui_mouse_x - selectedWindow->x;
      int windowY = gui_mouse_y - (selectedWindow->y + TITLEBAR_HEIGHT);

      if(windowX < 0 || windowY < 0 || windowX >= selectedWindow->width || windowY >= selectedWindow->height - TITLEBAR_HEIGHT)
         return;

      // check windowobjs
      for(int i = 0; i < selectedWindow->window_object_count; i++) {
         windowobj_t *wo = selectedWindow->window_objects[i];
         int woX = windowX - wo->x;
         int woY = windowY - wo->y;
         if(woX < 0 || woY < 0 || woX >= wo->width || woY >= wo->height)
            continue;
         windowobj_dragged(wo, woX, woY, relX, relY);
      }

      // call drag func
      if(selectedWindow->drag_func == NULL) return;
      if(get_task_from_window(getSelectedWindowIndex()) == -1) {
         // call as kernel
         getSelectedWindow()->drag_func(windowX, windowY);
      } else {
         // calling function as task
         if(!gui_interrupt_switchtask(regs)) return;

         uint32_t *args = malloc(sizeof(uint32_t) * 3);
         args[2] = windowX;
         args[1] = windowY;
         args[0] = get_cindex();
      
         task_call_subroutine(regs, "dragged", (uint32_t)(selectedWindow->drag_func), args, 3);
      
         return;
      }
   }

   // restore dotted outline
   draw_dottedrect(&surface, COLOUR_LIGHT_GREY, window->x, window->y, window->width, window->height, (int*)draw_buffer, true);
   
   if(window->dragged) {
      window->x += relX;
      window->y -= relY;
      if(window->x < 1)
         window->x = 1;
      if(window->x + window->width + 2 > (int)surface.width)
         window->x = surface.width - window->width - 2;
      if(window->y < 0)
         window->y = 0;
      if(window->y + window->height > (int)surface.height - TOOLBAR_HEIGHT)
         window->y = surface.height - window->height - TOOLBAR_HEIGHT;
   }
   if(selectedWindow->resized) {
      selectedWindow->width += relX;
      selectedWindow->height -= relY;
   }

   window->needs_redraw = true;

   // draw dotted outline
   gui_cursor_restore_bg();
   draw_dottedrect(&surface, COLOUR_LIGHT_GREY, window->x, window->y, window->width, window->height, (int*)draw_buffer, false);
   //gui_cursor_draw();
}

void windowmgr_launch_apps(registers_t *regs) {
   for(int i = 0; i < windowCount; i++) {
      if(gui_windows[i].closed) continue;
      if(strequ(gui_windows[i].title, "Apps")) {
         setSelectedWindowIndex(i);
         return;
      }
   }
   tasks_launch_elf(regs, "/sys/apps.elf", 0, NULL);
   selectedWindow->width = 120;
   selectedWindow->height = 280;
   selectedWindow->x = surface.width - selectedWindow->width - 5;
   selectedWindow->y = surface.height - selectedWindow->height - TOOLBAR_HEIGHT;
}

extern bool cursor_resize;
bool windowmgr_click(void *regs, int x, int y) {
   clicked_x = x;
   clicked_y = y;

   gui_window_t *prevSelected = selectedWindow;

   if(cursor_resize && selectedWindow != NULL) {
      selectedWindow->resized = true;
      return false;
   }

   if(default_menu->visible) {
      // clicked on menu
      if(x >= default_menu->x && x <= default_menu->x + default_menu->width
      && y >= default_menu->y && y <= default_menu->y + default_menu->height) {
         windowobj_click(regs, default_menu, 0, 0); // unused x, y
         default_menu->visible = false;
         gui_redrawall();
         return true;
      } else {
         default_menu->visible = false;
         gui_redrawall();
         return false;
      }
   }

   // clicked app btn
   if(x >= app_button->x && x <= app_button->x + app_button->width
      && y >= app_button->y && y <= app_button->y + app_button->height) {
      windowmgr_launch_apps(regs);
      return true;
   }

   if(!clicked_on_window(regs, getSelectedWindowIndex(), x, y)) {
      // check other windows
      setSelectedWindowIndex(-1);
      for(int i = 0; i < getWindowCount(); i++) {
         if(render_order[i] == NULL) continue;
         int index = get_window_index_from_pointer(render_order[i]);

         if(clicked_on_window(regs, index, x, y)) break;
      }
      if(selectedWindow) {
         // clicked_on_window sets dragged to true, so window content would be hidden on draw
         selectedWindow->dragged = false;
         gui_redrawall();
         selectedWindow->dragged = true;
      } else {
         gui_redrawall();
      }
   }

   if(selectedWindow == NULL) return false;

   // only call click routine when already the focused window
   if(selectedWindow != prevSelected) return true;
   
   if(selectedWindow->click_func == NULL) return true;

   if(y < selectedWindow->y + TITLEBAR_HEIGHT) {
      // clicked titlebar, don't call routine
      return true;
   }

   // switch to task
   gui_interrupt_switchtask(regs);
   if(get_task_from_window(getSelectedWindowIndex()) == -1) {
      // call as kernel
      getSelectedWindow()->click_func(x - selectedWindow->x, y - (selectedWindow->y + TITLEBAR_HEIGHT));
   } else {
      // calling as task
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = x - selectedWindow->x;
      args[1] = y - (selectedWindow->y + TITLEBAR_HEIGHT);
      args[0] = get_cindex();
      map(get_current_task_pagedir(), (uint32_t)args, (uint32_t)args, 1, 1);

      task_call_subroutine(regs, "click", (uint32_t)(selectedWindow->click_func), args, 3);
   }

   return true;
}

void windowmgr_rightclick(void *regs, int x, int y) {
   (void)(regs);
   if(selectedWindow == NULL || !(x - selectedWindow->x >= 0 && x - selectedWindow->x < selectedWindow->width
   && y - selectedWindow->y >= 0 && y - selectedWindow->y < selectedWindow->height)) {
      setSelectedWindowIndex(-1);
      gui_redrawall();
   }
   // draw menu
   default_menu->visible = true;
   app_button->visible = true;
   default_menu->x = x;
   default_menu->y = y;
   if(default_menu->y > surface.height - default_menu->height - TOOLBAR_HEIGHT) {
      // menu would be off screen, move up
      default_menu->y = surface.height - default_menu->height - TOOLBAR_HEIGHT;
   }
}

void windowmgr_draw() {
   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(render_order[i] == NULL) continue;
      window_draw(render_order[i]);
   }

   windowobj_draw(default_menu);
   windowobj_draw(app_button);
}

void windowmgr_redrawall() {
   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(render_order[i] == NULL) continue;
      render_order[i]->needs_redraw = true;
      window_draw(render_order[i]);
      window_draw_outline(render_order[i], false);
   }
}

extern uint16_t gui_bg;
void desktop_init() {
   // load add window icon
   if(icon_window == NULL) {
      fat_dir_t *entry = fat_parse_path("/bmp/window.bmp", true);
      if(entry == NULL) {
         debug_writestr("Window icon not found\n");
         return;
      }

      icon_window = fat_read_file(entry->firstClusterNo, entry->fileSize);
   }
   if(icon_files == NULL) {
      fat_dir_t *entry = fat_parse_path("/bmp/files.bmp", true);
      if(entry == NULL) {
         debug_writestr("Files icon not found\n");
         return;
      }
      icon_files = fat_read_file(entry->firstClusterNo, entry->fileSize);
   }

   if(gui_bgimage == NULL) {
      fat_dir_t *entry = fat_parse_path(wm_settings.desktop_bgimg, true);
      if(entry == NULL) {
         debug_writestr("BG not found\n");
         return;
      }

      gui_bgimage = fat_read_file(entry->firstClusterNo, entry->fileSize);
      gui_bgimage_size = entry->fileSize;
      gui_bg = bmp_get_colour(gui_bgimage, 0, 0);
   }

   wm_settings.desktop_enabled = true;
}

void desktop_setbgimg(uint8_t *img, int size) {
   if(gui_bgimage)
      free((uint32_t)gui_bgimage, gui_bgimage_size);
   gui_bgimage = img;
   gui_bgimage_size = size;
   gui_bg = bmp_get_colour(gui_bgimage, 0, 0);
}

void desktop_draw() {
   if(!wm_settings.desktop_enabled) return;

   int32_t width = bmp_get_width(gui_bgimage);
   int32_t height = bmp_get_height(gui_bgimage);
   int x = (surface.width - width) / 2;
   int y = ((surface.height - TOOLBAR_HEIGHT) - height) / 2;
   bmp_draw(gui_bgimage, (uint16_t *)surface.buffer, surface.width, surface.height, x, y, 0, 1);

   x = 10;
   y = 10;
   bmp_draw(icon_files, (uint16_t *)surface.buffer, surface.width, surface.height, x, y, 1, 1);
   y += 10 + bmp_get_height(icon_files);
   draw_string(&surface, "FileMgr", 0xFFFF, x, y);
   y += 10 + getFont()->height;
   bmp_draw(icon_window, (uint16_t *)surface.buffer, surface.width, surface.height, x, y, 1, 1);
   y += 10 + bmp_get_height(icon_window);
   draw_string(&surface, "UsrTerm", 0xFFFF, x, y);
   y += 10 + getFont()->height;
   bmp_draw(icon_window, (uint16_t *)surface.buffer, surface.width, surface.height, x, y, 1, 1);
   y += 10 + bmp_get_height(icon_window);
   draw_string(&surface, "KTerm", 0xFFFF, x, y);

}

void desktop_click(registers_t *regs, int x, int y) {
   if(!wm_settings.desktop_enabled) return;

   int icony = 10;
   int iconheight = bmp_get_height(icon_window);

   if(x >= 10 && y >= icony && x <= 60 && y <= icony + iconheight) {
      tasks_launch_elf(regs, "/sys/files.elf", 0, NULL);
   }

   icony += 10*2 + iconheight + getFont()->height;
   iconheight = bmp_get_height(icon_window);

   if(x >= 10 && y >= icony && x <= 60 && y <= icony + iconheight) {
      tasks_launch_elf(regs, "/sys/term.elf", 0, NULL);
   }

   icony += 10*2 + iconheight + getFont()->height;
   iconheight = bmp_get_height(icon_files);

   if(x >= 10 && y >= icony && x <= 60 && y <= icony + iconheight) {
      windowmgr_add();
      window_draw_outline(getSelectedWindow(), false);
   }
}

void windowmgr_mousemove(int x, int y) {
   // check desktop windowobjects
   if(default_menu->visible) {
      // check if within menu
      int relX_wo = x - default_menu->x;
      int relY_wo = y - default_menu->y;
      if(relX_wo > 0 && relX_wo < default_menu->width
      && relY_wo > 0 && relY_wo < default_menu->height) {
         if(default_menu->hover_func)
            default_menu->hover_func(default_menu, relX_wo, relY_wo);
         return;
      } else {
         if(default_menu->menuhovered != -1 || default_menu->hovering) {
            default_menu->menuhovered = -1;
            default_menu->hovering = false;
            windowobj_draw(default_menu);
         }
      }
   }

   if(selectedWindow) {
      // check if within selected window content

      int relX = x - selectedWindow->x;
      int relY = y - (selectedWindow->y + TITLEBAR_HEIGHT);

      if(relX > 0 && relX < selectedWindow->width
      && relY > 0 && relY < selectedWindow->height - TITLEBAR_HEIGHT) {
         // within current window, check window objects

         for(int i = 0; i < selectedWindow->window_object_count; i++) {
            windowobj_t *wo = selectedWindow->window_objects[i];
            int relX_wo = relX - wo->x;
            int relY_wo = relY - wo->y;
            bool inside = relX_wo > 0 && relX_wo < wo->width && relY_wo > 0 && relY_wo < wo->height;
        
            if(inside) {
               if(!wo->hovering || wo->type == WO_MENU || wo->type == WO_CANVAS || wo->type == WO_SCROLLBAR) {
                  //wo->hovering = true;
                  if(wo->hover_func)
                     wo->hover_func((void*)wo, relX_wo, relY_wo);
                  windowobj_redraw((void*)selectedWindow, (void*)wo);
               }
            } else {
               if(wo->hovering) {
                  wo->hovering = false;

                  for(int i = 0; i < wo->child_count; i++) {
                     ((windowobj_t*)(wo->children[i]))->hovering = false;
                  }

                  if(wo->draw_func)
                     wo->draw_func((void*)wo);
                  windowobj_redraw((void*)selectedWindow, (void*)wo);
               }
            }
         }

         // call hover function
         if(selectedWindow->hover_func) {
            int task = get_task_from_window(getSelectedWindowIndex());
            registers_t *regs = get_regs();
            if(task < 0) return;
            if(!switch_to_task(task, regs)) return;
            uint32_t *args = malloc(sizeof(uint32_t) * 2);
            args[2] = relX;
            args[1] = relY;
            args[0] = get_cindex();
            task_call_subroutine(regs, "hover", (uint32_t)(selectedWindow->hover_func), args, 3);

         }
      } else {
         // set all window objs as not hovered
         for(int i = 0; i < selectedWindow->window_object_count; i++) {
            windowobj_t *wo = selectedWindow->window_objects[i];
            if(wo->hovering) {
               wo->hovering = false;
               for(int i = 0; i < wo->child_count; i++) {
                  ((windowobj_t*)(wo->children[i]))->hovering = false;
               }
               if(wo->draw_func)
                  wo->draw_func((void*)wo);
               windowobj_redraw((void*)selectedWindow, (void*)wo);
            }
         }
      }

      // if just outside to the bottom right, display resize cursor
      if(selectedWindow->resizable && relX >= selectedWindow->width - 1 && relX < selectedWindow->width + 5
      && relY >= selectedWindow->height - 1 - TITLEBAR_HEIGHT && relY < selectedWindow->height + 5 - TITLEBAR_HEIGHT) {
         cursor_resize = true;
      } else {
         cursor_resize = false;
      }

   } else {
      cursor_resize = false;
   }
}

int get_window_index_from_pointer(gui_window_t *window) {
   for(int i = 0; i < windowCount; i++) {
      if(window == &gui_windows[i]) return i;
   }
   return -1;
}

static inline int min(int a, int b) { return (a < b) ? a : b; }
void window_resize(registers_t *regs, gui_window_t *window, int width, int height) {
   int maxheight = surface.height - TOOLBAR_HEIGHT - 5;
   if(height > maxheight) height = maxheight;
   int y_overflow = window->y + height - maxheight;
   if(y_overflow > 0) {
      window->y -= y_overflow;
   }

   uint16_t *old_framebuffer = window->framebuffer;
   uint32_t old_framebuffer_size = window->framebuffer_size;
   int old_width = window->surface.width;
   int old_height = window->surface.height;

   window->framebuffer_size = width*(height-TITLEBAR_HEIGHT)*2;
   window->framebuffer = malloc(window->framebuffer_size);
   window->width = width;
   window->height = height;
   window->surface.buffer = (uint32_t)window->framebuffer;
   window->surface.width = width;
   window->surface.height = height - TITLEBAR_HEIGHT;

   int scrollbarwidth = (window->scrollbar && window->scrollbar->visible) ? window->scrollbar->width : 0;

   int min_width = min(window->width, old_width-scrollbarwidth);
   int min_height = min(window->height, old_height);
   window_clearbuffer(window, window->bgcolour);
   // copy window
   for(int x = 0; x < min_width; x++) {
      for(int y = 0; y < min_height; y++) {
         window->framebuffer[x + window->surface.width*y] = old_framebuffer[x + old_width*y];
      }
   }

   free((uint32_t)old_framebuffer, old_framebuffer_size);

   if(window->scrollbar != NULL) {
      // resize scrollbar
      window->scrollbar->x = width - 14;
      window->scrollbar->height = height - TITLEBAR_HEIGHT;
      int scrollareaheight = (height - TITLEBAR_HEIGHT - 28);
      windowobj_t *scroller = window->scrollbar->children[0];
      if(window->scrollable_content_height == 0)
         scroller->height = 0;
      else
         scroller->height = (scrollareaheight * window->scrollbar->height) / window->scrollable_content_height;
      if(scroller->height < 10) scroller->height = 10;
      if(scroller->height >= scrollareaheight) {
         scroller->height = scrollareaheight;
         scroller->y = 14;
      }
      windowobj_t *downbtn = window->scrollbar->children[2];
      downbtn->y = height - TITLEBAR_HEIGHT - 14;

      window->scrollbar->visible = window->height - TITLEBAR_HEIGHT < window->scrollable_content_height;
      if(!window->scrollbar->visible)
         window_reset_scroll();

      debug_printf("%i %i\n", window->scrollbar->visible, window->scrollable_content_height);
   }

   // call resize func if exists
   int index = get_window_index_from_pointer(window);
   int task = get_task_from_window(index);
   if(regs && task > -1 && window->resize_func) {
      if(!switch_to_task(task, regs)) return;
      uint32_t *args = malloc(sizeof(uint32_t) * 4);
      args[3] = (uint32_t)window->framebuffer;
      args[2] = width - (window->scrollbar && window->scrollbar->visible ? 14 : 0);
      args[1] = height - TITLEBAR_HEIGHT;
      args[0] = get_cindex_from_window(window);
      map_size(get_current_task_pagedir(), (uint32_t)window->framebuffer, (uint32_t)window->framebuffer, window->framebuffer_size, 1, 1);
      map_size(get_current_task_pagedir(), (uint32_t)args, (uint32_t)args, sizeof(uint32_t)*4, 1, 1);
      task_call_subroutine(regs, "resize", (uint32_t)(window->resize_func), args, 4);
   }
   
}

void window_release(registers_t *regs, gui_window_t *window) {
   // call mouse release func if exists
   int index = get_window_index_from_pointer(window);
   int task = get_task_from_window(index);
   if(task > -1 && window->release_func) {
      if(switch_to_task(task, regs)) {
         uint32_t *args = malloc(sizeof(uint32_t) * 3);
         args[2] = gui_mouse_x - window->x; // x relative to window content
         args[1] = gui_mouse_y - window->y - TITLEBAR_HEIGHT; // y relative to window content
         args[0] = get_cindex();
         map_size(get_current_task_pagedir(), (uint32_t)args, (uint32_t)args, sizeof(uint32_t)*3, 1, 1);
         task_call_subroutine(regs, "release", (uint32_t)(window->release_func), args, 3);
      }
   }

   // check windowobjs
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      if(wo->clicked) {
         int relX = gui_mouse_x - window->x - wo->x;
         int relY = gui_mouse_y - window->y - TITLEBAR_HEIGHT - wo->y;
         if(windowobj_release((void*)regs, (void*)wo, relX, relY)) return;
      }
   }
}

windowmgr_settings_t *windowmgr_get_settings() {
   return &wm_settings;
}

// get 'cindex' of currently selected window
// main task window has index -1, children 0+, not found -2
int get_cindex() {
   int w = get_task_window(get_current_task()); // main window of current task
   if(w < 0)
      return -2;

   gui_window_t *window = getWindow(w);
   if(window == NULL || window->closed)
      return -2;
   if(window == selectedWindow)
      return -1;
   for(int i = 0; i < window->child_count; i++) {
      gui_window_t *child = window->children[i];
      if(child && child->active && child == selectedWindow)
         return i;
   }
   return -2;
}

int get_cindex_from_window(gui_window_t *window) {
   int w = get_task_window(get_current_task()); // main window of current task
   gui_window_t *mainwin = getWindow(w);
   if(w < 0)
      return -2;

   if(window == NULL || window->closed)
      return -2;
   if(window == mainwin)
      return -1; // main window
   for(int i = 0; i < mainwin->child_count; i++) {
      gui_window_t *child = mainwin->children[i];
      if(child && child->active && child == window)
         return i;
   }
   return -2;
}