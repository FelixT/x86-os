#ifndef WINMGR
#define WINMGR

#include <stdint.h>
#include <stdbool.h>
#include "window_t.h"
#include "registers_t.h"
#include "tasks.h"

#define WM_SETTING_STR_LEN 256
typedef struct windowmgr_settings_t {
   uint16_t default_window_bgcolour;
   uint16_t default_window_txtcolour;
   bool desktop_enabled;
   bool desktop_bgimg_enabled;
   char desktop_bgimg[WM_SETTING_STR_LEN]; // path
   uint16_t titlebar_colour;
   int theme; // 0 = classic, 1 = gradient
   uint16_t titlebar_colour2; // used for gradient
   int titlebar_gradientstyle; // 0 = horizontal, 1 = vertical
   char font_path[WM_SETTING_STR_LEN];
} windowmgr_settings_t;

#define MAX_WINDOWS 64

typedef enum {
   MOUSE_CLICK,
   MOUSE_RIGHTCLICK,
   MOUSE_RELEASE,
   MOUSE_HOVER,
   MOUSE_DRAG,
   MOUSE_SCROLL,
   KEY_PRESS,
   WINDOW_CLOSE
} wm_event_type_t;

#define SCROLL_UP 1
#define SCROLL_DOWN 0
#define SCROLL_AMOUNT 20

void wm_draw();
void wm_redrawall();
void wm_event(wm_event_type_t type, int x, int y, uint16_t c);
void wm_event_thread_exit_resume(registers_t *regs);
void wm_queue_launch(char *path, bool focus, bool minimised);
void wm_call_subroutine(task_state_t *task, char *name, uint32_t addr, uint32_t *args, int argc);
void wm_event_defer_yield(int task);

bool cursor_hide_region(int x, int y, int width, int height);
void cursor_show(bool hidden);

void window_draw_content_region(gui_window_t *window, int offsetX, int offsetY, int width, int height);
void window_draw_content(gui_window_t *window);
void window_draw_outline(gui_window_t *window, bool occlude);
void windowmgr_redrawall();

int windowmgr_add();
bool window_init(gui_window_t *window);
int getSelectedWindowIndex();
void setSelectedWindowIndex(int index);
int getWindowCount();
void windowmgr_init();
void debug_writestr(char *str);
void debug_writeuint(uint32_t num);
void debug_writehex(uint32_t num);
void debug_printf(char *format, ...);
gui_window_t *getWindow(int index);
gui_window_t *getSelectedWindow();
void windowmgr_keypress(int scan_code);
void window_draw(gui_window_t *window);
void toolbar_draw();
bool windowmgr_click(int x, int y);
void windowmgr_rightclick(int x, int y);
void windowmgr_dragged(int relX, int relY);
void desktop_draw();
void desktop_click(int x, int y);
void desktop_init();
void desktop_setbgimg(uint8_t *img, int size);
void windowmgr_mousemove(int x, int y);
void windowmgr_scroll(int y, int w);
void window_resize(registers_t *regs, gui_window_t *window, int width, int height, bool callback);
void window_close(int windowIndex, bool end);
void window_release(gui_window_t *window);
int get_window_index_from_pointer(gui_window_t *window);
void window_resetfuncs(gui_window_t *window);
void window_removefuncs(gui_window_t *window);
void window_disable(gui_window_t *window);
windowmgr_settings_t *windowmgr_get_settings();
int get_cindex(task_state_t *task);
int get_cindex_from_window(task_state_t *task, gui_window_t *window);

#endif