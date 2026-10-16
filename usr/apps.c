// read /sys folder for .elfs

#include <stddef.h>
#include <stdbool.h>

#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "../lib/maths.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_canvas.h"
#include "lib/ui/ui_grid.h"
#include "lib/ui/ui_button.h"
#include "lib/sort.h"
#include "lib/draw.h"

int items = 0;
ui_mgr_t *ui = NULL;
wo_t *launch_label;
wo_t *grid;
wo_t *current_apps_canvas;
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
   launch_task(path, 0, NULL, false, false);
}

int sort_filename(const void *v1, const void *v2) {
   const fs_dir_entry_t *d1 = (const fs_dir_entry_t*)v1;
   const fs_dir_entry_t *d2 = (const fs_dir_entry_t*)v2;

   return strcmp(d1->filename, d2->filename);
}

void scroll(int deltaY, int offsetY, int window) {
   (void)deltaY;
   (void)window;

   if(!ui)
      end_subroutine();
      
   clear();
   ui_scroll(ui, deltaY, offsetY);
   ui_redraw(ui);

   scrollOffsetY = offsetY;

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

void resize(uint32_t fb, int w, int h, int window) {
   (void)fb;
   (void)w;
   (void)h;
   (void)window;
   if(!ui)
      end_subroutine();

   surface = get_surface();
   ui->surface = &surface;
   ui_redraw(ui);
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
   if(!ui)
      end_subroutine();
   ui_hover(ui, -1, -1);
   end_subroutine();
}

void open_clicked(wo_t *btn, int w) {
   (void)w;
   int taskid = (int)btn->user_data;
   focus_task(taskid);
}

wo_t *check_open_tasks() {
   int task_count = get_tasks(NULL, 0);
   api_task_t *tasks = malloc(sizeof(api_task_t)*task_count);
   int read = get_tasks(tasks, task_count);
   int current_task = get_taskid();

   int w = 100;

   // positioned in scrolled coords, like the wos ui_scroll has moved
   wo_t *canvas = create_canvas(0, -scrollOffsetY, w, 100);
   int y = 2;
   wo_t *label = create_label(2, y, w-4, 20, "Running:");
   canvas_add(canvas, label);
   y += 22;

   for(int i = 0; i < read; i++) {
      api_task_t *task = &tasks[i];
      if(task->id == current_task) continue;
      if(strlen(task->main_window_name) == 0) continue;
      wo_t *button = create_button(2, y, w-4, 20, task->main_window_name);
      button->user_data = (void*)task->id;
      set_button_release(button, &open_clicked);
      canvas_add(canvas, button);
      y += 22;
   }

   canvas->height = y;

   free(tasks);

   return canvas;
}

void refresh_open() {
   ui_remove(ui, current_apps_canvas);
   destroy_wo(current_apps_canvas);
   free(current_apps_canvas);
   current_apps_canvas = check_open_tasks();
   ui_add(ui, current_apps_canvas);
}

void update_widths(int contentWidth) {
   current_apps_canvas->x = (contentWidth-current_apps_canvas->width)/2;
   grid->width = contentWidth;
   launch_label->width = contentWidth-2;
}

void reposition() {
   // reposition y (unscrolled)
   int y = 2 + current_apps_canvas->height;
   launch_label->y = y - scrollOffsetY;
   y += 20;
   grid->y = y - scrollOffsetY;

   y += grid->height;

   // resize window
   int height = min(480, y + 8);
   if(height != get_height()) {
      set_window_size(surface.width, height, -1);
   }
   int contentW = set_content_height(y + 8, -1);
   surface = get_surface();
   update_widths(contentW);

   scroll_to(scrollOffsetY, -1);
}

void refresh() {
   refresh_open();
   reposition();
   clear();
   ui_redraw(ui);

   queue_event(&refresh, 1000, NULL);
   end_subroutine();
}

void _start() {
   set_window_title("Apps");

   current_apps_canvas = check_open_tasks();

   char *path = "/sys";
   int file_count = read_dir(path, NULL, 0);
   bool read = file_count >= 0;
   fs_dir_entry_t *entries;
   if(read) {
      entries = malloc(sizeof(fs_dir_entry_t)*file_count);
      int read_count = read_dir(path, entries, file_count);
      read = read_count >= 0;
      if(!read) free(entries);
      else if(read_count < file_count) file_count = read_count;
   }
   if(!read) {
      printf("Couldn't read %s folder\n", path);
      set_window_minimised(false, -1);
      exit(1);
      return;
   }
   sort(entries, file_count, sizeof(fs_dir_entry_t), sort_filename);
   
   wm_override(WM_DRAW, 0, -1);
   wm_override(WM_CLICK, &click, -1);
   wm_override(WM_MOUSERELEASE, &release, -1);
   wm_override(WM_RESIZE, &resize, -1);
   wm_override(WM_HOVER, &hover, -1);
   wm_override(WM_KEYPRESS, &keypress, -1);
   wm_override(WM_MOUSEOUT, &mouseout, -1);

   create_scrollbar(&scroll, -1);
   int width = 120;

   current_apps_canvas->x = (width-current_apps_canvas->width)/2;

   int y = 2 + current_apps_canvas->height;

   launch_label = create_label(2, y, width-2, 20, "Launch:");
   get_label(launch_label)->colour_bg = 0xFFFF;
   get_label(launch_label)->filled = true;

   y += 20;

   items = 0;

   // get number of items
   for(int i = 0; i < file_count; i++) {
      fs_dir_entry_t *entry = &entries[i];
      if(strendswith(entry->filename, ".elf"))
         items++;
   }

   // create items*1 grid, with each cell having 21px height
   grid = create_grid(1, y, width, items*21, items, 1);
   grid_t *grid_data = grid->data;
   grid_data->colour_border_light = rgb16(240, 240, 240);
   grid_data->colour_border_dark = grid_data->colour_border_light;
   grid_data->colour_bg_hovered = rgb16(244, 244, 244);
   grid_data->colour_bg = 0xFFFF;

   int row = 0;

   // add buttons for each app to grid
   for(int i = 0; i < file_count; i++) {
      fs_dir_entry_t *entry = &entries[i];
      if(strendswith(entry->filename, ".elf")) {
         char name[10];
         strsplit(name, NULL, entry->filename, '.');
         wo_t *wo = create_label(1, 1, width-2, 20, name);
         label_t *label = wo->data;
         label->colour_txt = rgb16(30, 30, 30);
         label->release_func = (void*)&app_launch;
         label->halign = false;
         label->padding_left = 14;
         //label->colour_bg_hover = rgb16(255, 255, 255);
         grid_add(grid, wo, row, 0);

         row++;
      }
   }

   y += grid->height;

   int height = min(480, y + 8);
   set_window_size(width, height, -1);
   int contentW = set_content_height(y + 8, -1);

   surface = get_surface();
   ui = ui_init(&surface, -1);

   update_widths(contentW);

   // move to bottom right
   int cornerx = get_surface_w(-2).width;
   int cornery = get_surface_w(-2).height;
   int w_x = cornerx - width - 4;
   int w_y = cornery - height - 4;
   set_window_position(w_x, w_y, -1);

   ui_add(ui, current_apps_canvas);
   ui_add(ui, launch_label);
   ui_add(ui, grid);

   ui_draw(ui);

   free(entries);

   redraw();
   set_window_minimised(false, -1);

   queue_event(&refresh, 1000, NULL);
   
   while(true) {
      snooze();
   }

   exit(0);
}