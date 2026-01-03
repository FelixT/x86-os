// read /sys folder for .elfs

#include <stddef.h>
#include <stdbool.h>

#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_button.h"
#include "lib/ui/ui_grid.h"
#include "lib/sort.h"
#include "lib/draw.h"

int items = 0;
ui_mgr_t *ui = NULL;
wo_t *grid;
surface_t surface;

void app_launch(wo_t *label) {
   button_t *label_data = label->data;  
   if(!label_data)
      return;
   char path[256];
   strcpy(path, "/sys/");
   strcat(path, label_data->label);
   strcat(path, ".elf");
   debug_println(path);
   redraw();
   launch_task(path, 0, NULL, false);
}

int sort_filename(const void *v1, const void *v2) {
   const fs_dir_entry_t *d1 = (const fs_dir_entry_t*)v1;
   const fs_dir_entry_t *d2 = (const fs_dir_entry_t*)v2;

   return strcmp(d1->filename, d2->filename);
}

void scroll(int deltaY, int offsetY, int window) {
   (void)deltaY;
   (void)window;

   clear();

   int y = 2;
   grid->y = y - offsetY;
   
   if(ui)
      ui_draw(ui);
   redraw();

   end_subroutine();
}

void click(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_click(ui, x, y);

   end_subroutine();
}

void release(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_release(ui, x, y);

   end_subroutine();
}

void hover(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_hover(ui, x, y);

   end_subroutine();
}

void resize() {
   if(!ui)
      end_subroutine();

   surface = get_surface();
   ui->surface = &surface;
   ui_draw(ui);
   end_subroutine();
}

void _start() {
   
   set_window_size(120, 280);
   set_window_title("Apps");

   fs_dir_content_t *content = read_dir("/sys");
   if(content->entries)
      sort(content->entries, content->size, sizeof(fs_dir_entry_t), sort_filename);
   override_draw(0);
   override_click((uint32_t)&click, -1);
   override_release((uint32_t)&release, -1);
   override_resize((uint32_t)&resize, -1);
   override_hover((uint32_t)&hover, -1);

   items = 0;

   surface = get_surface();
   ui = ui_init(&surface, -1);

   // get number of items
   for(int i = 0; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(strendswith(entry->filename, ".elf"))
         items++;
   }

   create_scrollbar(&scroll, -1);
   set_content_height(items*20 + 4, -1);

   int width = get_width() - 4;

   // create items*1 grid, with each cell having 20px height
   grid = create_grid(2, 2, width, items*20, items, 1);
   grid_t *grid_data = grid->data;
   grid_data->colour_border_light = rgb16(230, 230, 230);
   grid_data->colour_border_dark = grid_data->colour_border_light;

   int row = 0;

   // add buttons for each app to grid
   for(int i = 0; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(strendswith(entry->filename, ".elf")) {
         char name[10];
         strsplit(name, NULL, entry->filename, '.');
         wo_t *wo = create_button(1, 1, width-2, 18, name);
         button_t *button = wo->data;
         button->colour_txt = rgb16(10, 30, 100);
         button->release_func = (void*)&app_launch;
         button->colour_bg_hover = rgb16(255, 255, 255);
         grid_add(grid, wo, row, 0);

         row++;
      }
   }

   ui_add(ui, grid);

   ui_draw(ui);

   free(content, sizeof(fs_dir_content_t) * content->size);

   redraw();
   
   while(true) {
      yield();
   }

   exit(0);
}