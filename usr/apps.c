// read /sys folder for .elfs

#include <stddef.h>
#include <stdbool.h>

#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_grid.h"
#include "lib/sort.h"
#include "lib/draw.h"

int items = 0;
ui_mgr_t *ui = NULL;
wo_t *grid;
surface_t surface;
int scrollOffsetY = 0;

void app_launch(wo_t *label) {
   label_t *label_data = label->data;  
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

   scrollOffsetY = offsetY;

   end_subroutine();
}

void click(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_hover(ui, -1, -1);
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

void resize(uint32_t fb, int w, int h, int window) {
   (void)fb;
   (void)w;
   (void)h;
   (void)window;
   if(!ui)
      end_subroutine();

   surface = get_surface();
   ui->surface = &surface;
   ui_draw(ui);
   end_subroutine();
}

void keypress(int c, int window) {
   if(c == 0x100)
      scroll_to(scrollOffsetY - 10, window);
   else if(c == 0x101)
      scroll_to(scrollOffsetY + 10, window);
   else if(c == 0x1B) {
      close_window(-1);
      exit(0);
   }
   end_subroutine();
}

void mouseout(int window) {
   (void)window;
   ui_hover(ui, -1, -1);
   end_subroutine();
}

void _start() {
   
   fs_dir_content_t *content = read_dir("/sys");
   if(content->entries)
      sort(content->entries, content->size, sizeof(fs_dir_entry_t), sort_filename);

   override_draw(0, -1);
   override_click((uint32_t)&click, -1);
   override_release((uint32_t)&release, -1);
   override_resize((uint32_t)&resize, -1);
   override_hover((uint32_t)&hover, -1);
   override_keypress((uint32_t)&keypress, -1);
   override_mouseout((uint32_t)&mouseout, -1);

   items = 0;

   // get number of items
   for(int i = 0; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(strendswith(entry->filename, ".elf"))
         items++;
   }

   int content_height = items*20 + 4;
   int w_height = content_height;
   if(w_height > 480)
      w_height = 480;

   set_window_size(120, w_height);

   // move to bottom right
   int cornerx = get_surface_w(-2).width;
   int cornery = get_surface_w(-2).height;
   int w_x = cornerx - 120 - 4;
   int w_y = cornery - w_height - 4;
   set_window_position(w_x, w_y, -1);

   set_window_title("Apps");

   surface = get_surface();
   ui = ui_init(&surface, -1);

   create_scrollbar(&scroll, -1);
   int width = set_content_height(content_height, -1) - 2;

   // create items*1 grid, with each cell having 20px height
   grid = create_grid(1, 1, width, items*20, items, 1);
   grid_t *grid_data = grid->data;
   grid_data->colour_border_light = rgb16(240, 240, 240);
   grid_data->colour_border_dark = grid_data->colour_border_light;
   grid_data->colour_bg = rgb16(230, 230, 230);

   int row = 0;

   // add buttons for each app to grid
   for(int i = 0; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(strendswith(entry->filename, ".elf")) {
         char name[10];
         strsplit(name, NULL, entry->filename, '.');
         wo_t *wo = create_label(1, 1, width-2, 18, name);
         label_t *label = wo->data;
         label->colour_txt = rgb16(10, 30, 100);
         label->release_func = (void*)&app_launch;
         label->halign = false;
         label->padding_left = 14;
         //label->colour_bg_hover = rgb16(255, 255, 255);
         grid_add(grid, wo, row, 0);

         row++;
      }
   }

   ui_add(ui, grid);

   ui_draw(ui);

   free(content, sizeof(fs_dir_content_t) * content->size);

   redraw();
   set_window_minimised(false, -1);
   
   while(true) {
      yield();
   }

   exit(0);
}