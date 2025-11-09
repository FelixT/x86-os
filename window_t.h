#ifndef WINDOW_T_H
#define WINDOW_T_H

#include <stdbool.h>
#include "surface_t.h"
#include "windowobj.h"

#define TEXT_BUFFER_LENGTH 64
#define CMD_HISTORY_LENGTH 10

#define W_CHILDCOUNT 10

typedef struct gui_window_t {
   char title[20];
   int x;
   int y;
   int width;
   int height; // includes 17px titlebar (titlebar_height)
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
   bool resizable;
   int toolbar_pos; // index in toolbar
   bool disabled;
   uint16_t bgcolour; // default
   uint16_t txtcolour; // default
   uint16_t *framebuffer; // width*(height-titlebar_height)
   uint32_t framebuffer_size;
   int scrollable_content_height; // excludes titlebar height
   int scrolledY;
   windowobj_t *scrollbar;

   surface_t surface;

   windowobj_t *window_objects[20];
   int window_object_count;

	// function pointers, window is type *gui_window_t
	void (*keypress_func)(uint16_t key, void *window);
   void (*click_func)(int x, int y);
   void (*drag_func)(int x, int y);
   void (*draw_func)(void *window);
   //void (*write_func)(void *window, char *string);
   void (*resize_func)(uint32_t fb, uint32_t w, uint32_t h);
   void (*release_func)();
   void (*checkcmd_func)(void *regs, void *window); // override terminal behaviour
   void (*read_func)(void *regs, char *buffer); // kernel override terminal behaviour
   int read_task; // task to switch on read
   char *read_buffer; // buffer for read_func, used by terminal

   void *state;
   int state_size;
   void (*state_free)(void *window);

   void *children[W_CHILDCOUNT]; // child windows
   int child_count;
} gui_window_t;

#endif