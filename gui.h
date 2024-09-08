#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ata.h"
#include "memory.h"
#include "tasks.h"
#include "fat.h"
#include "paging.h"
#include "string.h"
#include "font.h"
#include "bmp.h"
#include "elf.h"

#include "window_t.h"

#include "draw.h"

#define TITLEBAR_HEIGHT 17
#define TOOLBAR_ITEM_WIDTH 50
#define TOOLBAR_PADDING 4
#define TOOLBAR_ITEM_HEIGHT 13
#define TOOLBAR_HEIGHT 20

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
bool strsplit(char* dest1, char* dest2, char* src, char splitat);
void strtoupper(char* dest, char* src);

uint16_t gui_rgb16(uint8_t r, uint8_t g, uint8_t b);

#define COLOUR_WINDOW_OUTLINE gui_rgb16(230, 230, 230)
#define COLOUR_DARK_GREY gui_rgb16(65, 65, 65) 
#define COLOUR_LIGHT_GREY gui_rgb16(180, 180, 180) 
#define COLOUR_TASKBAR_ENTRY gui_rgb16(100, 100, 100)
#define COLOUR_TOOLBAR gui_rgb16(200, 200, 200)
#define COLOUR_TITLEBAR gui_rgb16(200, 200, 200)
#define COLOUR_WHITE gui_rgb16(255, 255, 255)
#define COLOUR_CYAN gui_rgb16(0, 128, 138)
#define COLOUR_ORANGE gui_rgb16(200, 125, 0)

void gui_init();
void gui_clear(uint16_t colour);
void gui_drawchar(char c, uint16_t colour);
void gui_writenumat(int num, uint16_t colour, int x, int y);
void gui_writenum(int num, uint16_t colour);
void gui_writestr(char *c, uint16_t colour);
void gui_drawrect(uint16_t colour, int x, int y, int width, int height);
void gui_writestrat(char *c, uint16_t colour, int x, int y);
void gui_draw();
void gui_writeuintat(uint32_t num, uint16_t colour, int x, int y);
void gui_writeuint(uint32_t num, uint16_t colour);
void gui_writeuint_hex(uint32_t num, uint16_t colour);
void gui_desktop_init();
void gui_drawline(uint16_t colour, int x, int y, bool vertical, int length);
void gui_drawunfilledrect(uint16_t colour, int x, int y, int width, int height);
void gui_drawdottedrect(uint16_t colour, int x, int y, int width, int height);
void gui_drawcharat(char c, uint16_t colour, int x, int y);

void gui_interrupt_switchtask(void *regs);
void gui_keypress(void *regs, char scan_code);
void gui_return(void *regs);
void gui_backspace();
void gui_uparrow();
void gui_downarrow();

void gui_checkcmd(void *regs);

int gui_window_add();
void gui_draw_window(int windowIndex);

uint16_t *gui_get_framebuffer();
uint32_t gui_get_framebuffer_size();
gui_window_t *gui_get_windows();
size_t gui_get_width();
size_t gui_get_height();
uint32_t gui_get_window_framebuffer(int windowIndex);
int gui_gettextwidth(int textlength);
surface_t *gui_get_surface(); 

void gui_showtimer(int number);

void mouse_enable();

#endif