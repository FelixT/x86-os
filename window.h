#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "lib/string.h"
#include "registers_t.h"
#include "window_t.h"

void window_scroll(gui_window_t *window);

void window_drawcharat(char c, uint16_t colour, int x, int y, int windowIndex);
void window_drawrect(uint16_t colour, int x, int y, int width, int height, int windowIndex);
void window_writestrat(char *c, uint16_t colour, int x, int y, int windowIndex);
void window_clearbuffer(gui_window_t *window, uint16_t colour);
void window_writeuint(uint32_t num, uint16_t colour, int windowIndex);
void window_writestr(char *c, uint16_t colour, int windowIndex);
void window_writestrn(char *c, size_t size, uint16_t colour, int windowIndex);
void window_drawchar(char c, uint16_t colour, int windowIndex);
void window_writenum(int num, uint16_t colour, int windowIndex);
void window_writenumat(int num, uint16_t colour, int x, int y, int windowIndex);
void window_newline(gui_window_t* window);
windowobj_t *window_create_button(gui_window_t *window, int x, int y, char *text, void (*func)(void *window, void *regs));
windowobj_t *window_create_text(gui_window_t *window, int x, int y, char *text);
windowobj_t *window_create_menu(gui_window_t *window, int x, int y, windowobj_menu_t *menuitems, int menuitem_count);
windowobj_t *window_create_scrollbar(gui_window_t *window, void (*callback)(int deltaY, int offsetY));
void window_set_scrollable_height(registers_t *regs, gui_window_t *window, int height);
void window_scroll_to(void *regs, int y);

#endif