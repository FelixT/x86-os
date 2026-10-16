#include "prog.h"
#include "lib/draw.h"
#include "lib/dialogs.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_canvas.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "../lib/maths.h"

typedef struct file_t {
   char name[16];
   int type;
   int x, y;
   wo_t *wo;
   bool open;
} file_t;

typedef struct folder_t {
   char path[256];
   int x, y;
   int w, h;
   wo_t *selected;
   int size;
} folder_t;

#define ITEM_WIDTH 70
#define ITEM_HEIGHT 75
#define ITEM_PADDING 5

uint16_t *file_icon, *folder_icon;
int folder_w, folder_h;
int file_w, file_h;

void drag_item(wo_t *wo, draw_context_t ctx, int x, int y) {
   dialog_t *dialog = dialog_from_window(ctx.window);
   if(!dialog || !dialog->active) return;

   wo->x = x - wo->width/2;
   wo->y = y - wo->height/2;

   clear_w(dialog->ui->window);
   ui_redraw(dialog->ui);
}

bool load_folder(char *path);

void click_item(wo_t *wo, draw_context_t ctx, int x, int y) {
   dialog_t *dialog = dialog_from_window(ctx.window);
   if(!dialog || !dialog->active) return;
   folder_t *folder = dialog->state;

   if(wo == folder->selected) {
      debug_println("selected");
      file_t *file = folder->selected->user_data;
      if(file->type == FS_TYPE_DIR) {
         char path[256];
         sprintf(path, "%s/%s", folder->path, file->name);
         load_folder(path);
      }
   } else {
      debug_println("not selected");
      ui_bring_to_front(dialog->ui, wo);
   }
   folder->selected = wo;
   get_canvas(wo)->colour_bg = rgb16(200, 200, 200);
   canvas_click(wo, ctx, x, y);
   clear_w(dialog->ui->window);
   ui_redraw(dialog->ui);
}

void unfocus_item(wo_t *wo, draw_context_t ctx) {
   dialog_t *dialog = dialog_from_window(ctx.window);
   if(!dialog || !dialog->active) return;
   folder_t *folder = dialog->state;

   if(folder->selected == wo)
      folder->selected = NULL;
   get_canvas(wo)->colour_bg = 0xFFFF;
   canvas_unfocus(wo, ctx);
   clear_w(dialog->ui->window);
   ui_redraw(dialog->ui);
}

static wo_t *create_item(dialog_t *dialog, int x, int y, fs_dir_entry_t *entry) {
   file_t *file = malloc(sizeof(file_t));
   strcpy(file->name, entry->filename);
   file->type = entry->type;
   file->x = x;
   file->y = y;

   wo_t *canvas = create_canvas(x, y, ITEM_WIDTH, ITEM_HEIGHT);
   canvas->user_data = file;
   canvas->drag_func = &drag_item;
   canvas->click_func = &click_item;
   canvas->unfocus_func = &unfocus_item;
   ui_add(dialog->ui, canvas);
   
   wo_t *image_canvas = create_canvas(15, 5, 40, 40);
   canvas_add(canvas, image_canvas);
   wo_t *image;
   if(file->type == FS_TYPE_FILE)
      image = create_image(0, 0, file_w, file_h, file_icon);
   else
      image = create_image(0, 0, folder_w, folder_h, folder_icon);
   canvas_add(image_canvas, image);
   canvas_item_center(image_canvas, image);

   wo_t *label = create_label(5, 50, 60, 20, entry->filename);
   canvas_add(canvas, label);
   
   return canvas;
}

bool load_folder(char *path) {
   // loads folder in new window

   int dir_size = read_dir(path, NULL, 0);
   if(dir_size < 0) return false;

   fs_dir_entry_t *dir_entries = malloc(dir_size*sizeof(fs_dir_entry_t));
   int read_count = read_dir(path, dir_entries, dir_size);
   int size = min(read_count, dir_size);

   int w = create_window(320, 180);
   if(w < -1) {
      free(dir_entries);
      return false;
   }

   int index = get_free_dialog();
   if(index < 0) {
      free(dir_entries);
      close_window(w);
      return false;
   }

   dialog_t *dialog = get_dialog(index);
   if(!dialog_init(dialog, w)) {
      free(dir_entries);
      close_window(w);
      return false;
   }

   folder_t *folder = malloc(sizeof(folder_t));
   coord_t pos = get_window_position(w);
   folder->size = size;
   folder->x = pos.x;
   folder->y = pos.y;
   folder->w = 320;
   folder->h = 160;
   folder->selected = NULL;
   strcpy(folder->path, path);
   dialog->state = folder;

   char name[32] = "";
   strsplit_last(NULL, name, path, '/');
   if(strlen(name) > 0) 
      dialog_set_title(dialog, name);

   wo_t *status = create_canvas(0, 0, folder->w, 20);
   status->fixed = true;
   ui_add(dialog->ui, status);
   char buf[256];
   sprintf(buf, "%i items", folder->size-2);
   wo_t *label = create_label(10, 2, 100, 16, buf);
   canvas_add(status, label);

   label = create_label(120, 2, 180, 16, path);
   canvas_add(status, label);

   int offsetY = 24;

   int row = 0;
   int col = 0;

   int cols = max(1, (folder->w - ITEM_PADDING) / (ITEM_WIDTH + ITEM_PADDING));

   for(int i = 2; i < folder->size; i++) {
      fs_dir_entry_t *entry = &dir_entries[i];
      int x = ITEM_PADDING + col * (ITEM_WIDTH + ITEM_PADDING);
      int y = offsetY + ITEM_PADDING + row * (ITEM_HEIGHT + ITEM_PADDING);
      create_item(dialog, x, y, entry);
      col++;
      if(col == cols) {
         col = 0;
         row++;
      }
   }

   free(dir_entries);

   int totalY = offsetY*2 + ITEM_PADDING + (row + (col > 0)) * (ITEM_HEIGHT + ITEM_PADDING);
   dialog_init_scroll(dialog);
   set_content_height(totalY, w);

   int newh = min(280, totalY);
   if(newh > folder->h) {
      folder->h = newh;
      set_window_size(folder->w, folder->h, w);
      dialog->surface = get_surface_w(w);
   }

   ui_redraw(dialog->ui);

   return true;
}

void _start() {
   close_window(-1);

   folder_icon = dialog_load_icon("/bmp/folder20.bmp", &folder_w, &folder_h);
   file_icon = dialog_load_icon("/bmp/file20.bmp", &file_w, &file_h);

   if(!load_folder("/home/desktop"))
      exit(1);

   while(true) {
      snooze();
   }
}
