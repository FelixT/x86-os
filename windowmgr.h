#ifndef WINMGR
#define WINMGR

#include <stdint.h>
#include <stdbool.h>
#include "window_t.h"
#include "registers_t.h"

typedef struct gui_menu_t {
   int x;
   int y;
} gui_menu_t;

int windowmgr_add();
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
void windowmgr_keypress(void *regs, int scan_code);
void window_draw(gui_window_t *window);
void toolbar_draw();
void gui_uparrow();
void gui_downarrow();
void window_draw_content_region(gui_window_t *window, int offsetX, int offsetY, int width, int height);
void window_draw_content(gui_window_t *window);
bool windowmgr_click(void *regs, int x, int y);
void windowmgr_rightclick(void *regs, int x, int y);
void windowmgr_draw();
void windowmgr_redrawall();
void windowmgr_dragged();
void desktop_draw();
void desktop_click(registers_t *regs, int x, int y);
void desktop_init();
void desktop_setbgimg(uint8_t *img);
void windowmgr_mousemove(int x, int y);
void menu_draw(gui_menu_t *menu);
void window_resize(registers_t *regs, gui_window_t *window, int width, int height);

#endif