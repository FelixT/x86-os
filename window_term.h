#ifndef WINDOW_TERM
#define WINDOW_TERM

#include "window_t.h"

// default, terminal style window behaviour
void window_term_return(void *regs, void *window);
void window_term_keypress(char key, int windowIndex);
void window_term_backspace(int windowIndex);
void window_term_uparrow(int windowIndex);
void window_term_downarrow(int windowIndex);
void window_term_draw(int windowIndex);
void window_checkcmd(void *regs, gui_window_t *window);

#endif