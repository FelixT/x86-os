#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "string.h"
#include "registers_t.h"
#include "window_t.h"

// default, terminal style window behaviour
void window_term_return(void *regs, int windowIndex);
void window_term_keypress(char key, int windowIndex);
void window_term_backspace(int windowIndex);
void window_term_uparrow(int windowIndex);
void window_term_downarrow(int windowIndex);
void window_term_draw(int windowIndex);

void window_checkcmd(void *regs, int windowIndex);
void window_scroll();

void window_drawcharat(char c, uint16_t colour, int x, int y, int windowIndex);
void window_drawrect(uint16_t colour, int x, int y, int width, int height, int windowIndex);
void window_writestrat(char *c, uint16_t colour, int x, int y, int windowIndex);
void window_clearbuffer(gui_window_t *window, uint16_t colour);
void window_writeuint(uint32_t num, uint16_t colour, int windowIndex);
void window_writestr(char *c, uint16_t colour, int windowIndex);
void window_drawchar(char c, uint16_t colour, int windowIndex);
void window_writenum(int num, uint16_t colour, int windowIndex);
void window_writenumat(int num, uint16_t colour, int x, int y, int windowIndex);


#endif