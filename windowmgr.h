#ifndef WINMGR
#define WINMGR

#include <stdint.h>
#include <stdbool.h>
#include "window_t.h"
#include "registers_t.h"

typedef struct windowmgr_settings_t {
   uint16_t default_window_bgcolour;
   uint16_t default_window_txtcolour;
   bool desktop_enabled;
   bool desktop_bgimg_enabled;
   char desktop_bgimg[256]; // path
   uint16_t titlebar_colour;
   int theme; // 0 = classic, 1 = gradient
   uint16_t titlebar_colour2; // used for gradient
   int titlebar_gradientstyle; // 0 = horizontal, 1 = vertical
   char font_path[256];
} windowmgr_settings_t;

int windowmgr_add();
bool window_init(gui_window_t *window);
int getSelectedWindowIndex();
void setSelectedWindowIndex();
int getWindowCount();
void windowmgr_init();
void debug_writestr(char *str);
void debug_writeuint(uint32_t num);
void debug_writehex(uint32_t num);
void debug_printf(char *format, ...);
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
void desktop_setbgimg(uint8_t *img, int size);
void windowmgr_mousemove(int x, int y);
void windowmgr_scroll(bool up);
void window_resize(registers_t *regs, gui_window_t *window, int width, int height);
void window_close(void *regs, int windowIndex);
void window_release(registers_t *regs, gui_window_t *window);
int get_window_index_from_pointer(gui_window_t *window);
void window_resetfuncs(gui_window_t *window);
void window_removefuncs(gui_window_t *window);
void window_disable(gui_window_t *window);
void window_draw_outline(gui_window_t *window, bool occlude);
windowmgr_settings_t *windowmgr_get_settings();
void windowmgr_launch_apps();
int get_cindex();
int get_cindex_from_window(gui_window_t *window);

#endif