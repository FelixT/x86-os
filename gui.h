#ifndef GUI_H
#define GUI_H

#include "memory.h"
#include "tasks.h"

#define FONT_WIDTH 7
#define FONT_HEIGHT 11
#define FONT_PADDING 2
#define TITLEBAR_HEIGHT 17
#define TOOLBAR_HEIGHT 22
#define TOOLBAR_ITEM_WIDTH 30
#define TOOLBAR_ITEM_HEIGHT 15
#define TOOLBAR_PADDING 4

#define NUM_WINDOWS 4

typedef struct gui_window_t {
   char title[20];
   int x;
   int y;
   int width;
   int height; // includes 10px titlebar
   char text_buffer[40];
   int text_index;
   int text_x;
   int text_y;
   bool needs_redraw;
   bool active;
   bool minimised;
	bool dragged;
   int toolbar_pos; // index in toolbar
   uint8_t *framebuffer; // width*(height-titlebar_height)
} gui_window_t;


// https://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
typedef struct vbe_mode_info_t {
	uint16_t attributes;
	uint8_t window_a;
	uint8_t window_b;
	uint16_t granularity;
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;
	uint16_t pitch;
	uint16_t width;
	uint16_t height;
	uint8_t w_char;
	uint8_t y_char;
	uint8_t planes;
	uint8_t bpp;
	uint8_t banks;
	uint8_t memory_model;
	uint8_t bank_size;
	uint8_t image_pages;
	uint8_t reserved0;
	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;
	uint32_t framebuffer;
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;
	uint8_t reserved1[206];
} __attribute__ ((packed)) vbe_mode_info_t;

void strcpy(char* dest, char* src);
void strcpy_fixed(char* dest, char* src, int length);

void gui_clear(uint8_t colour);
void gui_drawchar(char c, int colour);
void gui_writenumat(int num, int colour, int x, int y);
void gui_writenum(int num, int colour);
void gui_writestr(char *c, int colour);
void gui_drawrect(uint8_t colour, int x, int y, int width, int height);
void gui_keypress(char key);
void gui_return(void *regs);
void gui_backspace();
void gui_writestrat(char *c, int colour, int x, int y);
void gui_window_writenum(int num, int colour, int windowIndex);
void gui_window_draw(int windowIndex);
void gui_draw();
void gui_writeuintat(uint32_t num, int colour, int x, int y);
void gui_writeuint(uint32_t num, int colour);
void gui_writeuint_hex(uint32_t num, int colour);

#endif