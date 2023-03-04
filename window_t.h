#ifndef WINDOW_T_H
#define WINDOW_T_H

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
   bool active;
   bool minimised;
   bool closed;
	bool dragged;
   int toolbar_pos; // index in toolbar
   uint16_t *framebuffer; // width*(height-titlebar_height)

	// function pointers
	void (*return_func)(void *regs, int windowIndex);
	void (*keypress_func)(char key, int windowIndex);
   void (*backspace_func)(int windowIndex);
   void (*uparrow_func)(int windowIndex);
   void (*downarrow_func)(int windowIndex);
   void (*click_func)(int windowIndex, int x, int y);

} gui_window_t;

#endif