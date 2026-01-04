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

surface_t window_getsurface(int windowIndex) {
   gui_window_t *window = &(gui_get_windows()[windowIndex]);
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height - TITLEBAR_HEIGHT;
   surface.buffer = (uint32_t)window->framebuffer;
   return surface;
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
   for(int i = 0; i < window->surface.width*(window->surface.height); i++) {
      window->framebuffer[i] = colour;
   }
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
         window_draw_content_region(selected, 0, selected->text_y - getFont()->height - getFont()->padding_y, selected->width, getFont()->height + getFont()->padding_y);

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

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
   if(d < s) {
      while(n--) {
         *d++ = *s++;
      }
   } else if(d > s) {
      d += n;
      s += n;
      while(n--) {
         *--d = *--s;
      }
   }   
   return dest;
}


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

extern void window_draw_content(gui_window_t *window);
void window_newline(gui_window_t* window) {
   window->text_x = getFont()->padding;
   window->text_y += getFont()->height + getFont()->padding;

   if(window->text_y > window->height - (TITLEBAR_HEIGHT + (getFont()->height+getFont()->padding))) {
      window_scroll(window);
   }

   // immediately output each line
   window->needs_redraw=true;
   window_draw_content(window); // call windowmgr and tell it to redraw
}

// windowobj functions

windowobj_t *window_create_button(gui_window_t *window, int x, int y, char *text, void (*func)(void *window, void *regs)) {
   windowobj_t *button = malloc(sizeof(windowobj_t));
   windowobj_init(button, &window->surface);
   button->type = WO_BUTTON;
   button->x = x;
   button->y = y;
   button->width = 50;
   button->height = 14;
   if(text) {
      char *newtext = (char*)malloc(strlen(text));
      strcpy(newtext, text);
      button->text = newtext;
   }
   button->release_func = (void*)func;
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

void window_default_scroll(int deltaY) {
   // scroll every object by deltaY
   for(int i = 0; i < getSelectedWindow()->window_object_count; i++) {
      windowobj_t *wo = getSelectedWindow()->window_objects[i];
      if(wo != NULL && wo->type != WO_SCROLLBAR) {
         wo->y -= deltaY;
      }
   }
   window_clearbuffer(getSelectedWindow(), getSelectedWindow()->bgcolour);
   window_draw(getSelectedWindow());
}

void window_scroll_do_callback(void *regs, void *callback, int deltaY, int offsetY) {
   if(callback) {
      if(get_task_from_window(getSelectedWindowIndex()) == -1) {
         // kernel
         (*(void(*)(int, int))callback)(deltaY, offsetY);
      } else {
         gui_interrupt_switchtask(regs);
         uint32_t *args = malloc(sizeof(int) * 3);
         args[2] = deltaY;
         args[1] = offsetY;
         args[0] = get_cindex();

         task_call_subroutine(regs, "scroll", (uint32_t)(callback), args, 3);
      }
   } else {
      window_default_scroll(deltaY);
   }
}

void window_scroll_callback(void *wo, void *regs, int x, int y) {
   windowobj_t *scroller = (windowobj_t*)wo;
   (void)regs;
   (void)x;
   (void)y;

   int scrollArea = getSelectedWindow()->height - TITLEBAR_HEIGHT - 28;
   if(scrollArea - scroller->height <= 0) {
      getSelectedWindow()->scrollbar->visible = false;
      window_scroll_to(regs, 0);
      return;
   }

   int scrollPercent = (scroller->y - 14) * 100 / (scrollArea - scroller->height);
   int hiddenY = getSelectedWindow()->scrollable_content_height - (getSelectedWindow()->height - TITLEBAR_HEIGHT);
   if(hiddenY <= 0) return;
   int scrolledY = scrollPercent * hiddenY / 100;

   int scrollPixels = scrolledY - getSelectedWindow()->scrolledY;
   getSelectedWindow()->scrolledY = scrolledY;
   if(scrollPixels == 0) return;

   windowobj_t *scrollbar = (windowobj_t*)scroller->parent;
   window_scroll_do_callback(regs, scrollbar->return_func, scrollPixels, scrolledY);
}

// periodically called while scrolling
void window_scroll_event(void *regs, void *msg) {
   if(getSelectedWindowIndex() != (int)msg) return;
   if(!getSelectedWindow() || !getSelectedWindow()->scrollbar) return;
   windowobj_t *scroller = getSelectedWindow()->scrollbar->children[0];
   if(!scroller->clicked) return; // not being dragged
   window_scroll_callback(scroller, regs, 0, 0);
   events_add(20, &window_scroll_event, msg, -1);
}

void window_scroll_up_callback(void *wo, void *regs) {
   (void)regs;
   // scroll up by 10 pixels
   int deltaY = -10;
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

   window_scroll_do_callback(regs, scrollbar->return_func, deltaY, getSelectedWindow()->scrolledY);
}

void window_scroll_down_callback(void *wo, void *regs) {
   (void)regs;
   // scroll down by 10 pixels

   gui_window_t *window = getSelectedWindow();
   int visible_height = window->height - TITLEBAR_HEIGHT;
   int scrollable_height = window->scrollable_content_height;
   int hidden_height = scrollable_height - visible_height;
   if (hidden_height <= 0) return;

   int deltaY = 10;
    
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

   window_scroll_do_callback(regs, scrollbar->return_func, deltaY, window->scrolledY);
}

void window_scroll_to(void *regs, int y) {
   gui_window_t *window = getSelectedWindow();
   if(!window || !window->scrollbar) return;
   
   int scrollableHeight = window->scrollable_content_height - (window->height - TITLEBAR_HEIGHT);
   if(scrollableHeight <= 0) scrollableHeight = 0;
   
   int scrolledY = y;
   if(scrolledY < 0) scrolledY = 0;
   if(scrolledY > scrollableHeight) scrolledY = scrollableHeight;
   
   int deltaY = scrolledY - window->scrolledY;
   window->scrolledY = scrolledY;
   
   windowobj_t *scroller = (windowobj_t*)window->scrollbar->children[0];
   int scrollarea = window->height - TITLEBAR_HEIGHT - 28;
   if(scrollableHeight > 0)
      scroller->y = 14 + (window->scrolledY * (scrollarea - scroller->height)) / scrollableHeight;
   
   window_scroll_do_callback(regs, window->scrollbar->return_func, deltaY, window->scrolledY);
}

void window_scroll_start(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   events_add(20, &window_scroll_event, (void*)getSelectedWindowIndex(), -1);
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
   scroller->release_func = &window_scroll_callback;
   scroller->click_func = &window_scroll_start;

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
      int scrollareaheight = (window->height - TITLEBAR_HEIGHT - 28);
      if(window->scrollable_content_height)
         scroller->height = (scrollareaheight * (window->height - TITLEBAR_HEIGHT)) / window->scrollable_content_height;
      else
         scroller->height = 0;
      if(scroller->height < 10) scroller->height = 10;
      if(scroller->height > scrollareaheight) scroller->height = scrollareaheight;
      
      bool resize = false;

      if(window->scrollable_content_height > window->height - TITLEBAR_HEIGHT) {
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
            if(!switch_to_task(task, regs)) return;
            uint32_t *args = malloc(sizeof(uint32_t) * 4);
            args[3] = (uint32_t)window->framebuffer;
            args[2] = window->width - (window->scrollbar && window->scrollbar->visible ? 14 : 0);
            args[1] = window->height - TITLEBAR_HEIGHT;
            args[0] = get_cindex_from_window(window);
            map_size(get_current_task_pagedir(), (uint32_t)window->framebuffer, (uint32_t)window->framebuffer, window->framebuffer_size, 1, 1);
            map_size(get_current_task_pagedir(), (uint32_t)args, (uint32_t)args, sizeof(uint32_t)*4, 1, 1);
            window_clearbuffer(window, window->bgcolour);
            task_call_subroutine(regs, "resize", (uint32_t)(window->resize_func), args, 4);
         }
      }
   }
}
