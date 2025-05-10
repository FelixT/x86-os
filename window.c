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

surface_t window_getsurface(int windowIndex) {
   gui_window_t *window = &(gui_get_windows()[windowIndex]);
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height;
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
   for(int i = 0; i < window->width*(window->height-TITLEBAR_HEIGHT); i++) {
      window->framebuffer[i] = colour;
   }
}

void window_writenumat(int num, uint16_t colour, int x, int y, int windowIndex) {
   if(num < 0)
      window_drawcharat('-', colour, x+=(getFont()->width+getFont()->padding), y, windowIndex);

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
      if(!selected->minimised)
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

void window_writenum(int num, uint16_t colour, int windowIndex) {
   if(num < 0)
      window_drawchar('-', colour, windowIndex);

   char out[20];
   inttostr(num, out);
   window_writestr(out, colour, windowIndex);
}

void window_writeuint(uint32_t num, uint16_t colour, int windowIndex) {
   char out[20];
   uinttostr(num, out);
   window_writestr(out, colour, windowIndex);
}


void window_scroll(gui_window_t *window) {
   uint16_t *terminal_buffer = window->framebuffer;

   int scrollY = getFont()->height+getFont()->padding_y;
   for(int y = scrollY; y < window->height - TITLEBAR_HEIGHT; y++) {
      for(int x = window->x; x < window->x + window->width; x++) {
         int srcIndex = y*(int)window->width+x;
         int outIndex = (y-scrollY)*window->width+x;
         if(srcIndex >= 0 && srcIndex < (int)gui_get_width()*(int)gui_get_height()
         && outIndex >= 0 && outIndex < window->width*(window->height-TITLEBAR_HEIGHT))
            terminal_buffer[outIndex] = terminal_buffer[srcIndex];
      }
   }
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