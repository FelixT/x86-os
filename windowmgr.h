#ifndef WINMGR
#define WINMGR

#include <stdint.h>
#include <stdbool.h>
#include "window_t.h"

int gui_window_add();
bool window_init(gui_window_t *window);
int getSelectedWindowIndex();
void setSelectedWindowIndex();
int getWindowCount();
void windowmgr_init();
void debug_writestr(char *str);
void debug_writeuint(uint32_t num);
void debug_writehex(uint32_t num);
gui_window_t *getWindow(int index);
gui_window_t *getSelectedWindow();
void windowmgr_keypress();
void window_draw(int index);
void toolbar_draw();
void gui_uparrow();
void gui_downarrow();
void window_draw_content(gui_window_t *window);

#endif