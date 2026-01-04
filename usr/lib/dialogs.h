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

typedef struct dialog_t {
   bool active;
   int type;
   int window;
   surface_t surface;
   void (*callback)(char *out, int window);
   ui_mgr_t *ui;
   // txtinput specific
   wo_t *input_wo;
   // filepicker specific
   fs_dir_content_t *dir;
   uint16_t *file_icon_data;
   uint16_t *folder_icon_data;
   int content_height;
   int content_offsetY;
} dialog_t;

bool dialog_msg(char *title, char *text);
int dialog_input(char *text, void *return_func);
int dialog_colourpicker(uint16_t colour, void *return_func);
dialog_t *get_dialog(int index);
dialog_t *dialog_from_window(int window);
int dialog_filepicker(char *startdir, void *return_func);
void dialog_init_overrides(int window);
int get_free_dialog();
void dialog_init(dialog_t *dialog, int window);

#define MAX_DIALOGS 10

#define DIALOG_MSG 0
#define DIALOG_INPUT 1
#define DIALOG_COLOURPICKER 2
#define DIALOG_FILEPICKER 2

#endif