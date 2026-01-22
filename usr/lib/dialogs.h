#ifndef DIALOGS_H
#define DIALOGS_H

#include "ui/ui_mgr.h"
#include "ui/ui_input.h"
#include "ui/ui_button.h"
#include "ui/ui_label.h"
#include "ui/ui_grid.h"
#include "ui/ui_groupbox.h"
#include "ui/ui_canvas.h"
#include "ui/ui_image.h"
#include "ui/ui_menu.h"
#include "stdint.h"
#include "map.h"

typedef struct dialog_t {
   bool active;
   int type;
   int window;
   char title[32];
   surface_t surface;
   void (*callback)(char *out, int window);
   ui_mgr_t *ui;
   map_t wo_map; // string->wo_t* map
   int content_height; // should probably be in ui
   
   // txtinput specific
   wo_t *input_wo;
   // filepicker specific
   fs_dir_content_t *dir;
   uint16_t *file_icon_data;
   uint16_t *folder_icon_data;
   // window settings specific
   int parentWindow;

   void *state; // for extending dialog functionality
} dialog_t;

typedef struct dialog_wo_t {
   wo_t wo;
   void (*callback)(char *out, int window, wo_t *wo);
   int window; // window where this is displayed, used for callback
   void *state;
} dialog_wo_t;

bool dialog_msg(char *title, char *text);
int dialog_input(char *text, void *return_func);
int dialog_colourpicker(uint16_t colour, void (*return_func)(char *out, int window));
dialog_t *get_dialog(int index);
dialog_t *dialog_from_window(int window);
int dialog_filepicker(char *startdir, void (*return_func)(char *out, int window));
void dialog_init_overrides(int window);
int get_free_dialog();
void dialog_init(dialog_t *dialog, int window);
int dialog_window_settings(int window, char *title);
wo_t *dialog_create_colourbox(int x, int y, int width, int height, uint16_t colour, int window, void (*callback)(char *out, int window, wo_t *colourbox));
void dialog_set_title(dialog_t *dialog, char *title);
int dialog_yesno(char *title, char *text, void *return_func);
void dialog_add(dialog_t *dialog, char *key, wo_t *wo);
wo_t *dialog_get(dialog_t *dialog, char *key);
wo_t *dialog_create_browsebtn(int x, int y, int width, int height, int window, char *text, char *startpath, void (*callback)(char *out, int window, wo_t *browsebtn));
uint16_t *dialog_load_icon(char *path, int *width, int *height);

#define MAX_DIALOGS 10

#define DIALOG_MSG 0
#define DIALOG_INPUT 1
#define DIALOG_COLOURPICKER 2
#define DIALOG_FILEPICKER 2
#define DIALOG_SETTINGS 3
#define DIALOG_YESNO 4

#endif