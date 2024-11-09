#ifndef WINDOW_TERM
#define WINDOW_TERM

#include "window_t.h"

// default, terminal style window behaviour
void window_term_return(void *regs, void *window);
void window_term_keypress(char key, void *window);
void window_term_backspace(void *window);
void window_term_uparrow(void *window);
void window_term_downarrow(void *window);
void window_term_draw(void *window);
void window_checkcmd(void *regs, gui_window_t *window);

#endif