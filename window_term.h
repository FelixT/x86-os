#ifndef WINDOW_TERM
#define WINDOW_TERM

#include "window_t.h"

#define CMD_BUFFER_LENGTH 64

// default, terminal style window behaviour
void window_term_return(void *regs, void *window);
void window_term_keypress(uint16_t key, void *window);
void window_term_backspace(void *window);
void window_term_uparrow(void *window);
void window_term_downarrow(void *window);
void window_term_draw(void *window);
void window_term_checkcmd(void *regs, void *window);

#endif