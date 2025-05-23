#ifndef WINDOW_T_H
#define WINDOW_T_H

#include <stdbool.h>
#include "surface_t.h"
#include "windowobj.h"

#define TEXT_BUFFER_LENGTH 40
#define CMD_HISTORY_LENGTH 10

typedef struct gui_window_t {
   char title[20];
   int x;
   int y;
   int width;
   int height; // includes 10px titlebar
   char text_buffer[TEXT_BUFFER_LENGTH];
   char *cmd_history[CMD_HISTORY_LENGTH];
   int cmd_history_pos;
   int text_index;
   int text_x;
   int text_y;
   bool needs_redraw;
   bool active; // selected
   bool minimised;
   bool closed;
	bool dragged;
   bool resized;
   int toolbar_pos; // index in toolbar
   uint16_t bgcolour; // default
   uint16_t txtcolour; // default
   uint16_t *framebuffer; // width*(height-titlebar_height)
   uint32_t framebuffer_size;

   surface_t surface;

   windowobj_t *window_objects[20];
   int window_object_count;

	// function pointers, window is type *gui_window_t
	void (*return_func)(void *regs, void *window);
	void (*keypress_func)(char key, void *window);
   void (*backspace_func)(void *window);
   void (*uparrow_func)(void *window);
   void (*downarrow_func)(void *window);
   void (*click_func)(int x, int y);
   void (*drag_func)(int x, int y);
   void (*draw_func)(void *window);
   //void (*write_func)(void *window, char *string);
   void (*resize_func)(uint32_t fb, uint32_t w, uint32_t h);
   void (*mouserelease_func)();
   void (*checkcmd_func)(char *buffer); // override terminal behaviour

   void *state;
} gui_window_t;

#endif