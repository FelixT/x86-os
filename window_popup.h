#ifndef WINDOW_POPUP_H
#define WINDOW_POPUP_H

#include "window_t.h"
#include "fat.h"

typedef struct window_popup_dialog_t {
   gui_window_t *parent;
   windowobj_t *wo_okbtn;
   void (*callback_func)(void *dialog, void *res);
   void (*dismiss_func)(void *dialog); // run if window closed
   bool answered; // set on button click, false if dialog window closed
   uint32_t process_uid; // for callbacks related to a specific task
   int task_id;
} window_popup_dialog_t;

typedef struct window_popup_colourpicker_t {
   void (*callback_func)(uint16_t colour);
   gui_window_t *parent;
} window_popup_colourpicker_t;

window_popup_dialog_t *window_popup_dialog(gui_window_t *window, gui_window_t *parent, char *text);
window_popup_colourpicker_t *window_popup_colourpicker(gui_window_t *window, gui_window_t *parent, void *callback, uint16_t colour);

#endif