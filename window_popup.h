#ifndef WINDOW_POPUP_H
#define WINDOW_POPUP_H

#include "window_t.h"
#include "fat.h"

typedef struct window_popup_dialog_t {
   gui_window_t *parent;
   windowobj_t *wo_output;
   void (*callback_func)(char *output);
} window_popup_dialog_t;

typedef struct window_popup_filepicker_t {
   fat_dir_t *currentdir;
   windowobj_t *wo_path;
   void (*callback_func)(char *path);
   gui_window_t *parent;
} window_popup_filepicker_t;

typedef struct window_popup_colourpicker_t {
   void (*callback_func)(uint16_t colour);
   gui_window_t *parent;
} window_popup_colourpicker_t;

void window_popup_dialog(gui_window_t *window, gui_window_t *parent, char *text, bool output, void *return_func);
window_popup_filepicker_t *window_popup_filepicker(gui_window_t *window, gui_window_t *parent, void *callback);
window_popup_colourpicker_t *window_popup_colourpicker(gui_window_t *window, gui_window_t *parent, void *callback, uint16_t colour);

#endif