// default window behaviour

#include "window.h"
#include "gui.h"
#include "ata.h"
#include "memory.h"
#include "tasks.h"
#include "fat.h"
#include "paging.h"
#include "font.h"
#include "bmp.h"
#include "elf.h"
#include "events.h"
#include "draw.h"
#include "windowmgr.h"
#include "windowobj.h"
#include "window_term.h"
#include "lib/string.h"

surface_t window_getsurface(int windowIndex) {
   gui_window_t *window = &(gui_get_windows()[windowIndex]);
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height - TITLEBAR_HEIGHT;
   surface.pitch = window->width;
   surface.buffer = (uint32_t)window->framebuffer;
   return surface;
}

// window action handlers

void window_drag(gui_window_t *window, int x, int y, int relX, int relY) {
   // drag inside window

   // check windowobjs
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      int woX = x - wo->x;
      int woY = y - wo->y;
      if(woX < 0 || woY < 0 || woX >= wo->width || woY >= wo->height || wo == window->scrollbar)
         continue;
      windowobj_dragged(wo, woX, woY, relX, relY);
   }

   // call drag func
   if(window->drag_func == NULL) return;
   int taskIndex = get_task_from_window(get_window_index_from_pointer(window));
   if(taskIndex == -1) {
      // call as kernel
      window->drag_func(x, y);
   } else {
      // calling function as task
      task_state_t *task = &gettasks()[taskIndex];
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = x;
      args[1] = y;
      args[0] = get_cindex(task);
   
      wm_call_subroutine(task, "dragged", (uint32_t)(window->drag_func), args, 3);
   }
}

// only called if window is already selected
void window_click(gui_window_t *window, int x, int y) {
   // check if we clicked on window object
   int clicked_index = -1;
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         windowobj_click((void*)wo, x - wo->x, y - wo->y);
         windowobj_redraw((void*)window, (void*)wo);
         clicked_index = i;
         break;
      }
   }
   
   // unclick others
   for(int i = 0; i < window->window_object_count; i++) {
      if(i == clicked_index) continue;
      windowobj_t *wo = window->window_objects[i];
      if(!wo->clicked) continue;
      wo->clicked = false;
      for(int x = 0; x < wo->child_count; x++) {
         windowobj_t *child = wo->children[x];
         child->clicked = false;
      }
   }

   // call click_func
   if(window->click_func == NULL) return;

   // switch to task
   int taskIndex = get_task_from_window(get_window_index_from_pointer(window));
   if(taskIndex == -1) {
      // call as kernel
      window->click_func(x, y);
   } else {
      // calling as task
      task_state_t *task = &gettasks()[taskIndex];
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = x;
      args[1] = y;
      args[0] = get_cindex(task);
      wm_call_subroutine(task, "click", (uint32_t)(window->click_func), args, 3);
   }
}

void window_keypress(gui_window_t *window, uint16_t c) {
   // check if we have a window object selected
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      if(!wo->clicked) continue;

      windowobj_keydown((void*)wo, c);
      windowobj_redraw((void*)window, (void*)wo);
      return;
   }

   if(!window->keypress_func) return;

   int taskIndex = get_task_from_window(get_window_index_from_pointer(window));

   if(taskIndex == -1 || window->keypress_func == &window_term_keypress) {
      // launch into function directly as kernel
      (*(window->keypress_func))(c, window);
   } else {
      // run as task
      task_state_t *task = &gettasks()[taskIndex];
      uint32_t *args = malloc(sizeof(uint32_t) * 2);
      args[1] = c;
      args[0] = get_cindex(task);

      wm_call_subroutine(task, "keypress", (uint32_t)(window->keypress_func), args, 2);
   }
}

void window_keyrelease(gui_window_t *window, uint16_t c) {
   // keyrelease func
   if(!window->keyrelease_func) return;
   int taskIndex = get_task_from_window(get_window_index_from_pointer(window));
   if(taskIndex < 0) return;
   task_state_t *task = &gettasks()[taskIndex];

   uint32_t *args = malloc(sizeof(uint32_t) * 2);
   args[1] = c;
   args[0] = get_cindex(task);

   wm_call_subroutine(task, "keyrelease", (uint32_t)(window->keyrelease_func), args, 2);
}

void window_mousemove(gui_window_t *window, int x, int y) {
   // check window objects
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      int relX_wo = x - wo->x;
      int relY_wo = y - wo->y;
      bool inside = relX_wo > 0 && relX_wo < wo->width && relY_wo > 0 && relY_wo < wo->height;
   
      if(inside) {
         if(!wo->hovering || wo->type == WO_MENU || wo->type == WO_CANVAS || wo->type == WO_SCROLLBAR) {
            //wo->hovering = true;
            if(wo->hover_func)
               wo->hover_func((void*)wo, relX_wo, relY_wo);
            windowobj_redraw((void*)window, (void*)wo);
         }
      } else {
         if(wo->hovering) {
            wo->hovering = false;

            for(int i = 0; i < wo->child_count; i++) {
               ((windowobj_t*)(wo->children[i]))->hovering = false;
            }

            if(wo->draw_func)
               wo->draw_func((void*)wo);
            windowobj_redraw((void*)window, (void*)wo);
         }
      }
   }

   // call hover function
   if(window->hover_func) {
      int task = get_task_from_window(get_window_index_from_pointer(window));
      if(task < 0) return;
      task_state_t *task_state = &gettasks()[task];
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = x;
      args[1] = y;
      args[0] = get_cindex(task_state);
      wm_call_subroutine(task_state, "hover", (uint32_t)(window->hover_func), args, 3);
   }
}

void window_mouseout(gui_window_t *window) {
   if(!window->resized && window->mouseout_func) {
      // call mouseout routine
      int task = get_task_from_window(get_window_index_from_pointer(window));
      if(task < 0) return;
      task_state_t *task_state = &gettasks()[task];
      uint32_t *args = malloc(sizeof(uint32_t) * 1);
      args[0] = get_cindex(task_state);
      wm_call_subroutine(task_state, "mouseout", (uint32_t)(window->mouseout_func), args, 1);
   }
   // set all window objs as not hovered
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      if(wo->hovering) {
         wo->hovering = false;
         for(int i = 0; i < wo->child_count; i++) {
            ((windowobj_t*)(wo->children[i]))->hovering = false;
         }
         if(wo->draw_func)
            wo->draw_func((void*)wo);
         windowobj_redraw((void*)window, (void*)wo);
      }
   }
}

bool window_rightclick(gui_window_t *window, int x, int y) {
   if(!window->rightclick_func) return false;
   // switch to task
   int taskIndex = get_task_from_window(get_window_index_from_pointer(window));
   if(taskIndex > -1) {
      task_state_t *task = &gettasks()[taskIndex];
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = x;
      args[1] = y;
      args[0] = get_cindex(task);
      wm_call_subroutine(task, "rightclick", (uint32_t)(window->rightclick_func), args, 3);
      return true;
   }
   return false;
}

void window_release(gui_window_t *window, int x, int y) {
   // call mouse release func if exists
   int index = get_window_index_from_pointer(window);
   int task = get_task_from_window(index);
   if(task > -1 && window->release_func) {
      task_state_t *task_state = &gettasks()[task];
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = x; // relative to window content
      args[1] = y;
      args[0] = get_cindex(task_state);
      wm_call_subroutine(task_state, "release", (uint32_t)(window->release_func), args, 3);
   }

   // check windowobjs
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      if(wo->clicked) {
         int relX = x - wo->x;
         int relY = y - wo->y;
         bool r = windowobj_release((void*)wo, relX, relY);
         windowobj_redraw(window, wo);
         if(r) return;
      }
   }
}

// === window actions ===

void window_drawcharat(char c, uint16_t colour, int x, int y, int windowIndex) {
   surface_t surface = window_getsurface(windowIndex);
   draw_char(&surface, c, colour, x, y);
}

void window_drawrect(uint16_t colour, int x, int y, int width, int height, int windowIndex) {
   surface_t surface = window_getsurface(windowIndex);
   draw_rect(&surface, colour, x, y, width, height);
}

void window_writestrat(char *c, uint16_t colour, int x, int y, int windowIndex) {
   int i = 0;
   while(c[i] != '\0') {
      window_drawcharat(c[i++], colour, x, y, windowIndex);
      x+=getFont()->width+getFont()->padding;
   }
}

void window_clearbuffer(gui_window_t *window, uint16_t colour) {
   memset16(window->framebuffer, colour, window->surface.width*(window->surface.height));
}

void window_writenumat(int num, uint16_t colour, int x, int y, int windowIndex) {
   char out[20];
   inttostr(num, out);
   window_writestrat(out, colour, x, y, windowIndex);
}

void window_drawchar(char c, uint16_t colour, int windowIndex) {

   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   if(c == '\n') {
      selected->text_x = getFont()->padding;
      selected->text_y += getFont()->height + getFont()->padding_y;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (getFont()->height+getFont()->padding_y))) {
         window_scroll(selected);
         selected->needs_redraw=true;
         gui_draw_window(windowIndex); // whole window needs redaw
      }

      // immediately output each line
      if(!selected->minimised && !selected->dragged)
         window_draw_content_region(selected, 0, selected->text_y - getFont()->height - getFont()->padding_y, selected->width, getFont()->height + getFont()->padding_y, true);

      return;
   }

   // x overflow
   if(selected->text_x + getFont()->width + getFont()->padding >= selected->width) {
      window_drawcharat('-', colour, selected->text_x-2, selected->text_y, windowIndex);
      selected->text_x = getFont()->padding;
      selected->text_y += getFont()->height + getFont()->padding;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (getFont()->height+getFont()->padding))) {
         window_scroll(selected);
      }
   }

   window_drawcharat(c, colour, selected->text_x, selected->text_y, windowIndex);
   selected->text_x+=getFont()->width+getFont()->padding;

   if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (getFont()->height+getFont()->padding))) {
      window_scroll(selected);
   }

   selected->needs_redraw = true;

}

void window_writestr(char *c, uint16_t colour, int windowIndex) {
   int i = 0;
   while(c[i] != '\0')
      window_drawchar(c[i++], colour, windowIndex);
}

void window_writestrn(char *c, size_t size, uint16_t colour, int windowIndex) {
   size_t i = 0;
   while(c[i] != '\0' && i < size)
      window_drawchar(c[i++], colour, windowIndex);
}

void window_writenum(int num, uint16_t colour, int windowIndex) {
   char out[20];
   inttostr(num, out);
   window_writestr(out, colour, windowIndex);
}

void window_writeuint(uint32_t num, uint16_t colour, int windowIndex) {
   char out[20];
   uinttostr(num, out);
   window_writestr(out, colour, windowIndex);
}

// scroll window content by 1 row of text
void window_scroll(gui_window_t *window) {
   uint16_t *terminal_buffer = window->framebuffer;

   int scrollY = getFont()->height+getFont()->padding_y;
   int copy_height = window->height - TITLEBAR_HEIGHT - scrollY;
   int copy_width = window->width;
   memmove(terminal_buffer, terminal_buffer + scrollY * window->width, copy_height * copy_width * sizeof(uint16_t));
   // clear bottom
   int newY = window->height - (scrollY + TITLEBAR_HEIGHT);
   draw_rect(&(window->surface), window->bgcolour, 0, newY, window->width, scrollY);
   window->text_y = newY;
   window->text_x = getFont()->padding;
}

void window_newline(gui_window_t* window) {
   window->text_x = getFont()->padding;
   window->text_y += getFont()->height + getFont()->padding;

   if(window->text_y > window->height - (TITLEBAR_HEIGHT + (getFont()->height+getFont()->padding))) {
      window_scroll(window);
   }

   // immediately output each line
   window->needs_redraw=true;
   window_draw_content(window, true); // call windowmgr and tell it to redraw
}

// windowobj functions

windowobj_t *window_create_button(gui_window_t *window, int x, int y, char *text, void (*func)(void *windowobj, int x, int y)) {
   windowobj_t *button = malloc(sizeof(windowobj_t));
   windowobj_init(button, &window->surface);
   button->type = WO_BUTTON;
   button->x = x;
   button->y = y;
   button->width = 50;
   button->height = 14;
   if(text) {
      char *newtext = (char*)malloc(strlen(text)+1);
      strcpy(newtext, text);
      button->text = newtext;
   }
   button->release_func = func;
   window->window_objects[window->window_object_count++] = button;
   windowobj_draw(button);

   return button;
}

windowobj_t *window_create_text(gui_window_t *window, int x, int y, char *text) {
   windowobj_t *textobj = malloc(sizeof(windowobj_t));
   windowobj_init(textobj, &window->surface);
   textobj->type = WO_TEXT;
   textobj->x = x;
   textobj->y = y;
   textobj->width = 100;
   textobj->height = 14;
   textobj->colour_bg = window->bgcolour;
   if(text) {
      int len = strlen(text);
      char *newtext = (char*)malloc(len + 1);
      strcpy(newtext, text);
      textobj->text = newtext;
      textobj->textpos = len;
      textobj->cursor_textpos = len;
   }
   window->window_objects[window->window_object_count++] = textobj;
   windowobj_draw(textobj);

   return textobj;
}

windowobj_t *window_create_menu(gui_window_t *window, int x, int y, windowobj_menu_t *menuitems, int menuitem_count) {
   windowobj_t *menu = malloc(sizeof(windowobj_t));
   windowobj_init(menu, &window->surface);
   menu->type = WO_MENU;
   menu->x = x;
   menu->y = y;
   menu->width = 100;
   menu->height = 30;
   menu->menuitems = menuitems;
   menu->menuitem_count = menuitem_count;
   window->window_objects[window->window_object_count++] = menu;
   windowobj_draw(menu);

   return menu;
}

// window scroll functions

static void window_default_scroll(int deltaY, gui_window_t *window) {
   // scroll every object by deltaY
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      if(wo != NULL && wo->type != WO_SCROLLBAR) {
         wo->y -= deltaY;
      }
   }
   window_clearbuffer(window, window->bgcolour);
   window_draw(window, true);
}

void window_scroll_do_callback(void *regs, gui_window_t *window, int deltaY) {
   void *callback = window->scrollbar->return_func;
   if(!callback) {
      window_default_scroll(deltaY, window); // deprecated
      return;
   }
   int offsetY = window->scrolledY;
   int taskIndex = get_task_from_window(get_window_index_from_pointer(window));
   if(taskIndex == -1) {
      // kernel
      (*(void(*)(int, int))callback)(deltaY, offsetY);
   } else {
      task_state_t *task = &gettasks()[taskIndex];
      uint32_t *args = malloc(sizeof(int) * 3);
      args[2] = deltaY;
      args[1] = offsetY;
      args[0] = get_cindex_from_window(task, window);
      if(!regs)
         wm_call_subroutine(task, "scroll", (uint32_t)(callback), args, 3);
      else
         task_call_subroutine(regs, task, "scroll", (uint32_t)(callback), args, 3);
   }
}

// called from dragged scroller
void window_scroll_update(windowobj_t *scroller) {
   gui_window_t *window = getSelectedWindow();
   int track = window->height - TITLEBAR_HEIGHT - 28 - scroller->height;
   int hidden = window->scrollable_content_height - (window->height - TITLEBAR_HEIGHT);
   if(track <= 0 || hidden <= 0) return;
   int y = (scroller->y - 14) * hidden / track;
   window_scroll_to(NULL, window, y);
}

void window_scroll_up_callback(void *wo) {
   // handle scroll up btn
   int deltaY = -SCROLL_AMOUNT;
   if(getSelectedWindow()->scrolledY + deltaY < 0) {
      deltaY = -getSelectedWindow()->scrolledY; // don't scroll past the top
   }

   getSelectedWindow()->scrolledY += deltaY;
   // update scroller position
   windowobj_t *scrollbar = ((windowobj_t*)wo)->parent;
   windowobj_t *scroller = (windowobj_t*)scrollbar->children[0];
   int scrollarea = getSelectedWindow()->height - TITLEBAR_HEIGHT - 28;
   int hiddenHeight = getSelectedWindow()->scrollable_content_height - (getSelectedWindow()->height - TITLEBAR_HEIGHT);
   if(hiddenHeight <= 0)
      return;

   scroller->y = 14 + (getSelectedWindow()->scrolledY * (scrollarea - scroller->height)) / (getSelectedWindow()->scrollable_content_height - (getSelectedWindow()->height - TITLEBAR_HEIGHT));
   if(scroller->y < 14)
      scroller->y = 14;

   window_scroll_do_callback(NULL, getSelectedWindow(), deltaY);
}

void window_scroll_down_callback(void *wo) {
   // handle scroll down btn

   gui_window_t *window = getSelectedWindow();
   int visible_height = window->height - TITLEBAR_HEIGHT;
   int scrollable_height = window->scrollable_content_height;
   int hidden_height = scrollable_height - visible_height;
   if (hidden_height <= 0) return;

   int deltaY = SCROLL_AMOUNT;
    
   if (window->scrolledY + deltaY > hidden_height) {
      deltaY = hidden_height - window->scrolledY;
   }
    
   if (deltaY <= 0) return;

   window->scrolledY += deltaY;
   // update scroller position
   windowobj_t *scrollbar = ((windowobj_t*)wo)->parent;
   windowobj_t *scroller = (windowobj_t*)scrollbar->children[0];
   int scrollarea = visible_height - 28;
   scroller->y = 14 + (window->scrolledY * (scrollarea - scroller->height)) / hidden_height;
   if(scroller->y > 14 + scrollarea - scroller->height)
      scroller->y = 14 + scrollarea - scroller->height;

   window_scroll_do_callback(NULL, window, deltaY);
}

void window_scroll_to(void *regs, gui_window_t *window, int y) {
   if(!window || !window->scrollbar) return;
   
   int scrollableHeight = window->scrollable_content_height - (window->height - TITLEBAR_HEIGHT);
   if(scrollableHeight <= 0) scrollableHeight = 0;
   
   int scrolledY = y;
   if(scrolledY < 0) scrolledY = 0;
   if(scrolledY > scrollableHeight) scrolledY = scrollableHeight;
   
   int deltaY = scrolledY - window->scrolledY;
   if(window->scrolledY == scrolledY) return; // already at correct position
   window->scrolledY = scrolledY;
   
   windowobj_t *scroller = (windowobj_t*)window->scrollbar->children[0];
   int scrollarea = window->height - TITLEBAR_HEIGHT - 28;
   if(scrollableHeight > 0 && !scroller->clicked)
      scroller->y = 14 + (window->scrolledY * (scrollarea - scroller->height)) / scrollableHeight;
   
   window_scroll_do_callback(regs, window, deltaY);
}

windowobj_t *window_create_scrollbar(gui_window_t *window, void (*callback)(int deltaY, int offsetY)) {
   // create scrollbar on right side of window
   int x = window->width - 14;
   int y = 0;
   int height = window->height - TITLEBAR_HEIGHT;
   
   windowobj_t *scrollbar = malloc(sizeof(windowobj_t));
   windowobj_init(scrollbar, &window->surface);
   scrollbar->type = WO_SCROLLBAR;
   scrollbar->x = x;
   scrollbar->y = y;
   scrollbar->width = 14;
   scrollbar->height = height;
   scrollbar->return_func = (void*)callback;

   char buf[2];
   buf[1] = '\0';

   // up button
   windowobj_t *upbtn = malloc(sizeof(windowobj_t));
   windowobj_init(upbtn, &window->surface);
   upbtn->type = WO_BUTTON;
   upbtn->x = 0;
   upbtn->y = 0;
   upbtn->width = 14;
   upbtn->height = 14;
   buf[0] = 0x80; // uparrow
   char *text = (char*)malloc(2);
   strcpy(text, buf);
   upbtn->text = text;
   upbtn->parent = scrollbar;
   upbtn->click_func = &window_scroll_up_callback;

   // down button
   windowobj_t *downbtn = malloc(sizeof(windowobj_t));
   windowobj_init(downbtn, &window->surface);
   downbtn->type = WO_BUTTON;
   downbtn->x = 0;
   downbtn->y = height - 14;
   downbtn->width = 14;
   downbtn->height = 14;
   buf[0] = 0x81; // down
   text = (char*)malloc(2);
   strcpy(text, buf);
   downbtn->text = text;
   downbtn->parent = scrollbar;
   downbtn->click_func = &window_scroll_down_callback;

   // scroller
   windowobj_t *scroller = malloc(sizeof(windowobj_t));
   windowobj_init(scroller, &window->surface);
   scroller->type = WO_SCROLLER;
   scroller->x = 0;
   scroller->y = 14;
   scroller->width = 14;
   scroller->parent = scrollbar;
   // work out height
   int scrollareaheight = window->height - TITLEBAR_HEIGHT - 28;
   if(window->scrollable_content_height > 0)
      scroller->height = (scrollareaheight * (window->height - TITLEBAR_HEIGHT)) / window->scrollable_content_height;
   else
      scroller->height = 0;
   if(scroller->height < 10) scroller->height = 10;
   if(scroller->height > scrollareaheight) scroller->height = scrollareaheight;

   buf[0] = '=';
   text = (char*)malloc(2);
   strcpy(text, buf);
   scroller->text = text;
   scroller->parent = scrollbar;

   scrollbar->children[0] = scroller;
   scrollbar->children[1] = upbtn;
   scrollbar->children[2] = downbtn;
   scrollbar->child_count = 3;

   window->scrollbar = scrollbar;
   window->window_objects[window->window_object_count++] = scrollbar;

   if(window->scrollable_content_height > window->height - TITLEBAR_HEIGHT) {
      windowobj_draw(scrollbar);
   } else {
      scrollbar->visible = false;
   }

   return scrollbar;
}

void window_set_scrollable_height(registers_t *regs, gui_window_t *window, int height) {
   window->scrollable_content_height = height;
   if(window->scrollbar != NULL) {

      windowobj_t *scroller = (windowobj_t*)window->scrollbar->children[0];
      // work out height
      int scrollareaheight = (window->surface.height - 28);
      if(window->scrollable_content_height)
         scroller->height = (scrollareaheight * window->surface.height) / window->scrollable_content_height;
      else
         scroller->height = 0;
      if(scroller->height < 10) scroller->height = 10;
      if(scroller->height > scrollareaheight) scroller->height = scrollareaheight;
      
      bool resize = false;

      if(window->scrollable_content_height > window->surface.height) {
         if(!window->scrollbar->visible) {
            resize = true;
         }
         window->scrollbar->visible = true;
         windowobj_draw(window->scrollbar);
      } else {
         if(window->scrollbar->visible) {
            resize = true;
         }
         window->scrollbar->visible = false;
      }

      if(resize) {
         // call resize func if exists
         int index = get_window_index_from_pointer(window);
         int task = get_task_from_window(index);
         if(regs && task > -1 && window->resize_func) {
            task_state_t *task_state = &gettasks()[task];
            uint32_t *args = malloc(sizeof(uint32_t) * 4);
            args[3] = (uint32_t)window->framebuffer;
            args[2] = window->width - (window->scrollbar && window->scrollbar->visible ? 14 : 0);
            args[1] = window->height - TITLEBAR_HEIGHT;
            args[0] = get_cindex_from_window(task_state, window);
            map_size(task_state->process->page_dir, (uint32_t)window->framebuffer, (uint32_t)window->framebuffer, window->framebuffer_size, 1, 1, 0);
            window_clearbuffer(window, window->bgcolour);
            task_call_subroutine(regs, task_state, "resize", (uint32_t)(window->resize_func), args, 4);
         }
      }
   }
}
