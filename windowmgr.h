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
void debug_writestr();
gui_window_t *getWindow(int index);
gui_window_t *getSelectedWindow();
void windowmgr_keypress();
void window_draw(int index);

#endif